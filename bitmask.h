#pragma once

#include <type_traits>

#define _UNDERLYING_TYPE(_type) std::underlying_type<_type>::type
#define _TO_UNDERLYING_T(_type, obj) static_cast<_UNDERLYING_TYPE(_type)>(obj)

// Macro for using an enum as a bitmask
#define DECLARE_BITMASK(enum_t)                                                                \
    constexpr enum_t operator&(enum_t X, enum_t Y) {                                           \
        return static_cast<enum_t>(_TO_UNDERLYING_T(enum_t, X) & _TO_UNDERLYING_T(enum_t, Y)); \
    }                                                                                          \
    constexpr enum_t operator|(enum_t X, enum_t Y) {                                           \
        return static_cast<enum_t>(_TO_UNDERLYING_T(enum_t, X) | _TO_UNDERLYING_T(enum_t, Y)); \
    }                                                                                          \
    constexpr enum_t operator^(enum_t X, enum_t Y) {                                           \
        return static_cast<enum_t>(_TO_UNDERLYING_T(enum_t, X) ^ _TO_UNDERLYING_T(enum_t, Y)); \
    }                                                                                          \
    constexpr enum_t operator~(enum_t X) {                                                     \
        return static_cast<enum_t>(~_TO_UNDERLYING_T(enum_t, X));                              \
    }                                                                                          \
    inline enum_t &operator&=(enum_t &X, enum_t Y) {                                           \
        X = X & Y;                                                                             \
        return X;                                                                              \
    }                                                                                          \
    inline enum_t &operator|=(enum_t &X, enum_t Y) {                                           \
        X = X | Y;                                                                             \
        return X;                                                                              \
    }                                                                                          \
    inline enum_t &operator^=(enum_t &X, enum_t Y) {                                           \
        X = X ^ Y;                                                                             \
        return X;                                                                              \
    }
