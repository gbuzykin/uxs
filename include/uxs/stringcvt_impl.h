#pragma once

#include "uxs/stringcvt.h"

#include <array>

#if defined(_MSC_VER) && defined(_M_X64)
#    include <intrin.h>
#elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#endif

#ifdef __has_builtin
#    define SCVT_HAS_BUILTIN(x) __has_builtin(x)
#else
#    define SCVT_HAS_BUILTIN(x) 0
#endif

namespace uxs {
namespace scvt {

template<typename Ty>
struct fp_traits;

template<>
struct fp_traits<double> {
    static_assert(sizeof(double) == sizeof(uint64_t), "type size mismatch");
    enum : unsigned { kTotalBits = 64, kBitsPerMantissa = 52 };
    enum : uint64_t { kMantissaMask = (1ull << kBitsPerMantissa) - 1 };
    enum : int { kExpMax = (1 << (kTotalBits - kBitsPerMantissa - 1)) - 1 };
    static uint64_t to_u64(const double& f) { return *reinterpret_cast<const uint64_t*>(&f); }
    static double from_u64(const uint64_t& u64) { return *reinterpret_cast<const double*>(&u64); }
};

template<>
struct fp_traits<float> {
    static_assert(sizeof(float) == sizeof(uint32_t), "type size mismatch");
    enum : unsigned { kTotalBits = 32, kBitsPerMantissa = 23 };
    enum : uint64_t { kMantissaMask = (1ull << kBitsPerMantissa) - 1 };
    enum : int { kExpMax = (1 << (kTotalBits - kBitsPerMantissa - 1)) - 1 };
    static uint64_t to_u64(const float& f) { return *reinterpret_cast<const uint32_t*>(&f); }
    static float from_u64(const uint64_t& u64) { return *reinterpret_cast<const float*>(&u64); }
};

struct fp_m64_t {
    uint64_t m;
    int exp;
};

inline uint64_t lo32(uint64_t x) { return x & 0xffffffff; }
inline uint64_t hi32(uint64_t x) { return x >> 32; }

extern const unsigned g_exp2_digs[];
extern const uint64_t g_ten_pows[];
extern const char g_digits[][2];

#if defined(_MSC_VER) && defined(_M_X64)
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
#elif defined(__GNUC__) && defined(__x86_64__)
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
extern const ulog2_table_t g_ulog2_tbl;
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

// --------------------------

template<typename Ty>
struct type_substitution {
    using type = Ty;
};
template<>
struct type_substitution<int8_t> {
    using type = int32_t;
};
template<>
struct type_substitution<int16_t> {
    using type = int32_t;
};
template<>
struct type_substitution<uint8_t> {
    using type = uint32_t;
};
template<>
struct type_substitution<uint16_t> {
    using type = uint32_t;
};

// ---- from string to value

template<typename CharT>
const CharT* starts_with(const CharT* p, const CharT* end, const char* s, const size_t len) {
    if (static_cast<size_t>(end - p) < len) { return p; }
    for (const CharT* p1 = p; p1 < end; ++p1, ++s) {
        if (to_lower(*p1) != *s) { return p; }
    }
    return p + len;
}

template<typename CharT>
const CharT* skip_spaces(const CharT* p, const CharT* end) {
    while (p < end && is_space(*p)) { ++p; }
    return p;
}

template<typename Ty, typename CharT, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
Ty to_integer_limited(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) {
    bool neg = false;
    last = p;
    if (p == end) {
        return 0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }
    if (p == end || !is_digit(*p)) { return 0; }
    Ty val = static_cast<unsigned>(*p++ - '0');
    while (p < end && is_digit(*p)) {
        Ty val0 = val;
        val = 10u * val + static_cast<unsigned>(*p++ - '0');
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
Ty to_integer(const CharT* p, const CharT* end, const CharT*& last) {
    return to_integer_limited<typename type_substitution<typename std::make_unsigned<Ty>::type>::type>(
        p, end, last, std::numeric_limits<typename std::make_unsigned<Ty>::type>::max());
}

template<typename Ty, typename CharT>
Ty to_char(const CharT* p, const CharT* end, const CharT*& last) {
    last = p;
    if (p == end) { return '\0'; }
    ++last;
    return *p;
}

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, uint64_t& m, int& exp) {
    const uint64_t max_mantissa10 = 1000000000000000000ull;
    for (; p < end && is_digit(*p); ++p) {
        if (m < max_mantissa10) {  // decimal mantissa can hold up to 19 digits
            m = 10u * m + static_cast<int>(*p - '0');
        } else {
            ++exp;
        }
    }
    return p;
}

template<typename CharT>
const CharT* chars_to_fp10(const CharT* p, const CharT* end, fp_m64_t& fp10) {
    const char dot_symb = '.';
    if (p == end) { return p; }
    if (is_digit(*p)) {  // integral part
        fp10.m = static_cast<int>(*p++ - '0');
        p = accum_mantissa(p, end, fp10.m, fp10.exp);
        if (p < end && *p == dot_symb) { ++p; }  // skip decimal point
    } else if (*p == dot_symb && p + 1 < end && is_digit(*(p + 1))) {
        fp10.m = static_cast<int>(*(p + 1) - '0');  // tenth
        fp10.exp = -1, p += 2;
    } else {
        return p;
    }
    const CharT* p1 = accum_mantissa(p, end, fp10.m, fp10.exp);  // fractional part
    fp10.exp -= static_cast<unsigned>(p1 - p);
    if (p1 < end && (*p1 == 'e' || *p1 == 'E')) {  // optional exponent
        int exp_optional = to_integer<int>(p1 + 1, end, p);
        if (p > p1 + 1) { fp10.exp += exp_optional, p1 = p; }
    }
    return p1;
}

UXS_EXPORT uint64_t fp10_to_fp2(fp_m64_t fp10, const unsigned bpm, const int exp_max);

template<typename CharT>
uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, const unsigned bpm, const int exp_max) {
    uint64_t fp2 = 0;
    last = p;

    if (p == end) {
        return 0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, fp2 = static_cast<uint64_t>(1 + exp_max) << bpm;  // negative sign
    }

    fp_m64_t fp10{0, 0};
    const CharT* p1 = chars_to_fp10(p, end, fp10);
    if (p1 > p) {
        fp2 |= fp10_to_fp2(fp10, bpm, exp_max);
    } else if ((p1 = starts_with(p, end, "inf", 3)) > p) {  // infinity
        fp2 |= static_cast<uint64_t>(exp_max) << bpm;
    } else if ((p1 = starts_with(p, end, "nan", 3)) > p) {  // NaN
        fp2 |= (static_cast<uint64_t>(exp_max) << bpm) | ((1ull << bpm) - 1);
    } else {
        return 0;
    }

    last = p1;
    return fp2;
}

template<typename Ty, typename CharT>
Ty to_float(const CharT* p, const CharT* end, const CharT*& last) {
    return fp_traits<Ty>::from_u64(
        to_float_common(p, end, last, fp_traits<Ty>::kBitsPerMantissa, fp_traits<Ty>::kExpMax));
}

// ---- from value to string

template<typename StrTy, typename Func>
inline StrTy& fmt_adjusted(StrTy& s, Func fn, unsigned len, const fmt_state& fmt) {
    unsigned left = fmt.width - len, right = left;
    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: left = 0; break;
        case fmt_flags::kInternal: left >>= 1, right -= left; break;
        default: right = 0; break;
    }
    s.append(left, fmt.fill);
    return fn(s).append(right, fmt.fill);
}

// --------------------------

template<typename StrTy, typename Func>
inline StrTy& fmt_num_adjusted(StrTy& s, Func fn, unsigned len, unsigned n_prefix, const fmt_state& fmt) {
    unsigned left = fmt.width - len, right = left;
    int fill = fmt.fill;
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        switch (fmt.flags & fmt_flags::kAdjustField) {
            case fmt_flags::kLeft: left = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            default: right = 0; break;
        }
    } else {
        fill = '0';
    }
    s.append(left, fill);
    fn(s);
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        s.append(right, fmt.fill);
    } else if (n_prefix != 0) {
        *(&s.back() - fmt.width + n_prefix) = *(&s.back() - len + n_prefix);
        *(&s.back() - len + n_prefix) = '0';
    }
    return s;
}

