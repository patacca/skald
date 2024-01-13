#pragma once

#include <cstdint>

#include "binaryninjaapi.h"

namespace skald {

enum TypeInfo : uint32_t {
    CLASS_TYPE_INF0,
    VMI_CLASS_TYPE_INF0,
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
    void parseRTTI(unsigned long address, const TypeInfo derivedType);
    BinaryNinja::Ref<BinaryNinja::Type> defineClassType();
    BinaryNinja::Ref<BinaryNinja::Type> defineBaseClass();
    BinaryNinja::Ref<BinaryNinja::Type> defineVmiClassType(uint32_t base_count);
};

}  // namespace skald