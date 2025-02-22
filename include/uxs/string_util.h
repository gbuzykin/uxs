#pragma once

#include "chars.h"
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

UXS_EXPORT std::wstring from_utf8_to_wide(std::string_view s);
UXS_EXPORT std::string from_wide_to_utf8(std::wstring_view s);

template<typename CharT>
struct utf_string_adapter;
template<>
struct utf_string_adapter<char> {
    std::string_view operator()(std::string_view s) const { return s; }
    std::string operator()(std::wstring_view s) const { return from_wide_to_utf8(s); }
    template<typename StrTy>
    void append(StrTy& out, std::string_view s) const {
        out.append(s.data(), s.size());
    }
    template<typename StrTy>
    void append(StrTy& out, std::wstring_view s) const {
        std::uint32_t code = 0;
        for (auto p = s.begin(); from_wchar(p, s.end(), p, code) != 0;) { to_utf8(code, std::back_inserter(out)); }
    }
};
template<>
struct utf_string_adapter<wchar_t> {
    std::wstring_view operator()(std::wstring_view s) const { return s; }
    std::wstring operator()(std::string_view s) const { return from_utf8_to_wide(s); }
    template<typename StrTy>
    void append(StrTy& out, std::string_view s) const {
        std::uint32_t code = 0;
        for (auto p = s.begin(); from_utf8(p, s.end(), p, code) != 0;) { to_wchar(code, std::back_inserter(out)); }
    }
    template<typename StrTy>
    void append(StrTy& out, std::wstring_view s) const {
        out.append(s.data(), s.size());
    }
};

using utf8_string_adapter = utf_string_adapter<char>;
using wide_string_adapter = utf_string_adapter<wchar_t>;

}  // namespace uxs
