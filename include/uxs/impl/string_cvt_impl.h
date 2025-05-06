#pragma once

#include "uxs/string_cvt.h"

#include <cstdlib>

#define UXS_SCVT_USE_COMPILER_128BIT_EXTENSIONS 1

#if UXS_SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0
#    if defined(_MSC_VER) && defined(_M_X64)
#        include <intrin.h>
#    elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#    endif
#endif  // UXS_SCVT_USE_COMPILER_128BIT_EXTENSIONS

namespace uxs {
namespace scvt {

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

const UXS_CONSTEXPR std::uint64_t msb64 = 1ULL << 63;
inline UXS_CONSTEXPR std::uint64_t lo32(std::uint64_t x) { return x & 0xffffffff; }
inline UXS_CONSTEXPR std::uint64_t hi32(std::uint64_t x) { return x >> 32; }
template<typename TyH, typename TyL>
UXS_CONSTEXPR std::uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<std::uint64_t>(hi) << 32) | static_cast<std::uint64_t>(lo);
}

#if UXS_SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
inline unsigned ulog2(std::uint32_t x) {
    unsigned long ret;
    _BitScanReverse(&ret, x | 1);
    return ret;
}
inline unsigned ulog2(std::uint64_t x) {
    unsigned long ret;
    _BitScanReverse64(&ret, x | 1);
    return ret;
}
#elif UXS_SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
inline unsigned ulog2(std::uint32_t x) { return __builtin_clz(x | 1) ^ 31; }
inline unsigned ulog2(std::uint64_t x) { return __builtin_clzll(x | 1) ^ 63; }
#else
struct ulog2_table_t {
    const std::uint8_t* operator()() const noexcept {
        static const UXS_CONSTEXPR std::uint8_t v[] = {
            0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
            7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
        return v;
    }
};
inline unsigned ulog2(std::uint32_t x) {
    unsigned bias = 0;
    if (x >= 1u << 16) { x >>= 16, bias += 16; }
    if (x >= 1u << 8) { x >>= 8, bias += 8; }
    return bias + ulog2_table_t{}()[x];
}
inline unsigned ulog2(std::uint64_t x) {
    if (x >= 1ull << 32) { return 32 + ulog2(static_cast<std::uint32_t>(hi32(x))); }
    return ulog2(static_cast<std::uint32_t>(lo32(x)));
}
#endif

// ---- from string to value

template<typename CharT>
const CharT* starts_with(const CharT* p, const CharT* end, std::basic_string_view<CharT> s) noexcept {
    if (static_cast<std::size_t>(end - p) < s.size()) { return p; }
    const CharT* p_s = s.data();
    for (const CharT* p1 = p; p1 < end; ++p1, ++p_s) {
        if (to_lower(*p1) != *p_s) { return p; }
    }
    return p + s.size();
}

template<typename CharT>
bool to_boolean(const CharT* p, const CharT* end, const CharT*& last) noexcept {
    unsigned dig = 0;
    const CharT* p0 = p;
    bool val = false;
    if ((p = starts_with(p, end, default_numpunct<CharT>().truename(false))) > p0) {
        val = true;
    } else if ((p = starts_with(p, end, default_numpunct<CharT>().falsename(false))) > p0) {
    } else if (p < end && (dig = dig_v(*p)) < 10) {
        do {
            if (dig) { val = true; }
        } while (++p < end && (dig = dig_v(*p)) < 10);
    }
    last = p;
    return val;
}

template<typename Ty, typename CharT>
Ty to_integer_common(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) noexcept {
    bool neg = false;
    last = p;
    if (p == end) { return 0; }

    if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }

    unsigned dig = 0;
    if (p == end || (dig = dig_v(*p)) >= 10) { return 0; }
    Ty val = dig;
    while (++p < end && (dig = dig_v(*p)) < 10) {
        Ty val0 = val;
        val = 10U * val + dig;
        if (val < val0) { return 0; }  // too big integer
    }

