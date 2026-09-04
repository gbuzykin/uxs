#pragma once

#include "uxs/chars.h"
#include "uxs/string_conv.h"

#include <cstdlib>

#define UXS_SCONV_USE_COMPILER_EXTENSIONS 1

#if UXS_SCONV_USE_COMPILER_EXTENSIONS != 0
#    if defined(_MSC_VER) && defined(_M_X64)
#        include <intrin.h>
#    elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
using uint128 = __uint128_t;
}  // namespace gcc_ints
#    endif
#endif  // UXS_SCONV_USE_COMPILER_EXTENSIONS

namespace uxs {
namespace sconv {

template<typename CharT>
struct default_numpunct {
    UXS_CONSTEXPR CharT decimal_point() const { return '.'; }
    UXS_CONSTEXPR std::basic_string_view<CharT> infname(bool upper) const {
        return upper ? string_literal<CharT, 'I', 'N', 'F'>{}() : string_literal<CharT, 'i', 'n', 'f'>{}();
    }
    UXS_CONSTEXPR std::basic_string_view<CharT> nanname(bool upper) const {
        return upper ? string_literal<CharT, 'N', 'A', 'N'>{}() : string_literal<CharT, 'n', 'a', 'n'>{}();
    }
    UXS_CONSTEXPR std::basic_string_view<CharT> truename(bool upper) const {
        return upper ? string_literal<CharT, 'T', 'R', 'U', 'E'>{}() : string_literal<CharT, 't', 'r', 'u', 'e'>{}();
    }
    UXS_CONSTEXPR std::basic_string_view<CharT> falsename(bool upper) const {
        return upper ? string_literal<CharT, 'F', 'A', 'L', 'S', 'E'>{}() :
                       string_literal<CharT, 'f', 'a', 'l', 's', 'e'>{}();
    }
};

struct fp_m64_t {
    std::uint64_t m;
    int exp;
};

UXS_CONSTEXPR_DATA std::uint64_t msb64 = 1ULL << 63;
UXS_FORCE_INLINE std::uint64_t lo32(std::uint64_t x) { return x & 0xffffffff; }
UXS_FORCE_INLINE std::uint64_t hi32(std::uint64_t x) { return x >> 32; }
template<typename TyH, typename TyL>
UXS_FORCE_INLINE std::uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<std::uint64_t>(hi) << 32) | static_cast<std::uint64_t>(lo);
}

#if UXS_SCONV_USE_COMPILER_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
UXS_FORCE_INLINE unsigned ulog2(std::uint32_t x) {
    unsigned long ret;
    _BitScanReverse(&ret, x | 1);
    return ret;
}
UXS_FORCE_INLINE unsigned ulog2(std::uint64_t x) {
    unsigned long ret;
    _BitScanReverse64(&ret, x | 1);
    return ret;
}
#elif UXS_SCONV_USE_COMPILER_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
UXS_FORCE_INLINE unsigned ulog2(std::uint32_t x) { return __builtin_clz(x | 1) ^ 31; }
UXS_FORCE_INLINE unsigned ulog2(std::uint64_t x) { return __builtin_clzll(x | 1) ^ 63; }
#else
UXS_FORCE_INLINE unsigned ulog2(std::uint32_t x) {
    static UXS_CONSTEXPR_DATA std::uint8_t v[] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
    unsigned bias = 0;
    if (x >= 1U << 16) { x >>= 16, bias += 16; }
    if (x >= 1U << 8) { x >>= 8, bias += 8; }
    return bias + v[x];
}
UXS_FORCE_INLINE unsigned ulog2(std::uint64_t x) {
    if (x >= 1ULL << 32) { return 32 + ulog2(static_cast<std::uint32_t>(hi32(x))); }
    return ulog2(static_cast<std::uint32_t>(lo32(x)));
}
#endif

// ---- from string to value

template<typename CharT>
const CharT* starts_with(const CharT* p, const CharT* end, std::basic_string_view<CharT> s) noexcept {
    if (static_cast<std::size_t>(end - p) < s.size()) { return p; }
    const CharT* p0 = p;
    for (auto it = s.begin(); it != s.end(); ++it, ++p) {
        if (to_lower(*p) != *it) { return p0; }
    }
    return p;
}

template<typename CharT>
parse_result<bool, CharT> parse_boolean(const CharT* p, const CharT* end) noexcept {
    if (p == end) { return {false, p, sconv_errc::empty}; }
    bool val = false;
    unsigned dig = 0;
    const CharT* p0 = p;
    if ((p = starts_with(p, end, default_numpunct<CharT>().truename(false))) != p0) {
        val = true;
    } else if ((p = starts_with(p, end, default_numpunct<CharT>().falsename(false))) != p0) {
    } else if ((dig = dig_v(*p)) < 10) {
        do {
            if (dig) { val = true; }
        } while (++p != end && (dig = dig_v(*p)) < 10);
    } else {
        return {false, p0, sconv_errc::invalid};
    }
    return {val, p, sconv_errc::ok};
}

template<typename Ty, typename CharT>
parse_result<Ty, CharT> parse_signed_integer_common(const CharT* p, const CharT* end, Ty pos_limit) noexcept {
    static_assert(std::is_signed<Ty>::value, "Ty must be of signed type");
    using unsigned_ty = typename std::make_unsigned<Ty>::type;
    if (p == end) { return {0, p, sconv_errc::empty}; }

    const CharT* p0 = p;
    bool neg = false;
    if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }

    unsigned dig = 0;
    if (p == end || (dig = dig_v(*p)) >= 10) { return {0, p0, sconv_errc::invalid}; }
    unsigned_ty result = dig;
    while (++p != end && (dig = dig_v(*p)) < 10) {
        unsigned_ty result0 = result;
        result = 10U * result + dig;
        if (result < result0) {                              // too big integer
            while (++p != end && (dig = dig_v(*p)) < 10) {}  // find end of pattern
            return {0, p, sconv_errc::out_of_range};
        }
    }

    if (neg) {
        if (result > 1 + static_cast<unsigned_ty>(pos_limit)) { return {0, p, sconv_errc::out_of_range}; }
        result = ~result + 1;  // apply sign
    } else if (result > static_cast<unsigned_ty>(pos_limit)) {
        return {0, p, sconv_errc::out_of_range};
    }

    return {static_cast<Ty>(result), p, sconv_errc::ok};
}

