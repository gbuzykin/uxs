#pragma once

#include "membuffer.h"
#include "string_util.h"

#include <locale>

namespace uxs {

enum class sconv_errc { ok = 0, out_of_range, invalid, empty };

template<typename CharT>
struct from_chars_result {
#if __cplusplus < 201703L
    from_chars_result(const CharT* ptr, sconv_errc ec) noexcept : ptr(ptr), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit operator bool() const noexcept { return ec == sconv_errc::ok; }
    friend bool operator==(const from_chars_result& lhs, const from_chars_result& rhs) {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }
    friend bool operator!=(const from_chars_result& lhs, const from_chars_result& rhs) { return !(lhs == rhs); }
    const CharT* ptr;
    sconv_errc ec;
};

struct from_string_result {
#if __cplusplus < 201703L
    from_string_result(std::size_t count, sconv_errc ec) noexcept : count(count), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit operator bool() const noexcept { return ec == sconv_errc::ok; }
    friend bool operator==(const from_string_result& lhs, const from_string_result& rhs) {
        return lhs.count == rhs.count && lhs.ec == rhs.ec;
    }
    friend bool operator!=(const from_string_result& lhs, const from_string_result& rhs) { return !(lhs == rhs); }
    std::size_t count;
    sconv_errc ec;
};

enum class fmt_flags : unsigned {
    none = 0,
    dec = 1,
    bin = 2,
    oct = 3,
    hex = 4,
    character = 5,
    base_field = 7,
    uppercase = 8,
    fixed = 0x10,
    scientific = 0x20,
    general = 0x30,
    float_field = 0x30,
    sign_neg = 0x40,
    sign_pos = 0x80,
    sign_align = 0xc0,
    sign_field = 0xc0,
    left = 0x100,
    right = 0x200,
    internal = 0x300,
    adjust_field = 0x300,
    leading_zeroes = 0x400,
    alternate = 0x800,
    mandatory_frac = 0x1000,
    localize = 0x2000,
    debug_format = 0x4000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags);

struct fmt_opts {
    UXS_CONSTEXPR fmt_opts() noexcept = default;
    UXS_CONSTEXPR explicit fmt_opts(fmt_flags fl, int p = -1, unsigned w = 0, int f = ' ') noexcept
        : flags(fl), prec(p), width(w), fill(f) {}
    fmt_flags flags = fmt_flags::none;
    int prec = -1;
    unsigned width = 0;
    int fill = ' ';
};

class UXS_EXPORT_ALL_STUFF_FOR_GNUC format_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit format_error(const char* message);
    UXS_EXPORT explicit format_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};

class locale_ref {
 public:
    locale_ref() noexcept = default;
    explicit locale_ref(const std::locale& loc) noexcept : ref_(&loc) {}
    explicit operator bool() const noexcept { return ref_ != nullptr; }
    std::locale operator*() const noexcept { return ref_ ? *ref_ : std::locale(); }

 private:
    const std::locale* ref_ = nullptr;
};

template<typename Ty, typename CharT = char, typename = void>
struct formatter;

// --------------------------

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

}  // namespace sconv

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct from_string_impl;

template<typename Ty, typename CharT = char, typename = void>
struct is_from_string_convertible : std::false_type {};
template<typename Ty, typename CharT>
struct is_from_string_convertible<
    Ty, CharT,
    std::enable_if_t<
        std::is_same<decltype(std::declval<const from_string_impl<Ty, CharT>&>()(nullptr, nullptr, std::declval<Ty&>())),
                     from_chars_result<CharT>>::value>> : std::true_type {};

namespace sconv {

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

template<typename CharT, typename Ty, typename = std::enable_if_t<uxs::is_from_string_convertible<Ty, CharT>::value>>
from_chars_result<CharT> from_chars(const CharT* first, const CharT* last, Ty& val) {
    return from_string_impl<Ty, CharT>{}(first, last, val);
}

template<typename StrLikeTy, typename Ty, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
from_string_result from_string_v(const StrLikeTy& s, Ty& val) {
    const auto sv = to_string_view(s);
    const auto result = from_chars(sv.data(), sv.data() + sv.size(), val);
    return {static_cast<std::size_t>(result.ptr - sv.data()), result.ec};
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
Ty from_string(const StrLikeTy& s, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    from_string_v(s, val);
    return val;
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
Ty from_string_errc(const StrLikeTy& s, sconv_errc& ec, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    ec = from_string_v(s, val).ec;
    return val;
}

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct to_string_impl;

namespace detail {
template<typename Ty, typename StrTy, typename... Args>
struct is_to_string_convertible_impl {
    template<typename U>
    static auto test(U* out, const Ty* val)
        -> est::always_true<decltype(std::declval<const to_string_impl<Ty, typename StrTy::value_type>&>()(
            *out, *val, std::declval<const Args&>()...))>;
    template<typename U>
    static std::false_type test(...);
    using type = decltype(test<StrTy>(nullptr, nullptr));
};
}  // namespace detail

template<typename Ty, typename StrTy = membuffer, typename... Args>
struct is_to_string_convertible : detail::is_to_string_convertible_impl<Ty, StrTy, Args...>::type {};

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

// digit pairs
UXS_FORCE_INLINE const char* get_digits(std::size_t n) noexcept {
    alignas(2) static const UXS_CONSTEXPR char digs[] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";
    assert(n < 100);
    return &digs[2 * n];
}

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

// ---- to_string

template<typename StrTy, typename Ty, typename = std::enable_if_t<uxs::is_to_string_convertible<Ty, StrTy>::value>>
void to_string_append(StrTy& out, const Ty& val) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val);
}

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<uxs::is_to_string_convertible<Ty, StrTy, fmt_opts>::value>>
void to_string_append(StrTy& out, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt);
}

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<uxs::is_to_string_convertible<Ty, StrTy, fmt_opts, locale_ref>::value>>
void to_string_append(StrTy& out, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt, locale_ref{loc});
}

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

template<typename StrTy, typename Func>
void append_adjusted(StrTy& out, Func&& fn, unsigned len, fmt_opts fmt, bool prefer_right = false) {
    unsigned left = fmt.width - len;
    unsigned right = left;
    if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::left) {
        left = 0;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::internal) {
        left >>= 1, right -= left;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::right || prefer_right) {
        right = 0;
    } else {
        left = 0;
    }
    out.append(left, fmt.fill);
    fn(out);
    out.append(right, fmt.fill);
}

}  // namespace uxs
