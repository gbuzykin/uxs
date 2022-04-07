#pragma once

#include "util/stringcvt.h"

#include <array>
#include <limits>

#if defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#endif  // defined(_MSC_VER)

namespace util {
namespace scvt {

template<typename Ty>
struct fp_traits;

template<>
struct fp_traits<double> {
    enum : unsigned { kTotalBits = 64, kBitsPerMantissa = 52 };
    enum : uint64_t { kMantissaMask = (1ull << kBitsPerMantissa) - 1 };
    enum : int { kExpMax = (1 << (kTotalBits - kBitsPerMantissa - 1)) - 1 };
    static uint64_t to_u64(const double& f) { return *reinterpret_cast<const uint64_t*>(&f); }
    static double from_u64(const uint64_t& u64) { return *reinterpret_cast<const double*>(&u64); }
};

template<>
struct fp_traits<float> {
    enum : unsigned { kTotalBits = 32, kBitsPerMantissa = 23 };
    enum : uint64_t { kMantissaMask = (1ull << kBitsPerMantissa) - 1 };
    enum : int { kExpMax = (1 << (kTotalBits - kBitsPerMantissa - 1)) - 1 };
    static uint64_t to_u64(const float& f) { return *reinterpret_cast<const uint32_t*>(&f); }
    static float from_u64(const uint64_t& u64) { return *reinterpret_cast<const float*>(&u64); }
};

struct uint96_t {
    uint64_t hi;
    uint32_t lo;
};

struct uint128_t {
    uint64_t hi;
    uint64_t lo;
};

struct fp_m96_t {
    uint96_t m;
    int exp;
};

struct fp_m64_t {
    uint64_t m;
    int exp;
};

struct pow_table_t {
    enum : int { kPow10Max = 400, kPow2Max = 1100, kPrecLimit = 19 };
    enum : uint64_t { kMaxMantissa10 = 10000000000000000000ull };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, 2 * kPow2Max + 1> exp2to10;
    std::array<uint64_t, 20> ten_pows;
    std::array<int64_t, 70> decimal_mul;
    pow_table_t();
};

extern const pow_table_t g_pow_tbl;
extern const int g_default_prec[];
extern const char g_digits[][2];

inline uint64_t lo32(uint64_t x) { return x & 0xffffffff; }
inline uint64_t hi32(uint64_t x) { return x >> 32; }

template<typename TyH, typename TyL>
uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

#if defined(__GNUC__) && defined(__x86_64__)
inline unsigned ulog2(uint64_t x) { return __builtin_clzll(x) ^ 63; }
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
static const ulog2_table_t g_ulog2_tbl;
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

inline uint96_t mul64x32(uint64_t x, uint32_t y, uint32_t bias = 0) {
#if defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * static_cast<gcc_ints::uint128>(y) + bias;
    return uint96_t{static_cast<uint64_t>(p >> 32), static_cast<uint32_t>(p)};
#else
    const uint64_t lower = lo32(x) * y + bias;
    return uint96_t{hi32(x) * y + hi32(lower), static_cast<uint32_t>(lo32(lower))};
#endif
}

inline uint128_t mul64x64(uint64_t x, uint64_t y, uint64_t bias = 0) {
#if defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * static_cast<gcc_ints::uint128>(y) + bias;
    return uint128_t{static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    uint64_t mid = lo32(x) * hi32(y) + hi32(bias), mid0 = mid;
    mid += hi32(x) * lo32(y) + hi32(lower);
    if (mid < mid0) { higher += 0x100000000; }
    return uint128_t{higher + hi32(mid), make64(lo32(mid), lo32(lower))};
#endif
}

inline uint128_t mul96x64_hi128(uint96_t x, uint64_t y) { return mul64x64(x.hi, y, mul64x32(y, x.lo).hi); }

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

template<typename Ty, typename CharT>
Ty to_integer(const CharT* p, const CharT* end, const CharT*& last) {
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
    Ty val = static_cast<int>(*p++ - '0');
    while (p < end && is_digit(*p)) { val = 10 * val + static_cast<int>(*p++ - '0'); }
    if (neg) { val = -val; }  // apply sign
    last = p;
    return val;
}

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, uint64_t& m, int& exp) {
    for (; p < end && is_digit(*p); ++p) {
        if (m < pow_table_t::kMaxMantissa10 / 10) {  // decimal mantissa can hold up to 19 digits
            m = 10 * m + static_cast<int>(*p - '0');
        } else {
            ++exp;
        }
    }
    return p;
}

template<typename CharT>
const CharT* to_fp_exp10(const CharT* p, const CharT* end, fp_m64_t& fp10) {
    if (p == end) { return p; }
    if (is_digit(*p)) {  // integer part
        fp10.m = static_cast<int>(*p++ - '0');
        p = accum_mantissa(p, end, fp10.m, fp10.exp);
        if (p < end && *p == '.') { ++p; }  // skip decimal point
    } else if (*p == '.' && p + 1 < end && is_digit(*(p + 1))) {
        fp10.m = static_cast<int>(*(p + 1) - '0');  // tenth
        fp10.exp = -1, p += 2;
    } else {
        return p;
    }
    const CharT* p1 = accum_mantissa(p, end, fp10.m, fp10.exp);  // fractional part
    fp10.exp -= static_cast<unsigned>(p1 - p);
    if (p1 < end && (*p1 == 'e' || *p1 == 'E')) {  // optional exponent
        int exp_optional = to_integer<int32_t>(p1 + 1, end, p);
        if (p > p1 + 1) { fp10.exp += exp_optional, p1 = p; }
    }
    return p1;
}

static uint64_t fp_exp10_to_exp2(fp_m64_t fp10, const unsigned bpm, const int exp_max) {
    if (fp10.m == 0 || fp10.exp < -pow_table_t::kPow10Max) { return 0; }                      // perfect zero
    if (fp10.exp > pow_table_t::kPow10Max) { return static_cast<uint64_t>(exp_max) << bpm; }  // infinity

    unsigned log = 1 + ulog2(fp10.m);
    fp10.m <<= 64 - log;

    // Convert decimal mantissa to binary :
    // Note: coefficients in `coef10to2` are normalized and belong [1, 2) range
    // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
    // but we drop the lowest 32 bits
    fp_m96_t coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max + fp10.exp];
    uint128_t res128 = mul96x64_hi128(coef.m, fp10.m);
    res128.hi += fp10.m;  // apply implicit 1 term of normalized coefficient
    // Note: overflow is possible while summing `res128.hi += fp10.m`
    // Move binary mantissa to the left position so the most significant `1` is hidden
    if (res128.hi >= fp10.m) {
        res128.hi = (res128.hi << 1) | (res128.lo >> 63);
        res128.lo <<= 1, --log;
    }

