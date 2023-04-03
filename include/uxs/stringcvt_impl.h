#pragma once

#include "stringcvt.h"

#include <array>
#include <cstring>
#include <locale>

#define SCVT_USE_COMPILER_128BIT_EXTENSIONS 1

#if defined(_MSC_VER)
#    define SCVT_FORCE_INLINE __forceinline
#elif defined(__GNUC__)
#    define SCVT_FORCE_INLINE __attribute__((always_inline)) inline
#else
#    define SCVT_FORCE_INLINE inline
#endif

#if !defined(SCVT_CONSTEXPR_DATA)
#    if __cplusplus < 201703L
#        define SCVT_CONSTEXPR_DATA const
#    else  // __cplusplus < 201703L
#        define SCVT_CONSTEXPR_DATA constexpr
#    endif  // __cplusplus < 201703L
#endif      // !defined(SCVT_CONSTEXPR_DATA)

#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0
#    if defined(_MSC_VER) && defined(_M_X64)
#        include <intrin.h>
#    elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#    endif
#endif  // SCVT_USE_COMPILER_128BIT_EXTENSIONS

#ifdef __has_builtin
#    define SCVT_HAS_BUILTIN(x) __has_builtin(x)
#else  // __has_builtin
#    define SCVT_HAS_BUILTIN(x) 0
#endif  // __has_builtin

