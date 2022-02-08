#pragma once

#include "common.h"

#include <limits>

namespace util {

template<typename InputIt>
InputIt from_utf8(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in == in_end) { return in; }
    uint32_t code = static_cast<uint8_t>(*in);
    if (code >= 0xC0 && code < 0xF8) {
        const unsigned shift = (code >> 1) & 0x18;
        uint32_t count = (0x03020101 >> shift) & 0xFF;
        if (in_end - in <= count) { return in; }
        code &= (0x070F1F1F >> shift) & 0xFF;
        do {
            code = (code << 6) | ((*++in) & 0x3F);
            --count;
        } while (count > 0);
    }
    *pcode = code;
    return ++in;
}

template<typename InputIt>
InputIt from_utf16(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in == in_end) { return in; }
    uint32_t code = static_cast<uint16_t>(*in);
    if ((code & 0xDC00) == 0xD800) {
        if (in_end - in <= 1) { return in; }
        code = 0x10000 + ((code & 0x3FF) << 10) | ((*++in) & 0x3FF);
    }
    *pcode = code;
    return ++in;
}

template<typename OutputIt>
uint32_t to_utf8(uint32_t code, OutputIt out,  //
                 size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code < 0x80) {
        *out++ = static_cast<uint8_t>(code);
        return 1;
    }
    uint8_t ch[4];
    uint32_t count = 0, mask = 0x070F1F00, hdr = 0xF0E0C000;
    if (code >= 0x110000) { code = 0xFFFD; }
    do {
        ch[count++] = 0x80 | (code & 0x3F);
        code >>= 6, mask >>= 8, hdr >>= 8;
    } while (code > (mask & 0xFF));
    if (count >= max_count) { return 0; }
    *out++ = static_cast<uint8_t>(hdr | code);
    const uint32_t n = count + 1;
    do { *out++ = ch[--count]; } while (count > 0);
    return n;
}

template<typename OutputIt>
uint32_t to_utf16(uint32_t code, OutputIt out,  //
                  size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code >= 0x10000) {
        if (code < 0x110000) {
            if (max_count <= 1) { return 0; }
            code -= 0x10000;
            *out++ = static_cast<uint16_t>(0xD800 | (code >> 10));
            *out++ = static_cast<uint16_t>(0xDC00 | (code & 0x3FF));
            return 2;
        }
        code = 0xFFFD;
    } else if ((code & 0xD800) == 0xD800) {
        code = 0xFFFD;
    }
    *out++ = static_cast<uint16_t>(code);
    return 1;
}

inline bool is_leading_utf8_byte(char ch) { return (ch & 0xC0) != 0x80; }

template<typename Container>
void pop_utf8(Container& c) {
    while (!c.empty()) {
        char ch = c.back();
        c.pop_back();
        if (is_leading_utf8_byte(ch)) { break; }
    }
}

}  // namespace util
