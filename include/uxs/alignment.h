#pragma once

#include "metaprog_alg.h"

namespace est {

template<typename... Ts>
struct size_of : maximum<size_of<Ts>...> {};
template<typename Ty>
struct size_of<Ty> : std::integral_constant<std::size_t, sizeof(Ty)> {};
template<typename... Ts>
struct alignment_of : maximum<std::alignment_of<Ts>...> {};

template<std::size_t Alignment>
struct align_up {
    template<std::size_t Size>
    struct type : std::integral_constant<std::size_t, (Size + Alignment - 1) & ~(Alignment - 1)> {};
    static std::size_t value(std::size_t sz) { return (sz + Alignment - 1) & ~(Alignment - 1); }
};

}  // namespace est
