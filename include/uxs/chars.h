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
    kIsSpace = 1 << 1,
    kIsLower = 1 << 2,
    kIsUpper = 1 << 3,
    kIsAlpha = 1 << 4,
    kIsAlNum = 1 << 5,
};
struct char_tbl_t {
    std::array<uint8_t, 256> digs{};
    std::array<uint8_t, 256> flags{};
    CONSTEXPR char_tbl_t() {
        for (unsigned ch = 0; ch < 256; ++ch) {
            if (ch >= '0' && ch <= '9') {
                digs[ch] = ch - '0', flags[ch] = kIsAlNum;
            } else if (ch >= 'a' && ch <= 'z') {
                digs[ch] = ch - 'a' + 10, flags[ch] = kIsLower | kIsAlpha | kIsAlNum;
            } else if (ch >= 'A' && ch <= 'Z') {
                digs[ch] = ch - 'A' + 10, flags[ch] = kIsUpper | kIsAlpha | kIsAlNum;
            } else {
                digs[ch] = 255, flags[ch] = ch == ' ' || (ch >= '\t' && ch <= '\r') ? kIsSpace : 0;
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
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_digit(CharT ch) {
    return (ch & 0xff) == ch && detail::g_char_tbl.digs[static_cast<uint8_t>(ch)] < 10;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_digit(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<uint8_t>(ch)] < 10;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_xdigit(CharT ch) {
    return (ch & 0xff) == ch && detail::g_char_tbl.digs[static_cast<uint8_t>(ch)] < 16;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_xdigit(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<uint8_t>(ch)] < 16;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_space(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsSpace);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_space(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsSpace) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_lower(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsLower);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_lower(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsLower) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_upper(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsUpper);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_upper(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsUpper) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alpha(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsAlpha);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alpha(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsAlpha) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alnum(CharT ch) {
    return (ch & 0xff) == ch && (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsAlNum);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alnum(CharT ch) {
    return (detail::g_char_tbl.flags[static_cast<uint8_t>(ch)] & detail::char_bits::kIsAlNum) != 0;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR CharT to_lower(CharT ch) {
    return is_upper(ch) ? ch + ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR CharT to_upper(CharT ch) {
    return is_lower(ch) ? ch - ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, unsigned> dig_v(CharT ch) {
    return (ch & 0xff) == ch ? detail::g_char_tbl.digs[static_cast<uint8_t>(ch)] : 255;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, unsigned> dig_v(CharT ch) {
    return detail::g_char_tbl.digs[static_cast<uint8_t>(ch)];
}

}  // namespace uxs
