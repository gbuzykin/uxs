#pragma once

#include "utility.h"

#include <array>

namespace uxs {

template<typename CharT, typename = void>
struct is_character : std::false_type {};
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, char>::value>> : std::true_type {};
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, wchar_t>::value>> : std::true_type {};

namespace detail {
enum char_bits {
    is_space = 1 << 1,
    is_lower = 1 << 2,
    is_upper = 1 << 3,
    is_alpha = 1 << 4,
    is_alnum = 1 << 5,
};
struct char_tbl_t {
    std::array<std::uint8_t, 256> digs{};
    std::array<std::uint8_t, 256> flags{};
    UXS_CONSTEXPR char_tbl_t() {
        for (unsigned ch = 0; ch < 256; ++ch) {
            if (ch >= '0' && ch <= '9') {
                digs[ch] = ch - '0', flags[ch] = is_alnum;
            } else if (ch >= 'a' && ch <= 'z') {
                digs[ch] = ch - 'a' + 10, flags[ch] = is_lower | is_alpha | is_alnum;
            } else if (ch >= 'A' && ch <= 'Z') {
                digs[ch] = ch - 'A' + 10, flags[ch] = is_upper | is_alpha | is_alnum;
            } else {
                digs[ch] = 255, flags[ch] = ch == ' ' || (ch >= '\t' && ch <= '\r') ? is_space : 0;
            }
        }
    }
};
#if __cplusplus < 201703L
extern UXS_EXPORT const char_tbl_t g_char_tbl;
#else   // __cplusplus < 201703L
static constexpr char_tbl_t g_char_tbl{};
#endif  // __cplusplus < 201703L
}  // namespace detail

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_digit(CharT ch) {
    return (ch & 0xff) == ch && detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)] < 10;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_digit(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)] < 10;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_xdigit(CharT ch) {
    return (ch & 0xff) == ch && detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)] < 16;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_xdigit(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)] < 16;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_space(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_space);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_space(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_space) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_lower(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_lower);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_lower(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_lower) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_upper(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_upper);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_upper(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_upper) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alpha(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alpha);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alpha(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alpha) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alnum(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alnum);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alnum(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alnum) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR CharT to_lower(CharT ch) {
    return is_upper(ch) ? ch + ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR CharT to_upper(CharT ch) {
    return is_lower(ch) ? ch - ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, unsigned> dig_v(CharT ch) {
    return (ch & 0xff) == ch ? detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)] : 255;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, unsigned> dig_v(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<std::uint8_t>(ch)];
}

}  // namespace uxs
