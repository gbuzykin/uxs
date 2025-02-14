#pragma once

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
template<typename CharT>
struct is_contiguous_string_iterator<CharT*> : std::true_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter, std::enable_if_t<std::is_same<Iter, typename std::basic_string_view<iterator_value_t<Iter>>::iterator>::value>>
    : std::true_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter, std::enable_if_t<std::is_same<Iter, typename std::basic_string<iterator_value_t<Iter>>::iterator>::value>>
    : std::true_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter, std::enable_if_t<std::is_same<Iter, typename std::basic_string<iterator_value_t<Iter>>::const_iterator>::value>>
    : std::true_type {};
}  // namespace detail

template<typename Iter, typename = std::enable_if_t<detail::is_contiguous_string_iterator<Iter>::value>>
std::basic_string_view<iterator_value_t<Iter>> to_string_view(Iter first, Iter last) {
    return std::basic_string_view<iterator_value_t<Iter>>(&*first, static_cast<std::size_t>(last - first));
}

}  // namespace uxs
