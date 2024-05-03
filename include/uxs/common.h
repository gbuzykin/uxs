#pragma once

#include "uxs/config.h"

#include <cstddef>
#include <cstdint>

#define UXS_TOKENPASTE(x, y)  x##y
#define UXS_TOKENPASTE2(x, y) UXS_TOKENPASTE(x, y)

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

#if !defined(UXS_CONSTEVAL)
#    if __cplusplus >= 202002L && \
        (__GNUC__ >= 10 || __clang_major__ >= 12 || (_MSC_VER >= 1920 && defined(__cpp_consteval)))
#        define UXS_CONSTEVAL     consteval
#        define UXS_HAS_CONSTEVAL 1
#    else
#        define UXS_CONSTEVAL inline
#    endif
#endif  // !defined(UXS_CONSTEVAL)

#if !defined(UXS_CONSTEXPR)
#    if __cplusplus < 201703L
#        define UXS_CONSTEXPR inline
#    else  // __cplusplus < 201703L
#        define UXS_CONSTEXPR constexpr
#    endif  // __cplusplus < 201703L
#endif      // !defined(UXS_CONSTEXPR)

#if !defined(UXS_NODISCARD)
#    if __cplusplus < 201703L
#        define UXS_NODISCARD
#    else  // __cplusplus < 201703L
#        define UXS_NODISCARD [[nodiscard]]
#    endif  // __cplusplus < 201703L
#endif      // !defined(UXS_NODISCARD)

#if !defined(UXS_UNREACHABLE_CODE)
#    if defined(_MSC_VER)
#        define UXS_UNREACHABLE_CODE __assume(false)
#    elif defined(__GNUC__)
#        define UXS_UNREACHABLE_CODE __builtin_unreachable()
#    else
#        define UXS_UNREACHABLE_CODE
#    endif
#endif  // !defined(UXS_UNREACHABLE_CODE)

#if !defined(UXS_NOALIAS)
#    if defined(_MSC_VER)
#        define UXS_NOALIAS __declspec(noalias)
#    elif defined(__GNUC__)
#        define UXS_NOALIAS __attribute__((pure))
#    else
#        define UXS_NOALIAS
#    endif
#endif  // !defined(UXS_NOALIAS)