template<typename Ty, typename CharT>
parse_result<Ty, CharT> parse_unsigned_integer_common(const CharT* p, const CharT* end, Ty pos_limit) noexcept {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    if (p == end) { return {0, p, sconv_errc::empty}; }

    unsigned dig = 0;
    if (p == end || (dig = dig_v(*p)) >= 10) { return {0, p, sconv_errc::invalid}; }
    Ty result = dig;
    while (++p != end && (dig = dig_v(*p)) < 10) {
        Ty result0 = result;
        result = 10U * result + dig;
        if (result < result0) {                              // too big integer
            while (++p != end && (dig = dig_v(*p)) < 10) {}  // find end of pattern
            return {0, p, sconv_errc::out_of_range};
        }
    }

    if (result > pos_limit) { return {0, p, sconv_errc::out_of_range}; }

    return {result, p, sconv_errc::ok};
}

UXS_CONSTEXPR_DATA unsigned max_pow10_size = 13;
UXS_CONSTEXPR_DATA unsigned max_fp10_mantissa_size = 41;  // ceil(log2(10^(768 + 18)))
struct fp10_t {
    int exp = 0;
    std::uint8_t bits_used = 1;
    bool nonzero_tail = false;
    std::uint64_t bits[max_fp10_mantissa_size + max_pow10_size];
};

UXS_EXPORT std::uint64_t bignum_mul32(std::uint64_t* x, unsigned sz, std::uint32_t mul, std::uint32_t bias);

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, fp10_t& fp10) noexcept {
    UXS_CONSTEXPR_DATA std::uint64_t short_lim = 1000000000000000000ULL;
    std::uint64_t* m10 = &fp10.bits[max_fp10_mantissa_size - fp10.bits_used];
    if (fp10.bits_used == 1) {
        std::uint64_t m = *m10;
        for (unsigned dig = 0; p != end && (dig = dig_v(*p)) < 10 && m < short_lim; ++p) { m = 10U * m + dig; }
        *m10 = m;
    }
    for (unsigned dig = 0; p != end && (dig = dig_v(*p)) < 10; ++p) {
        if (fp10.bits_used < max_fp10_mantissa_size) {
            const std::uint64_t higher = bignum_mul32(m10, fp10.bits_used, 10U, dig);
            if (higher) { *--m10 = higher, ++fp10.bits_used; }
        } else {
            if (dig > 0) { fp10.nonzero_tail = true; }
            ++fp10.exp;
        }
    }
    return p;
}

template<typename CharT>
from_chars_result<CharT> from_chars_to_fp10(const CharT* p, const CharT* end, fp10_t& fp10) noexcept {
    unsigned dig = 0;
    const CharT* p0 = p;
    const CharT dec_point = default_numpunct<CharT>().decimal_point();
    if (p == end) { return {p, sconv_errc::invalid}; }
    if ((dig = dig_v(*p)) < 10) {  // integral part
        fp10.bits[max_fp10_mantissa_size - 1] = dig;
        p = accum_mantissa(p + 1, end, fp10);
        if (p == end) { return {p, sconv_errc::ok}; }
        if (*p != dec_point) { goto parse_exponent; }
    } else if (*p == dec_point && p + 1 != end && (dig = dig_v(*(p + 1))) < 10) {
        fp10.bits[max_fp10_mantissa_size - 1] = dig, fp10.exp = -1, ++p;  // tenth
    } else {
        return {p, sconv_errc::invalid};
    }

    p0 = p + 1;
    p = accum_mantissa(p0, end, fp10);  // fractional part
    fp10.exp -= static_cast<unsigned>(p - p0);
    if (p == end) { return {p, sconv_errc::ok}; }

parse_exponent:
    if (*p == 'e' || *p == 'E') {  // optional exponent
        const auto result = parse_signed_integer_common(p + 1, end, std::numeric_limits<std::int32_t>::max());
        if (result.ec == sconv_errc::ok) { fp10.exp += result.val, p = result.ptr; }
    }
    return {p, sconv_errc::ok};
}

UXS_EXPORT std::uint64_t fp10_to_fp2(fp10_t& fp10, unsigned bpm, int exp_max) noexcept;

template<typename CharT>
parse_result<std::uint64_t, CharT> parse_float_common(const CharT* p, const CharT* end, unsigned bpm,
                                                      int exp_max) noexcept {
    if (p == end) { return {0, p, sconv_errc::empty}; }

    const CharT* p0 = p;
    std::uint64_t sign = 0;
    if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, sign = static_cast<std::uint64_t>(1 + exp_max) << bpm;  // negative sign
    }

    fp10_t fp10;
    const CharT* p1 = p;
    const auto result = from_chars_to_fp10(p, end, fp10);
    if (result.ec == sconv_errc::ok) {
        return {sign | fp10_to_fp2(fp10, bpm, exp_max), result.ptr, sconv_errc::ok};
    } else if ((p = starts_with(p, end, default_numpunct<CharT>().infname(false))) != p1) {  // infinity
        return {sign | static_cast<std::uint64_t>(exp_max) << bpm, p, sconv_errc::ok};
    } else if ((p = starts_with(p, end, default_numpunct<CharT>().nanname(false))) != p1) {  // NaN
        return {sign | (static_cast<std::uint64_t>(exp_max) << bpm) | ((1ULL << bpm) - 1), p, sconv_errc::ok};
    }

    return {0, p0, sconv_errc::invalid};
}