namespace uxs {
namespace scvt {

template<typename CharT>
struct default_numpunct;

template<>
struct default_numpunct<char> {
    CONSTEXPR char decimal_point() const { return '.'; }
    CONSTEXPR std::string_view infname(const bool upper = false) const { return upper ? "INF" : "inf"; }
    CONSTEXPR std::string_view nanname(const bool upper = false) const { return upper ? "NAN" : "nan"; }
    CONSTEXPR std::string_view truename(const bool upper = false) const { return upper ? "TRUE" : "true"; }
    CONSTEXPR std::string_view falsename(const bool upper = false) const { return upper ? "FALSE" : "false"; }
};

template<>
struct default_numpunct<wchar_t> {
    CONSTEXPR wchar_t decimal_point() const { return '.'; }
    CONSTEXPR std::wstring_view infname(const bool upper = false) const { return upper ? L"INF" : L"inf"; }
    CONSTEXPR std::wstring_view nanname(const bool upper = false) const { return upper ? L"NAN" : L"nan"; }
    CONSTEXPR std::wstring_view truename(const bool upper = false) const { return upper ? L"TRUE" : L"true"; }
    CONSTEXPR std::wstring_view falsename(const bool upper = false) const { return upper ? L"FALSE" : L"false"; }
};

struct fp_m64_t {
    uint64_t m;
    int exp;
};

SCVT_CONSTEXPR_DATA uint64_t msb64 = 1ull << 63;
CONSTEXPR uint64_t lo32(uint64_t x) { return x & 0xffffffff; }
CONSTEXPR uint64_t hi32(uint64_t x) { return x >> 32; }
template<typename TyH, typename TyL>
CONSTEXPR uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
inline unsigned ulog2(uint32_t x) {
    unsigned long ret;
    _BitScanReverse(&ret, x | 1);
    return ret;
}
inline unsigned ulog2(uint64_t x) {
    unsigned long ret;
    _BitScanReverse64(&ret, x | 1);
    return ret;
}
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
inline unsigned ulog2(uint32_t x) { return __builtin_clz(x | 1) ^ 31; }
inline unsigned ulog2(uint64_t x) { return __builtin_clzll(x | 1) ^ 63; }
#else
struct ulog2_table_t {
    std::array<unsigned, 256> n_bit;
    CONSTEXPR ulog2_table_t() : n_bit() {
        for (uint32_t n = 0; n < n_bit.size(); ++n) {
            uint32_t u8 = n;
            n_bit[n] = 0;
            while (u8 >>= 1) { ++n_bit[n]; }
        }
    }
};
#    if __cplusplus < 201703L
extern UXS_EXPORT const ulog2_table_t g_ulog2_tbl;
#    else   // __cplusplus < 201703L
constexpr ulog2_table_t g_ulog2_tbl{};
#    endif  // __cplusplus < 201703L
inline unsigned ulog2(uint32_t x) {
    unsigned bias = 0;
    if (x >= 1u << 16) { x >>= 16, bias += 16; }
    if (x >= 1u << 8) { x >>= 8, bias += 8; }
    return bias + g_ulog2_tbl.n_bit[x];
}
inline unsigned ulog2(uint64_t x) {
    if (x >= 1ull << 32) { return 32 + ulog2(static_cast<uint32_t>(hi32(x))); }
    return ulog2(static_cast<uint32_t>(lo32(x)));
}
#endif

// ---- from string to value

template<typename CharT>
const CharT* starts_with(const CharT* p, const CharT* end, std::basic_string_view<CharT> s) NOEXCEPT {
    if (static_cast<size_t>(end - p) < s.size()) { return p; }
    const CharT* p_s = s.data();
    for (const CharT* p1 = p; p1 < end; ++p1, ++p_s) {
        if (to_lower(*p1) != *p_s) { return p; }
    }
    return p + s.size();
}

template<typename Ty, typename CharT>
Ty to_integral_common(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) NOEXCEPT {
    bool neg = false;
    last = p;
    if (p == end) {
        return 0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }
    unsigned dig = 0;
    if (p == end || (dig = dig_v(*p)) >= 10) { return 0; }
    Ty val = dig;
    while (++p < end && (dig = dig_v(*p)) < 10) {
        Ty val0 = val;
        val = 10u * val + dig;
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

template<typename Ty, typename CharT>
Ty to_bool(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT {
    unsigned dig = 0;
    const CharT* p0 = p;
    bool val = false;
    if ((p = starts_with(p, end, default_numpunct<CharT>().truename())) > p0) {
        val = true;
    } else if ((p = starts_with(p, end, default_numpunct<CharT>().falsename())) > p0) {
    } else if (p < end && (dig = dig_v(*p)) < 10) {
        do {
            if (dig) { val = true; }
        } while (++p < end && (dig = dig_v(*p)) < 10);
    }
    last = p;
    return val;
}

SCVT_CONSTEXPR_DATA int kMaxPow10Size = 13;
SCVT_CONSTEXPR_DATA int kMaxFp10MantissaSize = 41;  // ceil(log2(10^(768 + 18)))
SCVT_CONSTEXPR_DATA int kFp10BitsSize = kMaxFp10MantissaSize + kMaxPow10Size;
struct fp10_t {
    int exp = 0;
    unsigned bits_used = 1;
    uint64_t bits[kFp10BitsSize];
    bool zero_tail = true;
};

UXS_EXPORT uint64_t bignum_mul32(uint64_t* x, unsigned sz, uint32_t mul, uint32_t bias);

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, fp10_t& fp10) NOEXCEPT {
    SCVT_CONSTEXPR_DATA uint64_t short_lim = 1000000000000000000ull;
    uint64_t* m10 = &fp10.bits[kMaxFp10MantissaSize - fp10.bits_used];
    if (fp10.bits_used == 1) {
        uint64_t m = *m10;
        for (unsigned dig = 0; p < end && (dig = dig_v(*p)) < 10 && m < short_lim; ++p) { m = 10u * m + dig; }
        *m10 = m;
    }
    for (unsigned dig = 0; p < end && (dig = dig_v(*p)) < 10; ++p) {
        if (fp10.bits_used < kMaxFp10MantissaSize) {
            const uint64_t higher = bignum_mul32(m10, fp10.bits_used, 10u, dig);
            if (higher) { *--m10 = higher, ++fp10.bits_used; }
        } else {
            if (dig > 0) { fp10.zero_tail = false; }
            ++fp10.exp;
        }
    }
    return p;
}

template<typename CharT>
const CharT* chars_to_fp10(const CharT* p, const CharT* end, fp10_t& fp10) NOEXCEPT {
    const CharT* p0;
    unsigned dig = 0;
    const CharT dec_point = default_numpunct<CharT>().decimal_point();
    if (p == end) { return p; }
    if ((dig = dig_v(*p)) < 10) {  // integral part
        fp10.bits[kMaxFp10MantissaSize - 1] = dig;
        p = accum_mantissa(++p, end, fp10);
        if (p == end) { return p; }
        if (*p != dec_point) { goto parse_exponent; }
    } else if (*p == dec_point && p + 1 < end && (dig = dig_v(*(p + 1))) < 10) {
        fp10.bits[kMaxFp10MantissaSize - 1] = dig, fp10.exp = -1, ++p;  // tenth
    } else {
        return p;
    }
    p0 = p + 1, p = accum_mantissa(p0, end, fp10);  // fractional part
    fp10.exp -= static_cast<unsigned>(p - p0);
    if (p == end) { return p; }
parse_exponent:
    p0 = p;
    if (*p == 'e' || *p == 'E') {  // optional exponent
        int exp_optional = to_integral<int>(p + 1, end, p);
        if (p > p0 + 1) { fp10.exp += exp_optional, p0 = p; }
    }
    return p0;
}

UXS_EXPORT uint64_t fp10_to_fp2(fp10_t& fp10, const unsigned bpm, const int exp_max) NOEXCEPT;

template<typename CharT>
uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, const unsigned bpm,
                         const int exp_max) NOEXCEPT {
    uint64_t fp2 = 0;
    last = p;

    if (p == end) {
        return 0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, fp2 = static_cast<uint64_t>(1 + exp_max) << bpm;  // negative sign
    }

    fp10_t fp10;
    const CharT* p1 = chars_to_fp10(p, end, fp10);
    if (p1 > p) {
        fp2 |= fp10_to_fp2(fp10, bpm, exp_max);
    } else if ((p1 = starts_with(p, end, default_numpunct<CharT>().infname())) > p) {  // infinity
        fp2 |= static_cast<uint64_t>(exp_max) << bpm;
    } else if ((p1 = starts_with(p, end, default_numpunct<CharT>().nanname())) > p) {  // NaN
        fp2 |= (static_cast<uint64_t>(exp_max) << bpm) | ((1ull << bpm) - 1);
    } else {
        return 0;
    }

    last = p1;
    return fp2;
}

// ---- from value to string

// minimal digit count for numbers 2^N <= x < 2^(N+1), N = 0, 1, 2, ...
SCVT_FORCE_INLINE unsigned get_exp2_dig_count(unsigned exp) NOEXCEPT {
    static SCVT_CONSTEXPR_DATA unsigned dig_count[] = {
        1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  6,  6,  6,  7,  7,
        7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13,
        14, 14, 14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};
    assert(exp >= 0 && exp < sizeof(dig_count) / sizeof(dig_count[0]));
    return dig_count[exp];
}

// powers of ten 10^N, N = 0, 1, 2, ...
SCVT_FORCE_INLINE uint64_t get_pow10(int pow) NOEXCEPT {
#define UXS_SCVT_POWERS_OF_10(base) \
    base, (base)*10, (base)*100, (base)*1000, (base)*10000, (base)*100000, (base)*1000000, (base)*10000000, \
        (base)*100000000, (base)*1000000000
    static SCVT_CONSTEXPR_DATA uint64_t ten_pows[] = {UXS_SCVT_POWERS_OF_10(1ull),
                                                      UXS_SCVT_POWERS_OF_10(10000000000ull)};
#undef UXS_SCVT_POWERS_OF_10
    assert(pow >= 0 && pow < static_cast<int>(sizeof(ten_pows) / sizeof(ten_pows[0])));
    return ten_pows[pow];
}

// digit pairs
SCVT_FORCE_INLINE const char* get_digits(unsigned n) NOEXCEPT {
    static SCVT_CONSTEXPR_DATA char digs[][2] = {
        {'0', '0'}, {'0', '1'}, {'0', '2'}, {'0', '3'}, {'0', '4'}, {'0', '5'}, {'0', '6'}, {'0', '7'}, {'0', '8'},
        {'0', '9'}, {'1', '0'}, {'1', '1'}, {'1', '2'}, {'1', '3'}, {'1', '4'}, {'1', '5'}, {'1', '6'}, {'1', '7'},
        {'1', '8'}, {'1', '9'}, {'2', '0'}, {'2', '1'}, {'2', '2'}, {'2', '3'}, {'2', '4'}, {'2', '5'}, {'2', '6'},
        {'2', '7'}, {'2', '8'}, {'2', '9'}, {'3', '0'}, {'3', '1'}, {'3', '2'}, {'3', '3'}, {'3', '4'}, {'3', '5'},
        {'3', '6'}, {'3', '7'}, {'3', '8'}, {'3', '9'}, {'4', '0'}, {'4', '1'}, {'4', '2'}, {'4', '3'}, {'4', '4'},
        {'4', '5'}, {'4', '6'}, {'4', '7'}, {'4', '8'}, {'4', '9'}, {'5', '0'}, {'5', '1'}, {'5', '2'}, {'5', '3'},
        {'5', '4'}, {'5', '5'}, {'5', '6'}, {'5', '7'}, {'5', '8'}, {'5', '9'}, {'6', '0'}, {'6', '1'}, {'6', '2'},
        {'6', '3'}, {'6', '4'}, {'6', '5'}, {'6', '6'}, {'6', '7'}, {'6', '8'}, {'6', '9'}, {'7', '0'}, {'7', '1'},
        {'7', '2'}, {'7', '3'}, {'7', '4'}, {'7', '5'}, {'7', '6'}, {'7', '7'}, {'7', '8'}, {'7', '9'}, {'8', '0'},
        {'8', '1'}, {'8', '2'}, {'8', '3'}, {'8', '4'}, {'8', '5'}, {'8', '6'}, {'8', '7'}, {'8', '8'}, {'8', '9'},
        {'9', '0'}, {'9', '1'}, {'9', '2'}, {'9', '3'}, {'9', '4'}, {'9', '5'}, {'9', '6'}, {'9', '7'}, {'9', '8'},
        {'9', '9'}};
    assert(n < 100);
    return digs[n];
}

template<typename StrTy, typename Func, typename... Args>
void adjust_numeric(StrTy& s, Func fn, unsigned len, const unsigned n_prefix, const fmt_opts& fmt, Args&&... args) {
    unsigned left = fmt.width - len, right = left;
    int fill = fmt.fill;
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        switch (fmt.flags & fmt_flags::kAdjustField) {
            case fmt_flags::kLeft: left = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            case fmt_flags::kRight:
            default: right = 0; break;
        }
    } else {
        fill = '0';
    }
    s.append(left, fill);
    fn(s, len, std::forward<Args>(args)...);
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        s.append(right, fmt.fill);
    } else if (n_prefix != 0) {
        using CharT = typename StrTy::value_type;
        CharT *p = &s.back() - fmt.width, *p_end = p + n_prefix, *p_src = &s.back() - len;
        do {
            *++p = *++p_src;
            *p_src = '0';
        } while (p != p_end);
    }
}

template<typename CharT>
struct grouping_t {
    typename std::numpunct<CharT>::char_type thousands_sep;
    std::string grouping;
};

inline unsigned calc_len_with_grouping(unsigned desired_len, const std::string& grouping) {
    unsigned length = desired_len;
    unsigned grp = 1;
    for (char ch : grouping) {
        grp = ch > 0 ? ch : 1;
        if (desired_len <= grp) { return length; }
        desired_len -= grp, ++length;
    }
    return length + ((desired_len - 1) / grp);
}

template<typename CharT, typename Ty, typename PrintFn>
struct print_functor {
    Ty val;
    char sign;
    PrintFn print_fn;
    template<typename... Args>
    SCVT_FORCE_INLINE void operator()(basic_membuffer<CharT>& s, unsigned len, Args&&... args) {
        if (s.avail() >= len || s.try_grow(len)) {
            print_fn(s.curr() + len, val, std::forward<Args>(args)...);
            if (sign) { *s.curr() = sign; }
            s.advance(len);
        } else if (s.avail()) {
            CharT buf[256];
            print_fn(buf + len, val, std::forward<Args>(args)...);
            if (sign) { buf[0] = sign; }
            s.append_by_chunks(buf, buf + len);
        }
    }
};

template<typename CharT, typename Ty, typename PrintFn>
print_functor<CharT, Ty, PrintFn> make_print_functor(Ty val, char sign, PrintFn print_fn) {
    return print_functor<CharT, Ty, PrintFn>{val, sign, print_fn};
}

// ---- binary

template<typename CharT, typename Ty>
void fmt_gen_bin(CharT* p, Ty val, const fmt_opts& fmt) NOEXCEPT {
    do {
        *--p = '0' + static_cast<unsigned>(val & 1);
        val >>= 1;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = !!(fmt.flags & fmt_flags::kUpperCase) ? 'B' : 'b';
        *--p = '0';
    }
}

template<typename CharT, typename Ty>
void fmt_gen_bin_with_grouping(CharT* p, Ty val, const fmt_opts& fmt, const grouping_t<CharT>& grouping) NOEXCEPT {
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
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = !!(fmt.flags & fmt_flags::kUpperCase) ? 'B' : 'b';
        *--p = '0';
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_bin(basic_membuffer<CharT>& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = (sign ? 1 : 0) + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0);
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const unsigned len = n_prefix + calc_len_with_grouping(1 + ulog2(val), grouping.grouping);
            auto fn = make_print_functor<CharT>(val, sign, fmt_gen_bin_with_grouping<CharT, Ty>);
            return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt, grouping) :
                                     fn(s, len, fmt, grouping);
        }
    }
    const unsigned len = 1 + n_prefix + ulog2(val);
    auto fn = make_print_functor<CharT>(val, sign, fmt_gen_bin<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt) : fn(s, len, fmt);
}

