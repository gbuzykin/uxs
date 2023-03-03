#pragma once

#include "metaprog_alg.h"

namespace uxs {

template<typename... Ts>
struct size_of : maximum<size_of<Ts>...> {};
template<typename Ty>
struct size_of<Ty> : std::integral_constant<size_t, sizeof(Ty)> {};
template<typename... Ts>
struct alignment_of : maximum<std::alignment_of<Ts>...> {};

template<size_t Alignment>
struct align_up {
    template<size_t Size>
    struct type : std::integral_constant<size_t, (Size + Alignment - 1) & ~(Alignment - 1)> {};
    static size_t value(size_t sz) { return (sz + Alignment - 1) & ~(Alignment - 1); }
};

}  // namespace uxs