// ---- from value to string

// minimal digit count for numbers 2^N <= x < 2^(N+1), N = 0, 1, 2, ...
UXS_FORCE_INLINE unsigned get_exp2_dig_count(std::size_t exp) noexcept {
    static UXS_CONSTEXPR_DATA unsigned dig_count[] = {
        1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,
        7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13,
        14, 14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
    assert(exp < sizeof(dig_count) / sizeof(dig_count[0]));
    return dig_count[exp];
}

// powers of ten 10^N, N = 0, 1, 2, ...
UXS_FORCE_INLINE std::uint64_t get_pow10(std::make_signed<std::size_t>::type pow) noexcept {
#define UXS_SCONV_POWERS_OF_10(base) \
    base, (base) * 10, (base) * 100, (base) * 1000, (base) * 10000, (base) * 100000, (base) * 1000000, \
        (base) * 10000000, (base) * 100000000, (base) * 1000000000
    static UXS_CONSTEXPR_DATA std::uint64_t ten_pows[] = {UXS_SCONV_POWERS_OF_10(1ULL),
                                                          UXS_SCONV_POWERS_OF_10(10000000000ULL)};
#undef UXS_SCONV_POWERS_OF_10
    assert(pow >= 0 && pow < static_cast<int>(sizeof(ten_pows) / sizeof(ten_pows[0])));
    return ten_pows[pow];
}

struct numeric_prefix {
    std::uint8_t len = 0;
    std::array<char, 3> chars{};
    void push_back(char symbol) { chars[len++] = symbol; }
    template<typename OutputIt>
    void print(OutputIt out) const {
        for (unsigned n = 0; n < len; ++n) {
            *out = chars[n];
            ++out;
        }
    }
};

template<typename CharT, typename Func, typename... Args>
void adjust_numeric(basic_membuffer<CharT>& out, Func&& fn, unsigned len, numeric_prefix prefix, fmt_opts fmt,
                    Args&&... args) {
    unsigned left = fmt.width - len;
    unsigned right = left;
    if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::left) {
        left = 0;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::internal) {
        left >>= 1, right -= left;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::right || !(fmt.flags & fmt_flags::leading_zeroes)) {
        right = 0;
    } else {
        prefix.print(std::back_inserter(out));
        out.append(left, '0');
        return fn(len - prefix.len, numeric_prefix{}, std::forward<Args>(args)...);
    }
    out.append(left, fmt.fill);
    fn(len, prefix, std::forward<Args>(args)...);
    out.append(right, fmt.fill);
}

template<typename CharT>
struct grouping_t {
    typename std::numpunct<CharT>::char_type thousands_sep;
    std::string grouping;
};

inline unsigned calc_len_with_grouping(unsigned len, std::string_view grouping) {
    unsigned n = len;
    unsigned grp = 1;
    for (const char ch : grouping) {
        grp = ch > 0 ? ch : 1;
        if (n <= grp) { return len; }
        n -= grp, ++len;
    }
    return len + ((n - 1) / grp);
}

template<typename CharT, typename Ty, typename PrintFn>
struct print_functor {
    basic_membuffer<CharT>& out;
    Ty val;
    PrintFn print_fn;
    template<typename... Args>
    UXS_FORCE_INLINE void operator()(unsigned len, numeric_prefix prefix, Args&&... args) const {
        if (out.avail() >= len || out.try_grow(len) >= len) {
            prefix.print(out.endp());
            print_fn(out.endp(), val, len, std::forward<Args>(args)...);
            out.advance(len);
        } else {
            std::array<CharT, 256> buf;
            prefix.print(buf.data());
            print_fn(buf.data(), val, len, std::forward<Args>(args)...);
            out.append(buf.data(), len);
        }
    }
};

template<typename CharT, typename Ty, typename PrintFn>
print_functor<CharT, Ty, PrintFn> make_print_functor(basic_membuffer<CharT>& out, Ty val, PrintFn&& print_fn) {
    return print_functor<CharT, Ty, PrintFn>{out, val, std::forward<PrintFn>(print_fn)};
}

// ---- binary