// ---- octal

template<typename CharT, typename Ty>
void fmt_gen_oct(CharT* p, Ty val, const fmt_opts& fmt) NOEXCEPT {
    do {
        *--p = '0' + static_cast<unsigned>(val & 7);
        val >>= 3;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) { *--p = '0'; }
}

template<typename CharT, typename Ty>
void fmt_gen_oct_with_grouping(CharT* p, Ty val, const fmt_opts& fmt, const grouping_t<CharT>& grouping) NOEXCEPT {
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
    if (!!(fmt.flags & fmt_flags::kAlternate)) { *--p = '0'; }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_oct(basic_membuffer<CharT>& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = sign ? 1 : 0;
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const unsigned len = (!!(fmt.flags & fmt_flags::kAlternate) ? 1 : 0) + n_prefix +
                                 calc_len_with_grouping(1 + ulog2(val) / 3, grouping.grouping);
            auto fn = make_print_functor<CharT>(val, sign, fmt_gen_oct_with_grouping<CharT, Ty>);
            return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt, grouping) :
                                     fn(s, len, fmt, grouping);
        }
    }
    const unsigned len = (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 1) + n_prefix + ulog2(val) / 3;
    auto fn = make_print_functor<CharT>(val, sign, fmt_gen_oct<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt) : fn(s, len, fmt);
}