    // Note: resulting number must be in range [-(1 + pos_limit / 2), pos_limit]
    if (neg) {
        if (val > 1 + (pos_limit >> 1)) { return 0; }  // negative integer is out of range
        val = ~val + 1;                                // apply sign
    } else if (val > pos_limit) {
        return 0;  // positive integer is out of range
    }

    last = p;
    return val;
}

const UXS_CONSTEXPR int max_pow10_size = 13;
const UXS_CONSTEXPR int max_fp10_mantissa_size = 41;  // ceil(log2(10^(768 + 18)))
const UXS_CONSTEXPR int fp10_bits_size = max_fp10_mantissa_size + max_pow10_size;
struct fp10_t {
    int exp = 0;
    unsigned bits_used = 1;
    std::uint64_t bits[fp10_bits_size];
    bool zero_tail = true;
};

UXS_EXPORT std::uint64_t bignum_mul32(std::uint64_t* x, unsigned sz, std::uint32_t mul, std::uint32_t bias);

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, fp10_t& fp10) noexcept {
    const UXS_CONSTEXPR std::uint64_t short_lim = 1000000000000000000ULL;
    std::uint64_t* m10 = &fp10.bits[max_fp10_mantissa_size - fp10.bits_used];
    if (fp10.bits_used == 1) {
        std::uint64_t m = *m10;
        for (unsigned dig = 0; p < end && (dig = dig_v(*p)) < 10 && m < short_lim; ++p) { m = 10U * m + dig; }
        *m10 = m;
    }
    for (unsigned dig = 0; p < end && (dig = dig_v(*p)) < 10; ++p) {
        if (fp10.bits_used < max_fp10_mantissa_size) {
            const std::uint64_t higher = bignum_mul32(m10, fp10.bits_used, 10U, dig);
            if (higher) { *--m10 = higher, ++fp10.bits_used; }
        } else {
            if (dig > 0) { fp10.zero_tail = false; }
            ++fp10.exp;
        }
    }
    return p;
}

template<typename CharT>
const CharT* chars_to_fp10(const CharT* p, const CharT* end, fp10_t& fp10) noexcept {
    const CharT* p0 = nullptr;
    unsigned dig = 0;
    const CharT dec_point = default_numpunct<CharT>().decimal_point();
    if (p == end) { return p; }
    if ((dig = dig_v(*p)) < 10) {  // integral part
        fp10.bits[max_fp10_mantissa_size - 1] = dig;
        p = accum_mantissa(++p, end, fp10);
        if (p == end) { return p; }
        if (*p != dec_point) { goto parse_exponent; }
    } else if (*p == dec_point && p + 1 < end && (dig = dig_v(*(p + 1))) < 10) {
        fp10.bits[max_fp10_mantissa_size - 1] = dig, fp10.exp = -1, ++p;  // tenth
    } else {
        return p;
    }

    p0 = p + 1;
    p = accum_mantissa(p0, end, fp10);  // fractional part
    fp10.exp -= static_cast<unsigned>(p - p0);
    if (p == end) { return p; }

parse_exponent:
    p0 = p;
    if (*p == 'e' || *p == 'E') {  // optional exponent
        const int exp_optional = to_integer<int>(p + 1, end, p);
        if (p > p0 + 1) { fp10.exp += exp_optional, p0 = p; }
    }
    return p0;
}

UXS_EXPORT std::uint64_t fp10_to_fp2(fp10_t& fp10, unsigned bpm, int exp_max) noexcept;

template<typename CharT>
std::uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, unsigned bpm, int exp_max) noexcept {
    std::uint64_t fp2 = 0;
    last = p;
    if (p == end) { return 0; }

    if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, fp2 = static_cast<std::uint64_t>(1 + exp_max) << bpm;  // negative sign
    }

    fp10_t fp10;
    const CharT* p1 = chars_to_fp10(p, end, fp10);
    if (p1 > p) {
        fp2 |= fp10_to_fp2(fp10, bpm, exp_max);
    } else if ((p1 = starts_with(p, end, default_numpunct<CharT>().infname(false))) > p) {  // infinity
        fp2 |= static_cast<std::uint64_t>(exp_max) << bpm;
    } else if ((p1 = starts_with(p, end, default_numpunct<CharT>().nanname(false))) > p) {  // NaN
        fp2 |= (static_cast<std::uint64_t>(exp_max) << bpm) | ((1ULL << bpm) - 1);
    } else {
        return 0;
    }

    last = p1;
    return fp2;
}

