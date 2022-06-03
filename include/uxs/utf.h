#pragma once

#include "common.h"

#include <limits>

namespace uxs {

template<typename InputIt>
unsigned from_utf8(InputIt first, InputIt last, InputIt& next, uint32_t& code) {
    next = first;
    if (first == last) { return 0; }
    code = static_cast<uint8_t>(*first++);
    unsigned n_read = 1;
    if (code >= 0xc0 && code < 0xf8) {
        unsigned count = code < 0xe0 ? 1 : (code < 0xf0 ? 2 : 3);
        const uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
        code &= mask[count], n_read += count;
        do {
            if (first == last) { return 0; }
            code = (code << 6) | (*first++ & 0x3f);
        } while (--count > 0);
    }
    next = first;
    return n_read;
}

template<typename InputIt>
unsigned from_utf16(InputIt first, InputIt last, InputIt& next, uint32_t& code) {
    next = first;
    if (first == last) { return 0; }
    code = static_cast<uint16_t>(*first++);
    unsigned n_read = 1;
    if ((code & 0xdc00) == 0xd800) {
        if (first == last) { return 0; }
        code = 0x10000 + (((code & 0x3ff) << 10) | (*first++ & 0x3ff)), ++n_read;
    }
    next = first;
    return n_read;
}

template<typename OutputIt>
unsigned to_utf8(uint32_t code, OutputIt out) {
    if (code < 0x80) {
        *out++ = static_cast<uint8_t>(code);
        return 1;
    }
    uint8_t ch[4];
    const uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    const uint8_t hdr[] = {0, 0xc0, 0xe0, 0xf0};
    unsigned count = 0;
    if (code >= 0x110000) { code = 0xfffd; }
    do { ch[count] = 0x80 | (code & 0x3f); } while ((code >>= 6) > mask[++count]);
    *out++ = static_cast<uint8_t>(hdr[count] | code);
    const unsigned n_written = count + 1;
    do { *out++ = ch[--count]; } while (count > 0);
    return n_written;
}

template<typename OutputIt>
unsigned to_utf8_n(uint32_t code, OutputIt out, size_t n) {
    if (n == 0) { return 0; }
    if (code < 0x80) {
        *out++ = static_cast<uint8_t>(code);
        return 1;
    }
    uint8_t ch[4];
    const uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    const uint8_t hdr[] = {0, 0xc0, 0xe0, 0xf0};
    unsigned count = 0;
    if (code >= 0x110000) { code = 0xfffd; }
    do { ch[count] = 0x80 | (code & 0x3f); } while ((code >>= 6) > mask[++count]);
    if (n <= count) { return 0; }
    *out++ = static_cast<uint8_t>(hdr[count] | code);
    const unsigned n_written = count + 1;
    do { *out++ = ch[--count]; } while (count > 0);
    return n_written;
}

template<typename OutputIt>
unsigned to_utf16(uint32_t code, OutputIt out) {
    if (code >= 0x10000) {
        if (code < 0x110000) {
            code -= 0x10000;
            *out++ = static_cast<uint16_t>(0xd800 | (code >> 10));
            *out++ = static_cast<uint16_t>(0xdc00 | (code & 0x3ff));
            return 2;
        }
        code = 0xfffd;
    } else if ((code & 0xd800) == 0xd800) {
        code = 0xfffd;
    }
    *out++ = static_cast<uint16_t>(code);
    return 1;
}

template<typename OutputIt>
unsigned to_utf16_n(uint32_t code, OutputIt out, size_t n) {
    if (n == 0) { return 0; }
    if (code >= 0x10000) {
        if (code < 0x110000) {
            if (n <= 1) { return 0; }
            code -= 0x10000;
            *out++ = static_cast<uint16_t>(0xd800 | (code >> 10));
            *out++ = static_cast<uint16_t>(0xdc00 | (code & 0x3ff));
            return 2;
        }
        code = 0xfffd;
    } else if ((code & 0xd800) == 0xd800) {
        code = 0xfffd;
    }
    *out++ = static_cast<uint16_t>(code);
    return 1;
}

inline bool is_leading_utf8_byte(uint8_t ch) { return (ch & 0xc0) != 0x80; }

inline unsigned get_utf8_byte_count(uint8_t ch) {
    if (ch >= 0xc0 && ch < 0xf8) { return ch < 0xe0 ? 2 : (ch < 0xf0 ? 3 : 4); }
    return 1;
}

inline unsigned get_utf16_word_count(uint16_t ch) { return (ch & 0xdc00) == 0xd800 ? 2 : 1; }

template<typename Container>
void pop_utf8(Container& c) {
    while (!c.empty()) {
        char ch = c.back();
        c.pop_back();
        if (is_leading_utf8_byte(ch)) { break; }
    }
}

}  // namespace uxs