// ---- hexadecimal

template<typename CharT, typename Ty>
void fmt_gen_hex(CharT* p, Ty val, const fmt_opts& fmt) NOEXCEPT {
    const bool upper_case = !!(fmt.flags & fmt_flags::kUpperCase);
    const char* digs = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        *--p = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = upper_case ? 'X' : 'x';
        *--p = '0';
    }
}

template<typename CharT, typename Ty>
void fmt_gen_hex_with_grouping(CharT* p, Ty val, const fmt_opts& fmt, const grouping_t<CharT>& grouping) NOEXCEPT {
    const bool upper_case = !!(fmt.flags & fmt_flags::kUpperCase);
    const char* digs = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
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
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = upper_case ? 'X' : 'x';
        *--p = '0';
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_hex(basic_membuffer<CharT>& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = (sign ? 1 : 0) + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0);
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const unsigned len = n_prefix + calc_len_with_grouping(1 + (ulog2(val) >> 2), grouping.grouping);
            auto fn = make_print_functor<CharT>(val, sign, fmt_gen_hex_with_grouping<CharT, Ty>);
            return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt, grouping) :
                                     fn(s, len, fmt, grouping);
        }
    }
    const unsigned len = 1 + n_prefix + (ulog2(val) >> 2);
    auto fn = make_print_functor<CharT>(val, sign, fmt_gen_hex<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, fmt) : fn(s, len, fmt);
}

