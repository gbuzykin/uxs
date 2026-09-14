#pragma once

#include "iterator.h"
#include "string_view.h"
#include "utf.h"

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
template<typename Ty, typename = void>
struct string_char_traits_impl {
    using type = std::char_traits<est::array_element_t<Ty>>;
};
template<typename Ty>
struct string_char_traits_impl<
    Ty, std::enable_if_t<std::is_same<typename Ty::traits_type::char_type, est::array_element_t<Ty>>::value>> {
    using type = typename Ty::traits_type;
};
}  // namespace detail

template<typename Ty, typename = void>
struct is_string_like : std::false_type {};
template<typename Ty>
struct is_string_like<Ty, std::enable_if_t<est::is_character<est::array_element_t<Ty>>::value>>
    : std::is_convertible<const Ty&, std::basic_string_view<typename detail::string_char_traits_impl<Ty>::type::char_type,
                                                            typename detail::string_char_traits_impl<Ty>::type>> {};
#if __cplusplus >= 201402L
template<typename Ty>
constexpr bool is_string_like_v = is_string_like<Ty>::value;
#endif  // __cplusplus >= 201402L
#if __cplusplus >= 202002L && defined(__cpp_concepts)
template<typename Ty>
concept string_like = is_string_like_v<Ty>;
#endif  // cpp_concepts

template<typename Ty, typename = void>
struct string_char_traits {};
template<typename Ty>
struct string_char_traits<Ty, std::enable_if_t<is_string_like<Ty>::value>> {
    using type = typename detail::string_char_traits_impl<Ty>::type;
};
template<typename Ty>
using string_char_traits_t = typename string_char_traits<Ty>::type;

template<typename StrLikeTy>
using to_string_view_t =
    std::basic_string_view<typename string_char_traits_t<StrLikeTy>::char_type, string_char_traits_t<StrLikeTy>>;

template<typename StrLikeTy, typename Alloc = std::allocator<typename string_char_traits_t<StrLikeTy>::char_type>>
using make_string_t =
    std::basic_string<typename string_char_traits_t<StrLikeTy>::char_type, string_char_traits_t<StrLikeTy>, Alloc>;

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR to_string_view_t<StrLikeTy> to_string_view(const StrLikeTy& s) {
    return to_string_view_t<StrLikeTy>(s);
}

template<typename StrLikeTy, typename Alloc = std::allocator<typename string_char_traits_t<StrLikeTy>::char_type>,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
make_string_t<StrLikeTy, Alloc> make_string(const StrLikeTy& s) {
    return make_string_t<StrLikeTy, Alloc>(to_string_view(s));
}

namespace detail {
template<typename Iter, typename = void>
struct is_contiguous_string_iterator : std::false_type {};
template<typename Iter>
struct is_contiguous_string_iterator<
    Iter, std::enable_if_t<
              est::is_character<est::iterator_value_t<Iter>>::value &&
              (std::is_pointer<Iter>::value ||
               std::is_same<Iter, typename std::basic_string_view<est::iterator_value_t<Iter>>::iterator>::value ||
               std::is_same<Iter, typename std::basic_string<est::iterator_value_t<Iter>>::iterator>::value ||
               std::is_same<Iter, typename std::basic_string<est::iterator_value_t<Iter>>::const_iterator>::value)>>
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

template<typename CharT>
struct utf_string_adapter {
    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
             typename = std::enable_if_t<
                 std::is_same<typename string_char_traits_t<std::remove_cvref_t<StrLikeTy>>::char_type, CharT>::value>>
    auto operator()(StrLikeTy&& s) const -> decltype(std::forward<StrLikeTy>(s)) {
        return std::forward<StrLikeTy>(s);
    }

    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
             typename = std::enable_if_t<!std::is_same<typename string_char_traits_t<StrLikeTy>::char_type, CharT>::value>>
    std::basic_string<CharT> operator()(const StrLikeTy& s) const {
        std::basic_string<CharT> result;
        const auto sv = to_string_view(s);
        result.reserve(sv.size());
        append(result, sv.data(), sv.data() + sv.size());
        return result;
    }

    template<typename StrTy, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
    void append(StrTy& out, const StrLikeTy& s) const {
        const auto sv = to_string_view(s);
        append(out, sv.data(), sv.data() + sv.size());
    }

    template<typename StrTy>
    void append(StrTy& out, const CharT* first, const CharT* last) const {
        out.append(first, static_cast<std::size_t>(last - first));
    }

    template<typename StrTy, typename OtherCharT>
    void append(StrTy& out, const OtherCharT* first, const OtherCharT* last) const {
        while (first != last) {
            std::uint32_t code = 0;
            first = utf_decoder<OtherCharT>{}(first, last, code).iter;
            utf_encoder<CharT>{}(code, std::back_inserter(out));
        }
    }
};

using utf8_string_adapter = utf_string_adapter<char>;
using wide_string_adapter = utf_string_adapter<wchar_t>;

template<typename StrLikeTy, typename = std::enable_if_t<std::is_convertible<const StrLikeTy&, std::string_view>::value>>
std::wstring from_utf8_to_wide(const StrLikeTy& s) {
    return wide_string_adapter{}(s);
}

template<typename StrLikeTy, typename = std::enable_if_t<std::is_convertible<const StrLikeTy&, std::wstring_view>::value>>
std::string from_wide_to_utf8(const StrLikeTy& s) {
    return utf8_string_adapter{}(s);
}

template<typename StrTy, typename InputIt>
std::size_t append_escaped_text(StrTy& out, InputIt first, InputIt last, bool single_quoted,
                                std::size_t max_width = std::numeric_limits<std::size_t>::max()) {
    using char_type = typename StrTy::value_type;
    if (max_width == 0) { return 0; }
    out += single_quoted ? '\'' : '\"';
    std::size_t width = 1;
    auto first0 = first;
    while (first != last) {
        char esc = '\0';
        std::uint32_t code = 0;
        const auto result = utf_decoder<char_type>{}(first, last, code);
        switch (code) {
            case '\t': esc = 't'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\\': esc = '\\'; break;
            case '\"': {
                if (single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width, first = result.iter;
                    continue;
                }
                esc = '\"';
            } break;
            case '\'': {
                if (!single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width, first = result.iter;
                    continue;
                }
                esc = '\'';
            } break;
            default: {
                if (result && is_utf_printable(code)) {
                    const unsigned w = get_utf_printable_width(code);
                    if (max_width - width < w) { goto finish; }
                    width += w, first = result.iter;
                    continue;
                }
            } break;
        }
        out.append(first0, first);
        if (esc) {
            if (max_width - width < 2) { goto finish; }
            width += 2;
            out += '\\';
            out += esc;
        } else {
            std::array<char_type, 8> digs;
            char_type* p = digs.data();
            do { *p++ = "0123456789abcdef"[code & 0xf]; } while ((code >>= 4));
            const unsigned w = 4 + static_cast<unsigned>(p - digs.data());
            if (max_width - width < w) { goto finish; }
            width += w;
            out += result ? string_literal<char_type, '\\', 'u', '{'>{}() :
                            string_literal<char_type, '\\', 'x', '{'>{}();
            do { out += *--p; } while (p != digs.data());
            out += '}';
        }
        first = first0 = result.iter;
    }
finish:
    out.append(first0, first);
    if (width == max_width) { return width; }
    out += single_quoted ? '\'' : '\"';
    return width + 1;
}

}  // namespace uxs