template<typename CharT, typename Ty>
void fmt_gen_bin(CharT* p, Ty val, unsigned pos) noexcept {
    do {
        p[--pos] = '0' + static_cast<unsigned>(val & 1);
        val >>= 1;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_bin_with_grouping(CharT* p, Ty val, unsigned pos, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    p[--pos] = '0' + static_cast<unsigned>(val & 1);
    while ((val >>= 1) != 0) {
        if (--cnt <= 0) {
            p[--pos] = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        p[--pos] = '0' + static_cast<unsigned>(val & 1);
    }
}

template<typename CharT, typename Ty>
void fmt_bin(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    numeric_prefix prefix;
    const Ty sign_bit = static_cast<Ty>(1) << (8 * sizeof(Ty) - 1);
    if (is_signed && (val & sign_bit)) {  // negative value
        prefix.push_back('-');
        val = ~val + 1;
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_pos) {
        prefix.push_back('+');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_align) {
        prefix.push_back(' ');
    }
    if (!!(fmt.flags & fmt_flags::alternate)) {
        prefix.push_back('0');
        prefix.push_back(!!(fmt.flags & fmt_flags::uppercase) ? 'B' : 'b');
    }
    unsigned len = 1 + ulog2(val) + prefix.len;
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const auto fn = make_print_functor<CharT>(out, val, fmt_gen_bin_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len - prefix.len, grouping.grouping) + prefix.len;
            return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    const auto fn = make_print_functor<CharT>(out, val, fmt_gen_bin<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- octal

template<typename CharT, typename Ty>
void fmt_gen_oct(CharT* p, Ty val, unsigned pos) noexcept {
    do {
        p[--pos] = '0' + static_cast<unsigned>(val & 7);
        val >>= 3;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_oct_with_grouping(CharT* p, Ty val, unsigned pos, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    p[--pos] = '0' + static_cast<unsigned>(val & 7);
    while ((val >>= 3) != 0) {
        if (--cnt <= 0) {
            p[--pos] = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        p[--pos] = '0' + static_cast<unsigned>(val & 7);
    }
}

template<typename CharT, typename Ty>
void fmt_oct(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    numeric_prefix prefix;
    const Ty sign_bit = static_cast<Ty>(1) << (8 * sizeof(Ty) - 1);
    if (is_signed && (val & sign_bit)) {  // negative value
        prefix.push_back('-');
        val = ~val + 1;
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_pos) {
        prefix.push_back('+');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_align) {
        prefix.push_back(' ');
    }
    if (!!(fmt.flags & fmt_flags::alternate)) { prefix.push_back('0'); }
    unsigned len = 1 + ulog2(val) / 3 + prefix.len;
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const auto fn = make_print_functor<CharT>(out, val, fmt_gen_oct_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len - prefix.len, grouping.grouping) + prefix.len;
            return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    const auto fn = make_print_functor<CharT>(out, val, fmt_gen_oct<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- hexadecimal

template<typename CharT, typename Ty>
void fmt_gen_hex(CharT* p, Ty val, unsigned pos, bool uppercase) noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        p[--pos] = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_hex_with_grouping(CharT* p, Ty val, unsigned pos, bool uppercase,
                               const grouping_t<CharT>& grouping) noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    p[--pos] = digs[val & 0xf];
    while ((val >>= 4) != 0) {
        if (--cnt <= 0) {
            p[--pos] = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        p[--pos] = digs[val & 0xf];
    }
}

template<typename CharT, typename Ty>
void fmt_hex(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    numeric_prefix prefix;
    const Ty sign_bit = static_cast<Ty>(1) << (8 * sizeof(Ty) - 1);
    const bool uppercase = !!(fmt.flags & fmt_flags::uppercase);
    if (is_signed && (val & sign_bit)) {  // negative value
        prefix.push_back('-');
        val = ~val + 1;
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_pos) {
        prefix.push_back('+');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_align) {
        prefix.push_back(' ');
    }
    if (!!(fmt.flags & fmt_flags::alternate)) {
        prefix.push_back('0');
        prefix.push_back(uppercase ? 'X' : 'x');
    }
    unsigned len = 1 + (ulog2(val) >> 2) + prefix.len;
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const auto fn = make_print_functor<CharT>(out, val, fmt_gen_hex_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len - prefix.len, grouping.grouping) + prefix.len;
            return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, uppercase, grouping) :
                                     fn(len, prefix, uppercase, grouping);
        }
    }
    const auto fn = make_print_functor<CharT>(out, val, fmt_gen_hex<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, uppercase) : fn(len, prefix, uppercase);
}

// ---- decimal

template<typename Ty>
UXS_FORCE_INLINE unsigned fmt_dec_unsigned_len(Ty val) noexcept {
    const unsigned pow = get_exp2_dig_count(ulog2(val));
    return val >= get_pow10(pow) ? pow + 1 : pow;
}

template<typename CharT>
UXS_FORCE_INLINE void copy2(CharT* tgt, const char* src) {
    tgt[0] = src[0];
    tgt[1] = src[1];
}

UXS_FORCE_INLINE void copy2(char* tgt, const char* src) { std::memcpy(tgt, src, 2); }

template<unsigned N, typename Ty>
UXS_FORCE_INLINE Ty divmod(Ty& v) {
    const Ty mod = v % N;
    v /= N;
    return mod;
}

template<typename CharT>
UXS_FORCE_INLINE unsigned gen_digits(CharT* p, std::uint32_t v, unsigned pos) noexcept {
    using tbl = uxs::detail::char_tbl_t;
    while (v >= 100U) { copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v)))); }
    if (v >= 10U) {
        copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(v)));
    } else {
        p[--pos] = '0' + static_cast<unsigned>(v);
    }
    return pos;
}

template<typename CharT>
UXS_FORCE_INLINE unsigned gen_digits_8(CharT* p, std::uint32_t v, unsigned pos) noexcept {
    using tbl = uxs::detail::char_tbl_t;
    copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v))));
    copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v))));
    copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v))));
    copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(v)));
    return pos;
}

template<typename CharT>
UXS_FORCE_INLINE unsigned gen_digits(CharT* p, std::uint64_t v, unsigned pos) noexcept {
    if (v > std::numeric_limits<std::uint32_t>::max()) {
        do { pos = gen_digits_8(p, static_cast<std::uint32_t>(divmod<100000000U>(v)), pos); } while (v >= 100000000U);
    }
    return gen_digits(p, static_cast<std::uint32_t>(v), pos);
}

template<typename CharT>
UXS_FORCE_INLINE std::uint32_t gen_digits_n(CharT* p, std::uint32_t v, unsigned n, unsigned pos) noexcept {
    using tbl = uxs::detail::char_tbl_t;
    while (n >= 2) { copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v)))), n -= 2; }
    if (n) { p[--pos] = '0' + static_cast<unsigned>(divmod<10U>(v)); }
    return v;
}

template<typename CharT>
UXS_FORCE_INLINE std::uint64_t gen_digits_n(CharT* p, std::uint64_t v, unsigned n, unsigned pos) noexcept {
    using tbl = uxs::detail::char_tbl_t;
    if (v > std::numeric_limits<std::uint32_t>::max()) {
        while (n >= 8) { pos = gen_digits_8(p, static_cast<std::uint32_t>(divmod<100000000U>(v)), pos), n -= 8; }
        while (n >= 2) { copy2(&p[pos -= 2], tbl{}.digs100(static_cast<std::size_t>(divmod<100U>(v)))), n -= 2; }
        if (n) { p[--pos] = '0' + static_cast<unsigned>(divmod<10U>(v)); }
        return v;
    }
    return gen_digits_n(p, static_cast<std::uint32_t>(v), n, pos);
}