// ---- decimal

template<typename Ty>
SCVT_FORCE_INLINE unsigned fmt_dec_unsigned_len(Ty val) NOEXCEPT {
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
SCVT_FORCE_INLINE CharT* gen_digits(CharT* p, Ty v) NOEXCEPT {
    while (v >= 100u) { copy2(p -= 2, get_digits(static_cast<unsigned>(divmod<100u>(v)))); }
    if (v >= 10u) {
        copy2(p -= 2, get_digits(static_cast<unsigned>(v)));
        return p;
    }
    *--p = '0' + static_cast<unsigned>(v);
    return p;
}

template<typename CharT, typename Ty>
SCVT_FORCE_INLINE Ty gen_digits_n(CharT* p, Ty v, unsigned n) NOEXCEPT {
    CharT* p0 = p - (n & ~1);
    while (p != p0) { copy2(p -= 2, get_digits(static_cast<unsigned>(divmod<100u>(v)))); }
    if (!(n & 1)) { return v; }
    *--p = '0' + static_cast<unsigned>(divmod<10u>(v));
    return v;
}

template<typename CharT, typename Ty>
void fmt_gen_dec_with_grouping(CharT* p, Ty val, const grouping_t<CharT>& grouping) NOEXCEPT {
    auto grp_it = grouping.grouping.begin();
    int cnt = *grp_it;
    *--p = '0' + static_cast<unsigned>(divmod<10u>(val));
    while (val) {
        if (--cnt <= 0) {
            *--p = grouping.thousands_sep, cnt = std::next(grp_it) != grouping.grouping.end() ? *++grp_it : *grp_it;
        }
        *--p = '0' + static_cast<unsigned>(divmod<10u>(val));
    }
}

template<typename CharT, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
void fmt_dec(basic_membuffer<CharT>& s, Ty val, const bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = sign ? 1 : 0;
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        if (!grouping.grouping.empty()) {
            const unsigned len = n_prefix + calc_len_with_grouping(fmt_dec_unsigned_len(val), grouping.grouping);
            auto fn = make_print_functor<CharT>(val, sign, fmt_gen_dec_with_grouping<CharT, Ty>);
            return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, grouping) : fn(s, len, grouping);
        }
    }
    const unsigned len = n_prefix + fmt_dec_unsigned_len(val);
    auto fn = make_print_functor<CharT>(val, sign, gen_digits<CharT, Ty>);
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt) : fn(s, len);
}

