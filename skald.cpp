#include "skald.h"

#include <fmt/format.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "binaryninjaapi.h"

namespace skald {

using BinaryNinja::QualifiedName;
using BinaryNinja::Ref;
using BinaryNinja::Structure;
using BinaryNinja::StructureBuilder;
using BinaryNinja::Type;

std::array<std::string, 2> type_info_classes = {
    "_ZTVN10__cxxabiv117__class_type_infoE",
    "_ZTVN10__cxxabiv121__vmi_class_type_infoE",
};

Skald::Skald() { BinaryNinja::LogDebug("Initializing skald plugin"); }

bool Skald::init() {
    BinaryNinja::PluginCommand::Register(
        "skald", "RTTI recovery plugin",
        [](BinaryNinja::BinaryView* view) { Skald::get_instance().run(view); });

    return true;
}

void Skald::run(BinaryNinja::BinaryView* view) {
    this->_view = view;

    BinaryNinja::LogDebug("Searching for RTTI");

    const std::string id = _view->BeginUndoActions();

    // Search for RTTI entry point by looking at the relocations
    for (auto [start, end] : view->GetRelocationRanges()) {
        for (const auto& rel : view->GetRelocationsAt(start)) {
            // const BNRelocationInfo info = rel->GetInfo();
            const auto symbol = rel->GetSymbol();

            if (!symbol)  // No symbol for this relocation, just skip it
                continue;

            const std::string symbolName = symbol->GetRawName();

            for (size_t i = 0; i < type_info_classes.size(); ++i)
                if (symbolName == type_info_classes[i])
                    this->parseRTTI(start, static_cast<TypeInfo>(i));
        }
    }

    // Parse vtables
    for (uint64_t rttiAddress : this->typeinfoClasses) {
        // Find the data references that are not part of a type_info struct
        for (uint64_t addr : _view->GetDataReferences(rttiAddress)) {
            BinaryNinja::DataVariable var;
            _view->GetDataVariableAtAddress(addr, var);
            BinaryNinja::LogInfo("variabl from %p -> %s", rttiAddress, var.type->GetString().c_str());
        }
    }

    // Parse VTT

    _view->CommitUndoActions(id);
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

void Skald::parseRTTI(unsigned long address, const TypeInfo derivedType) {
    this->typeinfoClasses.push_back(address);

    switch (derivedType) {
        case CLASS_TYPE_INF0: {
            const auto type = this->defineClassType();
            _view->DefineUserDataVariable(address, type->WithConfidence(0xff));
            break;
        }
        case VMI_CLASS_TYPE_INF0: {
            // Read uint32_t at address + 0x14, that's base_count
            uint32_t base_count;
            _view->Read(static_cast<void*>(&base_count), address + 0x14, 4);
            const auto type = this->defineVmiClassType(base_count);
            _view->DefineUserDataVariable(address, type->WithConfidence(0xff));
            break;
        }
        default:
            BinaryNinja::LogWarn("std::type_info derived class at address %p is not implemented.",
                                 (void*)address);
            this->typeinfoClasses.pop_back();  // Remove the class from the list
            break;
    }
}

}  // namespace skald

extern "C" {
// Tells Binary Ninja which version of the API you compiled against
BN_DECLARE_CORE_ABI_VERSION

// Function run on plugin startup, do simple initialization here (Settings, BinaryViewTypes, etc)
BINARYNINJAPLUGIN bool CorePluginInit() { return skald::Skald::get_instance().init(); }

// (Optional) Function to add other plugin dependencies in case your plugin requires them
// BINARYNINJAPLUGIN void CorePluginDependencies() {
//     // For example, if you require the x86 to be loaded before your plugin
//     AddRequiredPluginDependency("arch_x86");
// }
}
