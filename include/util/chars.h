#pragma once

#include "utility.h"

namespace util {

template<typename CharT>
struct is_character : std::false_type {};
template<>
struct is_character<char> : std::true_type {};
template<>
struct is_character<wchar_t> : std::true_type {};

CONSTEXPR bool is_digit(int ch) { return ch >= '0' && ch <= '9'; }
CONSTEXPR bool is_xdigit(int ch) { return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'); }
CONSTEXPR bool is_space(int ch) { return ch == ' ' || (ch >= '\t' && ch <= '\r'); }
CONSTEXPR int is_lower(int ch) { return ch >= 'a' && ch <= 'z'; }
CONSTEXPR int is_upper(int ch) { return ch >= 'A' && ch <= 'Z'; }
CONSTEXPR int is_alpha(int ch) { return is_lower(ch) || is_upper(ch); }
CONSTEXPR int is_alnum(int ch) { return is_alpha(ch) || is_digit(ch); }
CONSTEXPR int to_lower(int ch) { return is_upper(ch) ? ch + ('a' - 'A') : ch; }
CONSTEXPR int to_upper(int ch) { return is_lower(ch) ? ch - ('a' - 'A') : ch; }

CONSTEXPR int xdigit_v(int ch) {
    if (ch >= 'a' && ch <= 'f') { return ch - 'a' + 10; }
    if (ch >= 'A' && ch <= 'F') { return ch - 'A' + 10; }
    if (ch >= '0' && ch <= '9') { return ch - '0'; }
    return -1;
}

template<unsigned base>
CONSTEXPR int dig_v(int ch) {
    return is_digit(ch) ? ch - '0' : -1;
}

template<>
CONSTEXPR int dig_v<16>(int ch) {
    return xdigit_v(ch);
}

}  // namespace util
