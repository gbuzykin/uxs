#pragma once

#include "common.h"

#include <limits>

namespace util {

template<typename InputIt>
InputIt from_utf8(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in >= in_end) { return in; }
    uint32_t code = static_cast<uint8_t>(*in);
    if ((code & 0xC0) == 0xC0) {
        static const uint32_t mask_tbl[] = {0xFF, 0x1F, 0xF, 0x7};
        static const uint32_t count_tbl[] = {1, 1, 1, 1, 2, 2, 3, 0};
        uint32_t count = count_tbl[(code >> 3) & 7];  // continuation byte count
        if (in_end - in <= count) { return in; }
        code &= mask_tbl[count];
        while (count > 0) {
            code = (code << 6) | ((*++in) & 0x3F);
            --count;
        }
    }
    *pcode = code;
    return ++in;
}

template<typename InputIt>
InputIt from_utf16(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in >= in_end) { return in; }
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
    if (code > 0x7F) {
        static const uint32_t mask_tbl[] = {0xFF, 0x1F, 0xF, 0x7};
        static const uint32_t hdr_tbl[] = {0, 0xC0, 0xE0, 0xF0};
        uint8_t ch[4];
        uint32_t count = 0;  // continuation byte count
        if (code > 0x10FFFF) { code = 0xFFFD; }
        do {
            ch[count++] = 0x80 | (code & 0x3F);
            code >>= 6;
        } while (code > mask_tbl[count]);
        if (count >= max_count) { return 0; }
        *out++ = static_cast<uint8_t>(hdr_tbl[count] | code);
        uint32_t n = count;
        do { *out++ = ch[--n]; } while (n > 0);
        return count + 1;
    }
    *out++ = static_cast<uint8_t>(code);
    return 1;
}

template<typename OutputIt>
uint32_t to_utf16(uint32_t code, OutputIt out,  //
                  size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code >= 0x10000) {
        if (code <= 0x10FFFF) {
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

inline bool is_leading_utf8_byte(char c) { return (c & 0xC0) != 0x80; }
inline void pop_utf8(std::string& s) {
    while (!s.empty()) {
        char c = s.back();
        s.pop_back();
        if (is_leading_utf8_byte(c)) { break; }
    }
}

}  // namespace util