template<typename CharT, typename Ty>
void fmt_gen_dec_with_grouping(CharT* p, Ty val, unsigned pos, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    p[--pos] = '0' + static_cast<unsigned>(divmod<10U>(val));
    while (val) {
        if (--cnt <= 0) {
            p[--pos] = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        p[--pos] = '0' + static_cast<unsigned>(divmod<10U>(val));
    }
}

template<typename CharT, typename Ty>
void fmt_dec(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    numeric_prefix prefix;
    const Ty sign_bit = static_cast<Ty>(1) << (8 * sizeof(Ty) - 1);
    if (is_signed && (val & sign_bit)) {  // negative value
        prefix.push_back('-');
        val = ~val + 1;
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_pos) {
        prefix.push_back('+');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_align) {
        prefix.push_back(' ');
    }
    unsigned len = fmt_dec_unsigned_len(val) + prefix.len;
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const auto fn = make_print_functor<CharT>(out, val, fmt_gen_dec_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len - prefix.len, grouping.grouping) + prefix.len;
            return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    const auto fn = make_print_functor<CharT>(out, val, [](CharT* p, Ty val, unsigned pos) { gen_digits(p, val, pos); });
    return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- integer

template<typename CharT, typename Ty>
void fmt_integer_common(basic_membuffer<CharT>& out, Ty val, bool is_signed) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    const Ty sign_bit = static_cast<Ty>(1) << (8 * sizeof(Ty) - 1);
    bool negative = false;
    if (is_signed && (val & sign_bit)) {
        negative = true;
        val = ~val + 1;
    }
    const unsigned len = fmt_dec_unsigned_len(val) + (negative ? 1 : 0);
    if (out.avail() >= len || out.try_grow(len) >= len) {
        if (negative) { *out.endp() = '-'; }
        gen_digits(out.endp(), val, len);
        out.advance(len);
    } else {
        std::array<CharT, 32> buf;
        if (negative) { *buf.data() = '-'; }
        gen_digits(buf.data(), val, len);
        out.append(buf.data(), len);
    }
}

template<typename CharT, typename Ty>
void fmt_integer_common(basic_membuffer<CharT>& out, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    static_assert(std::is_unsigned<Ty>::value, "Ty must be of unsigned type");
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::bin: return fmt_bin(out, val, is_signed, fmt, loc);
        case fmt_flags::oct: return fmt_oct(out, val, is_signed, fmt, loc);
        case fmt_flags::hex: return fmt_hex(out, val, is_signed, fmt, loc);
        case fmt_flags::character: {
            const Ty char_mask = static_cast<Ty>((1ULL << (8 * sizeof(CharT))) - 1);
            if ((val & char_mask) != val && (~val & char_mask) != val) {
                throw format_error("integral cannot be represented as a character");
            }
            const auto fn = [val](basic_membuffer<CharT>& out) { out += static_cast<CharT>(val); };
            return fmt.width > 1 ? append_adjusted(out, fn, 1, fmt) : fn(out);
        } break;
        default: return fmt_dec(out, val, is_signed, fmt, loc);
    }
}

// ---- boolean