    // Obtain binary exponent
    const int exp_bias = exp_max >> 1;
    fp_m64_t fp2{0, static_cast<int>(exp_bias + log + coef.exp)};
    if (fp2.exp >= exp_max) {
        return static_cast<uint64_t>(exp_max) << bpm;  // infinity
    } else if (fp2.exp <= -static_cast<int>(bpm)) {
        // corner cases: perfect zero or the smallest possible floating point number
        return fp2.exp == -static_cast<int>(bpm) ? 1ull : 0;
    }

    // General case: obtain rounded binary mantissa bits

    // When `exp <= 0` mantissa will be denormalized further, so store the real mantissa length
    const unsigned n_bits = fp2.exp > 0 ? bpm : bpm + fp2.exp - 1;

    // Do banker's or `nearest even` rounding :
    // It seems that perfect rounding is not possible, because theoretical binary mantissa
    // representation can be of infinite length. But we use some heuristic to detect exact halves.
    // If we take long enough range of bits after the point where we need to break series then
    // analyzing this bits we can make the decision in which direction to round.
    // The following bits: x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
    //                     x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
    //                    | needed    | to be rounded     | unknown   |
    // In our case we need `n_bits` left bits in `res128.hi`, all other bits are rounded and dropped.
    // To decide in which direction to round we use reliable bits after `n_bits`. Totally 96 bits are
    // reliable, because `coef10to2` has 96-bit precision. So, we use only `res128.hi` and higher 32-bit
    // part of `res128.lo`.

    // Drop unreliable bits and resolve the case of periodical `1`
    // Note: we do not need to reset lower 32-bit part of `res128.lo`, because it is ignored further
    const uint64_t before_rounding = res128.hi, lsb_half = 0x80000000;
    res128.lo += lsb_half;
    if (res128.lo < lsb_half) { ++res128.hi; }  // handle lower part overflow

    // Do banker's rounding
    const uint64_t half = 1ull << (63 - n_bits);
    res128.hi += hi32(res128.lo) == 0 && (res128.hi & (half << 1)) == 0 ? half - 1 : half;
    if (res128.hi < before_rounding) {  // overflow while rounding
        // Note: the value can become normalized if `exp == 0` or infinity if `exp == exp_max - 1`
        // Note: in case of overflow 'fp2.m' will be `0`
        ++fp2.exp;
    } else {
        // shift mantissa to the right position
        fp2.m = res128.hi >> (64 - bpm);
    }

    // Compose floating point value
    if (fp2.exp <= 0) { return (fp2.m | (1ull << bpm)) >> (1 - fp2.exp); }  // denormalized
    return (static_cast<uint64_t>(fp2.exp) << bpm) | fp2.m;
}

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
    const CharT* p1 = to_fp_exp10(p, end, fp10);
    if (p1 > p) {
        fp2 |= fp_exp10_to_exp2(fp10, bpm, exp_max);
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
}  // namespace scvt

// ---- from value to string

template<typename Ty, typename CharT>
CharT* gen_bin_digits(CharT* p, Ty v) {
    do {
        *--p = '0' + static_cast<unsigned>(v & 0x1);
        v >>= 1;
    } while (v != 0);
    return p;
}

template<typename Ty, typename CharT>
CharT* gen_oct_digits(CharT* p, Ty v) {
    do {
        *--p = '0' + static_cast<unsigned>(v & 0x7);
        v >>= 3;
    } while (v != 0);
    return p;
}

template<typename Ty, typename CharT>
CharT* gen_hex_digits(CharT* p, Ty v, bool upper) {
    const char* digs = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        *--p = digs[v & 0xf];
        v >>= 4;
    } while (v != 0);
    return p;
}