// ---- binary

template<typename CharT, typename Ty>
CharT* fmt_bin(CharT* p, Ty val, unsigned len, const fmt_state& fmt) {
    CharT* last = (p += len);
    if (!!(fmt.flags & fmt_flags::kAlternate)) { *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'b' : 'B'; }
    do {
        *--p = '0' + static_cast<unsigned>(val & 1);
        val >>= 1;
    } while (val != 0);
    return last;
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_bin(basic_unlimbuf_appender<CharT>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_bin(s.curr(), val, len, fmt));
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_bin(basic_dynbuffer<CharT, Alloc>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_bin(s.reserve_at_curr(len), val, len, fmt));
}

template<typename StrTy, typename Ty>
StrTy& fmt_bin(StrTy& s, Ty val, unsigned len, const fmt_state& fmt) {
    typename StrTy::value_type buf[65];
    return s.append(buf, fmt_bin(buf, val, len, fmt));
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_bin(StrTy& s, Ty val, const fmt_state& fmt) {
    const unsigned len = (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 1) + ulog2(val);
    const auto fn = [val, len, &fmt](StrTy& s) -> StrTy& { return fmt_bin(s, val, len, fmt); };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, 0, fmt) : fn(s);
}

// ---- octal

template<typename CharT, typename Ty>
CharT* fmt_oct(CharT* p, Ty val, unsigned len, const fmt_state& fmt) {
    CharT* last = (p += len);
    do {
        *--p = '0' + static_cast<unsigned>(val & 7);
        val >>= 3;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) { *--p = '0'; }
    return last;
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_oct(basic_unlimbuf_appender<CharT>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_oct(s.curr(), val, len, fmt));
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_oct(basic_dynbuffer<CharT, Alloc>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_oct(s.reserve_at_curr(len), val, len, fmt));
}

