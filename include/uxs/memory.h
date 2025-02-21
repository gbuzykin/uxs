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

template<typename ToTy, typename FromTy>
std::unique_ptr<ToTy> static_pointer_cast(std::unique_ptr<FromTy> p) {
    return std::unique_ptr<ToTy>(static_cast<ToTy*>(p.release()));
}

template<typename ToTy, typename FromTy>
std::unique_ptr<ToTy> dynamic_pointer_cast(std::unique_ptr<FromTy> p) {
    return std::unique_ptr<ToTy>(dynamic_cast<ToTy*>(p.release()));
}

template<typename ToTy, typename FromTy>
std::unique_ptr<ToTy> const_pointer_cast(std::unique_ptr<FromTy> p) {
    return std::unique_ptr<ToTy>(const_cast<ToTy*>(p.release()));
}

}  // namespace uxs

namespace est {

#if __cplusplus < 201402L
template<typename Ty, typename... Args>
std::enable_if_t<!std::is_array<Ty>::value, std::unique_ptr<Ty>> make_unique(Args&&... args) {
    return std::unique_ptr<Ty>(new Ty(std::forward<Args>(args)...));
}
template<typename Ty>
std::enable_if_t<std::is_array<Ty>::value &&  //
                     (std::extent<Ty>::value == 0),
                 std::unique_ptr<Ty>>
make_unique(std::size_t size) {
    return std::unique_ptr<Ty>(new typename std::remove_extent<Ty>::type[size]());
}
template<typename Ty, typename... Args>
std::enable_if_t<std::extent<Ty>::value != 0> make_unique(Args&&...) = delete;
#else   // __cplusplus < 201402L
template<typename Ty, typename... Args>
std::unique_ptr<Ty> make_unique(Args&&... args) {
    return std::make_unique<Ty>(std::forward<Args>(args)...);
}
#endif  // __cplusplus < 201402L

}  // namespace est
