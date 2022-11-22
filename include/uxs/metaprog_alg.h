#pragma once

#include "utility.h"

#include <algorithm>

#if __cplusplus < 201703L
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
#endif  // __cplusplus < 201703L

namespace uxs {
#if __cplusplus < 201402L
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