template<typename CharT>
void fmt_boolean(basic_membuffer<CharT>& out, bool val, fmt_opts fmt, locale_ref loc) {
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::dec: return fmt_dec(out, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::bin: return fmt_bin(out, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::oct: return fmt_oct(out, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::hex: return fmt_hex(out, static_cast<std::uint32_t>(val), false, fmt, loc);
        default: {
            if (!!(fmt.flags & fmt_flags::localize)) {
                const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
                const auto sval = val ? numpunct.truename() : numpunct.falsename();
                const auto fn = [&sval](basic_membuffer<CharT>& out) { out += sval; };
                return fmt.width > sval.size() ? append_adjusted(out, fn, static_cast<unsigned>(sval.size()), fmt) :
                                                 fn(out);
            }
            const bool uppercase = !!(fmt.flags & fmt_flags::uppercase);
            const auto sval = val ? default_numpunct<CharT>().truename(uppercase) :
                                    default_numpunct<CharT>().falsename(uppercase);
            const auto fn = [&sval](basic_membuffer<CharT>& out) { out += sval; };
            return fmt.width > sval.size() ? append_adjusted(out, fn, static_cast<unsigned>(sval.size()), fmt) :
                                             fn(out);
        } break;
    }
}

// ---- character

template<typename CharT>
void fmt_character(basic_membuffer<CharT>& out, CharT val, fmt_opts fmt, locale_ref loc) {
    const std::uint32_t code = static_cast<typename std::make_unsigned<CharT>::type>(val);
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::dec: return fmt_dec(out, code, false, fmt, loc);
        case fmt_flags::bin: return fmt_bin(out, code, false, fmt, loc);
        case fmt_flags::oct: return fmt_oct(out, code, false, fmt, loc);
        case fmt_flags::hex: return fmt_hex(out, code, false, fmt, loc);
        default: {
            if (!(fmt.flags & fmt_flags::debug_format)) {
                const auto fn = [val](basic_membuffer<CharT>& out) { out += val; };
                return fmt.width > 1 ? append_adjusted(out, fn, 1, fmt) : fn(out);
            }
            if (fmt.width == 0) {
                append_escaped_text(out, &val, &val + 1, true);
                return;
            }
            std::array<CharT, 16> buf;
            basic_membuffer<CharT> membuf(buf.data());
            const std::size_t width = append_escaped_text(membuf, &val, &val + 1, true);
            const auto fn = [&membuf](basic_membuffer<CharT>& out) { out.append(membuf.data(), membuf.endp()); };
            return fmt.width > width ? append_adjusted(out, fn, static_cast<unsigned>(width), fmt) : fn(out);
        } break;
    }
}

// ---- string

template<typename CharT>
void fmt_string(basic_membuffer<CharT>& out, std::basic_string_view<CharT> val, fmt_opts fmt, locale_ref) {
    if (!(fmt.flags & fmt_flags::debug_format)) {
        std::size_t width = 0;
        auto first = val.begin();
        auto last = val.end();
        if (fmt.prec >= 0 || fmt.width > 0) {
            const std::size_t max_width = fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max();
            auto limit = first;
            while (limit != last) {
                std::uint32_t code = 0;
                const auto next = utf_decoder<CharT>{}.decode(limit, last, code).iter;
                const unsigned w = get_utf_code_width(code);
                if (max_width - width < w) { break; }
                width += w, limit = next;
            }
            last = limit;
        }
        const auto fn = [first, last](basic_membuffer<CharT>& out) { out += to_string_view(first, last); };
        return fmt.width > width ? append_adjusted(out, fn, static_cast<unsigned>(width), fmt) : fn(out);
    }
    if (fmt.width == 0) {
        append_escaped_text(out, val.begin(), val.end(), false,
                            fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max());
        return;
    }
    basic_inline_dynbuffer<CharT> buf;
    const std::size_t width = append_escaped_text<basic_membuffer<CharT>>(
        buf, val.begin(), val.end(), false, fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max());
    const auto fn = [&buf](basic_membuffer<CharT>& out) { out.append(buf.data(), buf.size()); };
    return fmt.width > width ? append_adjusted(out, fn, static_cast<unsigned>(width), fmt) : fn(out);
}

// ---- float hex

class fp_hex_fmt_t {
 public:
    fp_hex_fmt_t(const fp_m64_t& fp2, int prec, bool alternate) noexcept
        : significand_(fp2.m), exp_(fp2.exp), prec_(prec), n_zeroes_(0), alternate_(alternate) {}

    UXS_EXPORT void format(unsigned bpm, int exp_bias) noexcept;

    unsigned get_len() const noexcept {
        return 3 + (prec_ > 0 || alternate_ ? prec_ + 1 : 0) + fmt_dec_unsigned_len<std::uint32_t>(std::abs(exp_));
    }

    template<typename CharT>
    UXS_EXPORT void generate(CharT* p, unsigned pos, bool uppercase, CharT dec_point) const noexcept;

 private:
    std::uint64_t significand_;
    int exp_;
    int prec_;
    int n_zeroes_;
    bool alternate_;
};

template<typename CharT>
void fp_hex_fmt_t::generate(CharT* p, unsigned pos, bool uppercase, CharT dec_point) const noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    // generate exponent
    int exp2 = exp_;
    char exp_sign = '+';
    if (exp2 < 0) { exp_sign = '-', exp2 = -exp2; }
    pos = gen_digits(p, static_cast<unsigned>(exp2), pos);
    p[--pos] = exp_sign;
    p[--pos] = uppercase ? 'P' : 'p';
    std::uint64_t m = significand_;
    if (prec_ > 0) {  // has fractional part
        assert(prec_ >= n_zeroes_);
        std::fill_n(&p[pos -= n_zeroes_], n_zeroes_, '0');
        for (int n = prec_ - n_zeroes_; n > 0; --n) {
            p[--pos] = digs[m & 0xf];
            m >>= 4;
        }
        p[--pos] = dec_point;
    } else if (alternate_) {  // only one digit
        p[--pos] = dec_point;
    }
    p[--pos] = digs[m & 0xf];
}

// ---- float dec

UXS_CONSTEXPR_DATA int max_double_digits = 767;
UXS_CONSTEXPR_DATA int digs_per_64 = 18;  // size of 64-bit digit pack

class fp_dec_fmt_t {
 public:
    fp_dec_fmt_t() noexcept : significand_(0), exp_(0), prec_(0), n_zeroes_(0), fixed_(false), alternate_(false) {}
    fp_dec_fmt_t(int prec, bool alternate) noexcept
        : significand_(0), exp_(0), prec_(prec), n_zeroes_(0), fixed_(false), alternate_(alternate) {}

    UXS_EXPORT void format(fp_m64_t fp2, unsigned bpm, int exp_bias, fmt_flags fp_fmt) noexcept;
    UXS_EXPORT void format_default(fp_m64_t fp2, unsigned bpm, int exp_bias, bool mandatory_frac) noexcept;

    unsigned get_len() const noexcept {
        return (fixed_ ? get_integral_len() : 1 + get_exponent_len()) + get_frac_len();
    }

    unsigned get_len_with_grouping(std::string_view grouping) const noexcept {
        return (fixed_ ? calc_len_with_grouping(get_integral_len(), grouping) : 1 + get_exponent_len()) +
               get_frac_len();
    }

    template<typename CharT, typename... Args>
    void generate(CharT* p, unsigned pos, bool uppercase, CharT dec_point, Args&&... args) const noexcept {
        if (!fixed_) {
            generate_scientific(p, pos, uppercase, dec_point);
        } else if (significand_) {
            std::uint64_t significand = significand_;
            pos = generate_fractional(p, significand, pos, dec_point);
            if (exp_ >= 0) { generate_integral(p, significand, pos, std::forward<Args>(args)...); }
        } else {
            int n_zeroes = n_zeroes_;
            pos = generate_fractional_long(p, n_zeroes, pos, dec_point);
            if (exp_ >= 0) { generate_integral_long(p, exp_ + 1, n_zeroes, pos, std::forward<Args>(args)...); }
        }
    }

 private:
    std::uint64_t significand_;
    int exp_;
    int prec_;
    int n_zeroes_;
    bool fixed_;
    bool alternate_;
    char digs_buf_[max_double_digits + digs_per_64 - 1];

