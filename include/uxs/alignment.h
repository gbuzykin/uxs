#pragma once

#include "utility.h"

#include <algorithm>

namespace uxs {

#if __cplusplus < 201402L
template<typename... Ts>
struct maximum;
template<typename... Ts>
struct minimum;
template<typename Ty>
struct maximum<Ty> : Ty {};
template<typename Ty>
struct minimum<Ty> : Ty {};
template<typename Ty1, typename Ty2, typename... Rest>
struct maximum<Ty1, Ty2, Rest...> : maximum<std::conditional_t<(Ty1::value < Ty2::value), Ty2, Ty1>, Rest...> {};
template<typename Ty1, typename Ty2, typename... Rest>
struct minimum<Ty1, Ty2, Rest...> : minimum<std::conditional_t<(Ty1::value < Ty2::value), Ty1, Ty2>, Rest...> {};
#else   // __cplusplus < 201402L
template<typename Ty1, typename... Ts>
struct maximum : std::integral_constant<typename Ty1::value_type, std::max({Ty1::value, Ts::value...})> {};
template<typename Ty1, typename... Ts>
struct minimum : std::integral_constant<typename Ty1::value_type, std::min({Ty1::value, Ts::value...})> {};
#endif  // __cplusplus < 201402L

template<typename... Ts>
struct size_of : maximum<size_of<Ts>...> {};
template<typename... Ts>
struct alignment_of : maximum<std::alignment_of<Ts>...> {};
template<typename Ty>
struct size_of<Ty> : std::integral_constant<size_t, sizeof(Ty)> {};
template<typename Ty>
struct alignment_of<Ty> : std::alignment_of<Ty> {};

template<size_t Alignment>
struct aligned {
    template<size_t Size>
    struct type : std::integral_constant<size_t, (Size + Alignment - 1) & ~(Alignment - 1)> {};
    static size_t size(size_t sz) { return (sz + Alignment - 1) & ~(Alignment - 1); }
};

}  // namespace uxs