// ---- integral

template<typename CharT, typename Ty>
void fmt_integral_common(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt) {
    using UTy = typename std::make_unsigned<Ty>::type;
    const bool is_signed = std::is_signed<Ty>::value;
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kDec: fmt_dec(s, static_cast<UTy>(val), is_signed, fmt); break;
        case fmt_flags::kBin: fmt_bin(s, static_cast<UTy>(val), is_signed, fmt); break;
        case fmt_flags::kOct: fmt_oct(s, static_cast<UTy>(val), is_signed, fmt); break;
        case fmt_flags::kHex: fmt_hex(s, static_cast<UTy>(val), is_signed, fmt); break;
        default: UNREACHABLE_CODE;
    }
}

// ---- char

template<typename CharT, typename Ty>
void fmt_char(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt) {
    const auto fn = [val](basic_membuffer<CharT>& s) { s.push_back(val); };
    return fmt.width > 1 ? append_adjusted(s, fn, 1, fmt) : fn(s);
}

// ---- float

SCVT_CONSTEXPR_DATA int kMaxDoubleDigits = 767;
SCVT_CONSTEXPR_DATA int kDigsPer64 = 18;  // size of 64-bit digit pack

class UXS_EXPORT fp_hex_fmt_t {
 public:
    fp_hex_fmt_t(const fp_m64_t& fp2, const fmt_opts& fmt, const unsigned bpm, const int exp_bias) NOEXCEPT;

    unsigned get_len() const NOEXCEPT {
        return 3 + (prec_ > 0 || alternate_ ? prec_ + 1 : 0) + fmt_dec_unsigned_len<uint32_t>(std::abs(exp_));
    }