template<typename Ty, typename CharT>
CharT* gen_digits(CharT* p, Ty v) {
    if (v < 10) {
        *--p = '0' + static_cast<unsigned>(v);
        return p;
    }
    do {
        const Ty t = v / 100;
        const char* d = &g_digits[static_cast<unsigned>(v - 100 * t)][0];
        p -= 2, v = t;
        p[0] = d[0], p[1] = d[1];
    } while (v >= 10);
    if (v > 0) { *--p = '0' + static_cast<unsigned>(v); }
    return p;
}

template<typename StrTy, typename CharT>
StrTy& fmt_adjusted(const CharT* first, const CharT* last, StrTy& s, const fmt_state& fmt) {
    unsigned len = static_cast<unsigned>(last - first);
    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: return s.append(first, last).append(fmt.width - len, fmt.fill);
        case fmt_flags::kInternal: {
            unsigned right = fmt.width - len;
            unsigned left = right >> 1;
            right -= left;
            return s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
        } break;
        default: return s.append(fmt.width - len, fmt.fill).append(first, last);
    }
}

template<typename CharT>
basic_dynbuf_appender<CharT>& fmt_adjusted(const CharT* first, const CharT* last, basic_dynbuf_appender<CharT>& s,
                                           const fmt_state& fmt) {
    basic_unlimbuf_appender<CharT> appender(s.reserve(fmt.width));
    return s.setcurr(fmt_adjusted(first, last, appender, fmt).curr());
}

template<typename CharT>
basic_limbuf_appender<CharT>& fmt_adjusted(const CharT* first, const CharT* last, basic_limbuf_appender<CharT>& s,
                                           const fmt_state& fmt) {
    if (static_cast<size_t>(s.last() - s.curr()) >= fmt.width) {
        basic_unlimbuf_appender<CharT> appender(s.curr());
        return s.setcurr(fmt_adjusted(first, last, appender, fmt).curr());
    }
    basic_dynbuf_appender<CharT> appender;
    fmt_adjusted(first, last, appender, fmt);
    return s.append(appender.data(), appender.curr());
}

template<unsigned NPref, typename StrTy, typename CharT>
StrTy& fmt_num_adjusted(const CharT* first, const CharT* last, StrTy& s, unsigned len, const fmt_state& fmt) {
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(first, last, s, fmt); }
    for (unsigned n = 0; n < NPref; ++n) { s.push_back(*first++); }
    return s.append(fmt.width - len, '0').append(first, last);
}

template<unsigned NPref, typename CharT>
basic_dynbuf_appender<CharT>& fmt_num_adjusted(const CharT* first, const CharT* last, basic_dynbuf_appender<CharT>& s,
                                               unsigned len, const fmt_state& fmt) {
    basic_unlimbuf_appender<CharT> appender(s.reserve(fmt.width));
    return s.setcurr(fmt_num_adjusted<NPref>(first, last, appender, len, fmt).curr());
}

