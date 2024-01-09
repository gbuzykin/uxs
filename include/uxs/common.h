#pragma once

#include "uxs/config.h"

#include <cstddef>
#include <cstdint>

#define TOKENPASTE(x, y)  x##y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)

#if !defined(UXS_HAS_INCLUDE)
#    if defined(__has_include) || _MSC_VER > 1900
#        define UXS_HAS_INCLUDE(x) __has_include(x)
#    else
#        define UXS_HAS_INCLUDE(x) 0
#    endif
#endif

#if !defined(UXS_EXPORT_ALL_STUFF_FOR_GNUC)
#    ifdef __GNUC__
#        define UXS_EXPORT_ALL_STUFF_FOR_GNUC UXS_EXPORT
#    else
#        define UXS_EXPORT_ALL_STUFF_FOR_GNUC
#    endif
#endif

#if !defined(CONSTEVAL)
#    if __cplusplus >= 202002L && \
        (__GNUC__ >= 10 || __clang_major__ >= 12 || (_MSC_VER >= 1920 && defined(__cpp_consteval)))
#        define CONSTEVAL     consteval
#        define HAS_CONSTEVAL 1
#    else
#        define CONSTEVAL inline
#    endif
#endif  // !defined(CONSTEVAL)

#if !defined(CONSTEXPR)
#    if __cplusplus < 201703L
#        define CONSTEXPR inline
#    else  // __cplusplus < 201703L
#        define CONSTEXPR constexpr
#    endif  // __cplusplus < 201703L
#endif      // !defined(CONSTEXPR)

#if !defined(NODISCARD)
#    if __cplusplus < 201703L
#        define NODISCARD
#    else  // __cplusplus < 201703L
#        define NODISCARD [[nodiscard]]
#    endif  // __cplusplus < 201703L
#endif      // !defined(NODISCARD)

#if !defined(UNREACHABLE_CODE)
#    if defined(_MSC_VER)
#        define UNREACHABLE_CODE __assume(false)
#    elif defined(__GNUC__)
#        define UNREACHABLE_CODE __builtin_unreachable()
#    else
#        define UNREACHABLE_CODE
#    endif
#endif  // !defined(UNREACHABLE_CODE)

#if !defined(NOALIAS)
#    if defined(_MSC_VER)
#        define NOALIAS __declspec(noalias)
#    elif defined(__GNUC__)
#        define NOALIAS __attribute__((pure))
#    else
#        define NOALIAS
#    endif
#endif  // !defined(NOALIAS)
