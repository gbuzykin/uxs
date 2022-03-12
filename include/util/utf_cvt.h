#pragma once

#include "common.h"

#include <limits>

namespace util {

template<typename InputIt>
InputIt from_utf8(InputIt first, InputIt last, uint32_t* pcode) {
    if (first == last) { return first; }
    uint32_t code = static_cast<uint8_t>(*first);
    if (code >= 0xc0 && code < 0xf8) {
        unsigned count = code < 0xe0 ? 1 : (code < 0xf0 ? 2 : 3);
        if (last - first <= count) { return first; }
        const uint8_t mask[] = {0x1f, 0x1f, 0xf, 0x7};
        code &= mask[count];
        do { code = (code << 6) | ((*++first) & 0x3f), --count; } while (count > 0);
    }
    *pcode = code;
    return ++first;
}

template<typename InputIt>
InputIt from_utf16(InputIt first, InputIt last, uint32_t* pcode) {
    if (first == last) { return first; }
    uint32_t code = static_cast<uint16_t>(*first);
    if ((code & 0xdc00) == 0xd800) {
        if (last - first <= 1) { return first; }
        code = 0x10000 + ((code & 0x3ff) << 10) | ((*++first) & 0x3ff);
    }
    *pcode = code;
    return ++first;
}

template<typename OutputIt>
unsigned to_utf8(uint32_t code, OutputIt out,  //
                 size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code < 0x80) {
        *out++ = static_cast<uint8_t>(code);
        return 1;
    }
    uint8_t ch[4];
    const uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    const uint8_t hdr[] = {0, 0xc0, 0xe0, 0xf0};
    unsigned count = 0;
    if (code >= 0x110000) { code = 0xfffd; }
    do {
        ch[count++] = 0x80 | (code & 0x3f);
        code >>= 6;
    } while (code > mask[count]);
    if (count >= max_count) { return 0; }
    *out++ = static_cast<uint8_t>(hdr[count] | code);
    const unsigned n = count + 1;
    do { *out++ = ch[--count]; } while (count > 0);
    return n;
}

template<typename OutputIt>
unsigned to_utf16(uint32_t code, OutputIt out,  //
                  size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code >= 0x10000) {
        if (code < 0x110000) {
            if (max_count <= 1) { return 0; }
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

template<typename Container>
void pop_utf8(Container& c) {
    while (!c.empty()) {
        char ch = c.back();
        c.pop_back();
        if (is_leading_utf8_byte(ch)) { break; }
    }
}

}  // namespace util