template<unsigned NPref, typename CharT>
basic_limbuf_appender<CharT>& fmt_num_adjusted(const CharT* first, const CharT* last, basic_limbuf_appender<CharT>& s,
                                               unsigned len, const fmt_state& fmt) {
    if (static_cast<size_t>(s.last() - s.curr()) >= fmt.width) {
        basic_unlimbuf_appender<CharT> appender(s.curr());
        return s.setcurr(fmt_num_adjusted<NPref>(first, last, appender, len, fmt).curr());
    }
    basic_dynbuf_appender<CharT> appender;
    fmt_num_adjusted<NPref>(first, last, appender, len, fmt);
    return s.append(appender.data(), appender.curr());
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_bin(Ty val, StrTy& s, const fmt_state& fmt) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 65> buf;
    CharT *last = buf.data() + buf.size(), *p = last;
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'b' : 'B'; }
    p = gen_bin_digits(p, val);
    const unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) { return fmt_num_adjusted<0>(p, last, s, len, fmt); }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_oct(Ty val, StrTy& s, const fmt_state& fmt) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT* last = buf.data() + buf.size();
    CharT* p = gen_oct_digits(last, val);
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = '0'; }
    const unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) { return fmt_num_adjusted<0>(p, last, s, len, fmt); }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_hex(Ty val, StrTy& s, const fmt_state& fmt) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT* last = buf.data() + buf.size();
    CharT* p = gen_hex_digits(last, val, !!(fmt.flags & fmt_flags::kUpperCase));
    const unsigned len = static_cast<unsigned>(last - p);
    if (!!(fmt.flags & fmt_flags::kShowBase)) {
        p -= 2, p[0] = '0', p[1] = !(fmt.flags & fmt_flags::kUpperCase) ? 'x' : 'X';
        if (fmt.width > 2 + len) { return fmt_num_adjusted<2>(p, last, s, 2 + len, fmt); }
    } else if (fmt.width > len) {
        return fmt_num_adjusted<0>(p, last, s, len, fmt);
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec_unsigned(Ty val, StrTy& s, const fmt_state& fmt) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT* last = buf.data() + buf.size();
    CharT* p = gen_digits(last, val);
    const unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) { return fmt_num_adjusted<0>(p, last, s, len, fmt); }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_dec_signed(Ty val, StrTy& s, const fmt_state& fmt) {
    char sign = '\0';
    if (val < 0) {
        sign = '-', val = -val;  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT* last = buf.data() + buf.size();
    CharT* p = gen_digits(last, static_cast<typename std::make_unsigned<Ty>::type>(val));
    const unsigned len = static_cast<unsigned>(last - p);
    if (sign != '\0') {
        *--p = sign;
        if (fmt.width > 1 + len) { return fmt_num_adjusted<1>(p, last, s, 1 + len, fmt); }
    } else if (fmt.width > len) {
        return fmt_num_adjusted<0>(p, last, s, len, fmt);
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_unsigned(Ty val, StrTy& s, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kBin: fmt_bin(val, s, fmt); break;
        case fmt_flags::kOct: fmt_oct(val, s, fmt); break;
        case fmt_flags::kHex: fmt_hex(val, s, fmt); break;
        default: fmt_dec_unsigned(val, s, fmt); break;
    }
    return s;
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_signed(Ty val, StrTy& s, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kBin: {
            fmt_bin(static_cast<typename std::make_unsigned<Ty>::type>(val), s, fmt);
        } break;
        case fmt_flags::kOct: {
            fmt_oct(static_cast<typename std::make_unsigned<Ty>::type>(val), s, fmt);
        } break;
        case fmt_flags::kHex: {
            fmt_hex(static_cast<typename std::make_unsigned<Ty>::type>(val), s, fmt);
        } break;
        default: fmt_dec_signed(val, s, fmt); break;
    }
    return s;
}

inline unsigned fmt_float_len(int exp, char sign, fmt_flags flags, int prec) {
    return (sign != '\0' ? 6 : 5) + (std::abs(exp) >= 100 ? 1 : 0) +
           (prec > 0 || !!(flags & fmt_flags::kShowPoint) ? prec + 1 : 0);
}

inline unsigned fmt_float_fixed_len(int exp, char sign, fmt_flags flags, int prec) {
    return (sign != '\0' ? 2 : 1) + std::max(exp, 0) + (prec > 0 || !!(flags & fmt_flags::kShowPoint) ? prec + 1 : 0);
}

template<typename StrTy>
StrTy& fmt_fp_exp10(const fp_m64_t& fp10, char sign, StrTy& s, fmt_flags flags, int prec) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT *p_exp = buf.data() + 24, *last = p_exp + 4;

    int exp10 = fp10.exp;
    p_exp[0] = !(flags & fmt_flags::kUpperCase) ? 'e' : 'E';
    if (exp10 < 0) {
        p_exp[1] = '-', exp10 = -exp10;
    } else {
        p_exp[1] = '+';
    }
    if (exp10 >= 100) {
        const int t = (656 * exp10) >> 16;
        const char* d = &g_digits[static_cast<unsigned>(exp10 - 100 * t)][0];
        p_exp[2] = '0' + static_cast<unsigned>(t), p_exp[3] = d[0], p_exp[4] = d[1];
        ++last;
    } else {
        p_exp[2] = g_digits[exp10][0], p_exp[3] = g_digits[exp10][1];
    }

    CharT* p = p_exp;
    int n_digs = 1;
    if (prec > 0) {
        p = scvt::gen_digits(p, fp10.m);
        n_digs = static_cast<int>(p_exp - p);
        --p, p[0] = p[1], p[1] = '.';
    } else {
        if (!!(flags & fmt_flags::kShowPoint)) { *--p = '.'; }
        *--p = '0' + static_cast<int>(fp10.m);
    }
    if (sign != '\0') { *--p = sign; }
    if (n_digs <= prec) { return s.append(p, p_exp).append(1 + prec - n_digs, '0').append(p_exp, last); }
    return s.append(p, last);
}

template<typename StrTy>
StrTy& fmt_fp_exp10_fixed(const fp_m64_t& fp10, char sign, StrTy& s, fmt_flags flags, int prec) {
    using CharT = typename StrTy::value_type;
    std::array<CharT, 32> buf;
    CharT* last = buf.data() + buf.size();
    CharT* p = scvt::gen_digits(last, fp10.m);
    if (sign != '\0') { s.push_back(sign); }

    int k = 1 + fp10.exp, n_digs = static_cast<int>(last - p);
    if (k > 0) {
        if (n_digs < k) {
            s.append(p, last).append(k - n_digs, '0');
            if (prec > 0) {
                s.push_back('.');
                return s.append(prec, '0');
            }
        } else {
            s.append(p, p + k);
            if (prec > 0) {
                s.push_back('.');
                return s.append(p + k, last).append(prec + k - n_digs, '0');
            }
        }
    } else {
        s.push_back('0');
        if (prec > 0) {
            s.push_back('.');
            return s.append(-k, '0').append(p, last).append(prec + k - n_digs, '0');
        }
    }

    if (!!(flags & fmt_flags::kShowPoint)) { s.push_back('.'); }
    return s;
}

template<typename CharT>
basic_dynbuf_appender<CharT>& fmt_fp_exp10_fixed(const fp_m64_t& fp10, char sign, basic_dynbuf_appender<CharT>& s,
                                                 fmt_flags flags, int prec) {
    basic_unlimbuf_appender<CharT> appender(s.reserve(fmt_float_fixed_len(fp10.exp, sign, flags, prec)));
    return s.setcurr(fmt_fp_exp10_fixed(fp10, sign, appender, flags, prec).curr());
}

template<typename CharT>
basic_limbuf_appender<CharT>& fmt_fp_exp10_fixed(const fp_m64_t& fp10, char sign, basic_limbuf_appender<CharT>& s,
                                                 fmt_flags flags, int prec) {
    if (static_cast<size_t>(s.last() - s.curr()) >= fmt_float_fixed_len(fp10.exp, sign, flags, prec)) {
        basic_unlimbuf_appender<CharT> appender(s.curr());
        return s.setcurr(fmt_fp_exp10_fixed(fp10, sign, appender, flags, prec).curr());
    }
    basic_dynbuf_appender<CharT> appender;
    fmt_fp_exp10_fixed(fp10, sign, appender, flags, prec);
    return s.append(appender.data(), appender.curr());
}

// Compilers should be able to optimize this into the ror instruction
inline uint32_t rotr1(uint32_t n) { return (n >> 1) | (n << 31); }
inline uint32_t rotr2(uint32_t n) { return (n >> 2) | (n << 30); }
inline uint64_t rotr1(uint64_t n) { return (n >> 1) | (n << 63); }
inline uint64_t rotr2(uint64_t n) { return (n >> 2) | (n << 62); }

// Removes trailing zeros and returns the number of zeros removed (< 2^32)
inline int remove_trailing_zeros_small(uint64_t& n) {
    const uint32_t mod_inv_5 = 0xcccccccd;
    const uint32_t mod_inv_25 = 0xc28f5c29;

    int s = 0;
    while (true) {
        uint32_t q = rotr2(static_cast<uint32_t>(n) * mod_inv_25);
        if (q > std::numeric_limits<uint32_t>::max() / 100) { break; }
        s += 2, n = q;
    }
    uint32_t q = rotr1(static_cast<uint32_t>(n) * mod_inv_5);
    if (q <= std::numeric_limits<uint32_t>::max() / 10) { ++s, n = q; }
    return s;
}

// Removes trailing zeros and returns the number of zeros removed
inline int remove_trailing_zeros(uint64_t& n) {
    if (n <= std::numeric_limits<uint32_t>::max()) { return remove_trailing_zeros_small(n); }

    // this magic number is ceil(2^90 / 10^8).
    const uint64_t magic_number = 12379400392853802749ull;
    const uint128_t nm = mul64x64(n, magic_number);

    int s = 0;

    // is n is divisible by 10^8?
    if ((nm.hi & ((1ull << (90 - 64)) - 1)) == 0 && nm.lo < magic_number) {
        // if yes, work with the quotient
        n = nm.hi >> (90 - 64);
        if (n <= std::numeric_limits<uint32_t>::max()) { return 8 + remove_trailing_zeros_small(n); }
        s += 8;
    }

    const uint64_t mod_inv_5 = 0xcccccccccccccccd;
    const uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;

    while (true) {
        uint64_t q = rotr2(n * mod_inv_25);
        if (q > std::numeric_limits<uint64_t>::max() / 100) { break; }
        s += 2, n = q;
    }
    uint64_t q = rotr1(n * mod_inv_5);
    if (q <= std::numeric_limits<uint64_t>::max() / 10) { ++s, n = q; }
    return s;
}

static fp_m64_t fp_exp2_to_exp10(fp_m64_t fp2, bool is_default, bool& is_fixed, int& prec, const unsigned bpm,
                                 const int exp_max) {
    if (fp2.m != 0 || fp2.exp > 0) {
        bool optimal = false;
        unsigned log = bpm;

        // Shift binary mantissa so the MSB bit is `1`
        if (fp2.exp > 0) {
            fp2.m <<= 63 - bpm;
            fp2.m |= 1ull << 63;
        } else {  // handle denormalized form
            log = ulog2(fp2.m);
            fp2.m <<= 63 - log, fp2.exp -= bpm - 1 - log;
        }

        if (prec < 0) {
            prec = g_default_prec[log], optimal = is_default;
        } else {
            prec &= 0xffff;
        }

        // Obtain decimal power
        const int exp_bias = exp_max >> 1;
        fp_m64_t fp10{0, g_pow_tbl.exp2to10[pow_table_t::kPow2Max - exp_bias + fp2.exp]};

        // Evaluate desired decimal mantissa length (its integer part)
        // Note: integer part of decimal mantissa is the digits to output,
        // fractional part of decimal mantissa is to be rounded and dropped
        if (is_default) { prec = std::max(prec - 1, 0); }
        int n_digs = 1 + prec;                 // desired digit count for scientific format
        if (is_fixed) { n_digs += fp10.exp; }  // for fixed format

        if (n_digs >= 0) {
            n_digs = std::min<int>(n_digs, pow_table_t::kPrecLimit);

            // Calculate decimal mantissa representation :
            // Note: coefficients in `coef10to2` are normalized and belong [1, 2) range
            // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
            // but we drop the lowest 32 bits
            // To get the desired count of digits we move up the `coef10to2` table, it's equivalent to
            // multiplying the result by a certain power of 10
            fp_m96_t coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp + n_digs - 1];
            uint128_t res128 = mul96x64_hi128(coef.m, fp2.m);
            res128.hi += fp2.m;  // apply implicit 1 term of normalized coefficient

            // Do banker's or `nearest even` rounding :
            // It seems that perfect rounding is not possible, because theoretical decimal mantissa
            // representation can be of infinite length. But we use some heuristic to detect exact halves.
            // If we take long enough range of bits after the point where we need to break series then
            // analyzing this bits we can make the decision in which direction to round.
            // The following bits: x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
            //                     x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
            //                    | needed    | to be truncated   | unknown   |
            // In our case we need known count of left bits `res128.hi`, which represent integer part of
            // decimal mantissa, all other bits are rounded and dropped. To decide in which direction to round
            // we use reliable bits after decimal mantissa. Totally 96 bits are reliable, because `coef10to2`
            // has 96-bit precision. So, we use only `res128.hi` and higher 32-bit part of `res128.lo`.

            // Drop unreliable bits and resolve the case of periodical `1`
            const uint64_t lsb_half = 0x80000000;
            res128.lo += lsb_half;
            if (res128.lo < lsb_half) { ++res128.hi; }  // handle lower part overflow
            res128.lo &= ~((1ull << 32) - 1);           // zero lower 32-bit part of `res128.lo`

            // Overflow is possible while summing `res128.hi += mantissa` or rounding - store this
            // overflowed bit. We must take it into account in further calculations
            const uint64_t higher_bit = res128.hi < fp2.m ? 1ull : 0ull;

            // Store the shift needed to align integer part of decimal mantissa with 64-bit boundary
            const unsigned shift = 63 + exp_bias - fp2.exp - coef.exp;  // sum coefficient exponent as well
            // Note: resulting decimal mantissa has the form `C * 10^n_digs`, where `C` belongs [1, 20) range.
            // So, if `C >= 10` we get decimal mantissa with one excess digit. We should detect these cases
            // and divide decimal mantissa by 10 for scientific format

            int64_t err = 0;
            const int64_t* err_mul = g_pow_tbl.decimal_mul.data();
            if (shift == 0 && higher_bit != 0) {
                assert(n_digs == pow_table_t::kPrecLimit);
                // Decimal mantissa contains one excess digit, which doesn't fit 64 bits.
                // We can't handle more than 64 bits while outputting the number, so remove it.
                // Note: if `higher_bit == 1` decimal mantissa has 65 significant bits
                ++fp10.exp;
                // Divide 65-bit value by 10. We know the division result for 10^64 summand, so we just use it.
                const uint64_t div64 = 1844674407370955161ull, mod64 = 6;
                fp10.m = div64 + (res128.hi + mod64) / 10;
                unsigned mod = static_cast<unsigned>(res128.hi - 10 * fp10.m);
                if (mod > 5 || (mod == 5 && (res128.lo != 0 || (fp10.m & 1) != 0))) { ++fp10.m; }
            } else {
                // Align integer part of decimal mantissa with 64-bit boundary
                if (shift == 0) {
                } else if (shift < 64) {
                    res128.lo = (res128.lo >> shift) | (res128.hi << (64 - shift));
                    res128.hi = (res128.hi >> shift) | (higher_bit << (64 - shift));
                } else if (shift > 64) {
                    res128.lo = (res128.hi >> (shift - 64)) | (higher_bit << (128 - shift));
                    res128.hi = 0;
                } else {
                    res128.lo = res128.hi;
                    res128.hi = higher_bit;
                }

                if (!is_fixed && res128.hi >= g_pow_tbl.ten_pows[n_digs]) {
                    ++fp10.exp, err_mul += 10;  // one excess digit
                    // Remove one excess digit for scientific format, do 'nearest even' rounding
                    fp10.m = res128.hi / 10;
                    err = res128.hi - 10 * fp10.m;
                    if (err > 5 || (err == 5 && (res128.lo != 0 || (fp10.m & 1) != 0))) { ++fp10.m, err -= 10; }
                } else {
                    // Do 'nearest even' rounding
                    const uint64_t half = 1ull << 63;
                    const uint64_t frac = (res128.hi & 1) == 0 ? res128.lo + half - 1 : res128.lo + half;
                    fp10.m = res128.hi;
                    if (frac < res128.lo) { ++fp10.m, err = -1; }
                    if (fp10.m >= g_pow_tbl.ten_pows[n_digs]) {
                        ++fp10.exp;  // one excess digit
                        if (!is_fixed) {
                            // Remove one excess digit for scientific format
                            // Note: `fp10.m` is exact power of 10 in this case
                            fp10.m /= 10;
                        }
                    }
                }
            }

            if (fp10.m != 0) {
                if (optimal) {
                    // Evaluate acceptable error range to pass roundtrip test
                    assert(log + shift >= 30);
                    const unsigned shift2 = log + shift - 30;
                    int64_t delta_minus = (coef.m.hi >> shift2) | (1ull << (64 - shift2));
                    int64_t delta_plus = delta_minus;
                    if (fp2.exp > 1 && fp2.m == 1ull << 63) { delta_plus >>= 1; }
                    err = (err << 32) | (res128.lo >> 32);

                    // Trim trailing unsignificant digits
                    const int64_t max_err_mul = delta_minus << 1;
                    while (true) {
                        uint64_t t = fp10.m / 10;
                        unsigned mod = static_cast<unsigned>(fp10.m - 10 * t);
                        if (mod > 0) {
                            err += err_mul[mod];
                            err_mul += 10;
                            int64_t err2 = *err_mul - err;
                            assert(err >= 0 && err2 >= 0);
                            // If both round directions are acceptable, use banker's rounding
                            if (err < delta_plus) {
                                if (err2 < delta_minus && (err2 < err || (err2 == err && (t & 1) != 0))) {
                                    ++t, err = -err2;
                                }
                            } else if (err2 < delta_minus) {
                                ++t, err = -err2;
                            } else {
                                break;
                            }
                        } else {
                            err_mul += 10;
                        }
                        --prec, fp10.m = t;
                        if (*err_mul >= max_err_mul) {
                            prec -= remove_trailing_zeros(fp10.m);
                            break;
                        }
                    }
                    if (prec < 0) { ++fp10.exp, prec = 0; }
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec + 4) {
                        is_fixed = true, prec = std::max(prec - fp10.exp, 0);
                    }
                } else if (is_default) {
                    const int prec0 = prec;
                    prec = n_digs - 1 - remove_trailing_zeros(fp10.m);
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec0) { is_fixed = true, prec = std::max(prec - fp10.exp, 0); }
                }

                return fp10;
            }
        }
    }

    if (is_default) {
        is_fixed = true, prec = 0;
    } else if (prec < 0) {
        prec = 0;
    } else {
        prec &= 0xffff;
    }

    return fp_m64_t{0, 0};
}

