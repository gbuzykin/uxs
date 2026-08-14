#pragma once

#include "common.h"

#include <limits>

namespace uxs {

enum class utf_errc { wellformed = 0, invalid, empty };

template<typename InputIt>
struct from_utf_result {
#if __cplusplus < 201703L
    from_utf_result(InputIt iter, utf_errc ec) : iter(iter), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit operator bool() const noexcept { return ec == utf_errc::wellformed; }
    InputIt iter;
    utf_errc ec;
};

template<typename OutputIt>
struct to_utf_result {
#if __cplusplus < 201703L
    to_utf_result(OutputIt out, unsigned count) : out(out), count(count) {}
#endif  // __cplusplus < 201703L
    OutputIt out;
    unsigned count;
};

inline bool is_acceptable_utf32(std::uint32_t ch) { return ch < 0x110000 && (ch & 0x1ff800) != 0xd800; }

template<typename InputIt>
from_utf_result<InputIt> from_utf8(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    std::uint8_t ch0 = static_cast<std::uint8_t>(*first++);
    code = ch0;
    if (ch0 < 0x80) { return {first, utf_errc::wellformed}; }
    unsigned count = 1;
    if ((ch0 & 0xe0) == 0xc0) {
        ch0 &= 0x1f;
    } else if ((ch0 & 0xf0) == 0xe0) {
        count = 2, ch0 &= 0xf;
    } else if ((ch0 & 0xf8) == 0xf0) {
        count = 3, ch0 &= 0x7;
    } else {
        return {first, utf_errc::invalid};
    }
    std::uint32_t result = ch0;
    const auto first0 = first;
    do {
        if (first == last || (*first & 0xc0) != 0x80) { return {first0, utf_errc::invalid}; }
        result = (result << 6) | (*first++ & 0x3f);
    } while (--count > 0);
    if (!is_acceptable_utf32(result)) { return {first0, utf_errc::invalid}; }
    code = result;
    return {first, utf_errc::wellformed};
}

template<typename OutputIt>
to_utf_result<OutputIt> to_utf8(std::uint32_t code, OutputIt out,
                                std::size_t avail = std::numeric_limits<std::size_t>::max()) {
    if (avail == 0) { return {out, 0}; }
    if (code < 0x80) {
        *out = static_cast<std::uint8_t>(code);
        ++out;
        return {out, 1};
    }
    if (!is_acceptable_utf32(code)) { code = 0xfffd; }
    const std::uint8_t mask[] = {0, 0x1f, 0xf, 0x7};
    const std::uint8_t hdr[] = {0, 0xc0, 0xe0, 0xf0};
    std::uint8_t ch[4];
    unsigned count = 0;
    do { ch[count] = 0x80 | (code & 0x3f); } while ((code >>= 6) > mask[++count]);
    const unsigned n_written = count + 1;
    if (avail < n_written) { return {out, 0}; }
    *out = static_cast<std::uint8_t>(hdr[count] | code);
    ++out;
    do {
        *out = ch[--count];
        ++out;
    } while (count > 0);
    return {out, n_written};
}

template<typename InputIt>
from_utf_result<InputIt> from_utf16(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    const std::uint16_t ch0 = static_cast<std::uint16_t>(*first++);
    code = ch0;
    if ((ch0 & 0xf800) != 0xd800) { return {first, utf_errc::wellformed}; }
    if (first == last || (ch0 & 0xfc00) != 0xd800 || (*first & 0xfc00) != 0xdc00) { return {first, utf_errc::invalid}; }
    code = 0x10000 + ((static_cast<std::uint32_t>(ch0 & 0x3ff) << 10) | (*first++ & 0x3ff));
    return {first, utf_errc::wellformed};
}

template<typename OutputIt>
to_utf_result<OutputIt> to_utf16(std::uint32_t code, OutputIt out,
                                 std::size_t avail = std::numeric_limits<std::size_t>::max()) {
    if (avail == 0) { return {out, 0}; }
    if (code >= 0x10000) {
        if (code < 0x110000) {
            if (avail < 2) { return {out, 0}; }
            code -= 0x10000;
            *out = static_cast<std::uint16_t>(0xd800 | (code >> 10));
            ++out;
            *out = static_cast<std::uint16_t>(0xdc00 | (code & 0x3ff));
            ++out;
            return {out, 2};
        }
        code = 0xfffd;
    } else if ((code & 0xf800) == 0xd800) {
        code = 0xfffd;
    }
    *out = static_cast<std::uint16_t>(code);
    ++out;
    return {out, 1};
}

template<typename InputIt>
from_utf_result<InputIt> from_utf32(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    code = static_cast<std::uint32_t>(*first++);
    return {first, is_acceptable_utf32(code) ? utf_errc::wellformed : utf_errc::invalid};
}

template<typename OutputIt>
to_utf_result<OutputIt> to_utf32(std::uint32_t code, OutputIt out,
                                 std::size_t avail = std::numeric_limits<std::size_t>::max()) {
    if (avail == 0) { return {out, 0}; }
    *out = is_acceptable_utf32(code) ? code : 0xfffd;
    ++out;
    return {out, 1};
}

#if WCHAR_MAX > 0xffff
template<typename InputIt>
from_utf_result<InputIt> from_wchar(InputIt first, InputIt last, std::uint32_t& code) {
    return from_utf32(first, last, code);
}
template<typename OutputIt>
to_utf_result<OutputIt> to_wchar(std::uint32_t code, OutputIt out,
                                 std::size_t avail = std::numeric_limits<std::size_t>::max()) {
    return to_utf32(code, out, avail);
}
#else   // WCHAR_MAX > 0xffff
template<typename InputIt>
from_utf_result<InputIt> from_wchar(InputIt first, InputIt last, std::uint32_t& code) {
    return from_utf16(first, last, code);
}
template<typename OutputIt>
to_utf_result<OutputIt> to_wchar(std::uint32_t code, OutputIt out,
                                 std::size_t avail = std::numeric_limits<std::size_t>::max()) {
    return to_utf16(code, out, avail);
}
#endif  // WCHAR_MAX > 0xffff

template<typename CharT>
struct utf_decoder;

template<>
struct utf_decoder<char> {
    template<typename InputIt>
    from_utf_result<InputIt> decode(InputIt first, InputIt last, std::uint32_t& code) const {
        return from_utf8(first, last, code);
    }
};

template<>
struct utf_decoder<wchar_t> {
    template<typename InputIt>
    from_utf_result<InputIt> decode(InputIt first, InputIt last, std::uint32_t& code) const {
        return from_wchar(first, last, code);
    }
};

template<typename CharT>
struct utf_encoder;

template<>
struct utf_encoder<char> {
    template<typename OutputIt>
    to_utf_result<OutputIt> encode(std::uint32_t code, OutputIt out,
                                   std::size_t avail = std::numeric_limits<std::size_t>::max()) const {
        return to_utf8(code, out, avail);
    }
};

template<>
struct utf_encoder<wchar_t> {
    template<typename OutputIt>
    to_utf_result<OutputIt> encode(std::uint32_t code, OutputIt out,
                                   std::size_t avail = std::numeric_limits<std::size_t>::max()) const {
        return to_wchar(code, out, avail);
    }
};

UXS_EXPORT bool is_utf_code_printable(std::uint32_t code) noexcept;
UXS_EXPORT unsigned get_utf_code_width(std::uint32_t code) noexcept;

}  // namespace uxs