    template<typename CharT>
    void generate(CharT* p, const bool upper_case, const CharT dec_point) const NOEXCEPT;

 private:
    uint64_t significand_;
    int exp_;
    int prec_;
    int n_zeroes_;
    bool alternate_;
};

template<typename CharT>
void fp_hex_fmt_t::generate(CharT* p, const bool upper_case, const CharT dec_point) const NOEXCEPT {
    const char* digs = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
    // generate exponent
    int exp2 = exp_;
    char exp_sign = '+';
    if (exp2 < 0) { exp_sign = '-', exp2 = -exp2; }
    p = gen_digits(p, static_cast<unsigned>(exp2));
    *--p = exp_sign;
    *--p = upper_case ? 'P' : 'p';
    uint64_t m = significand_;
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
    if (alternate_) {
        *--p = upper_case ? 'X' : 'x';
        *--p = '0';
    }
}

class UXS_EXPORT fp_dec_fmt_t {
 public:
    fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, const int exp_bias) NOEXCEPT;

    unsigned get_len() const NOEXCEPT {
        return (fixed_ ? 1 + std::max(exp_, 0) : (exp_ <= -100 || exp_ >= 100 ? 6 : 5)) +
               (prec_ > 0 || alternate_ ? prec_ + 1 : 0);
    }

    unsigned get_len_with_grouing(const std::string& grouping) const NOEXCEPT {
        return (fixed_ ? calc_len_with_grouping(1 + std::max(exp_, 0), grouping) :
                         (exp_ <= -100 || exp_ >= 100 ? 6 : 5)) +
               (prec_ > 0 || alternate_ ? prec_ + 1 : 0);
    }

    template<typename CharT>
    void generate(CharT* p, const bool upper_case, const CharT dec_point,
                  const grouping_t<CharT>* grouping) const NOEXCEPT {
        if (!fixed_) {
            generate_scientific(p, upper_case, dec_point);
        } else {
            generate_fixed<CharT>(p, dec_point, grouping);
        }
    }

    template<typename CharT>
    void generate_scientific(CharT* p, bool upper_case, const CharT dec_point) const NOEXCEPT;

    template<typename CharT>
    void generate_fixed(CharT* p, const CharT dec_point, const grouping_t<CharT>* grouping) const NOEXCEPT;

 private:
    uint64_t significand_;
    int exp_;
    int prec_;
    int n_zeroes_;
    bool fixed_;
    bool alternate_;
    char digs_buf_[kMaxDoubleDigits + kDigsPer64 - 1];

    void format_short_decimal(const fp_m64_t& fp2, int n_digs, const fmt_flags fp_fmt) NOEXCEPT;
    void format_short_decimal_slow(const fp_m64_t& fp2, int n_digs, const fmt_flags fp_fmt) NOEXCEPT;
    void format_long_decimal(const fp_m64_t& fp2, int n_digs, const fmt_flags fp_fmt) NOEXCEPT;
};