template<typename StrTy>
StrTy& fmt_float_adjusted_finite(const fp_m64_t& fp10, char sign, StrTy& s, bool is_fixed, int prec, unsigned len,
                                 const fmt_state& fmt) {
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        if ((fmt.flags & fmt_flags::kAdjustField) != fmt_flags::kRight) {
            unsigned left = 0, right = fmt.width - len;
            if ((fmt.flags & fmt_flags::kAdjustField) == fmt_flags::kInternal) {
                left = right >> 1;
                right -= left;
            }
            s.append(left, fmt.fill);
            if (is_fixed) {
                fmt_fp_exp10_fixed(fp10, sign, s, fmt.flags, prec);
            } else {
                fmt_fp_exp10(fp10, sign, s, fmt.flags, prec);
            }
            return s.append(right, fmt.fill);
        }
        s.append(fmt.width - len, fmt.fill);
    } else {
        if (sign != '\0') { s.push_back(sign); }
        s.append(fmt.width - len, '0');
        sign = '\0';
    }
    if (is_fixed) { return fmt_fp_exp10_fixed(fp10, sign, s, fmt.flags, prec); }
    return fmt_fp_exp10(fp10, sign, s, fmt.flags, prec);
}

template<typename CharT>
basic_dynbuf_appender<CharT>& fmt_float_adjusted_finite(const fp_m64_t& fp10, char sign,
                                                        basic_dynbuf_appender<CharT>& s, bool is_fixed, int prec,
                                                        unsigned len, const fmt_state& fmt) {
    basic_unlimbuf_appender<CharT> appender(s.reserve(fmt.width));
    return s.setcurr(fmt_float_adjusted_finite(fp10, sign, appender, is_fixed, prec, len, fmt).curr());
}

