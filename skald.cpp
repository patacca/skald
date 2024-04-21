#include "skald.h"

#include <fmt/format.h>

#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "binaryninjaapi.h"
#include "inheritance_graph.h"

namespace skald {

using BinaryNinja::BaseStructure;
using BinaryNinja::DataVariable;
using BinaryNinja::QualifiedName;
using BinaryNinja::Ref;
using BinaryNinja::Structure;
using BinaryNinja::StructureBuilder;
using BinaryNinja::Type;

Skald::Skald(BinaryNinja::BinaryView* view) : _view(view), accessor(view) {}

void Skald::run() {
    // TODO add mutex to avoid multiple skald instances to run at the same time

    BinaryNinja::LogDebug("Searching for RTTI");

    const std::string id = _view->BeginUndoActions();

    // Search for RTTI entry point by looking at the relocations
    for (auto [start, end] : _view->GetRelocationRanges()) {
        for (const auto& rel : _view->GetRelocationsAt(start)) {
            const auto symbol = rel->GetSymbol();

            if (!symbol)  // No symbol for this relocation, just skip it
                continue;

            this->parseRTTI(start, symbol->GetRawName());
        }
    }

    // Parse vtables
    BinaryNinja::LogDebug("Searching for vtables");

    // Queue for BFS
    std::queue<node_identifier_t> queue =
        this->inheritanceGraph.getRoots() |
        std::views::transform([](const Node& node) -> node_identifier_t { return node.id; }) |
        std::ranges::to<std::queue>();

    // Node parents' counter
    std::unordered_map<node_identifier_t, size_t> counters;

    // Each object that has a RTTI can potentially have a vtable, hence do a BFS on the class
    // inheritance polytree. It is mandatory that the parents are accessed before the children
    while (!queue.empty()) {
        // Pop current node
        const Node& node = this->inheritanceGraph.getNodeById(queue.front());
        queue.pop();

        // Find the vtable for the current node by looking at the data references that are not part
        // of a type_info struct
        for (uint64_t addr : _view->GetDataReferences(node.rttiAddress)) {
            DataVariable var;
            _view->GetDataVariableAtAddress(addr, var);

            // RTTI type previously defined. Skip it
            if (var.type->GetString().find("_class_type") != std::string::npos) continue;

            // Create vtable struct
            this->parseVtable(addr);
        }

        // Add the children to the queue
        for (const auto& [childId, e_flags] : node.children) {
            // On the first encounter initialize the parents' counter
            if (!counters.contains(childId))
                counters[childId] = this->inheritanceGraph.getNodeById(childId).parents.size();

            --counters[childId];         // Reduce the number of remeaning parents by one
            if (counters[childId] == 0)  // No parents left, it can be safely explored
                queue.push(childId);
        }
    }

    // Parse VTT

    _view->CommitUndoActions(id);
}

void Skald::parseVtable(uint64_t typeInfoPointer) {
    uint64_t rttiAddr = std::any_cast<uint64_t>(
        this->accessor.readValue(Type::IntegerType(8, false, "uin64_t"), typeInfoPointer));
    auto curr_section = _view->GetSectionsAt(typeInfoPointer);

    uint64_t vtableStart = typeInfoPointer + 8;

    try {
        Node& node = this->inheritanceGraph.getNodeByAddr(rttiAddr);

        // if there are no children or there is only one and it is public non-virtual
        if (node.isLeaf() ||
            (node.children.size() == 1 && node.children[0].second & EdgeFlag::PUBLIC &&
             !(node.children[0].second & EdgeFlag::VIRTUAL))) {
            // offset_to_top
            // RTTI pointer
            // vtable
            uint64_t method = std::any_cast<uint64_t>(
                this->accessor.readValue(Type::IntegerType(8, false, "uint64_t"), vtableStart));
            uint32_t vtableSize = 0;

            // There is no reliable way of knowing how large a vtable is but to rely on heuristics
            while (_view->IsOffsetExecutable(method)) {
                ++vtableSize;

                // Read next method in vtable
                uint64_t next_method_ptr = vtableStart + 8 * vtableSize;
                if (!_view->IsValidOffset(next_method_ptr) ||
                    !_view->IsOffsetReadable(next_method_ptr) ||
                    _view->GetSectionsAt(next_method_ptr) != curr_section)
                    break;
                method = std::any_cast<uint64_t>(this->accessor.readValue(
                    Type::IntegerType(8, false, "uint64_t"), next_method_ptr));
            }

            BinaryNinja::LogInfo("vtable size = %d", vtableSize);
            std::string className = this->accessor.readString(rttiAddr, "__type_name");
            auto it = className.begin();
            while (it != className.end() && *it >= '0' && *it <= '9') ++it;
            std::string_view classNameDemangled = std::string_view(it, className.end());
            auto type = this->createVtableType(classNameDemangled, vtableStart, vtableSize);

            // Assign variable
            _view->DefineUserDataVariable(vtableStart, type->WithConfidence(0xff));

            // Create user symbol
            auto* symbol =
                new BinaryNinja::Symbol(BNSymbolType::DataSymbol,
                                        fmt::format("vtable_{}", classNameDemangled), vtableStart);
            _view->DefineUserSymbol(symbol);

            BinaryNinja::LogDebug("Found vtable at addr 0x%lx for RTTI at address 0x%lx (%s)",
                                  vtableStart, rttiAddr, className.c_str());

        } else {
            BinaryNinja::LogDebug("Found vtable at addr 0x%lx for RTTI at address 0x%lx",
                                  vtableStart, rttiAddr);

            BinaryNinja::LogInfo("Node at address %p has %lu children", (void*)rttiAddr,
                                 node.children.size());
        }
    } catch (const std::invalid_argument& e) {
        BinaryNinja::LogWarn("Node at address %p is not present in graph", (void*)rttiAddr);
    }
}

Ref<Type> Skald::createVtableType(const std::string_view& className, uint64_t startAddr,
                                  uint32_t size) {
    StructureBuilder vtableBuilder;
    vtableBuilder.SetPropagateDataVariableReferences(true);  // same as __vtable or __data_var_ref

    // Add each function pointer
    for (uint64_t funPtr = startAddr; funPtr < startAddr + 8 * size; funPtr += 8) {
        uint64_t addr = std::any_cast<uint64_t>(
            this->accessor.readValue(Type::IntegerType(8, false, "uint64_t"), funPtr));
        // Get function at current address
        auto functions = _view->GetAnalysisFunctionsForAddress(addr);
        if (functions.empty()) {  // No function defined. Create it
            BinaryNinja::LogInfo("No functions at addr %p. Creating one for default platform",
                                 (void*)addr);
            functions.push_back(_view->CreateUserFunction(_view->GetDefaultPlatform(), addr));
        } else if (functions.size() > 1) {  // More than one function, pick the first one
            BinaryNinja::LogWarn(
                "More than one function defined at address %p. Optimistically picking the first "
                "one",
                (void*)addr);
        }

        // Recover previous type definition for the function
        auto functionType = functions[0]->GetType();
        auto retType = functionType->GetChildType();
        auto params = functionType->GetParameters();
        // Use a `void * this` for the time being
        if (params.empty()) params.resize(1);
        params[0] = {"this", Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType())};

        // Create the new function type
        auto newFunType = Type::FunctionType(
            retType, _view->GetDefaultPlatform()->GetDefaultCallingConvention(), params);

        // Adding method to the vtable
        BinaryNinja::LogDebug("Adding function `%s (*%s)(void *this, ...)`",
                              retType->GetString().c_str(),
                              functions[0]->GetSymbol()->GetShortName().c_str());
        vtableBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), newFunType),
                                functions[0]->GetSymbol()->GetShortName());
    }

    // Create the type `vtable_for_className`
    Ref<Structure> vtableStruct = vtableBuilder.Finalize();
    QualifiedName typeName = QualifiedName(fmt::format("vtable_{}_t", className));
    _view->DefineUserType(typeName, Type::StructureType(vtableStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::defineClassType() {
    QualifiedName typeName = QualifiedName("__class_type");
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    StructureBuilder typeInfoBuilder;
    typeInfoBuilder.AddMember(
        Type::PointerType(this->_view->GetDefaultArchitecture(), Type::VoidType()), "type_info");
    typeInfoBuilder.AddMember(Type::PointerType(this->_view->GetDefaultArchitecture(),
                                                Type::IntegerType(1, false, "char")),
                              "__type_name");
    Ref<Structure> typeInfoStruct = typeInfoBuilder.Finalize();
    this->_view->DefineUserType(typeName, Type::StructureType(typeInfoStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::defineBaseClass() {
    QualifiedName typeName = QualifiedName("__base_class_type_info");
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    StructureBuilder typeBuilder;
    typeBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                          "__base_type");
    typeBuilder.AddMember(Type::IntegerType(8, false, "uint64_t"), "__offset_flags");
    Ref<Structure> typeStruct = typeBuilder.Finalize();
    _view->DefineUserType(typeName, Type::StructureType(typeStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::defineVmiClassType(uint32_t base_count) {
    QualifiedName typeName = QualifiedName(fmt::format("__vmi_class_type_{}", base_count));
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    // Define inner struct
    const auto baseClassTypeInfo = this->defineBaseClass();

    StructureBuilder typeInfoBuilder;
    typeInfoBuilder.AddMember(
        Type::PointerType(this->_view->GetDefaultArchitecture(), Type::VoidType()), "type_info");
    typeInfoBuilder.AddMember(Type::PointerType(this->_view->GetDefaultArchitecture(),
                                                Type::IntegerType(1, false, "char")),
                              "__type_name");
    typeInfoBuilder.AddMember(Type::IntegerType(4, false, "unsigned int"), "__flags");
    typeInfoBuilder.AddMember(Type::IntegerType(4, false, "unsigned int"), "__base_count");
    typeInfoBuilder.AddMember(Type::ArrayType(baseClassTypeInfo, base_count), "__base_info");
    Ref<Structure> typeInfoStruct = typeInfoBuilder.Finalize();
    this->_view->DefineUserType(typeName, Type::StructureType(typeInfoStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::defineSiClassType() {
    QualifiedName typeName = QualifiedName("__si_class_type");
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    StructureBuilder typeInfoBuilder;
    typeInfoBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                              "type_info");
    typeInfoBuilder.AddMember(
        Type::PointerType(_view->GetDefaultArchitecture(), Type::IntegerType(1, false, "char")),
        "__type_name");
    typeInfoBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                              "__base_type");
    Ref<Structure> typeInfoStruct = typeInfoBuilder.Finalize();
    _view->DefineUserType(typeName, Type::StructureType(typeInfoStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::definePbaseClassType() {
    QualifiedName typeName = QualifiedName("__pbase_class_type");
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    StructureBuilder typeInfoBuilder;
    typeInfoBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                              "type_info");
    typeInfoBuilder.AddMember(
        Type::PointerType(_view->GetDefaultArchitecture(), Type::IntegerType(1, false, "char")),
        "__type_name");
    typeInfoBuilder.AddMember(Type::IntegerType(4, false, "unsigned int"), "__flags");
    typeInfoBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                              "__pointee");
    Ref<Structure> typeInfoStruct = typeInfoBuilder.Finalize();
    _view->DefineUserType(typeName, Type::StructureType(typeInfoStruct));

    return _view->GetTypeByName(typeName);
}

Ref<Type> Skald::definePointerToMemberClassType() {
    QualifiedName typeName = QualifiedName("__pointer_to_member_type_info");
    const auto type = _view->GetTypeByName(typeName);
    if (!!type) return type;  // Define it once

    StructureBuilder typeInfoBuilder;
    typeInfoBuilder.SetBaseStructures({BaseStructure(this->definePbaseClassType(), 0)});
    typeInfoBuilder.AddMember(Type::PointerType(_view->GetDefaultArchitecture(), Type::VoidType()),
                              "__context");
    Ref<Structure> typeInfoStruct = typeInfoBuilder.Finalize();
    _view->DefineUserType(typeName, Type::StructureType(typeInfoStruct));

    return _view->GetTypeByName(typeName);
}

void Skald::parseRTTI(unsigned long address, const std::string& symbolName) {
    // Mangled names of type_info derived classes
    using namespace std::literals;
    static constexpr const std::array<std::pair<std::string_view, TypeInfo>, 10> type_info_classes{{
        {"_ZTVN10__cxxabiv116__enum_type_infoE"sv, TypeInfo::ENUM_TYPE_INFO},
        {"_ZTVN10__cxxabiv117__class_type_infoE"sv, TypeInfo::CLASS_TYPE_INFO},
        {"_ZTVN10__cxxabiv117__array_type_infoE"sv, TypeInfo::ARRAY_TYPE_INFO},
        {"_ZTVN10__cxxabiv121__vmi_class_type_infoE"sv, TypeInfo::VMI_CLASS_TYPE_INFO},
        {"_ZTVN10__cxxabiv120__si_class_type_infoE"sv, TypeInfo::SI_CLASS_TYPE_INFO},
        {"_ZTVN10__cxxabiv120__function_type_infoE"sv, TypeInfo::FUNCTION_TYPE_INFO},
        {"_ZTVN10__cxxabiv119__pointer_type_infoE"sv, TypeInfo::POINTER_TYPE_INFO},
        {"_ZTVN10__cxxabiv117__pbase_type_infoE"sv, TypeInfo::PBASE_TYPE_INFO},
        {"_ZTVN10__cxxabiv123__fundamental_type_infoE"sv, TypeInfo::FUNDAMENTAL_TYPE_INFO},
        {"_ZTVN10__cxxabiv129__pointer_to_member_type_infoE"sv,
         TypeInfo::POINTER_TO_MEMBER_TYPE_INFO},
    }};

    if (symbolName.find("_ZTVN10__cxxabiv1") == std::string::npos) return;  // Not a type_info class

    TypeInfo derivedType = TypeInfo::UNSUPPORTED;
    for (const auto& type_info : type_info_classes)
        if (symbolName == type_info.first) derivedType = type_info.second;

    this->typeinfoClasses.push_back(address);

    Ref<Type> type;
    switch (derivedType) {
        case CLASS_TYPE_INFO:
        case FUNDAMENTAL_TYPE_INFO:
        case ARRAY_TYPE_INFO:
        case ENUM_TYPE_INFO:
        case FUNCTION_TYPE_INFO:
            type = this->defineClassType();
            break;
        case VMI_CLASS_TYPE_INFO: {
            // Read uint32_t at address + 0x14, that's base_count
            uint32_t base_count;
            _view->Read(static_cast<void*>(&base_count), address + 0x14, 4);
            type = this->defineVmiClassType(base_count);
            break;
        }
        case SI_CLASS_TYPE_INFO:
            type = this->defineSiClassType();
            break;
        case PBASE_TYPE_INFO:
        case POINTER_TYPE_INFO:
            type = this->definePbaseClassType();
            break;
        case POINTER_TO_MEMBER_TYPE_INFO:
            type = this->definePointerToMemberClassType();
            break;
        default:
            BinaryNinja::LogDebug("Unsupported std::type_info derived class at address %p.",
                                  (void*)address);
            this->typeinfoClasses.pop_back();  // Remove the class from the list
            return;
    }

    // Define variable
    _view->DefineUserDataVariable(address, type->WithConfidence(0xff));

    // Read content of RTTI
    auto rtti = this->accessor.readVar(address);
    auto className = this->accessor.readString(address, "__type_name");
    BinaryNinja::LogDebug("Found RTTI at address 0x%lx named `%s`", address, className.c_str());

    // Add the node in the inheritance graph
    if (rtti.contains("__base_count")) {  // Contains >= 1 base class and they might be virtual
        uint32_t baseCount = std::any_cast<uint32_t>(rtti["__base_count"]);
        std::vector<edge_t> children(baseCount);
        auto baseInfoArray = std::any_cast<std::vector<std::any>>(rtti["__base_info"]);
        for (int i = 0; i < baseCount; ++i) {
            auto baseInfo =
                std::any_cast<std::unordered_map<std::string, std::any>>(baseInfoArray[i]);
            children[i] = {
                std::any_cast<uint64_t>(baseInfo["__base_type"]),
                static_cast<EdgeFlag>(std::any_cast<uint64_t>(baseInfo["__offset_flags"]) & 0xff)};
        }
        this->inheritanceGraph.addNode(className, address, children);

    } else if (rtti.contains("__base_type")) {  // Contains only a single, public, non-virtual base
        this->inheritanceGraph.addNode(
            className, address,
            {{{std::any_cast<uint64_t>(rtti["__base_type"]), EdgeFlag::PUBLIC}}});

    } else {  // No base classes
        this->inheritanceGraph.addNode(className, address, {});
    }
}

}  // namespace skald

extern "C" {
// Tells Binary Ninja which version of the API you compiled against
BN_DECLARE_CORE_ABI_VERSION

// Function run on plugin startup, do simple initialization here (Settings, BinaryViewTypes, etc)
// BINARYNINJAPLUGIN bool CorePluginInit() { return skald::Skald::get_instance().init(); }
BINARYNINJAPLUGIN bool CorePluginInit() {
    BinaryNinja::LogDebug("Initializing skald plugin");

    BinaryNinja::PluginCommand::Register(
        "skald", "RTTI recovery plugin",
        [](BinaryNinja::BinaryView* view) { skald::Skald(view).run(); });

    return true;
}
}
