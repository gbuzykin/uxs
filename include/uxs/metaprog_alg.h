#pragma once

#include "utility.h"

#include <algorithm>

#if __cplusplus < 201703L && !defined(__cpp_lib_logical_traits)
namespace std {
template<typename...>
struct conjunction : true_type {};
template<typename Ty>
struct conjunction<Ty> : Ty {};
template<typename Ty, typename... Ts>
struct conjunction<Ty, Ts...> : conditional_t<Ty::value, conjunction<Ts...>, Ty> {};
template<typename...>
struct disjunction : false_type {};
template<typename Ty>
struct disjunction<Ty> : Ty {};
template<typename Ty, typename... Ts>
struct disjunction<Ty, Ts...> : conditional_t<Ty::value, Ty, disjunction<Ts...>> {};
template<typename Ty>
struct negation : bool_constant<!Ty::value> {};
}  // namespace std
#endif  // logical traits

namespace uxs {

template<typename...>
struct sum;
template<typename Ty>
struct sum<Ty> : std::integral_constant<typename Ty::value_type, Ty::value> {};
template<typename Ty1, typename Ty2, typename... Rest>
struct sum<Ty1, Ty2, Rest...>
    : std::integral_constant<typename Ty1::value_type, Ty1::value + sum<Ty2, Rest...>::value> {};

template<size_t I, typename...>
struct type_pack_element;
template<size_t I, typename Ty, typename... Rest>
struct type_pack_element<I, Ty, Rest...> : type_pack_element<I - 1, Rest...> {};
template<typename Ty, typename... Rest>
struct type_pack_element<0, Ty, Rest...> {
    using type = Ty;
};

#if __cplusplus < 201402L && _MSC_VER < 1920  // VS2019 bug
template<typename...>
struct minimum;
template<typename Ty>
struct minimum<Ty> : Ty {};
template<typename Ty1, typename Ty2, typename... Rest>
struct minimum<Ty1, Ty2, Rest...> : minimum<std::conditional_t<(Ty1::value < Ty2::value), Ty1, Ty2>, Rest...> {};
template<typename...>
struct maximum;
template<typename Ty>
struct maximum<Ty> : Ty {};
template<typename Ty1, typename Ty2, typename... Rest>
struct maximum<Ty1, Ty2, Rest...> : maximum<std::conditional_t<(Ty1::value < Ty2::value), Ty2, Ty1>, Rest...> {};
#else   // __cplusplus < 201402L
template<typename Ty1, typename... Ts>
struct minimum : std::integral_constant<typename Ty1::value_type, std::min({Ty1::value, Ts::value...})> {};
template<typename Ty1, typename... Ts>
struct maximum : std::integral_constant<typename Ty1::value_type, std::max({Ty1::value, Ts::value...})> {};
#endif  // __cplusplus < 201402L

}  // namespace uxs
