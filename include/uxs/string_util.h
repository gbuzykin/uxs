#pragma once

#include "chars.h"
#include "iterator.h"
#include "string_view.h"

#include <array>
#include <string>

namespace uxs {

template<typename CharT, CharT... C>
struct string_literal {
#if __cplusplus < 201703L
    operator std::basic_string_view<CharT>() const {
        static const std::array<CharT, sizeof...(C)> value{C...};
        return {value.data(), value.size()};
    }
#else   // __cplusplus < 201703L
    static constexpr std::array<CharT, sizeof...(C)> value{C...};
    constexpr operator std::basic_string_view<CharT>() const { return {value.data(), value.size()}; }
#endif  // __cplusplus < 201703L
    UXS_CONSTEXPR std::basic_string_view<CharT> operator()() const { return *this; }
};

namespace detail {
template<typename Iter, typename = void>
struct is_contiguous_string_iterator : std::false_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter,
    std::enable_if_t<is_character<iterator_value_t<Iter>>::value &&
                     (std::is_pointer<Iter>::value ||
                      std::is_same<Iter, typename std::basic_string_view<iterator_value_t<Iter>>::iterator>::value ||
                      std::is_same<Iter, typename std::basic_string<iterator_value_t<Iter>>::iterator>::value ||
                      std::is_same<Iter, typename std::basic_string<iterator_value_t<Iter>>::const_iterator>::value)>>
    : std::true_type {};
}  // namespace detail

template<typename Iter, typename = std::enable_if_t<detail::is_contiguous_string_iterator<Iter>::value>>
UXS_CONSTEXPR std::basic_string_view<iterator_value_t<Iter>> to_string_view(Iter first, Iter last) {
#if __cplusplus >= 202002L
    return std::basic_string_view<iterator_value_t<Iter>>{first, last};
#else   // __cplusplus >= 202002L
    const std::size_t size = static_cast<std::size_t>(last - first);
    if (!size) { return {}; }
    return std::basic_string_view<iterator_value_t<Iter>>(&*first, size);
#endif  // __cplusplus >= 202002L
}

}  // namespace uxs