template<typename CharT>
basic_limbuf_appender<CharT>& fmt_float_adjusted_finite(const fp_m64_t& fp10, char sign,
                                                        basic_limbuf_appender<CharT>& s, bool is_fixed, int prec,
                                                        unsigned len, const fmt_state& fmt) {
    if (static_cast<size_t>(s.last() - s.curr()) >= fmt.width) {
        basic_unlimbuf_appender<CharT> appender(s.curr());
        return s.setcurr(fmt_float_adjusted_finite(fp10, sign, appender, is_fixed, prec, len, fmt).curr());
    }
    basic_dynbuf_appender<CharT> appender;
    fmt_float_adjusted_finite(fp10, sign, appender, is_fixed, prec, len, fmt);
    return s.append(appender.data(), appender.curr());
}

template<typename StrTy>
StrTy& fmt_float_common(uint64_t u64, StrTy& s, const fmt_state& fmt, const unsigned bpm, const int exp_max) {
    char sign = '\0';
    if (u64 & (static_cast<uint64_t>(1 + exp_max) << bpm)) {
        sign = '-';  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    // Binary exponent and mantissa
    const fp_m64_t fp2{u64 & ((1ull << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};

    if (fp2.exp == exp_max) {
        using CharT = typename StrTy::value_type;
        std::array<CharT, 4> buf;
        CharT *p = buf.data(), *p0 = p;
        if (sign != '\0') { *p++ = sign; }
        if (fp2.m == 0) {
            if (!(fmt.flags & fmt_flags::kUpperCase)) {
                p[0] = 'i', p[1] = 'n', p[2] = 'f';
            } else {
                p[0] = 'I', p[1] = 'N', p[2] = 'F';
            }
        } else if (!(fmt.flags & fmt_flags::kUpperCase)) {
            p[0] = 'n', p[1] = 'a', p[2] = 'n';
        } else {
            p[0] = 'N', p[1] = 'A', p[2] = 'N';
        }
        p += 3;
        if (fmt.width > static_cast<unsigned>(p - p0)) { return fmt_adjusted(p0, p, s, fmt); }
        return s.append(p0, p);
    }

    int prec = fmt.prec;
    bool is_fixed = (fmt.flags & fmt_flags::kFloatField) == fmt_flags::kFixed;
    const fp_m64_t fp10 = fp_exp2_to_exp10(fp2, !(fmt.flags & fmt_flags::kFloatField), is_fixed, prec, bpm, exp_max);
    if (fmt.width > 0) {
        const unsigned len = is_fixed ? fmt_float_fixed_len(fp10.exp, sign, fmt.flags, prec) :
                                        fmt_float_len(fp10.exp, sign, fmt.flags, prec);
        if (fmt.width > len) { return fmt_float_adjusted_finite(fp10, sign, s, is_fixed, prec, len, fmt); }
    }

    if (is_fixed) { return fmt_fp_exp10_fixed(fp10, sign, s, fmt.flags, prec); }
    return fmt_fp_exp10(fp10, sign, s, fmt.flags, prec);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_floating_point<Ty>::value>>
StrTy& fmt_float(Ty val, StrTy& s, const fmt_state& fmt) {
    return fmt_float_common(fp_traits<Ty>::to_u64(val), s, fmt, fp_traits<Ty>::kBitsPerMantissa, fp_traits<Ty>::kExpMax);
}

}  // namespace scvt

#define SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(ty, from_string_func, to_string_func) \
    template<typename CharT> \
    const CharT* basic_string_converter<CharT, ty>::from_string(const CharT* first, const CharT* last, ty& val) { \
        const CharT* p = scvt::skip_spaces(first, last); \
        ty t = scvt::from_string_func(p, last, last); \
        if (last > p) { \
            val = t; \
            return last; \
        } \
        return first; \
    } \
    template<typename CharT> \
    template<typename StrTy> \
    StrTy& basic_string_converter<CharT, ty>::to_string(ty val, StrTy& s, const fmt_state& fmt) { \
        return scvt::to_string_func(val, s, fmt); \
    }
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int8_t, to_integer<int32_t>, fmt_signed<int32_t>)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int16_t, to_integer<int32_t>, fmt_signed<int32_t>)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int32_t, to_integer<int32_t>, fmt_signed)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(int64_t, to_integer<int64_t>, fmt_signed)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint8_t, to_integer<int32_t>, fmt_unsigned<uint32_t>)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint16_t, to_integer<int32_t>, fmt_unsigned<uint32_t>)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint32_t, to_integer<int32_t>, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(uint64_t, to_integer<int64_t>, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(float, to_float<float>, fmt_float)
SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER(double, to_float<double>, fmt_float)
#undef SCVT_IMPLEMENT_STANDARD_STRING_CONVERTER

template<typename CharT>
const CharT* basic_string_converter<CharT, CharT>::from_string(const CharT* first, const CharT* last, CharT& val) {
    const CharT* p = scvt::skip_spaces(first, last);
    if (p == last) { return first; }
    val = *p;
    return ++p;
}

template<typename CharT>
template<typename StrTy>
StrTy& basic_string_converter<CharT, CharT>::to_string(CharT val, StrTy& s, const fmt_state& fmt) {
    if (fmt.width > 1) { return scvt::fmt_adjusted(&val, &val + 1, s, fmt); }
    s.push_back(val);
    return s;
}

template<typename CharT>
const CharT* basic_string_converter<CharT, bool>::from_string(const CharT* first, const CharT* last, bool& val) {
    const CharT *p = scvt::skip_spaces(first, last), *p0 = p;
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
        return first;
    }
    return p;
}