// ---- from value to string

// minimal digit count for numbers 2^N <= x < 2^(N+1), N = 0, 1, 2, ...
UXS_FORCE_INLINE unsigned get_exp2_dig_count(unsigned exp) noexcept {
    static const UXS_CONSTEXPR unsigned dig_count[] = {
        1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,
        7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13,
        14, 14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
    assert(exp >= 0 && exp < sizeof(dig_count) / sizeof(dig_count[0]));
    return dig_count[exp];
}

// powers of ten 10^N, N = 0, 1, 2, ...
UXS_FORCE_INLINE std::uint64_t get_pow10(int pow) noexcept {
#define UXS_SCVT_POWERS_OF_10(base) \
    base, (base) * 10, (base) * 100, (base) * 1000, (base) * 10000, (base) * 100000, (base) * 1000000, \
        (base) * 10000000, (base) * 100000000, (base) * 1000000000
    static const UXS_CONSTEXPR std::uint64_t ten_pows[] = {UXS_SCVT_POWERS_OF_10(1ULL),
                                                           UXS_SCVT_POWERS_OF_10(10000000000ULL)};
#undef UXS_SCVT_POWERS_OF_10
    assert(pow >= 0 && pow < static_cast<int>(sizeof(ten_pows) / sizeof(ten_pows[0])));
    return ten_pows[pow];
}

struct numeric_prefix {
    std::uint8_t len = 0;
    std::array<char, 3> chars{};
    void push_back(char symbol) { chars[len++] = symbol; }
    template<typename OutputIt>
    void print(OutputIt out) const {
        for (unsigned n = 0; n < len; ++n) { *out++ = chars[n]; }
    }
};

template<typename CharT, typename Func, typename... Args>
void adjust_numeric(basic_membuffer<CharT>& s, Func fn, unsigned len, numeric_prefix prefix, fmt_opts fmt,
                    Args&&... args) {
    unsigned left = fmt.width - len - prefix.len;
    unsigned right = left;
    if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::left) {
        left = 0;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::internal) {
        left >>= 1, right -= left;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::right || !(fmt.flags & fmt_flags::leading_zeroes)) {
        right = 0;
    } else {
        prefix.print(std::back_inserter(s));
        s.append(left, '0');
        return fn(len, numeric_prefix{}, std::forward<Args>(args)...);
    }
    s.append(left, fmt.fill);
    fn(len, prefix, std::forward<Args>(args)...);
    s.append(right, fmt.fill);
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
    basic_membuffer<CharT>& s;
    Ty val;
    PrintFn generate_fn;
    template<typename... Args>
    UXS_FORCE_INLINE void operator()(unsigned len, numeric_prefix prefix, Args&&... args) const {
        len += prefix.len;
        if (s.avail() >= len) {
            prefix.print(s.endp());
            generate_fn(s.endp() + len, val, std::forward<Args>(args)...);
            s.advance(len);
        } else {
            std::array<CharT, 256> buf;
            prefix.print(buf.data());
            generate_fn(buf.data() + len, val, std::forward<Args>(args)...);
            s.append(buf.data(), len);
        }
    }
};

template<typename CharT, typename Ty, typename PrintFn>
print_functor<CharT, Ty, PrintFn> make_print_functor(basic_membuffer<CharT>& s, Ty val, PrintFn generate_fn) {
    return print_functor<CharT, Ty, PrintFn>{s, val, generate_fn};
}

// ---- binary

