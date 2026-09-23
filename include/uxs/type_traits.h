#pragma once

#include "utility.h"

#if __cplusplus < 201703L && !defined(__cpp_lib_logical_traits)
namespace std {
template<typename... B>
struct conjunction : std::true_type {};
template<typename B>
struct conjunction<B> : B {};
template<typename B, typename... Rest>
struct conjunction<B, Rest...> : std::conditional_t<B::value, std::conjunction<Rest...>, B> {};
template<typename... B>
struct disjunction : std::false_type {};
template<typename B>
struct disjunction<B> : B {};
template<typename B, typename... Rest>
struct disjunction<B, Rest...> : std::conditional_t<B::value, B, std::disjunction<Rest...>> {};
template<typename B>
struct negation : std::bool_constant<!B::value> {};
#    if __cplusplus >= 201402L
template<typename... B>
constexpr bool conjunction_v = conjunction<B...>::value;
template<typename... B>
constexpr bool disjunction_v = disjunction<B...>::value;
template<typename B>
constexpr bool negation_v = negation<B>::value;
#    endif  // __cplusplus >= 201402L
}  // namespace std
#endif  // logical traits

namespace est {

template<typename... V>
struct sum {};
template<typename V>
struct sum<V> : std::integral_constant<typename V::value_type, V::value> {};
template<typename V1, typename V2, typename... Rest>
struct sum<V1, V2, Rest...> : std::integral_constant<typename V1::value_type, V1::value + sum<V2, Rest...>::value> {};

template<typename... V>
struct minimum {};
template<typename V>
struct minimum<V> : V {};
template<typename V1, typename V2, typename... Rest>
struct minimum<V1, V2, Rest...> : minimum<std::conditional_t<(V1::value < V2::value), V1, V2>, Rest...> {};

template<typename... V>
struct maximum {};
template<typename V>
struct maximum<V> : V {};
template<typename V1, typename V2, typename... Rest>
struct maximum<V1, V2, Rest...> : maximum<std::conditional_t<(V1::value < V2::value), V2, V1>, Rest...> {};

#if __cplusplus >= 201402L
template<typename... V>
constexpr auto sum_v = sum<V...>::value;
template<typename... V>
constexpr auto minimum_v = minimum<V...>::value;
template<typename... V>
constexpr auto maximum_v = maximum<V...>::value;
#endif  // __cplusplus >= 201402L

template<std::size_t I, typename... Ts>
struct type_pack_element {};
template<std::size_t I, typename Ty, typename... Rest>
struct type_pack_element<I, Ty, Rest...> : type_pack_element<I - 1, Rest...> {};
template<typename Ty, typename... Rest>
struct type_pack_element<0, Ty, Rest...> {
    using type = Ty;
};
template<std::size_t I, typename... Ts>
using type_pack_element_t = typename type_pack_element<I, Ts...>::type;

template<typename Ty>
using size_of = std::integral_constant<std::size_t, sizeof(Ty)>;
#if __cplusplus >= 201402L
template<typename Ty>
constexpr std::size_t size_of_v = size_of<Ty>::value;
#endif  // __cplusplus >= 201402L

template<std::size_t Alignment>
struct align_up {
    template<std::size_t V>
    using type = std::integral_constant<std::size_t, (V + Alignment - 1) & ~(Alignment - 1)>;
    UXS_CONSTEXPR std::size_t operator()(std::size_t v) const noexcept {
        return (v + Alignment - 1) & ~(Alignment - 1);
    }
};

}  // namespace est
