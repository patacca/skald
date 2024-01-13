#pragma once

#include <cstdint>

#include "binaryninjaapi.h"

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
    UNSUPPORTED,
};

class Skald {
   public:
    // Delete copy constructor and copy assignment
    Skald(const Skald&) = delete;
    Skald& operator=(const Skald&) = delete;

    static Skald& get_instance() {
        static Skald singleton;
        return singleton;
    };

    bool init();
    void run(BinaryNinja::BinaryView* view);

   private:
    BinaryNinja::BinaryView* _view;
    std::vector<uint64_t> typeinfoClasses;  // Vector containing the address of each typeinfo class

    Skald();
    void parseRTTI(unsigned long address, const std::string& symbolName);
    BinaryNinja::Ref<BinaryNinja::Type> defineClassType();
    BinaryNinja::Ref<BinaryNinja::Type> defineBaseClass();
    BinaryNinja::Ref<BinaryNinja::Type> defineVmiClassType(uint32_t base_count);
    BinaryNinja::Ref<BinaryNinja::Type> defineSiClassType();
    BinaryNinja::Ref<BinaryNinja::Type> definePbaseClassType();
};

}  // namespace skald