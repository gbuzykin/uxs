#pragma once

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace uxs {

enum class utf_errc { wellformed = 0, invalid, empty };

template<typename InputIt>
struct from_utf_result {
#if __cplusplus < 201703L
    from_utf_result(InputIt iter, utf_errc ec) : iter(iter), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit UXS_CONSTEXPR operator bool() const noexcept { return ec == utf_errc::wellformed; }
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

constexpr bool is_utf_surrogate(std::uint32_t code) { return (code & ~0x7ff) == 0xd800; }
constexpr bool is_utf_lower_surrogate(std::uint32_t code) { return (code & ~0x3ff) == 0xd800; }
constexpr bool is_utf_upper_surrogate(std::uint32_t code) { return (code & ~0x3ff) == 0xdc00; }
constexpr bool is_utf_wellformed(std::uint32_t code) { return code < 0x110000 && !is_utf_surrogate(code); }
constexpr std::uint32_t combine_utf_surrogate_code(std::uint32_t lower, std::uint32_t upper) {
    return 0x10000 + (((lower & 0x3ff) << 10) | (upper & 0x3ff));
}

constexpr unsigned count_utf8(std::uint32_t code) {
    return code < 0x80 ? 1 : (code < 0x800 ? 2 : (code < 0x10000 || code >= 0x110000 ? 3 : 4));
}

constexpr unsigned count_utf16(std::uint32_t code) { return code < 0x10000 || code >= 0x110000 ? 1 : 2; }

template<typename InputIt>
UXS_CONSTEXPR from_utf_result<InputIt> from_utf8(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    std::uint8_t ch0 = static_cast<std::uint8_t>(*first);
    ++first;
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
        result = (result << 6) | static_cast<std::uint32_t>(*first & 0x3f);
        ++first;
    } while (--count > 0);
    if (!is_utf_wellformed(result)) { return {first0, utf_errc::invalid}; }
    code = result;
    return {first, utf_errc::wellformed};
}

template<typename OutputIt>
UXS_CONSTEXPR to_utf_result<OutputIt> to_utf8(std::uint32_t code, OutputIt out) {
    unsigned count = 1;
    std::uint8_t ch[4] = {};
    if (code < 0x80) {
        ch[0] = static_cast<std::uint8_t>(code);
    } else if (is_utf_wellformed(code)) {
        ch[0] = 0x80 | static_cast<std::uint8_t>(code & 0x3f);
        code >>= 6;
        if (code < 0x20) {
            count = 2;
            ch[1] = 0xc0 | static_cast<std::uint8_t>(code);
        } else {
            ch[1] = 0x80 | static_cast<std::uint8_t>(code & 0x3f);
            code >>= 6;
            if (code < 0x10) {
                count = 3;
                ch[2] = 0xe0 | static_cast<std::uint8_t>(code);
            } else {
                count = 4;
                ch[2] = 0x80 | static_cast<std::uint8_t>(code & 0x3f);
                ch[3] = 0xf0 | static_cast<std::uint8_t>(code >> 6);
            }
        }
    } else {
        count = 3;
        ch[0] = 0xbd, ch[1] = 0xbf, ch[2] = 0xef;
    }
    const unsigned n_written = count;
    do {
        *out = ch[--count];
        ++out;
    } while (count > 0);
    return {std::move(out), n_written};
}

template<typename InputIt>
UXS_CONSTEXPR from_utf_result<InputIt> from_utf16(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    const std::uint16_t ch0 = static_cast<std::uint16_t>(*first);
    ++first;
    code = ch0;
    if (!is_utf_surrogate(ch0)) { return {first, utf_errc::wellformed}; }
    if (first == last || !is_utf_lower_surrogate(ch0) || !is_utf_upper_surrogate(*first)) {
        return {first, utf_errc::invalid};
    }
    code = combine_utf_surrogate_code(ch0, *first);
    ++first;
    return {first, utf_errc::wellformed};
}

template<typename OutputIt>
UXS_CONSTEXPR to_utf_result<OutputIt> to_utf16(std::uint32_t code, OutputIt out) {
    unsigned count = 1;
    std::uint16_t ch[2] = {};
    if (!is_utf_wellformed(code)) {
        ch[0] = 0xfffd;
    } else if (code < 0x10000) {
        ch[0] = static_cast<std::uint16_t>(code);
    } else {
        count = 2;
        code -= 0x10000;
        ch[0] = 0xdc00 | static_cast<std::uint16_t>(code & 0x3ff);
        ch[1] = 0xd800 | static_cast<std::uint16_t>(code >> 10);
    }
    const unsigned n_written = count;
    do {
        *out = ch[--count];
        ++out;
    } while (count > 0);
    return {std::move(out), n_written};
}

template<typename InputIt>
UXS_CONSTEXPR from_utf_result<InputIt> from_utf32(InputIt first, InputIt last, std::uint32_t& code) {
    if (first == last) { return {first, utf_errc::empty}; }
    code = static_cast<std::uint32_t>(*first);
    ++first;
    return {first, is_utf_wellformed(code) ? utf_errc::wellformed : utf_errc::invalid};
}

template<typename OutputIt>
UXS_CONSTEXPR to_utf_result<OutputIt> to_utf32(std::uint32_t code, OutputIt out) {
    *out = is_utf_wellformed(code) ? code : 0xfffd;
    ++out;
    return {std::move(out), 1};
}

template<typename CharT>
struct utf_codec;

template<>
struct utf_codec<char> {
    enum : unsigned { max_code_length = 4 };
    template<typename InputIt>
    UXS_CONSTEXPR from_utf_result<InputIt> decode(InputIt first, InputIt last, std::uint32_t& code) const {
        return from_utf8(first, last, code);
    }
    template<typename OutputIt>
    UXS_CONSTEXPR to_utf_result<OutputIt> encode(std::uint32_t code, OutputIt out) const {
        return to_utf8(code, std::move(out));
    }
    constexpr unsigned count(std::uint32_t code) const { return count_utf8(code); }
};

template<>
struct utf_codec<char16_t> {
    enum : unsigned { max_code_length = 2 };
    template<typename InputIt>
    UXS_CONSTEXPR from_utf_result<InputIt> decode(InputIt first, InputIt last, std::uint32_t& code) const {
        return from_utf16(first, last, code);
    }
    template<typename OutputIt>
    UXS_CONSTEXPR to_utf_result<OutputIt> encode(std::uint32_t code, OutputIt out) const {
        return to_utf16(code, std::move(out));
    }
    constexpr unsigned count(std::uint32_t code) const { return count_utf16(code); }
};

template<>
struct utf_codec<char32_t> {
    enum : unsigned { max_code_length = 1 };
    template<typename InputIt>
    UXS_CONSTEXPR from_utf_result<InputIt> decode(InputIt first, InputIt last, std::uint32_t& code) const {
        return from_utf32(first, last, code);
    }
    template<typename OutputIt>
    UXS_CONSTEXPR to_utf_result<OutputIt> encode(std::uint32_t code, OutputIt out) const {
        return to_utf32(code, std::move(out));
    }
    constexpr unsigned count(std::uint32_t /*code*/) const { return 1; }
};

#if WCHAR_MAX > 0xffff
template<>
struct utf_codec<wchar_t> : utf_codec<char32_t> {};
#else   // WCHAR_MAX > 0xffff
template<>
struct utf_codec<wchar_t> : utf_codec<char16_t> {};
#endif  // WCHAR_MAX > 0xffff

#if __cplusplus >= 202002L
template<>
struct utf_codec<char8_t> : utf_codec<char> {};
#endif  // __cplusplus >= 202002L

UXS_EXPORT bool is_utf_printable(std::uint32_t code) noexcept;
UXS_EXPORT unsigned get_utf_printable_width(std::uint32_t code) noexcept;

template<typename CharT, typename InputIt>
std::size_t eval_string_printable_width(InputIt first, InputIt last) {
    std::size_t width = 0;
    while (first != last) {
        std::uint32_t code = 0;
        first = utf_codec<CharT>{}.decode(first, last, code).iter;
        width += get_utf_printable_width(code);
    }
    return width;
}

}  // namespace uxs