template<typename CharT, typename Ty>
void fmt_gen_bin(CharT* p, Ty val) noexcept {
    do {
        *--p = '0' + static_cast<unsigned>(val & 1);
        val >>= 1;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_bin_with_grouping(CharT* p, Ty val, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    *--p = '0' + static_cast<unsigned>(val & 1);
    while ((val >>= 1) != 0) {
        if (--cnt <= 0) {
            *--p = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = '0' + static_cast<unsigned>(val & 1);
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_bin(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
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
    unsigned len = 1 + ulog2(val);
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            auto fn = make_print_functor<CharT>(s, val, fmt_gen_bin_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len, grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    auto fn = make_print_functor<CharT>(s, val, fmt_gen_bin<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- octal

template<typename CharT, typename Ty>
void fmt_gen_oct(CharT* p, Ty val) noexcept {
    do {
        *--p = '0' + static_cast<unsigned>(val & 7);
        val >>= 3;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_oct_with_grouping(CharT* p, Ty val, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    *--p = '0' + static_cast<unsigned>(val & 7);
    while ((val >>= 3) != 0) {
        if (--cnt <= 0) {
            *--p = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = '0' + static_cast<unsigned>(val & 7);
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_oct(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
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
    unsigned len = 1 + ulog2(val) / 3;
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            auto fn = make_print_functor<CharT>(s, val, fmt_gen_oct_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len, grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    auto fn = make_print_functor<CharT>(s, val, fmt_gen_oct<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- hexadecimal

template<typename CharT, typename Ty>
void fmt_gen_hex(CharT* p, Ty val, bool uppercase) noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        *--p = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
}

template<typename CharT, typename Ty>
void fmt_gen_hex_with_grouping(CharT* p, Ty val, bool uppercase, const grouping_t<CharT>& grouping) noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    *--p = digs[val & 0xf];
    while ((val >>= 4) != 0) {
        if (--cnt <= 0) {
            *--p = grouping.thousands_sep;
            cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = digs[val & 0xf];
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_hex(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
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
    unsigned len = 1 + (ulog2(val) >> 2);
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            auto fn = make_print_functor<CharT>(s, val, fmt_gen_hex_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len, grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, uppercase, grouping) :
                                     fn(len, prefix, uppercase, grouping);
        }
    }
    auto fn = make_print_functor<CharT>(s, val, fmt_gen_hex<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, uppercase) : fn(len, prefix, uppercase);
}

// ---- decimal

template<typename Ty>
UXS_FORCE_INLINE unsigned fmt_dec_unsigned_len(Ty val) noexcept {
    const unsigned pow = get_exp2_dig_count(ulog2(val));
    return val >= get_pow10(pow) ? pow + 1 : pow;
}

template<typename CharT>
void copy2(CharT* tgt, const char* src) {
    *tgt++ = *src++;
    *tgt = *src;
}

inline void copy2(char* tgt, const char* src) { std::memcpy(tgt, src, 2); }

template<unsigned N, typename Ty>
Ty divmod(Ty& v) {
    const Ty v0 = v;
    v /= N;
    return v0 - N * v;
}

template<typename CharT, typename Ty>
UXS_FORCE_INLINE CharT* gen_digits(CharT* p, Ty v) noexcept {
    while (v >= 100U) { copy2(p -= 2, get_digits(static_cast<unsigned>(divmod<100U>(v)))); }
    if (v >= 10U) {
        copy2(p -= 2, get_digits(static_cast<unsigned>(v)));
        return p;
    }
    *--p = '0' + static_cast<unsigned>(v);
    return p;
}

template<typename CharT, typename Ty>
UXS_FORCE_INLINE Ty gen_digits_n(CharT* p, Ty v, unsigned n) noexcept {
    CharT* p0 = p - (n & ~1);
    while (p != p0) { copy2(p -= 2, get_digits(static_cast<unsigned>(divmod<100U>(v)))); }
    if (!(n & 1)) { return v; }
    *--p = '0' + static_cast<unsigned>(divmod<10U>(v));
    return v;
}

template<typename CharT, typename Ty>
void fmt_gen_dec_with_grouping(CharT* p, Ty val, const grouping_t<CharT>& grouping) noexcept {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    *--p = '0' + static_cast<unsigned>(divmod<10U>(val));
    while (val) {
        if (--cnt <= 0) {
            *--p = grouping.thousands_sep, cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = '0' + static_cast<unsigned>(divmod<10U>(val));
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_dec(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
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
    unsigned len = fmt_dec_unsigned_len(val);
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        const grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            auto fn = make_print_functor<CharT>(s, val, fmt_gen_dec_with_grouping<CharT, Ty>);
            len = calc_len_with_grouping(len, grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    auto fn = make_print_functor<CharT>(s, val, gen_digits<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt) : fn(len, prefix);
}

// ---- integer

template<typename CharT, typename Ty>
void fmt_integer_common(basic_membuffer<CharT>& s, Ty val, bool is_signed, fmt_opts fmt, locale_ref loc) {
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::bin: return fmt_bin(s, val, is_signed, fmt, loc);
        case fmt_flags::oct: return fmt_oct(s, val, is_signed, fmt, loc);
        case fmt_flags::hex: return fmt_hex(s, val, is_signed, fmt, loc);
        case fmt_flags::character: {
            const Ty char_mask = static_cast<Ty>((1ULL << (8 * sizeof(CharT))) - 1);
            if ((val & char_mask) != val && (~val & char_mask) != val) {
                throw format_error("integral cannot be represented as a character");
            }
            const auto fn = [val](basic_membuffer<CharT>& s) { s += static_cast<CharT>(val); };
            return fmt.width > 1 ? append_adjusted(s, fn, 1, fmt) : fn(s);
        } break;
        default: return fmt_dec(s, val, is_signed, fmt, loc);
    }
}

// ---- boolean

template<typename CharT>
void fmt_boolean(basic_membuffer<CharT>& s, bool val, fmt_opts fmt, locale_ref loc) {
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::dec: return fmt_dec(s, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::bin: return fmt_bin(s, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::oct: return fmt_oct(s, static_cast<std::uint32_t>(val), false, fmt, loc);
        case fmt_flags::hex: return fmt_hex(s, static_cast<std::uint32_t>(val), false, fmt, loc);
        default: {
            if (!!(fmt.flags & fmt_flags::localize)) {
                const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
                const auto sval = val ? numpunct.truename() : numpunct.falsename();
                const auto fn = [&sval](basic_membuffer<CharT>& s) { s += sval; };
                return fmt.width > sval.size() ? append_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) :
                                                 fn(s);
            }
            const bool uppercase = !!(fmt.flags & fmt_flags::uppercase);
            const auto sval = val ? default_numpunct<CharT>().truename(uppercase) :
                                    default_numpunct<CharT>().falsename(uppercase);
            const auto fn = [&sval](basic_membuffer<CharT>& s) { s += sval; };
            return fmt.width > sval.size() ? append_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) : fn(s);
        } break;
    }
}

// ---- character

template<typename CharT>
void fmt_character(basic_membuffer<CharT>& s, CharT val, fmt_opts fmt, locale_ref loc) {
    const std::uint32_t code = static_cast<typename std::make_unsigned<CharT>::type>(val);
    switch (fmt.flags & fmt_flags::base_field) {
        case fmt_flags::dec: return fmt_dec(s, code, false, fmt, loc);
        case fmt_flags::bin: return fmt_bin(s, code, false, fmt, loc);
        case fmt_flags::oct: return fmt_oct(s, code, false, fmt, loc);
        case fmt_flags::hex: return fmt_hex(s, code, false, fmt, loc);
        default: {
            if (!(fmt.flags & fmt_flags::debug_format)) {
                const auto fn = [val](basic_membuffer<CharT>& s) { s += val; };
                return fmt.width > 1 ? append_adjusted(s, fn, 1, fmt) : fn(s);
            }
            if (fmt.width == 0) {
                append_escaped_text(s, &val, &val + 1, true);
                return;
            }
            std::array<CharT, 16> buf;
            basic_membuffer<CharT> membuf(buf.data());
            const std::size_t width = append_escaped_text(membuf, &val, &val + 1, true);
            const auto fn = [&buf, &membuf](basic_membuffer<CharT>& s) { s.append(buf.data(), membuf.endp()); };
            return fmt.width > width ? append_adjusted(s, fn, static_cast<unsigned>(width), fmt) : fn(s);
        } break;
    }
}

// ---- string

template<typename CharT>
void fmt_string(basic_membuffer<CharT>& s, std::basic_string_view<CharT> val, fmt_opts fmt, locale_ref) {
    if (!(fmt.flags & fmt_flags::debug_format)) {
        std::size_t width = 0;
        std::uint32_t code = 0;
        auto first = val.begin();
        auto last = val.end();
        if (fmt.prec >= 0 || fmt.width > 0) {
            const std::size_t max_width = fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max();
            last = first;
            for (auto next = first; utf_decoder<CharT>{}.decode(last, val.end(), next, code) != 0; last = next) {
                const unsigned w = get_utf_code_width(code);
                if (max_width - width < w) { break; }
                width += w;
            }
        }
        const auto fn = [first, last](basic_membuffer<CharT>& s) { s += to_string_view(first, last); };
        return fmt.width > width ? append_adjusted(s, fn, static_cast<unsigned>(width), fmt) : fn(s);
    }
    if (fmt.width == 0) {
        append_escaped_text(s, val.begin(), val.end(), false,
                            fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max());
        return;
    }
    inline_basic_dynbuffer<CharT> buf;
    const std::size_t width = append_escaped_text<basic_membuffer<CharT>>(
        buf, val.begin(), val.end(), false, fmt.prec >= 0 ? fmt.prec : std::numeric_limits<std::size_t>::max());
    const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.data(), buf.size()); };
    return fmt.width > width ? append_adjusted(s, fn, static_cast<unsigned>(width), fmt) : fn(s);
}

// ---- float

const UXS_CONSTEXPR int max_double_digits = 767;
const UXS_CONSTEXPR int digs_per_64 = 18;  // size of 64-bit digit pack

class fp_hex_fmt_t {
 public:
    UXS_EXPORT fp_hex_fmt_t(const fp_m64_t& fp2, fmt_opts fmt, unsigned bpm, int exp_bias) noexcept;

    unsigned get_len() const noexcept {
        return 3 + (prec_ > 0 || alternate_ ? prec_ + 1 : 0) + fmt_dec_unsigned_len<std::uint32_t>(std::abs(exp_));
    }

    template<typename CharT>
    UXS_EXPORT void generate(CharT* p, bool uppercase, CharT dec_point) const noexcept;

 private:
    std::uint64_t significand_;
    int exp_;
    int prec_;
    int n_zeroes_;
    bool alternate_;
};

template<typename CharT>
void fp_hex_fmt_t::generate(CharT* p, bool uppercase, CharT dec_point) const noexcept {
    const char* digs = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    // generate exponent
    int exp2 = exp_;
    char exp_sign = '+';
    if (exp2 < 0) { exp_sign = '-', exp2 = -exp2; }
    p = gen_digits(p, static_cast<unsigned>(exp2));
    *--p = exp_sign;
    *--p = uppercase ? 'P' : 'p';
    std::uint64_t m = significand_;
    if (prec_ > 0) {  // has fractional part
        assert(prec_ >= n_zeroes_);
        std::fill_n((p -= n_zeroes_), n_zeroes_, '0');
        for (int n = prec_ - n_zeroes_; n > 0; --n) {
            *--p = digs[m & 0xf];
            m >>= 4;
        }
        *--p = dec_point;
    } else if (alternate_) {  // only one digit
        *--p = dec_point;
    }
    *--p = digs[m & 0xf];
}

class fp_dec_fmt_t {
 public:
    UXS_EXPORT fp_dec_fmt_t(fp_m64_t fp2, fmt_opts fmt, unsigned bpm, int exp_bias) noexcept;

    unsigned get_len() const noexcept {
        return (fixed_ ? get_integral_len() : 1 + get_exponent_len()) + get_frac_len();
    }

    unsigned get_len_with_grouing(std::string_view grouping) const noexcept {
        return (fixed_ ? calc_len_with_grouping(get_integral_len(), grouping) : 1 + get_exponent_len()) +
               get_frac_len();
    }

    template<typename CharT>
    void generate(CharT* p, bool uppercase, CharT dec_point) const noexcept {
        return fixed_ ? generate_fixed<CharT>(p, dec_point, nullptr) : generate_scientific(p, uppercase, dec_point);
    }

    template<typename CharT>
    void generate(CharT* p, bool uppercase, CharT dec_point, const grouping_t<CharT>& grouping) const noexcept {
        return fixed_ ? generate_fixed<CharT>(p, dec_point, &grouping) : generate_scientific(p, uppercase, dec_point);
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
    void generate_scientific(CharT* p, bool uppercase, CharT dec_point) const noexcept;

    template<typename CharT>
    void generate_fixed(CharT* p, CharT dec_point, const grouping_t<CharT>* grouping) const noexcept;

    unsigned get_frac_len() const noexcept { return prec_ > 0 || alternate_ ? prec_ + 1 : 0; }
    unsigned get_integral_len() const noexcept { return 1 + std::max(exp_, 0); }
    unsigned get_exponent_len() const noexcept { return exp_ <= -100 || exp_ >= 100 ? 5 : 4; }

    UXS_EXPORT void format_short_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
    UXS_EXPORT void format_short_decimal_slow(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
    UXS_EXPORT void format_long_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept;
};

template<typename CharT>
void fp_dec_fmt_t::generate_scientific(CharT* p, bool uppercase, CharT dec_point) const noexcept {
    // generate exponent
    int exp10 = exp_;
    char exp_sign = '+';
    if (exp10 < 0) { exp_sign = '-', exp10 = -exp10; }
    if (exp10 < 100) {
        copy2(p -= 2, get_digits(exp10));
    } else {
        const int t = (656 * exp10) >> 16;
        copy2(p -= 2, get_digits(exp10 - 100 * t));
        *--p = '0' + t;
    }
    *--p = exp_sign;
    *--p = uppercase ? 'E' : 'e';

    if (prec_ > 0) {         // has fractional part
        if (significand_) {  // generate from significand
            p = gen_digits(p, significand_);
        } else {  // generate from chars
            p -= prec_ + 1;
            std::fill_n(std::copy_n(digs_buf_, prec_ + 1 - n_zeroes_, p), n_zeroes_, '0');
        }
        // insert decimal point
        *(p - 1) = *p;
        *p = dec_point;
    } else {  // only one digit
        if (alternate_) { *--p = dec_point; }
        *--p = '0' + static_cast<unsigned>(significand_);
    }
}

template<typename CharT>
void fp_dec_fmt_t::generate_fixed(CharT* p, CharT dec_point, const grouping_t<CharT>* grouping) const noexcept {
    std::uint64_t m = significand_;
    int k = 1 + exp_;
    int n_zeroes = n_zeroes_;
    if (prec_ > 0) {             // has fractional part
        if (k > 0) {             // fixed form [1-9]+.[0-9]+
            if (significand_) {  // generate fractional part from significand
                m = gen_digits_n(p, m, prec_);
            } else {  // generate from chars
                if (n_zeroes < prec_) {
                    std::fill_n(std::copy_n(digs_buf_ + k, prec_ - n_zeroes, p - prec_), n_zeroes, '0');
                } else {  // all zeroes
                    std::fill_n(p - prec_, prec_, '0');
                }
                n_zeroes -= prec_;
            }
            *(p -= 1 + prec_) = dec_point;
        } else {  // fixed form 0.0*[1-9][0-9]*
            // append trailing fractional part later as is was integral part
            std::fill_n(p - prec_ - 2, 2 - k, '0');  // one `0` will be replaced with decimal point later
            *(p - prec_ - 1) = dec_point;            // put decimal point into reserved cell
            k += prec_;
        }
    } else if (alternate_) {
        *--p = dec_point;
    }

    if (!grouping || exp_ <= 0) {  // no grouping is needed
        if (significand_) {        // generate integral part from significand
            gen_digits(p, m);
        } else if (n_zeroes > 0) {  // generate from chars
            std::fill_n(std::copy_n(digs_buf_, k - n_zeroes, p - k), n_zeroes, '0');
        } else {
            std::copy_n(digs_buf_, k, p - k);
        }
        return;
    }

    auto grp_it = grouping->grouping.begin();
    int cnt = *grp_it;
    if (significand_) {
        *--p = '0' + static_cast<unsigned>(divmod<10U>(m));
        while (m) {
            if (--cnt <= 0) {
                *--p = grouping->thousands_sep;
                cnt = std::next(grp_it) != grouping->grouping.end() ? *++grp_it : *grp_it;
            }
            *--p = '0' + static_cast<unsigned>(divmod<10U>(m));
        }
        return;
    }

    const char* digs = digs_buf_;
    if (n_zeroes > 0) {
        digs += k - n_zeroes;
        *--p = '0';
        while (--n_zeroes) {
            if (--cnt <= 0) {
                *--p = grouping->thousands_sep;
                cnt = std::next(grp_it) != grouping->grouping.end() ? *++grp_it : *grp_it;
            }
            *--p = '0';
        }
    } else {
        digs += k;
        *--p = *--digs;
    }
    while (digs != digs_buf_) {
        if (--cnt <= 0) {
            *--p = grouping->thousands_sep;
            cnt = std::next(grp_it) != grouping->grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = *--digs;
    }
}

template<typename CharT, typename FpFmtTy>
struct print_float_functor {
    basic_membuffer<CharT>& s;
    const FpFmtTy& fp;
    bool uppercase;
    CharT dec_point;
    template<typename... Args>
    UXS_FORCE_INLINE void operator()(unsigned len, numeric_prefix prefix, Args&&... args) const {
        len += prefix.len;
        if (s.avail() >= len) {
            if (prefix.len) { *s.endp() = prefix.chars[0]; }
            fp.generate(s.endp() + len, uppercase, dec_point, std::forward<Args>(args)...);
            s.advance(len);
        } else {
            inline_basic_dynbuffer<CharT> buf;
            buf.reserve(len);
            if (prefix.len) { *buf.data() = prefix.chars[0]; }
            fp.generate(buf.data() + len, uppercase, dec_point, std::forward<Args>(args)...);
            s.append(buf.data(), len);
        }
    }
};

template<typename CharT>
void fmt_float_common(basic_membuffer<CharT>& s, std::uint64_t u64, fmt_opts fmt, unsigned bpm, int exp_max,
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
        // Print infinity or NaN
        const auto sval = fp2.m == 0 ? default_numpunct<CharT>().infname(uppercase) :
                                       default_numpunct<CharT>().nanname(uppercase);
        const unsigned len = prefix.len + static_cast<unsigned>(sval.size());
        const auto fn = [&sval, prefix](basic_membuffer<CharT>& s) {
            if (prefix.len) { s += prefix.chars[0]; }
            s += sval;
        };
        return fmt.width > len ? append_adjusted(s, fn, len, fmt, true) : fn(s);
    }

    if ((fmt.flags & fmt_flags::base_field) == fmt_flags::hex) {
        // Print hexadecimal representation
        const fp_hex_fmt_t fp(fp2, fmt, bpm, exp_max >> 1);
        const unsigned len = fp.get_len();
        print_float_functor<CharT, fp_hex_fmt_t> fn{s, fp, uppercase, default_numpunct<CharT>().decimal_point()};
        if (!!(fmt.flags & fmt_flags::localize)) {
            fn.dec_point = std::use_facet<std::numpunct<CharT>>(*loc).decimal_point();
        }
        return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt) : fn(len, prefix);
    }

    // Print decimal representation
    const fp_dec_fmt_t fp(fp2, fmt, bpm, exp_max >> 1);
    print_float_functor<CharT, fp_dec_fmt_t> fn{s, fp, uppercase, default_numpunct<CharT>().decimal_point()};
    if (!!(fmt.flags & fmt_flags::localize)) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        fn.dec_point = numpunct.decimal_point();
        if (!grouping.grouping.empty()) {
            const unsigned len = fp.get_len_with_grouing(grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt, grouping) : fn(len, prefix, grouping);
        }
    }
    const unsigned len = fp.get_len();
    return fmt.width > len ? adjust_numeric(s, fn, len, prefix, fmt) : fn(len, prefix);
}

}  // namespace scvt
}  // namespace uxs