template<typename StrTy, typename Ty>
StrTy& fmt_oct(StrTy& s, Ty val, unsigned len, const fmt_state& fmt) {
    typename StrTy::value_type buf[32];
    return s.append(buf, fmt_oct(buf, val, len, fmt));
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_oct(StrTy& s, Ty val, const fmt_state& fmt) {
    const unsigned len = (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 1) + ulog2(val) / 3;
    const auto fn = [val, len, &fmt](StrTy& s) -> StrTy& { return fmt_oct(s, val, len, fmt); };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, 0, fmt) : fn(s);
}

// ---- hexadecimal

template<typename CharT, typename Ty>
CharT* fmt_hex(CharT* p, Ty val, unsigned len, const fmt_state& fmt) {
    const char* digs = !!(fmt.flags & fmt_flags::kUpperCase) ? "0123456789ABCDEF" : "0123456789abcdef";
    CharT* last = (p += len);
    do {
        *--p = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        p -= 2, p[0] = '0', p[1] = !(fmt.flags & fmt_flags::kUpperCase) ? 'x' : 'X';
    }
    return last;
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_hex(basic_unlimbuf_appender<CharT>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_hex(s.curr(), val, len, fmt));
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_hex(basic_dynbuffer<CharT, Alloc>& s, Ty val, unsigned len, const fmt_state& fmt) {
    return s.setcurr(fmt_hex(s.reserve_at_curr(len), val, len, fmt));
}

template<typename StrTy, typename Ty>
StrTy& fmt_hex(StrTy& s, Ty val, unsigned len, const fmt_state& fmt) {
    typename StrTy::value_type buf[32];
    return s.append(buf, fmt_hex(buf, val, len, fmt));
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_hex(StrTy& s, Ty val, const fmt_state& fmt) {
    const unsigned len = (!!(fmt.flags & fmt_flags::kAlternate) ? 3 : 1) + (ulog2(val) >> 2);
    const auto fn = [val, len, &fmt](StrTy& s) -> StrTy& { return fmt_hex(s, val, len, fmt); };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, !!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0, fmt) : fn(s);
}

// ---- decimal unsigned

template<typename Ty>
unsigned fmt_dec_unsigned_len(Ty val) {
    const unsigned pow = g_exp2_digs[ulog2(val)];
    return val >= g_ten_pows[pow] ? pow + 1 : pow;
}

template<typename CharT, typename Ty>
CharT* gen_digits(CharT* p, Ty v) {
    while (v >= 100u) {
        const Ty t = v / 100u;
        std::copy_n(g_digits[static_cast<unsigned>(v - 100u * t)], 2, p -= 2);
        v = t;
    }
    if (v >= 10u) {
        std::copy_n(g_digits[v], 2, p -= 2);
        return p;
    }
    *--p = '0' + static_cast<unsigned>(v);
    return p;
}

template<typename CharT, typename Ty>
Ty gen_digits_n(CharT* p, Ty v, unsigned n) {
    CharT* p0 = p - (n & ~1);
    while (p != p0) {
        const Ty t = v / 100u;
        std::copy_n(g_digits[static_cast<unsigned>(v - 100u * t)], 2, p -= 2);
        v = t;
    }
    if (!(n & 1)) { return v; }
    const Ty t = v / 10u;
    *--p = '0' + static_cast<unsigned>(v - 10u * t);
    return t;
}

template<typename CharT, typename Ty>
CharT* fmt_dec_unsigned(CharT* p, Ty val, unsigned len) {
    gen_digits(p += len, val);
    return p;
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_dec_unsigned(basic_unlimbuf_appender<CharT>& s, Ty val, unsigned len) {
    return s.setcurr(fmt_dec_unsigned(s.curr(), val, len));
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_dec_unsigned(basic_dynbuffer<CharT, Alloc>& s, Ty val, unsigned len) {
    return s.setcurr(fmt_dec_unsigned(s.reserve_at_curr(len), val, len));
}

template<typename StrTy, typename Ty>
StrTy& fmt_dec_unsigned(StrTy& s, Ty val, unsigned len) {
    typename StrTy::value_type buf[32];
    return s.append(buf, fmt_dec_unsigned(buf, val, len));
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec_unsigned(StrTy& s, Ty val, const fmt_state& fmt) {
    const unsigned len = fmt_dec_unsigned_len(val);
    const auto fn = [val, len](StrTy& s) -> StrTy& { return fmt_dec_unsigned(s, val, len); };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, 0, fmt) : fn(s);
}

// ---- decimal signed

template<typename CharT, typename Ty>
CharT* fmt_dec_signed(CharT* p, Ty val, char sign, unsigned len) {
    *p = sign;
    gen_digits(p += len, val);
    return p;
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_dec_signed(basic_unlimbuf_appender<CharT>& s, Ty val, char sign, unsigned len) {
    return s.setcurr(fmt_dec_signed(s.curr(), val, sign, len));
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_dec_signed(basic_dynbuffer<CharT, Alloc>& s, Ty val, char sign, unsigned len) {
    return s.setcurr(fmt_dec_signed(s.reserve_at_curr(len), val, sign, len));
}

template<typename StrTy, typename Ty>
StrTy& fmt_dec_signed(StrTy& s, Ty val, char sign, unsigned len) {
    typename StrTy::value_type buf[32];
    return s.append(buf, fmt_dec_signed(buf, val, sign, len));
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec_signed(StrTy& s, Ty val, const fmt_state& fmt) {
    char sign = '\0';
    if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
        sign = '-', val = ~val + 1;  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    const unsigned len = (sign != '\0' ? 1 : 0) + fmt_dec_unsigned_len(val);
    const auto fn = [val, len, sign](StrTy& s) -> StrTy& { return fmt_dec_signed(s, val, sign, len); };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, sign != '\0' ? 1 : 0, fmt) : fn(s);
}

// --------------------------

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_unsigned(StrTy& s, Ty val, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kDec: return fmt_dec_unsigned(s, val, fmt);
        case fmt_flags::kBin: return fmt_bin(s, val, fmt);
        case fmt_flags::kOct: return fmt_oct(s, val, fmt);
        case fmt_flags::kHex: return fmt_hex(s, val, fmt);
        default: UNREACHABLE_CODE;
    }
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_signed(StrTy& s, Ty val, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kDec: return fmt_dec_signed(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        case fmt_flags::kBin: return fmt_bin(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        case fmt_flags::kOct: return fmt_oct(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        case fmt_flags::kHex: return fmt_hex(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        default: UNREACHABLE_CODE;
    }
}

// ---- char

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_same<Ty, typename StrTy::value_type>::value>>
StrTy& fmt_char(StrTy& s, Ty val, const fmt_state& fmt) {
    const auto fn = [val](StrTy& s) -> StrTy& {
        s.push_back(val);
        return s;
    };
    return fmt.width > 1 ? fmt_adjusted(s, fn, 1, fmt) : fn(s);
}

// ---- float

struct fp10_t {
    uint64_t significand;
    const char* digs;
    int exp;
    int n_zeroes;
};

inline unsigned fmt_float_len(int exp, char sign, bool is_fixed, bool alternate, int prec) {
    return (sign != '\0' ? 2 : 1) + (is_fixed ? std::max(exp, 0) : (exp <= -100 || exp >= 100 ? 5 : 4)) +
           (prec > 0 || alternate ? prec + 1 : 0);
}

template<typename CharT>
CharT* fmt_gen_float(CharT* p, const fp10_t& fp10, char sign, bool is_fixed, fmt_flags flags, int prec) {
    const char dot_symb = '.';
    const char e_symb = !(flags & fmt_flags::kUpperCase) ? 'e' : 'E';

    if (sign != '\0') { *p++ = sign; }  // generate sign

    if (is_fixed) {     // generate fixed representation
        CharT* p0 = p;  // where to generate integral part
        uint64_t m = fp10.significand;
        int k = 1 + fp10.exp, n_zeroes = fp10.n_zeroes;
        if (prec > 0) {   // has fractional part
            if (k > 0) {  // fixed form [1-9]+.[0-9]+
                p += k + prec + 1;
                if (!fp10.digs) {  // generate fractional part from significand
                    m = gen_digits_n(p, m, prec);
                } else {  // generate from chars
                    if (n_zeroes < prec) {
                        CharT* t = std::copy_n(fp10.digs + k, prec - n_zeroes, p - prec);
                        std::fill_n(t, n_zeroes, '0');
                    } else {  // all zeroes
                        std::fill_n(p - prec, prec, '0');
                    }
                    n_zeroes -= prec;
                }
            } else {                             // fixed form 0.0*[1-9][0-9]*
                p = std::fill_n(p, 2 - k, '0');  // one `0` will be replaced with decimal point later
                // append trailing fractional part later as is was integral part
                p0 = p, k += prec;
                p += k;
            }
            *(p - prec - 1) = dot_symb;  // put decimal point into reserved cell
        } else {
            p += k;  // `k` is always > 0 here
            if (!!(flags & fmt_flags::kAlternate)) { *p++ = dot_symb; }
        }
        if (!fp10.digs) {  // generate integral part from significand
            gen_digits(p0 + k, m);
        } else if (n_zeroes > 0) {  // generate from chars
            CharT* t = std::copy_n(fp10.digs, k - n_zeroes, p0);
            std::fill_n(t, n_zeroes, '0');
        } else {
            std::copy_n(fp10.digs, k, p0);
        }
        return p;
    }

    // generate scientific representation
    if (prec > 0) {        // has fractional part
        if (!fp10.digs) {  // generate from significand
            gen_digits(p + prec + 2, fp10.significand);
        } else {  // generate from chars
            CharT* t = std::copy_n(fp10.digs, prec + 1 - fp10.n_zeroes, p + 1);
            std::fill_n(t, fp10.n_zeroes, '0');
        }
        // insert decimal point
        p[0] = p[1], p[1] = dot_symb;
        p += prec + 2;
    } else {  // only one digit
        *p++ = '0' + static_cast<unsigned>(fp10.significand);
        if (!!(flags & fmt_flags::kAlternate)) { *p++ = dot_symb; }
    }

    // generate exponent
    int exp10 = fp10.exp;
    *p++ = e_symb;
    if (exp10 < 0) {
        *p++ = '-', exp10 = -exp10;
    } else {
        *p++ = '+';
    }
    if (exp10 >= 100) {
        const int t = (656 * exp10) >> 16;
        *p++ = '0' + t, p = std::copy_n(g_digits[exp10 - 100 * t], 2, p);
    } else {
        p = std::copy_n(g_digits[exp10], 2, p);
    }
    return p;
}

template<typename CharT>
basic_unlimbuf_appender<CharT>& fmt_gen_float(basic_unlimbuf_appender<CharT>& s, const fp10_t& fp10, char sign,
                                              bool is_fixed, unsigned len, fmt_flags flags, int prec) {
    return s.setcurr(fmt_gen_float(s.curr(), fp10, sign, is_fixed, flags, prec));
}

template<typename CharT, typename Alloc>
basic_dynbuffer<CharT, Alloc>& fmt_gen_float(basic_dynbuffer<CharT, Alloc>& s, const fp10_t& fp10, char sign,
                                             bool is_fixed, unsigned len, fmt_flags flags, int prec) {
    return s.setcurr(fmt_gen_float(s.reserve_at_curr(len), fp10, sign, is_fixed, flags, prec));
}

template<typename StrTy>
StrTy& fmt_gen_float(StrTy& s, const fp10_t& fp10, char sign, bool is_fixed, unsigned len, fmt_flags flags, int prec) {
    const char dot_symb = '.';
    const char e_symb = !(flags & fmt_flags::kUpperCase) ? 'e' : 'E';

    std::array<char, 32> buf;

    // generate chars from significand or use already generated
    const char* first = fp10.digs ? fp10.digs : gen_digits(buf.data() + buf.size(), fp10.significand);

    if (sign != '\0') { s.push_back(sign); }  // append sign

    if (is_fixed) {  // append fixed representation
        int k = 1 + fp10.exp, n_digs = k + prec, n_zeroes = fp10.n_zeroes;
        if (prec > 0) {                 // has fractional part
            if (k > 0) {                // fixed form [1-9]+.[0-9]+
                if (n_zeroes < prec) {  // decimal point is between two digits in digit buffer
                    s.append(first, first + k);
                    // future appending will duplicate the digit before decimal point
                    first += k - 1, n_digs -= k - 1;
                } else {  // decimal point is after all digits in digit buffer
                    // append one more zero character; it will be replaced with decimal point later
                    ++n_zeroes, ++n_digs;
                }
            } else {                   // fixed form 0.0*[1-9][0-9]*
                s.append(2 - k, '0');  // one `0` will be replaced with decimal point later
            }
        }
        // append fully integral or fractional part
        s.append(first, first + n_digs - n_zeroes).append(n_zeroes, '0');
        if (prec > 0) {
            *(&s.back() - prec) = dot_symb;  // put decimal point into reserved cell
        } else if (!!(flags & fmt_flags::kAlternate)) {
            s.push_back(dot_symb);
        }
        return s;
    }

    // append scientific representation
    if (prec > 0) {  // has fractional part
        s.push_back(first[0]);
        s.append(first, first + prec - fp10.n_zeroes + 1).append(fp10.n_zeroes, '0');
        *(&s.back() - prec) = dot_symb;
    } else {  // only one digit
        s.push_back('0' + static_cast<unsigned>(fp10.significand));
        if (!!(flags & fmt_flags::kAlternate)) { s.push_back(dot_symb); }
    }

    // append exponent
    int exp10 = fp10.exp;
    char* p = buf.data();
    *p++ = e_symb;
    if (exp10 < 0) {
        *p++ = '-', exp10 = -exp10;
    } else {
        *p++ = '+';
    }
    if (exp10 >= 100) {
        const int t = (656 * exp10) >> 16;
        *p++ = '0' + t, p = std::copy_n(g_digits[exp10 - 100 * t], 2, p);
    } else {
        p = std::copy_n(g_digits[exp10], 2, p);
    }
    return s.append(buf.data(), p);
}

UXS_EXPORT fp10_t fp2_to_fp10(dynbuffer& digs, fp_m64_t fp2, fmt_flags& fp_fmt, int& prec, bool alternate, unsigned bpm,
                              const int exp_bias);

template<typename StrTy>
StrTy& fmt_float_common(StrTy& s, uint64_t u64, const fmt_state& fmt, const unsigned bpm, const int exp_max) {
    char sign = '\0';
    if (u64 & (static_cast<uint64_t>(1 + exp_max) << bpm)) {
        sign = '-';  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    // Binary exponent and mantissa
    const fp_m64_t fp2{u64 & ((1ull << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};

    if (fp2.exp == exp_max) {
        const char* p0 = fp2.m == 0 ? (!(fmt.flags & fmt_flags::kUpperCase) ? "inf" : "INF") :
                                      (!(fmt.flags & fmt_flags::kUpperCase) ? "nan" : "NAN");
        const unsigned len = sign != '\0' ? 4 : 3;
        const auto fn = [p0, sign](StrTy& s) -> StrTy& {
            if (sign != '\0') { s.push_back(sign); }
            return s.append(p0, p0 + 3);
        };
        return fmt.width > len ? fmt_adjusted(s, fn, len, fmt) : fn(s);
    }

    inline_dynbuffer digs;
    int prec = fmt.prec;
    fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    const bool alternate = !!(fmt.flags & fmt_flags::kAlternate);
    const fp10_t fp10 = fp2_to_fp10(digs, fp2, fp_fmt, prec, alternate, bpm, exp_max >> 1);
    const bool is_fixed = fp_fmt == fmt_flags::kFixed;
    const unsigned len = fmt_float_len(fp10.exp, sign, is_fixed, alternate, prec);
    const auto fn = [&fp10, sign, is_fixed, len, &fmt, prec](StrTy& s) -> StrTy& {
        return fmt_gen_float(s, fp10, sign, is_fixed, len, fmt.flags, prec);
    };
    return fmt.width > len ? fmt_num_adjusted(s, fn, len, sign != '\0' ? 1 : 0, fmt) : fn(s);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_floating_point<Ty>::value>>
StrTy& fmt_float(StrTy& s, Ty val, const fmt_state& fmt) {
    return fmt_float_common(s, fp_traits<Ty>::to_u64(val), fmt, fp_traits<Ty>::kBitsPerMantissa, fp_traits<Ty>::kExpMax);
}

}  // namespace scvt

#define UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(ty, from_string_func, to_string_func) \
    template<typename CharT> \
    size_t string_converter<ty>::from_string(std::basic_string_view<CharT> s, ty& val) { \
        const CharT* last = s.data() + s.size(); \
        const CharT* first = scvt::skip_spaces(s.data(), last); \
        auto t = scvt::from_string_func<ty>(first, last, last); \
        if (last == first) { return 0; } \
        val = t; \
        return static_cast<size_t>(last - s.data()); \
    } \
    template<typename StrTy> \
    StrTy& string_converter<ty>::to_string(StrTy& s, ty val, const fmt_state& fmt) { \
        return scvt::to_string_func<StrTy, scvt::type_substitution<ty>::type>(s, val, fmt); \
    }
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int8_t, to_integer, fmt_signed)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int16_t, to_integer, fmt_signed)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int32_t, to_integer, fmt_signed)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int64_t, to_integer, fmt_signed)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint8_t, to_integer, fmt_unsigned)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint16_t, to_integer, fmt_unsigned)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint32_t, to_integer, fmt_unsigned)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint64_t, to_integer, fmt_unsigned)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(char, to_char, fmt_char)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(wchar_t, to_char, fmt_char)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(float, to_float, fmt_float)
UXS_SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(double, to_float, fmt_float)
#undef SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER

template<typename CharT>
size_t string_converter<bool>::from_string(std::basic_string_view<CharT> s, bool& val) {
    const CharT* last = s.data() + s.size();
    const CharT* p = scvt::skip_spaces(s.data(), last);
    const CharT* p0 = p;
    if ((p = scvt::starts_with(p, last, "true", 4)) > p0) {
        val = true;
    } else if ((p = scvt::starts_with(p, last, "false", 5)) > p0) {
        val = false;
    } else if (p < last && is_digit(*p)) {
        val = false;
        do {
            if (*p++ != '0') { val = true; }
        } while (p < last && is_digit(*p));
    } else {
        return 0;
    }
    return static_cast<size_t>(p - s.data());
}

template<typename StrTy>
StrTy& string_converter<bool>::to_string(StrTy& s, bool val, const fmt_state& fmt) {
    const std::string_view sval = val ? std::string_view(!(fmt.flags & fmt_flags::kUpperCase) ? "true" : "TRUE", 4) :
                                        std::string_view(!(fmt.flags & fmt_flags::kUpperCase) ? "false" : "FALSE", 5);
    const auto fn = [sval](StrTy& s) -> StrTy& { return s.append(sval.begin(), sval.end()); };
    return fmt.width > sval.size() ? scvt::fmt_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) : fn(s);
}

}  // namespace uxs
