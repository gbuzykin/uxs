#pragma once

#include "common.h"

#include <limits>

namespace uxs {

template<typename InputIt>
unsigned from_utf8(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) {
    if (first == last) { return 0; }
    code = static_cast<std::uint8_t>(*first++);
    next = first;
    if (code < 0xc0 || code >= 0xf8) { return 1; }
    std::uint32_t result = code;
    unsigned count = result < 0xe0 ? 1 : (result < 0xf0 ? 2 : 3);
    const unsigned n_read = count + 1;
    const std::uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    result &= mask[count];
    do {
        if (first == last || (*first & 0xc0) != 0x80) { return 1; }
        result = (result << 6) | (*first++ & 0x3f);
    } while (--count > 0);
    if (result >= 0x110000 || (result & 0x1ff800) == 0xd800) { return 1; }
    code = result, next = first;
    return n_read;
}

template<typename OutputIt>
unsigned to_utf8(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (n == 0) { return 0; }
    if (code < 0x80) {
        *out++ = static_cast<std::uint8_t>(code);
        return 1;
    }
    std::uint8_t ch[4];
    const std::uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    const std::uint8_t hdr[] = {0, 0xc0, 0xe0, 0xf0};
    unsigned count = 0;
    if (code >= 0x110000 || (code & 0x1ff800) == 0xd800) { code = 0xfffd; }
    do { ch[count] = 0x80 | (code & 0x3f); } while ((code >>= 6) > mask[++count]);
    const unsigned n_written = count + 1;
    if (n < n_written) { return 0; }
    *out++ = static_cast<std::uint8_t>(hdr[count] | code);
    do { *out++ = ch[--count]; } while (count > 0);
    return n_written;
}

template<typename InputIt>
unsigned from_utf16(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) {
    if (first == last) { return 0; }
    code = static_cast<std::uint16_t>(*first++);
    next = first;
    if (first == last || (code & 0xfc00) != 0xd800 || (*first & 0xfc00) != 0xdc00) { return 1; }
    code = 0x10000 + (((code & 0x3ff) << 10) | (*first & 0x3ff));
    next = first + 1;
    return 2;
}

template<typename OutputIt>
unsigned to_utf16(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (n == 0) { return 0; }
    if (code >= 0x10000) {
        if (code < 0x110000) {
            if (n < 2) { return 0; }
            code -= 0x10000;
            *out++ = static_cast<std::uint16_t>(0xd800 | (code >> 10));
            *out++ = static_cast<std::uint16_t>(0xdc00 | (code & 0x3ff));
            return 2;
        }
        code = 0xfffd;
    } else if ((code & 0xf800) == 0xd800) {
        code = 0xfffd;
    }
    *out++ = static_cast<std::uint16_t>(code);
    return 1;
}

#if WCHAR_MAX > 0xffff
template<typename InputIt>
unsigned from_wchar(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) {
    return from_utf16(first, last, next, code);
}
template<typename OutputIt>
unsigned to_wchar(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return to_utf16(code, out, n);
}
#else   // WCHAR_MAX > 0xffff
template<typename InputIt>
unsigned from_wchar(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) {
    if (first == last) { return 0; }
    code = static_cast<std::uint32_t>(*first++);
    next = first;
    return 1;
}
template<typename OutputIt>
unsigned to_wchar(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (n == 0) { return 0; }
    *out++ = code;
    return 1;
}
#endif  // WCHAR_MAX > 0xffff

template<typename CharT>
struct utf_decoder;

template<>
struct utf_decoder<char> {
    bool is_wellformed(char ch) const { return static_cast<std::uint8_t>(ch) < 0x80; }
    template<typename InputIt>
    unsigned decode(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) const {
        return from_utf8(first, last, next, code);
    }
};

template<>
struct utf_decoder<wchar_t> {
#if WCHAR_MAX > 0xffff
    bool is_wellformed(wchar_t ch) const { return ch < 0x110000 && (ch & 0x1ff800) != 0xd800; }
#else   // WCHAR_MAX > 0xffff
    bool is_wellformed(wchar_t ch) const { return (ch & 0xf800) != 0xd800; }
#endif  // WCHAR_MAX > 0xffff
    template<typename InputIt>
    unsigned decode(InputIt first, InputIt last, InputIt& next, std::uint32_t& code) const {
        return from_wchar(first, last, next, code);
    }
};

template<typename CharT>
struct utf_encoder;

template<>
struct utf_encoder<char> {
    template<typename OutputIt>
    unsigned encode(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) const {
        return to_utf8(code, out, n);
    }
};

template<>
struct utf_encoder<wchar_t> {
    template<typename OutputIt>
    unsigned encode(std::uint32_t code, OutputIt out, std::size_t n = std::numeric_limits<std::size_t>::max()) const {
        return to_wchar(code, out, n);
    }
};

UXS_EXPORT bool is_utf_code_printable(std::uint32_t code) noexcept;
UXS_EXPORT unsigned get_utf_code_width(std::uint32_t code) noexcept;

inline bool is_leading_utf8_byte(std::uint8_t ch) { return (ch & 0xc0) != 0x80; }

template<typename Container>
void pop_utf8(Container& c) {
    while (!c.empty()) {
        char ch = c.back();
        c.pop_back();
        if (is_leading_utf8_byte(ch)) { break; }
    }
}

}  // namespace uxs
