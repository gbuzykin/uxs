#pragma once

#include "utility.h"

#include <memory>

namespace util {

#if __cplusplus < 201703L
template<typename Alloc, typename = void>
struct is_alloc_always_equal : std::is_empty<Alloc> {};
template<typename Alloc>
struct is_alloc_always_equal<Alloc, std::void_t<typename Alloc::is_always_equal>> : Alloc::is_always_equal {};
#else   // __cplusplus < 201703L
template<typename Alloc>
using is_alloc_always_equal = typename std::allocator_traits<Alloc>::is_always_equal;
#endif  // __cplusplus < 201703L

}  // namespace util
