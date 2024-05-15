#pragma once

#include "utility.h"

namespace uxs {

template<typename CharT, typename = void>
struct is_character : std::false_type {};
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, char>::value>> : std::true_type {};
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, wchar_t>::value>> : std::true_type {};

namespace detail {
enum char_bits : std::uint8_t {
    is_space = 1 << 0,
    is_lower = 1 << 1,
    is_upper = 1 << 2,
    is_alpha = 1 << 3,
    is_alnum = 1 << 4,
    is_json_space = 1 << 5,
    is_xml_special = 1 << 6,
};
struct char_tbl_t {
#define UXS_CHAR_FLAGS_TABLE_DATA \
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x01, 0x01, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, \
        0x10, 0x10, 0x10, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, \
        0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, \
        0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x1a, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
        0x00, 0x00, 0x00
#if __cplusplus < 201703L
    const std::uint8_t* flags() const noexcept {
        static const std::uint8_t v[] = {UXS_CHAR_FLAGS_TABLE_DATA};
        return v;
    }
#else   // __cplusplus < 201703L
    static constexpr std::uint8_t v_flags[] = {UXS_CHAR_FLAGS_TABLE_DATA};
    constexpr const std::uint8_t* flags() const noexcept { return v_flags; }
#endif  // __cplusplus < 201703L
#undef UXS_CHAR_FLAGS_TABLE_DATA
#define UXS_CHAR_DIGS_TABLE_DATA \
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, \
        0x07, 0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, \
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, \
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
        0xff, 0xff, 0xff
#if __cplusplus < 201703L
    const std::uint8_t* digs() const noexcept {
        static const std::uint8_t v[] = {UXS_CHAR_DIGS_TABLE_DATA};
        return v;
    }
#else   // __cplusplus < 201703L
    static constexpr std::uint8_t v_digs[] = {UXS_CHAR_DIGS_TABLE_DATA};
    constexpr const std::uint8_t* digs() const noexcept { return v_digs; }
#endif  // __cplusplus < 201703L
#undef UXS_CHAR_DIGS_TABLE_DATA
};
}  // namespace detail

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_digit(CharT ch) noexcept {
    return (ch & 0xff) == ch && detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)] < 10;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_digit(CharT ch) noexcept {
    return detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)] < 10;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_xdigit(CharT ch) noexcept {
    return (ch & 0xff) == ch && detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)] < 16;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_xdigit(CharT ch) noexcept {
    return detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)] < 16;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_space(CharT ch) noexcept {
    return (ch & 0xff) == ch &&
           (detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_space);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_space(CharT ch) noexcept {
    return detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_space;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_lower(CharT ch) noexcept {
    return (ch & 0xff) == ch &&
           (detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_lower);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_lower(CharT ch) noexcept {
    return detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_lower;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_upper(CharT ch) noexcept {
    return (ch & 0xff) == ch &&
           (detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_upper);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_upper(CharT ch) noexcept {
    return detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_upper;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alpha(CharT ch) noexcept {
    return (ch & 0xff) == ch &&
           (detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alpha);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alpha(CharT ch) noexcept {
    return detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alpha;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, bool> is_alnum(CharT ch) noexcept {
    return (ch & 0xff) == ch &&
           (detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alnum);
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, bool> is_alnum(CharT ch) noexcept {
    return detail::char_tbl_t{}.flags()[static_cast<std::uint8_t>(ch)] & detail::char_bits::is_alnum;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR CharT to_lower(CharT ch) noexcept {
    return is_upper(ch) ? ch + ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR CharT to_upper(CharT ch) noexcept {
    return is_lower(ch) ? ch - ('a' - 'A') : ch;
}

template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) != 1, unsigned> dig_v(CharT ch) noexcept {
    return (ch & 0xff) == ch ? detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)] : 255;
}
template<typename CharT, typename = std::enable_if_t<std::is_integral<CharT>::value>>
UXS_CONSTEXPR std::enable_if_t<sizeof(CharT) == 1, unsigned> dig_v(CharT ch) noexcept {
    return detail::char_tbl_t{}.digs()[static_cast<std::uint8_t>(ch)];
}

}  // namespace uxs
