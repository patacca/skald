#include "type_accessor.h"

#include <fmt/format.h>

#include <any>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "binaryninjaapi.h"

namespace skald {

using BinaryNinja::DataVariable;
using BinaryNinja::Ref;
using BinaryNinja::Structure;

TypeAccessor::TypeAccessor(BinaryNinja::BinaryView *view) : _view(view) {}

std::string TypeAccessor::readString(uint64_t address, const std::string &name) {
    DataVariable var;
    _view->GetDataVariableAtAddress(address, var);
    if (!var.type->IsStructure())
        throw std::runtime_error(fmt::format(
            "Trying to read string from a member of a non structured variable type at address %p",
            address));

    // Get member offset
    Ref<Structure> t = var.type->GetStructure();
    BinaryNinja::StructureMember r;
    t->GetMemberByName(name, r);

    // Get pointer to (char *) string
    uint64_t ptr;
    if (r.type->GetWidth() > 8)
        throw std::runtime_error(fmt::format(
            "Trying to read string from a non-string member type. Address = %p  member = `%s`",
            address, name));
    _view->Read(&ptr, address + r.offset, r.type->GetWidth());

    // Get the length of the string
    BNStringReference str;
    _view->GetStringAtAddress(ptr, str);

    // Read the string
    return _view->ReadBuffer(ptr, str.length).ToEscapedString();
}

std::any TypeAccessor::readValue(const BinaryNinja::Ref<BinaryNinja::Type> &type,
                                 uint64_t address) {
    if (type->IsVoid() || type->IsFunction()) {
        return 0;
    } else if (type->IsBool()) {
        uint64_t data;
        _view->Read(&data, address, type->GetWidth());
        return !!data;
    } else if (type->IsInteger() || type->IsPointer()) {
        uint64_t data;
        _view->Read(&data, address, type->GetWidth());
        switch (type->GetWidth()) {
            case 1:
                return static_cast<bool>(data);
            case 2:
                if (type->IsSigned())
                    return static_cast<int16_t>(data);
                else
                    return static_cast<uint16_t>(data);
            case 4:
                if (type->IsSigned())
                    return static_cast<int32_t>(data);
                else
                    return static_cast<uint32_t>(data);
            case 8:
                if (type->IsSigned())
                    return static_cast<int64_t>(data);
                else
                    return static_cast<uint64_t>(data);
            default:
                return data;
        };
    } else if (type->IsStructure()) {
        std::unordered_map<std::string, std::any> retVal;

        Ref<Structure> t = type->GetStructure();
        for (auto &member : t->GetMembers())
            retVal[member.name] = this->readValue(member.type, address + member.offset);

        return retVal;
    } else if (type->IsArray()) {
        uint64_t count = type->GetElementCount();
        std::vector<std::any> retVal(count);
        auto childType = type->GetChildType();
        for (int i = 0; i < count; ++i)
            retVal[i] = this->readValue(childType, address + i * childType->GetWidth());
        return retVal;
        // } else if (type->IsFloat()) {
        //     // 		return float(self)
        // } else if (type->IsWideChar()) {
        //     // 		return data.decode(f"utf-16-{'le' if self.endian ==
        //     Endianness.LittleEndian
        //     // else 'be'}")
    } else {
        throw std::runtime_error(fmt::format("Unhandled `Type` {}", TYPE_CLASS[type->GetClass()]));
    }
}

std::unordered_map<std::string, std::any> TypeAccessor::readVar(uint64_t address) {
    DataVariable var;
    _view->GetDataVariableAtAddress(address, var);
    if (!var.type->IsStructure())
        throw std::runtime_error(fmt::format(
            "Trying to read string from a member of a non structured variable type at address %p",
            address));

    // Return container
    std::unordered_map<std::string, std::any> retVal;

    // Get members
    Ref<Structure> t = var.type->GetStructure();
    for (auto &member : t->GetMembers())
        retVal[member.name] = this->readValue(member.type, address + member.offset);

    return retVal;
}

}  // namespace skald