    template<typename CharT>
    void generate_scientific(CharT* p, unsigned pos, bool uppercase, CharT dec_point) const noexcept;

    template<typename CharT>
    unsigned generate_fractional(CharT* p, std::uint64_t& significand, unsigned pos, CharT dec_point) const noexcept;

    template<typename CharT>
    unsigned generate_fractional_long(CharT* p, int& n_zeroes, unsigned pos, CharT dec_point) const noexcept;

    template<typename CharT>
    void generate_integral(CharT* p, std::uint64_t significand, unsigned pos) const noexcept {
        gen_digits(p, significand, pos);
    }

    template<typename CharT>
    void generate_integral_long(CharT* p, int len, int n_zeroes, unsigned pos) const noexcept {
        std::fill_n(std::copy_n(digs_buf_, len - n_zeroes, &p[pos - len]), n_zeroes, '0');
    }

    template<typename CharT>
    void generate_integral(CharT* p, std::uint64_t significand, unsigned pos,
                           const grouping_t<CharT>& grouping) const noexcept {
        fmt_gen_dec_with_grouping(p, significand, pos, grouping);
    }

    template<typename CharT>
    void generate_integral_long(CharT* p, int len, int n_zeroes, unsigned pos,
                                const grouping_t<CharT>& grouping) const noexcept;

    unsigned get_frac_len() const noexcept { return prec_ > 0 || alternate_ ? prec_ + 1 : 0; }
    unsigned get_integral_len() const noexcept { return 1 + std::max(exp_, 0); }
    unsigned get_exponent_len() const noexcept { return exp_ <= -100 || exp_ >= 100 ? 5 : 4; }

