#pragma once

#include "string_conv_base.h"

namespace uxs {

namespace sconv {

template<typename Ty>
struct fp_traits;

template<typename TyTo, typename TyFrom>
TyTo bit_cast(const TyFrom& v) noexcept {
    static_assert(sizeof(TyTo) == sizeof(TyFrom), "bad bit cast");
    TyTo ret;
    std::memcpy(&ret, &v, sizeof(TyFrom));
    return ret;
}

template<>
struct fp_traits<float> {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "type size mismatch");
    enum : unsigned { total_bits = 32, bits_per_mantissa = 23 };
    enum : std::uint64_t { mantissa_mask = (1ULL << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static std::uint64_t to_u64(float f) noexcept { return bit_cast<std::uint32_t>(f); }
    static float from_u64(std::uint64_t u64) noexcept { return bit_cast<float>(static_cast<std::uint32_t>(u64)); }
};

template<>
struct fp_traits<double> {
    static_assert(sizeof(double) == sizeof(std::uint64_t), "type size mismatch");
    enum : unsigned { total_bits = 64, bits_per_mantissa = 52 };
    enum : std::uint64_t { mantissa_mask = (1ULL << bits_per_mantissa) - 1 };
    enum : int { exp_max = (1 << (total_bits - bits_per_mantissa - 1)) - 1 };
    static std::uint64_t to_u64(double f) noexcept { return bit_cast<std::uint64_t>(f); }
    static double from_u64(std::uint64_t u64) noexcept { return bit_cast<double>(u64); }
};

template<>
struct fp_traits<long double> : fp_traits<double> {
    static std::uint64_t to_u64(long double f) noexcept { return bit_cast<std::uint64_t>(static_cast<double>(f)); }
    static long double from_u64(std::uint64_t u64) noexcept { return static_cast<long double>(bit_cast<double>(u64)); }
};

// --------------------------

template<typename Ty, typename CharT>
struct parse_result {
#if __cplusplus < 201703L
    parse_result(Ty val, const CharT* ptr,
                 sconv_errc ec) noexcept(noexcept(std::is_nothrow_move_constructible<Ty>::value))
        : val(std::move(val)), ptr(ptr), ec(ec) {}
#endif  // __cplusplus < 201703L
    Ty val;
    const CharT* ptr;
    sconv_errc ec;
};

template<typename CharT>
UXS_EXPORT parse_result<bool, CharT> parse_boolean(const CharT* p, const CharT* end) noexcept;

template<typename Ty, typename CharT>
UXS_EXPORT parse_result<Ty, CharT> parse_signed_integer_common(const CharT* p, const CharT* end, Ty pos_limit) noexcept;

template<typename Ty, typename CharT>
UXS_EXPORT parse_result<Ty, CharT> parse_unsigned_integer_common(const CharT* p, const CharT* end,
                                                                 Ty pos_limit) noexcept;

template<typename CharT>
UXS_EXPORT parse_result<std::uint64_t, CharT> parse_float_common(const CharT* p, const CharT* end, unsigned bpm,
                                                                 int exp_max) noexcept;

template<typename CharT>
from_chars_result<CharT> parse_boolean(const CharT* p, const CharT* end, bool& val) noexcept {
    const auto result = parse_boolean(p, end);
    if (result.ec == sconv_errc::ok) { val = result.val; }
    return {result.ptr, result.ec};
}

template<typename CharT, typename Ty>
from_chars_result<CharT> parse_signed_integer(const CharT* p, const CharT* end, Ty& val) noexcept {
    using reduced_type = std::conditional_t<(sizeof(Ty) <= sizeof(std::int32_t)), std::int32_t, std::int64_t>;
    const auto result = parse_signed_integer_common<reduced_type>(p, end, std::numeric_limits<Ty>::max());
    if (result.ec == sconv_errc::ok) { val = static_cast<Ty>(result.val); }
    return {result.ptr, result.ec};
}

template<typename CharT, typename Ty>
from_chars_result<CharT> parse_unsigned_integer(const CharT* p, const CharT* end, Ty& val) noexcept {
    using reduced_type = std::conditional_t<(sizeof(Ty) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
    const auto result = parse_unsigned_integer_common<reduced_type>(p, end, std::numeric_limits<Ty>::max());
    if (result.ec == sconv_errc::ok) { val = static_cast<Ty>(result.val); }
    return {result.ptr, result.ec};
}

template<typename CharT, typename Ty>
from_chars_result<CharT> parse_float(const CharT* p, const CharT* end, Ty& val) noexcept {
    const auto result = parse_float_common(p, end, fp_traits<Ty>::bits_per_mantissa, fp_traits<Ty>::exp_max);
    if (result.ec == sconv_errc::ok) { val = fp_traits<Ty>::from_u64(result.val); }
    return {result.ptr, result.ec};
}

}  // namespace sconv

#define UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(ty, func) \
    template<typename CharT> \
    struct from_string_impl<ty, CharT> { \
        from_chars_result<CharT> operator()(const CharT* first, const CharT* last, ty& val) const noexcept { \
            return func(first, last, val); \
        } \
    };
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(bool, sconv::parse_boolean)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed char, sconv::parse_signed_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed short, sconv::parse_signed_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed, sconv::parse_signed_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed long, sconv::parse_signed_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed long long, sconv::parse_signed_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned char, sconv::parse_unsigned_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned short, sconv::parse_unsigned_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned, sconv::parse_unsigned_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned long, sconv::parse_unsigned_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned long long, sconv::parse_unsigned_integer)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(float, sconv::parse_float)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(double, sconv::parse_float)
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(long double, sconv::parse_float)
#undef UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER

// --------------------------

namespace sconv {

template<typename CharT>
UXS_EXPORT void fmt_boolean(basic_membuffer<CharT>& out, bool val, fmt_opts fmt = {}, locale_ref loc = {});

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_integer_common(basic_membuffer<CharT>& out, Ty val, bool is_signed);

template<typename CharT, typename Ty>
UXS_EXPORT void fmt_integer_common(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt,
                                   locale_ref loc = {});

template<typename CharT>
UXS_EXPORT void fmt_float_common(basic_membuffer<CharT>& out, std::uint64_t u64, unsigned bpm, int exp_max,
                                 fmt_flags flags = fmt_flags::none);

template<typename CharT>
UXS_EXPORT void fmt_float_common(basic_membuffer<CharT>& out, std::uint64_t u64, unsigned bpm, int exp_max,
                                 fmt_opts fmt, locale_ref loc = {});

template<typename StrTy, typename Ty, typename... Opts>
void fmt_integer(StrTy& out, Ty val, Opts&&... opts) {
    using reduced_unsigned_type =
        std::conditional_t<(sizeof(Ty) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
    using reduced_type = std::conditional_t<
        std::is_signed<Ty>::value, typename std::make_signed<reduced_unsigned_type>::type, reduced_unsigned_type>;
    fmt_integer_common(out, static_cast<reduced_unsigned_type>(static_cast<reduced_type>(val)),
                       std::is_signed<Ty>::value, std::forward<Opts>(opts)...);
}

template<typename StrTy, typename Ty, typename... Opts>
void fmt_float(StrTy& out, Ty val, Opts&&... opts) {
    fmt_float_common(out, fp_traits<Ty>::to_u64(val), fp_traits<Ty>::bits_per_mantissa, fp_traits<Ty>::exp_max,
                     std::forward<Opts>(opts)...);
}

template<typename CharT>
UXS_EXPORT void fmt_character(basic_membuffer<CharT>& out, CharT val, fmt_opts fmt = {}, locale_ref loc = {});

template<typename CharT>
UXS_EXPORT void fmt_string(basic_membuffer<CharT>& out, std::basic_string_view<CharT> val, fmt_opts fmt = {},
                           locale_ref loc = {});

}  // namespace sconv

#define UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(ty, func) \
    template<typename CharT> \
    struct to_string_impl<ty, CharT> { \
        template<typename... Opts> \
        void operator()(basic_membuffer<CharT>& out, ty val, Opts&&... opts) const { \
            func(out, val, std::forward<Opts>(opts)...); \
        } \
        template<typename StrTy, \
                 typename = std::enable_if_t< \
                     !std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>, \
                 typename... Opts> \
        void operator()(StrTy& out, ty val, Opts&&... opts) const { \
            basic_inline_dynbuffer<typename StrTy::value_type> buf; \
            func(buf, val, std::forward<Opts>(opts)...); \
            out.append(buf.data(), buf.size()); \
        } \
    };
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(bool, sconv::fmt_boolean)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed char, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed short, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed long, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed long long, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned char, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned short, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned long, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned long long, sconv::fmt_integer)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(float, sconv::fmt_float)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(double, sconv::fmt_float)
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(long double, sconv::fmt_float)
#undef UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER

// --------------------------

template<typename StrTy, typename InputIt>
std::size_t append_escaped_text(StrTy& out, InputIt first, InputIt last, bool single_quoted,
                                std::size_t max_width = std::numeric_limits<std::size_t>::max()) {
    using char_type = typename StrTy::value_type;
    if (max_width == 0) { return 0; }
    out += single_quoted ? '\'' : '\"';
    std::size_t width = 1;
    auto first0 = first;
    while (first != last) {
        char esc = '\0';
        std::uint32_t code = 0;
        const auto result = utf_decoder<char_type>{}.decode(first, last, code);
        switch (code) {
            case '\t': esc = 't'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\\': esc = '\\'; break;
            case '\"': {
                if (single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width, first = result.iter;
                    continue;
                }
                esc = '\"';
            } break;
            case '\'': {
                if (!single_quoted) {
                    if (width == max_width) { goto finish; }
                    ++width, first = result.iter;
                    continue;
                }
                esc = '\'';
            } break;
            default: {
                if (result && is_utf_code_printable(code)) {
                    const unsigned w = get_utf_code_width(code);
                    if (max_width - width < w) { goto finish; }
                    width += w, first = result.iter;
                    continue;
                }
            } break;
        }
        out.append(first0, first);
        if (esc) {
            if (max_width - width < 2) { goto finish; }
            width += 2;
            out += '\\';
            out += esc;
        } else {
            std::array<char_type, 8> digs;
            char_type* p = digs.data();
            do { *p++ = "0123456789abcdef"[code & 0xf]; } while ((code >>= 4));
            const unsigned w = 4 + static_cast<unsigned>(p - digs.data());
            if (max_width - width < w) { goto finish; }
            width += w;
            out += result ? string_literal<char_type, '\\', 'u', '{'>{}() :
                            string_literal<char_type, '\\', 'x', '{'>{}();
            do { out += *--p; } while (p != digs.data());
            out += '}';
        }
        first = first0 = result.iter;
    }
finish:
    out.append(first0, first);
    if (width == max_width) { return width; }
    out += single_quoted ? '\'' : '\"';
    return width + 1;
}

template<typename CharT, typename InputIt>
std::size_t estimate_string_width(InputIt first, InputIt last) {
    std::size_t width = 0;
    while (first != last) {
        std::uint32_t code = 0;
        first = utf_decoder<CharT>{}.decode(first, last, code).iter;
        width += get_utf_code_width(code);
    }
    return width;
}

}  // namespace uxs
