#pragma once

#include "util/config.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER) && __cplusplus < 201703L
#    define NOEXCEPT throw()
#    define NOEXCEPT_IF(x)
#else  // __cplusplus
#    define NOEXCEPT       noexcept
#    define NOEXCEPT_IF(x) noexcept(x)
#endif  // __cplusplus

#if __cplusplus < 201703L
#    define CONSTEXPR inline
#else  // __cplusplus
#    define CONSTEXPR constexpr
#endif  // __cplusplus