template<typename CharT>
template<typename StrTy>
StrTy& basic_string_converter<CharT, bool>::to_string(bool val, StrTy& s, const fmt_state& fmt) {
    std::array<CharT, 5> buf;
    CharT *p = buf.data(), *p0 = p;
    if (val) {
        if (!(fmt.flags & fmt_flags::kUpperCase)) {
            p[0] = 't', p[1] = 'r', p[2] = 'u', p[3] = 'e';
        } else {
            p[0] = 'T', p[1] = 'R', p[2] = 'U', p[3] = 'E';
        }
        p += 4;
    } else {
        if (!(fmt.flags & fmt_flags::kUpperCase)) {
            p[0] = 'f', p[1] = 'a', p[2] = 'l', p[3] = 's', p[4] = 'e';
        } else {
            p[0] = 'F', p[1] = 'A', p[2] = 'L', p[3] = 'S', p[4] = 'E';
        }
        p += 5;
    }
    if (fmt.width > static_cast<unsigned>(p - p0)) { return scvt::fmt_adjusted(p0, p, s, fmt); }
    return s.append(p0, p);
}

// ---- dynamic buffer implementation

template<typename CharT>
basic_dynbuf_appender<CharT>::~basic_dynbuf_appender() {
    if (first_ != buf_) { std::allocator<CharT>().deallocate(first_, last_ - first_); }
}

template<typename CharT>
void basic_dynbuf_appender<CharT>::grow(size_t extra) {
    size_t sz = dst_ - first_;
    sz += std::max(extra, sz >> 1);
    CharT* first = std::allocator<CharT>().allocate(sz);
    dst_ = std::copy(first_, dst_, first);
    if (first_ != buf_) { std::allocator<CharT>().deallocate(first_, last_ - first_); }
    first_ = first, last_ = first + sz;
}

}  // namespace util
