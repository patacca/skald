#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "binaryninjaapi.h"
#include "inheritance_graph.h"
#include "type_accessor.h"

namespace skald {

enum TypeInfo : uint32_t {
    CLASS_TYPE_INFO,
    VMI_CLASS_TYPE_INFO,
    SI_CLASS_TYPE_INFO,
    FUNCTION_TYPE_INFO,
    PBASE_TYPE_INFO,
    POINTER_TYPE_INFO,
    FUNDAMENTAL_TYPE_INFO,
    ARRAY_TYPE_INFO,
    ENUM_TYPE_INFO,
    POINTER_TO_MEMBER_TYPE_INFO,
    UNSUPPORTED,
};

class Skald {
   public:
    Skald(BinaryNinja::BinaryView* view);
    bool init();
    void run();

   private:
    BinaryNinja::BinaryView* _view;
    std::vector<uint64_t> typeinfoClasses;  // Vector containing the address of each typeinfo class
    InheritanceGraph inheritanceGraph;      // Class inheritance graph
    TypeAccessor accessor;                  // Accessor for reading values from typed variables

    void parseVtable(uint64_t typeInfoPointer);
    void parseRTTI(unsigned long address, const std::string& symbolName);
    BinaryNinja::Ref<BinaryNinja::Type> createVtableType(const std::string_view& className,
                                                         uint64_t addr, uint32_t size);

    BinaryNinja::Ref<BinaryNinja::Type> defineClassType();
    BinaryNinja::Ref<BinaryNinja::Type> defineBaseClass();
    BinaryNinja::Ref<BinaryNinja::Type> defineVmiClassType(uint32_t base_count);
    BinaryNinja::Ref<BinaryNinja::Type> defineSiClassType();
    BinaryNinja::Ref<BinaryNinja::Type> definePbaseClassType();
    BinaryNinja::Ref<BinaryNinja::Type> definePointerToMemberClassType();
};

}  // namespace skald