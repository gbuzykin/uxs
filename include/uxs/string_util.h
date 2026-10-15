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
        static constexpr std::array<CharT, sizeof...(C)> value{C...};
        return {value.data(), value.size()};
    }
#else   // __cplusplus < 201703L
    static constexpr std::array<CharT, sizeof...(C)> value{C...};
    constexpr operator std::basic_string_view<CharT>() const { return {value.data(), value.size()}; }
#endif  // __cplusplus < 201703L
    UXS_CONSTEXPR std::basic_string_view<CharT> operator()() const { return *this; }
};

namespace detail {
template<typename Ty, typename CharT = est::array_element_t<Ty>, typename = void>
struct string_char_traits_impl {
    using type = std::char_traits<CharT>;
};
template<typename Ty, typename CharT>
struct string_char_traits_impl<Ty, CharT,
                               std::enable_if_t<std::is_same<typename Ty::traits_type::char_type, CharT>::value>> {
    using type = typename Ty::traits_type;
};
}  // namespace detail

template<typename Ty, typename CharT = void, typename = void>
struct is_string_like : std::false_type {};
template<typename Ty>
struct is_string_like<Ty, void, std::enable_if_t<est::is_character<est::array_element_t<Ty>>::value>>
    : std::is_convertible<const Ty&, std::basic_string_view<est::array_element_t<Ty>,
                                                            typename detail::string_char_traits_impl<Ty>::type>> {};
template<typename Ty, typename CharT>
struct is_string_like<Ty, CharT, std::enable_if_t<est::is_character<CharT>::value>>
    : std::is_convertible<const Ty&,
                          std::basic_string_view<CharT, typename detail::string_char_traits_impl<Ty, CharT>::type>> {};

template<typename Ty, typename CharT = void, typename = void>
struct is_normal_string_like : std::false_type {};
template<typename Ty>
struct is_normal_string_like<Ty, void, std::enable_if_t<est::is_character<est::array_element_t<Ty>>::value>>
    : std::is_convertible<const Ty&, std::basic_string_view<est::array_element_t<Ty>>> {};
template<typename Ty, typename CharT>
struct is_normal_string_like<Ty, CharT, std::enable_if_t<est::is_character<CharT>::value>>
    : std::is_convertible<const Ty&, std::basic_string_view<CharT>> {};

#if __cplusplus >= 201402L
template<typename Ty, typename CharT = void>
constexpr bool is_string_like_v = is_string_like<Ty, CharT>::value;
template<typename Ty, typename CharT = void>
constexpr bool is_normal_string_like_v = is_normal_string_like<Ty, CharT>::value;
#endif  // __cplusplus >= 201402L
#if __cplusplus >= 202002L && defined(__cpp_concepts)
template<typename Ty, typename CharT = void>
concept string_like = is_string_like_v<Ty, CharT>;
template<typename Ty, typename CharT = void>
concept normal_string_like = is_normal_string_like_v<Ty, CharT>;
#endif  // cpp_concepts

template<typename Ty, typename = void>
struct string_char_traits {};
template<typename Ty>
struct string_char_traits<Ty, std::enable_if_t<is_string_like<Ty>::value>> {
    using type = typename detail::string_char_traits_impl<Ty>::type;
};
template<typename Ty>
using string_char_traits_t = typename string_char_traits<Ty>::type;
template<typename Ty>
using string_char_type = typename string_char_traits_t<Ty>::char_type;

template<typename StrLikeTy>
using to_string_view_t = std::basic_string_view<string_char_type<StrLikeTy>, string_char_traits_t<StrLikeTy>>;

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR to_string_view_t<StrLikeTy> to_string_view(const StrLikeTy& s) {
    return to_string_view_t<StrLikeTy>(s);
}

template<typename StrLikeTy>
using to_normal_string_view_t = std::basic_string_view<string_char_type<StrLikeTy>>;

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR to_normal_string_view_t<StrLikeTy> to_normal_string_view(const StrLikeTy& s) {
    to_string_view_t<StrLikeTy> sv(s);
    return to_normal_string_view_t<StrLikeTy>(sv.data(), sv.size());
}

template<typename StrLikeTy, typename Alloc = std::allocator<string_char_type<StrLikeTy>>>
using make_string_t = std::basic_string<string_char_type<StrLikeTy>, string_char_traits_t<StrLikeTy>, Alloc>;

template<typename StrLikeTy, typename Alloc = std::allocator<string_char_type<StrLikeTy>>,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
make_string_t<StrLikeTy, Alloc> make_string(const StrLikeTy& s) {
    return make_string_t<StrLikeTy, Alloc>(to_string_view(s));
}

template<typename StrLikeTy, typename Alloc = std::allocator<string_char_type<StrLikeTy>>>
using make_normal_string_t =
    std::basic_string<string_char_type<StrLikeTy>, std::char_traits<string_char_type<StrLikeTy>>, Alloc>;

template<typename StrLikeTy, typename Alloc = std::allocator<string_char_type<StrLikeTy>>,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
make_normal_string_t<StrLikeTy, Alloc> make_normal_string(const StrLikeTy& s) {
    return make_normal_string_t<StrLikeTy, Alloc>(to_normal_string_view(s));
}

template<typename CharT, typename = std::enable_if_t<est::is_character<CharT>::value>>
UXS_CONSTEXPR std::basic_string_view<CharT> to_string_view(const CharT* s, std::size_t length) {
    return std::basic_string_view<CharT>(s, length);
}

namespace detail {
template<typename Iter, typename = void>
struct is_contiguous_string_iterator : std::false_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter,
    std::enable_if_t<est::is_character<est::iterator_value_t<Iter>>::value &&
                     (std::is_pointer<Iter>::value ||
                      est::is_one_of<Iter, typename std::basic_string_view<est::iterator_value_t<Iter>>::iterator,
                                     typename std::basic_string<est::iterator_value_t<Iter>>::iterator,
                                     typename std::basic_string<est::iterator_value_t<Iter>>::const_iterator>::value)>>
    : std::true_type {};
}  // namespace detail

template<typename Iter, typename = std::enable_if_t<detail::is_contiguous_string_iterator<Iter>::value>>
UXS_CONSTEXPR std::basic_string_view<est::iterator_value_t<Iter>> to_string_view(Iter first, Iter last) {
#if __cplusplus >= 202002L
    if constexpr (std::is_constructible_v<std::basic_string_view<est::iterator_value_t<Iter>>, Iter, Iter>) {
        return std::basic_string_view<est::iterator_value_t<Iter>>(first, last);
    } else {
#endif  // __cplusplus >= 202002L
        const std::size_t size = static_cast<std::size_t>(last - first);
        if (!size) { return {}; }
        return std::basic_string_view<est::iterator_value_t<Iter>>(&*first, size);
#if __cplusplus >= 202002L
    }
#endif  // __cplusplus >= 202002L
}

}  // namespace uxs
