#pragma once

#include "string_conv_common.h"  // IWYU pragma: export

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
    }
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(bool, sconv::parse_boolean);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed char, sconv::parse_signed_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed short, sconv::parse_signed_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed, sconv::parse_signed_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed long, sconv::parse_signed_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(signed long long, sconv::parse_signed_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned char, sconv::parse_unsigned_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned short, sconv::parse_unsigned_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned, sconv::parse_unsigned_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned long, sconv::parse_unsigned_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(unsigned long long, sconv::parse_unsigned_integer);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(float, sconv::parse_float);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(double, sconv::parse_float);
UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER(long double, sconv::parse_float);
#undef UXS_SCONV_IMPLEMENT_STANDARD_FROM_STRING_CONVERTER

// --------------------------

template<typename CharT, typename Ty, typename = std::enable_if_t<is_from_string_convertible<Ty, CharT>::value>>
UXS_CONSTEXPR from_chars_result<CharT> from_chars(const CharT* first, const CharT* last, Ty& val) {
    return from_string_impl<Ty, CharT>{}(first, last, val);
}

template<typename StrLikeTy, typename Ty, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR from_string_result from_string_v(const StrLikeTy& s, Ty& val) {
    const auto sv = to_string_view(s);
    const auto result = from_chars(sv.data(), sv.data() + sv.size(), val);
    return {static_cast<std::size_t>(result.ptr - sv.data()), result.ec};
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
UXS_CONSTEXPR Ty from_string(const StrLikeTy& s, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    from_string_v(s, val);
    return val;
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
UXS_CONSTEXPR Ty from_string_errc(const StrLikeTy& s, sconv_errc& ec, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    ec = from_string_v(s, val).ec;
    return val;
}

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
UXS_EXPORT void fmt_character(basic_membuffer<CharT>& out, char32_t val, fmt_opts fmt = {}, locale_ref loc = {});

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
    }
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(bool, sconv::fmt_boolean);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed char, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed short, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed long, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(signed long long, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned char, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned short, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned long, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(unsigned long long, sconv::fmt_integer);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(float, sconv::fmt_float);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(double, sconv::fmt_float);
UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(long double, sconv::fmt_float);
#undef UXS_SCONV_IMPLEMENT_STANDARD_TO_STRING_CONVERTER

// ---- to_string_append

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type>::value>>
void to_string_append(StrTy& out, const Ty& val) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val);
}

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type, fmt_opts>::value>>
void to_string_append(StrTy& out, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt);
}

template<
    typename StrTy, typename Ty,
    typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type, fmt_opts, locale_ref>::value>>
void to_string_append(StrTy& out, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt, locale_ref(loc));
}

// ---- to_string

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const Ty& val) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, val);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const Ty& val, fmt_opts fmt) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, val, fmt);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, loc, val, fmt);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

// ---- to_chars

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const Ty& val) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, val);
    return buf.endp();
}

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const Ty& val, fmt_opts fmt) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, val, fmt);
    return buf.endp();
}

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, loc, val, fmt);
    return buf.endp();
}

// ---- to_chars_n

template<typename CharT>
struct chars_to_n_result {
#if __cplusplus < 201703L
    chars_to_n_result(CharT* out, std::size_t size) noexcept : out(out), size(size) {}
#endif  // __cplusplus < 201703L
    CharT* out;
    std::size_t size;
};

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const Ty& val) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, val);
    return {buf.endp(), buf.tracked_size()};
}

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const Ty& val, fmt_opts fmt) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, val, fmt);
    return {buf.endp(), buf.tracked_size()};
}

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, loc, val, fmt);
    return {buf.endp(), buf.tracked_size()};
}

}  // namespace uxs