    UXS_EXPORT void format_short_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
    UXS_EXPORT void format_short_decimal_slow(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
    UXS_EXPORT void format_long_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
};

template<typename CharT>
void fp_dec_fmt_t::generate_scientific(CharT* p, unsigned pos, bool uppercase, CharT dec_point) const noexcept {
    // generate exponent
    using tbl = uxs::detail::char_tbl_t;
    int exp10 = exp_;
    char exp_sign = '+';
    if (exp10 < 0) { exp_sign = '-', exp10 = -exp10; }
    if (exp10 < 100) {
        copy2(&p[pos -= 2], tbl{}.digs100(exp10));
    } else {
        const int t = (656 * exp10) >> 16;
        copy2(&p[pos -= 2], tbl{}.digs100(exp10 - 100 * t));
        p[--pos] = '0' + t;
    }
    p[--pos] = exp_sign;
    p[--pos] = uppercase ? 'E' : 'e';
    if (prec_ > 0) {         // has fractional part
        if (significand_) {  // generate from significand
            pos = gen_digits(p, significand_, pos);
        } else {  // generate from chars
            generate_integral_long(p, prec_ + 1, n_zeroes_, pos);
            pos -= prec_ + 1;
        }
        // insert decimal point
        p[pos - 1] = p[pos];
        p[pos] = dec_point;
    } else {  // only one digit
        if (alternate_) { p[--pos] = dec_point; }
        p[--pos] = '0' + static_cast<unsigned>(significand_);
    }
}

template<typename CharT>
unsigned fp_dec_fmt_t::generate_fractional(CharT* p, std::uint64_t& significand, unsigned pos,
                                           CharT dec_point) const noexcept {
    if (prec_ > 0) {      // has fractional part
        if (exp_ >= 0) {  // fixed form [1-9][0-9]*.[0-9]+
            significand = gen_digits_n(p, significand, prec_, pos);
            pos -= prec_;
        } else {  // fixed form 0.0*[1-9][0-9]*
            gen_digits(p, significand, pos);
            pos -= prec_;
            std::fill_n(&p[pos - 2], 1 - exp_, '0');
        }
        p[--pos] = dec_point;
    } else if (alternate_) {
        p[--pos] = dec_point;
    }
    return pos;
}

template<typename CharT>
unsigned fp_dec_fmt_t::generate_fractional_long(CharT* p, int& n_zeroes, unsigned pos, CharT dec_point) const noexcept {
    if (prec_ > 0) {      // has fractional part
        if (exp_ >= 0) {  // fixed form [1-9][0-9]*.[0-9]+
            pos -= prec_;
            if (n_zeroes < prec_) {
                std::fill_n(std::copy_n(digs_buf_ + exp_ + 1, prec_ - n_zeroes, &p[pos]), n_zeroes, '0');
                n_zeroes = 0;
            } else {  // all zeroes
                std::fill_n(&p[pos], prec_, '0');
                n_zeroes -= prec_;
            }
        } else {  // fixed form 0.0*[1-9][0-9]*
            generate_integral_long(p, exp_ + prec_ + 1, n_zeroes, pos);
            pos -= prec_;
            std::fill_n(&p[pos - 2], 1 - exp_, '0');
        }
        p[--pos] = dec_point;
    } else if (alternate_) {
        p[--pos] = dec_point;
    }
    return pos;
}

template<typename CharT>
void fp_dec_fmt_t::generate_integral_long(CharT* p, int len, int n_zeroes, unsigned pos,
                                          const grouping_t<CharT>& grouping) const noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    const char* digs = digs_buf_;
    if (n_zeroes > 0) {
        digs += len - n_zeroes;
        p[--pos] = '0';
        for (int n = n_zeroes; --n;) {
            if (--cnt <= 0) {
                p[--pos] = grouping.thousands_sep;
                cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
            }
            p[--pos] = '0';
        }
    } else {
        digs += len;
        p[--pos] = *--digs;
    }
    while (digs != digs_buf_) {
        if (--cnt <= 0) {
            p[--pos] = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        p[--pos] = *--digs;
    }
}

template<typename CharT, typename FpFmtTy>
struct print_float_functor {
    basic_membuffer<CharT>& out;
    const FpFmtTy& fp;
    bool uppercase;
    CharT dec_point;
    template<typename... Args>
    UXS_FORCE_INLINE void operator()(unsigned len, numeric_prefix prefix, Args&&... args) const {
        if (out.avail() >= len || out.try_grow(len) >= len) {
            if (prefix.len) { *out.endp() = prefix.chars[0]; }
            fp.generate(out.endp(), len, uppercase, dec_point, std::forward<Args>(args)...);
            out.advance(len);
        } else {
            basic_inline_dynbuffer<CharT> buf;
            buf.reserve(len);
            if (prefix.len) { *buf.data() = prefix.chars[0]; }
            fp.generate(buf.data(), len, uppercase, dec_point, std::forward<Args>(args)...);
            out.append(buf.data(), len);
        }
    }
};

template<typename CharT>
void fmt_float_common(basic_membuffer<CharT>& out, std::uint64_t u64, unsigned bpm, int exp_max, fmt_flags flags) {
    const std::uint64_t sign_bit = static_cast<std::uint64_t>(1 + exp_max) << bpm;
    const bool negative = u64 & sign_bit;

    // Binary exponent and mantissa
    const bool uppercase = !!(flags & fmt_flags::uppercase);
    const fp_m64_t fp2{u64 & ((1ULL << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};
    if (fp2.exp == exp_max) {
        if (!!(flags & fmt_flags::throw_on_inf_nan)) { throw std::out_of_range("floating point number is inf of nan"); }

        // Print infinity or NaN
        const auto sval = fp2.m == 0 ? default_numpunct<CharT>().infname(uppercase) :
                                       default_numpunct<CharT>().nanname(uppercase);
        if (negative) { out += '-'; }
        out += sval;
        return;
    }

    fp_dec_fmt_t fp;
    fp.format_default(fp2, bpm, exp_max >> 1, !!(flags & fmt_flags::mandatory_frac));

    const unsigned len = fp.get_len() + (negative ? 1 : 0);
    const CharT dec_point = default_numpunct<CharT>().decimal_point();
    if (out.avail() >= len || out.try_grow(len) >= len) {
        if (negative) { *out.endp() = '-'; }
        fp.generate(out.endp(), len, uppercase, dec_point);
        out.advance(len);
    } else {
        basic_inline_dynbuffer<CharT> buf;
        buf.reserve(len);
        if (negative) { *buf.data() = '-'; }
        fp.generate(buf.data(), len, uppercase, dec_point);
        out.append(buf.data(), len);
    }
}

template<typename CharT>
void fmt_float_common(basic_membuffer<CharT>& out, std::uint64_t u64, unsigned bpm, int exp_max, fmt_opts fmt,
                      locale_ref loc) {
    numeric_prefix prefix;
    const std::uint64_t sign_bit = static_cast<std::uint64_t>(1 + exp_max) << bpm;
    if (u64 & sign_bit) {  // negative value
        prefix.push_back('-');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_pos) {
        prefix.push_back('+');
    } else if ((fmt.flags & fmt_flags::sign_field) == fmt_flags::sign_align) {
        prefix.push_back(' ');
    }

    // Binary exponent and mantissa
    const bool uppercase = !!(fmt.flags & fmt_flags::uppercase);
    const fp_m64_t fp2{u64 & ((1ULL << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};
    if (fp2.exp == exp_max) {
        if (!!(fmt.flags & fmt_flags::throw_on_inf_nan)) {
            throw std::out_of_range("floating point number is inf of nan");
        }

        // Print infinity or NaN
        const auto sval = fp2.m == 0 ? default_numpunct<CharT>().infname(uppercase) :
                                       default_numpunct<CharT>().nanname(uppercase);
        const unsigned len = static_cast<unsigned>(sval.size()) + prefix.len;
        const auto fn = [&sval, prefix](basic_membuffer<CharT>& out) {
            if (prefix.len) { out += prefix.chars[0]; }
            out += sval;
        };
        return fmt.width > len ? append_adjusted(out, fn, len, fmt, true) : fn(out);
    }

    if ((fmt.flags & fmt_flags::base_field) == fmt_flags::hex) {
        // Print hexadecimal representation
        fp_hex_fmt_t fp(fp2, fmt.prec, !!(fmt.flags & fmt_flags::alternate));
        fp.format(bpm, exp_max >> 1);

        print_float_functor<CharT, fp_hex_fmt_t> fn{out, fp, uppercase, default_numpunct<CharT>().decimal_point()};
        const unsigned len = fp.get_len() + prefix.len;
        if (!!(fmt.flags & fmt_flags::localize)) {
            fn.dec_point = std::use_facet<std::numpunct<CharT>>(*loc).decimal_point();
        }
        return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt) : fn(len, prefix);
    }

    // Print decimal representation
    fp_dec_fmt_t fp(fmt.prec, !!(fmt.flags & fmt_flags::alternate));
    const fmt_flags fp_fmt = fmt.flags & fmt_flags::float_field;
    if (fp_fmt == fmt_flags::none && fmt.prec < 0) {
        fp.format_default(fp2, bpm, exp_max >> 1, !!(fmt.flags & fmt_flags::mandatory_frac));
    } else {
        fp.format(fp2, bpm, exp_max >> 1, fp_fmt);
    }

    print_float_functor<CharT, fp_dec_fmt_t> fn{out, fp, uppercase, default_numpunct<CharT>().decimal_point()};
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        fn.dec_point = numpunct.decimal_point();
        if (!grouping.grouping.empty()) {
            const unsigned len = fp.get_len_with_grouping(grouping.grouping) + prefix.len;
            return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }

    const unsigned len = fp.get_len() + prefix.len;
    return fmt.width > len ? adjust_numeric(out, fn, len, prefix, fmt) : fn(len, prefix);
}

}  // namespace sconv
}  // namespace uxs
