#pragma once

#include <any>
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

#include "binaryninjaapi.h"

namespace skald {

static const constexpr std::array<const char *, 13> TYPE_CLASS = {
    "VoidTypeClass",      "BoolTypeClass",        "IntegerTypeClass", "FloatTypeClass",
    "StructureTypeClass", "EnumerationTypeClass", "PointerTypeClass", "ArrayTypeClass",
    "FunctionTypeClass",  "VarArgsTypeClass",     "ValueTypeClass",   "NamedTypeReferenceClass",
    "WideCharTypeClass"};

class TypeAccessor {
   public:
    TypeAccessor(BinaryNinja::BinaryView *view);
    std::string readString(uint64_t address, const std::string &name);
    std::unordered_map<std::string, std::any> readVar(uint64_t address);
    std::any readValue(const BinaryNinja::Ref<BinaryNinja::Type> &type, uint64_t address);

   private:
    BinaryNinja::BinaryView *_view;
};

}  // namespace skald