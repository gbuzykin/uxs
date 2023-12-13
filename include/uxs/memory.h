#pragma once

#include "utility.h"

#include <memory>

namespace uxs {

#if __cplusplus < 201703L
template<typename Alloc, typename = void>
struct is_alloc_always_equal : std::is_empty<Alloc> {};
template<typename Alloc>
struct is_alloc_always_equal<Alloc, std::void_t<typename Alloc::is_always_equal>> : Alloc::is_always_equal {};
#else   // __cplusplus < 201703L
template<typename Alloc>
using is_alloc_always_equal = typename std::allocator_traits<Alloc>::is_always_equal;
template<typename Alloc, typename = void>
struct is_allocator : std::false_type {};
template<typename Alloc>
struct is_allocator<Alloc, std::void_t<typename Alloc::value_type, decltype(std::declval<Alloc&>().allocate(0))>>
    : std::true_type {};
#endif  // __cplusplus < 201703L

}  // namespace uxs