template<typename CharT>
void fp_dec_fmt_t::generate_scientific(CharT* p, const bool upper_case, const CharT dec_point) const NOEXCEPT {
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
    *--p = upper_case ? 'E' : 'e';

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
void fp_dec_fmt_t::generate_fixed(CharT* p, const CharT dec_point, const grouping_t<CharT>* grouping) const NOEXCEPT {
    uint64_t m = significand_;
    int k = 1 + exp_, n_zeroes = n_zeroes_;
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
        *--p = '0' + static_cast<unsigned>(divmod<10u>(m));
        while (m) {
            if (--cnt <= 0) {
                *--p = grouping->thousands_sep;
                cnt = std::next(grp_it) != grouping->grouping.end() ? *++grp_it : *grp_it;
            }
            *--p = '0' + static_cast<unsigned>(divmod<10u>(m));
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
    const FpFmtTy& fp;
    char sign;
    bool upper_case;
    template<typename... Args>
    SCVT_FORCE_INLINE void operator()(basic_membuffer<CharT>& s, unsigned len, Args&&... args) {
        if (s.avail() >= len || s.try_grow(len)) {
            fp.template generate<CharT>(s.curr() + len, upper_case, std::forward<Args>(args)...);
            if (sign) { *s.curr() = sign; }
            s.advance(len);
        } else if (s.avail()) {
            inline_basic_dynbuffer<CharT> buf;
            if (buf.avail() < len) { buf.try_grow(len); }
            fp.template generate<CharT>(buf.data() + len, upper_case, std::forward<Args>(args)...);
            if (sign) { *buf.data() = sign; }
            s.append_by_chunks(buf.data(), buf.data() + len);
        }
    }
};

template<typename CharT>
void fmt_float_common(basic_membuffer<CharT>& s, uint64_t u64, const fmt_opts& fmt, const unsigned bpm,
                      const int exp_max) {
    char sign = '\0';
    if (u64 & (static_cast<uint64_t>(1 + exp_max) << bpm)) {
        sign = '-';  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    // Binary exponent and mantissa
    const bool upper_case = !!(fmt.flags & fmt_flags::kUpperCase);
    const fp_m64_t fp2{u64 & ((1ull << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};
    if (fp2.exp == exp_max) {
        // Print infinity or NaN
        const auto sval = fp2.m == 0 ? default_numpunct<CharT>().infname(upper_case) :
                                       default_numpunct<CharT>().nanname(upper_case);
        const unsigned len = (sign ? 1 : 0) + static_cast<unsigned>(sval.size());
        const auto fn = [&sval, sign](basic_membuffer<CharT>& s) {
            if (sign) { s.push_back(sign); }
            s.append(sval.begin(), sval.end());
        };
        return fmt.width > len ? append_adjusted(s, fn, len, fmt) : fn(s);
    }

    if ((fmt.flags & fmt_flags::kBaseField) == fmt_flags::kHex) {
        // Print hexadecimal representation
        fp_hex_fmt_t fp(fp2, fmt, bpm, exp_max >> 1);
        const CharT dec_point = fmt.loc ? std::use_facet<std::numpunct<CharT>>(*fmt.loc).decimal_point() :
                                          default_numpunct<CharT>().decimal_point();
        const unsigned n_prefix = (sign ? 1 : 0) + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0);
        const unsigned len = n_prefix + fp.get_len();
        print_float_functor<CharT, fp_hex_fmt_t> fn{fp, sign, upper_case};
        return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, dec_point) : fn(s, len, dec_point);
    }

    // Print decimal representation
    fp_dec_fmt_t fp(fp2, fmt, bpm, exp_max >> 1);
    CharT dec_point = default_numpunct<CharT>().decimal_point();
    const unsigned n_prefix = sign ? 1 : 0;
    print_float_functor<CharT, fp_dec_fmt_t> fn{fp, sign, upper_case};
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        grouping_t<CharT> grouping{numpunct.thousands_sep(), numpunct.grouping()};
        dec_point = numpunct.decimal_point();
        if (!grouping.grouping.empty()) {
            const unsigned len = n_prefix + fp.get_len_with_grouing(grouping.grouping);
            return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, dec_point, &grouping) :
                                     fn(s, len, dec_point, &grouping);
        }
    }
    const unsigned len = n_prefix + fp.get_len();
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt, dec_point, nullptr) :
                             fn(s, len, dec_point, nullptr);
}

// ---- float

template<typename CharT, typename Ty>
void fmt_bool(basic_membuffer<CharT>& s, Ty val, const fmt_opts& fmt) {
    if (fmt.loc) {
        const auto& numpunct = std::use_facet<std::numpunct<CharT>>(*fmt.loc);
        const auto sval = val ? numpunct.truename() : numpunct.falsename();
        const auto fn = [&sval](basic_membuffer<CharT>& s) { s.append(sval.begin(), sval.end()); };
        return fmt.width > sval.size() ? append_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) : fn(s);
    }
    const bool upper_case = !!(fmt.flags & fmt_flags::kUpperCase);
    const auto sval = val ? default_numpunct<CharT>().truename(upper_case) :
                            default_numpunct<CharT>().falsename(upper_case);
    const auto fn = [&sval](basic_membuffer<CharT>& s) { s.append(sval.begin(), sval.end()); };
    return fmt.width > sval.size() ? append_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) : fn(s);
}

}  // namespace scvt

}  // namespace uxs
