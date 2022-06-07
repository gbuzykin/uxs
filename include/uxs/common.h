#pragma once

#include "uxs/config.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER) && __cplusplus < 201703L
#    define NOEXCEPT throw()
#    define NOEXCEPT_IF(x)
#else  // defined(_MSC_VER) && __cplusplus < 201703L
#    define NOEXCEPT       noexcept
#    define NOEXCEPT_IF(x) noexcept(x)
#endif  // defined(_MSC_VER) && __cplusplus < 201703L

#if __cplusplus < 201703L
#    define CONSTEXPR inline
#else  // __cplusplus < 201703L
#    define CONSTEXPR constexpr
#endif  // __cplusplus < 201703L

#if defined(_MSC_VER)
#    define UNREACHABLE_CODE __assume(false)
#elif defined(__GNUC__)
#    define UNREACHABLE_CODE __builtin_unreachable()
#else
#    define UNREACHABLE_CODE
#endif

#if defined(_MSC_VER)
#    define NOALIAS __declspec(noalias)
#elif defined(__GNUC__)
#    define NOALIAS __attribute__((pure))
#else
#    define NOALIAS
#endif
