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
    static const uint64_t kSignBit = 1ull << (kTotalBits - 1);
    static const uint64_t kMantissaMask = (1ull << kBitsPerMantissa) - 1;
    static const uint64_t kExpMask = ~kMantissaMask & ~kSignBit;
    static const int kExpMax = kExpMask >> kBitsPerMantissa;
    static const int kExpBias = kExpMax >> 1;
    static uint64_t to_u64(const double& f) { return *reinterpret_cast<const uint64_t*>(&f); }
    static double from_u64(const uint64_t& u64) { return *reinterpret_cast<const double*>(&u64); }
};

template<>
struct fp_traits<float> {
    enum : unsigned { kTotalBits = 32, kBitsPerMantissa = 23 };
    static const uint64_t kSignBit = 1ull << (kTotalBits - 1);
    static const uint64_t kMantissaMask = (1ull << kBitsPerMantissa) - 1;
    static const uint64_t kExpMask = ~kMantissaMask & ~kSignBit & ((1ull << kTotalBits) - 1);
    static const int kExpMax = kExpMask >> kBitsPerMantissa;
    static const int kExpBias = kExpMax >> 1;
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

struct pow_table_t {
    enum : int { kPow10Max = 400, kPow2Max = 1100, kPrecLimit = 19 };
    enum : uint64_t { kMaxMantissa10 = 10000000000000000000ull };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, 2 * kPow2Max + 1> exp2to10;
    std::array<uint64_t, 20> ten_pows;
    std::array<int64_t, 70> decimal_mul;
    pow_table_t();
};

struct fp_exp10_format {
    uint64_t mantissa = 0;
    int exp = 0;
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
inline unsigned ulog2(uint64_t x) { return 63 - __builtin_clzll(x); }
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

inline const char* starts_with(const char* p, const char* end, const char* s, size_t len) {
    if (static_cast<size_t>(end - p) < len) { return p; }
    for (const char *p1 = p, *p2 = s; p1 < end; ++p1, ++p2) {
        char ch1 = to_lower(*p1), ch2 = to_lower(*p2);
        if (ch1 != ch2) { return p; }
    }
    return p + len;
}

inline const char* skip_spaces(const char* p, const char* end) {
    while (p < end && is_space(static_cast<unsigned char>(*p))) { ++p; }
    return p;
}

template<typename Ty>
char* gen_digits(char* p, Ty v) {
    while (v >= 10) {
        const Ty t = v / 100;
        const char* d = &g_digits[static_cast<unsigned>(v - 100 * t)][0];
        p -= 2, v = t;
        p[0] = d[0], p[1] = d[1];
    }
    if (v > 0) { *--p = '0' + static_cast<unsigned>(v); }
    return p;
}

template<typename StrTy>
StrTy& fmt_adjusted(const char* first, const char* last, StrTy& s, const fmt_state& fmt) {
    unsigned len = static_cast<unsigned>(last - first);
    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: {
            s.append(first, last).append(fmt.width - len, fmt.fill);
        } break;
        case fmt_flags::kInternal: {
            unsigned right = fmt.width - len, left = right >> 1;
            right -= left;
            s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
        } break;
        default: {
            s.append(fmt.width - len, fmt.fill).append(first, last);
        } break;
    }
    return s;
}

// ---- from string to value

template<typename Ty>
const char* to_integer(const char* p, const char* end, Ty& val) {
    const char* p0 = p;
    bool neg = false;
    if (p == end) {
        return p0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }
    if (p == end || !is_digit(*p)) { return p0; }
    val = static_cast<Ty>(*p++ - '0');
    while (p < end && is_digit(*p)) { val = 10 * val + static_cast<Ty>(*p++ - '0'); }
    if (neg) { val = ~val + 1; }  // apply sign
    return p;
}

UTIL_EXPORT const char* to_fp_exp10(const char* p, const char* end, fp_exp10_format& fp10);

template<typename Ty>
const char* to_float(const char* p, const char* end, Ty& val) {
    enum class fp_spec_t { kFinite = 0, kInf, kNaN } special = fp_spec_t::kFinite;
    fp_exp10_format fp10;
    const char* p0 = p;
    bool neg = false;

    if (p == end) {
        return p0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }

    int exp = 0;
    uint64_t mantissa2 = 0;
    const char* p1 = to_fp_exp10(p, end, fp10);
    if (p1 > p) {
        if (fp10.mantissa == 0 || fp10.exp < -pow_table_t::kPow10Max) {  // perfect zero
        } else if (fp10.exp > pow_table_t::kPow10Max) {                  // infinity
            exp = fp_traits<Ty>::kExpMax;
        } else {
            unsigned log = 1 + ulog2(fp10.mantissa);
            fp10.mantissa <<= 64 - log;

            // Convert decimal mantissa to binary :
            // Note: coefficients in `coef10to2` are normalized and belong [1, 2) range
            // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
            // but we drop the lowest 32 bits
            const fp_m96_t& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max + fp10.exp];
            uint128_t res128 = mul96x64_hi128(coef.m, fp10.mantissa);
            res128.hi += fp10.mantissa;  // apply implicit 1 term of normalized coefficient
            // Note: overflow is possible while summing `res128.hi += fp10.mantissa`
            // Move binary mantissa to the left position so the most significant `1` is hidden
            if (res128.hi >= fp10.mantissa) {
                res128.hi = (res128.hi << 1) | (res128.lo >> 63);
                res128.lo <<= 1, --log;
            }

            // Obtain binary exponent
            exp = fp_traits<Ty>::kExpBias + log + coef.exp;
            if (exp >= fp_traits<Ty>::kExpMax) {
                exp = fp_traits<Ty>::kExpMax;  // infinity
            } else if (exp <= -static_cast<int>(fp_traits<Ty>::kBitsPerMantissa)) {
                // corner cases: perfect zero or the smallest possible floating point number
                if (exp == -static_cast<int>(fp_traits<Ty>::kBitsPerMantissa)) { mantissa2 = 1ull; }
                exp = 0;
            } else {
                // General case: obtain rounded binary mantissa bits

                // When `exp <= 0` mantissa will be denormalized further, so store the real mantissa length
                const unsigned n_bits = exp > 0 ? fp_traits<Ty>::kBitsPerMantissa :
                                                  fp_traits<Ty>::kBitsPerMantissa + exp - 1;

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
                    // Note: the value can become normalized if `exp == 0`
                    // or infinity if `exp == fp_traits<Ty>::kExpMax - 1`
                    // Note: in case of overflow mantissa will be `0`
                    ++exp;
                }

                // shift mantissa to the right position
                mantissa2 = res128.hi >> (64 - fp_traits<Ty>::kBitsPerMantissa);
                if (exp <= 0) {  // denormalize
                    mantissa2 |= 1ull << fp_traits<Ty>::kBitsPerMantissa;
                    mantissa2 >>= 1 - exp;
                    exp = 0;
                }
            }
        }
    } else if ((p1 = starts_with(p, end, "inf", 3)) > p) {  // infinity
        exp = fp_traits<Ty>::kExpMax;
    } else if ((p1 = starts_with(p, end, "nan", 3)) > p) {  // NaN
        exp = fp_traits<Ty>::kExpMax;
        mantissa2 = fp_traits<Ty>::kMantissaMask;
    } else {
        return p0;
    }

    // Compose floating point value
    val = fp_traits<Ty>::from_u64((neg ? fp_traits<Ty>::kSignBit : 0) |
                                  (static_cast<uint64_t>(exp) << fp_traits<Ty>::kBitsPerMantissa) | mantissa2);
    return p1;
}  // namespace scvt

// ---- from value to string

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_bin(Ty val, StrTy& s, const fmt_state& fmt) {
    std::array<char, 65> buf;
    char *last = buf.data() + buf.size(), *p = last;
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'b' : 'B'; }
    do {
        *--p = '0' + static_cast<unsigned>(val & 0x1);
        val >>= 1;
    } while (val != 0);
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(p, last, s, fmt); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_oct(Ty val, StrTy& s, const fmt_state& fmt) {
    std::array<char, 23> buf;
    char *last = buf.data() + buf.size(), *p = last;
    do {
        *--p = '0' + static_cast<unsigned>(val & 0x7);
        val >>= 3;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = '0'; }
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(p, last, s, fmt); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_hex(Ty val, StrTy& s, const fmt_state& fmt) {
    std::array<char, 18> buf;
    char *last = buf.data() + buf.size(), *p = last;
    const char* digs = !(fmt.flags & fmt_flags::kUpperCase) ? "0123456789abcdef" : "0123456789ABCDEF";
    do {
        *--p = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
    unsigned len = static_cast<unsigned>(last - p);
    if (!!(fmt.flags & fmt_flags::kShowBase)) {
        len += 2, p -= 2;
        p[0] = '0', p[1] = !(fmt.flags & fmt_flags::kUpperCase) ? 'x' : 'X';
        if (fmt.width > len) {
            if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(p, last, s, fmt); }
            s.append(p, p + 2).append(fmt.width - len, '0');
            p += 2;
        }
    } else if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(p, last, s, fmt); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec_unsigned(Ty val, StrTy& s, const fmt_state& fmt) {
    std::array<char, 20> buf;
    char* last = buf.data() + buf.size();
    char* p = gen_digits(last, val);
    if (p == last) { *--p = '0'; }
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return fmt_adjusted(p, last, s, fmt); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_dec_signed(Ty val, StrTy& s, const fmt_state& fmt) {
    char sign = '+', show_sign = 0;
    if (val < 0) {
        sign = '-', show_sign = 1, val = -val;  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos) {
        show_sign = 1;
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignAlign) {
        show_sign = 1, sign = ' ';
    }

    std::array<char, 21> buf;
    char* last = buf.data() + buf.size();
    char* p = gen_digits(last, static_cast<typename std::make_unsigned<Ty>::type>(val));
    if (p == last) { *--p = '0'; }
    unsigned len = static_cast<unsigned>(last - p) + show_sign;
    if (fmt.width > len && !!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        if (show_sign) { s.push_back(sign); }
        s.append(fmt.width - len, '0');
    } else {
        if (show_sign) { *--p = sign; }
        if (fmt.width > len) { return fmt_adjusted(p, last, s, fmt); }
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

inline unsigned fmt_fp_exp10_len(const fp_exp10_format& fp10, fmt_flags flags, int prec) {
    return 1 + prec + (prec > 0 || !!(flags & fmt_flags::kShowPoint) ? 1 : 0) + (fp10.exp >= 100 ? 5 : 4);
}

inline unsigned fmt_fp_exp10_fixed_len(const fp_exp10_format& fp10, fmt_flags flags, int prec) {
    return 1 + std::max(fp10.exp, 0) + prec + (prec > 0 || !!(flags & fmt_flags::kShowPoint) ? 1 : 0);
}

template<typename StrTy>
StrTy& fmt_fp_exp10(const fp_exp10_format& fp10, StrTy& s, fmt_flags flags, int prec) {
    std::array<char, 25> buf;
    char *p_exp = buf.data() + 20, *last = p_exp + 4;
    char* p = scvt::gen_digits(p_exp, fp10.mantissa);

    // exponent
    int exp10 = fp10.exp;
    p_exp[0] = !(flags & fmt_flags::kUpperCase) ? 'e' : 'E';
    if (exp10 < 0) {
        exp10 = -exp10, p_exp[1] = '-';
    } else {
        p_exp[1] = '+';
    }
    if (exp10 >= 100) {
        static const int coef[] = {0, 100, 200, 300};
        const int t = (656 * exp10) >> 16;
        const char* d = &g_digits[static_cast<unsigned>(exp10 - coef[t])][0];
        p_exp[2] = '0' + static_cast<unsigned>(t), p_exp[3] = d[0], p_exp[4] = d[1];
        ++last;
    } else {
        p_exp[2] = g_digits[exp10][0], p_exp[3] = g_digits[exp10][1];
    }

    // integer part
    s.push_back(p != p_exp ? *p++ : '0');
    if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s.push_back('.'); }

    // fractional part + exponent
    int n_digs = static_cast<int>(p_exp - p);
    if (n_digs < prec) { return s.append(p, p_exp).append(prec - n_digs, '0').append(p_exp, last); }
    return s.append(p, last);
}

template<typename StrTy>
StrTy& fmt_fp_exp10_fixed(const fp_exp10_format& fp10, StrTy& s, fmt_flags flags, int prec) {
    std::array<char, 20> buf;
    char* last = buf.data() + buf.size();
    char* p = scvt::gen_digits(last, fp10.mantissa);

    int k = 1 + fp10.exp;
    if (k > 0) {
        // integer part
        int n_digs = static_cast<int>(last - p);
        if (n_digs <= k) {
            s.append(p, last).append(k - n_digs, '0');
            if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s.push_back('.'); }
            return s.append(prec, '0');
        }

        s.append(p, p + k);
        if (prec > 0) {
            s.push_back('.');
            return s.append(p + k, last).append(prec + k - n_digs, '0');
        }
        if (!!(flags & fmt_flags::kShowPoint)) { s.push_back('.'); }
        return s;
    }

    s.push_back('0');
    if (prec > 0) {
        s.push_back('.');
        return s.append(-k, '0').append(p, last);
    }
    if (!!(flags & fmt_flags::kShowPoint)) { s.push_back('.'); }
    return s;
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

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_floating_point<Ty>::value>>
StrTy& fmt_float(Ty val, StrTy& s, const fmt_state& fmt) {
    uint64_t mantissa = fp_traits<Ty>::to_u64(val);
    char sign = '+', show_sign = 0;
    if (mantissa & fp_traits<Ty>::kSignBit) {
        sign = '-', show_sign = 1;  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos) {
        show_sign = 1;
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignAlign) {
        show_sign = 1, sign = ' ';
    }

    // Binary exponent and mantissa
    int exp = static_cast<int>((mantissa & fp_traits<Ty>::kExpMask) >> fp_traits<Ty>::kBitsPerMantissa);
    mantissa &= fp_traits<Ty>::kMantissaMask;

    if (exp == fp_traits<Ty>::kExpMax) {
        std::array<char, 4> buf;
        char *p = buf.data(), *p0 = p;
        if (show_sign) { *p++ = sign; }
        if (!(fmt.flags & fmt_flags::kUpperCase)) {
            if (mantissa == 0) {  // infinity
                p[0] = 'i', p[1] = 'n', p[2] = 'f';
            } else {  // NaN
                p[0] = 'n', p[1] = 'a', p[2] = 'n';
            }
        } else {
            if (mantissa == 0) {  // infinity
                p[0] = 'I', p[1] = 'N', p[2] = 'F';
            } else {  // NaN
                p[0] = 'N', p[1] = 'A', p[2] = 'N';
            }
        }
        p += 3;
        if (fmt.width > static_cast<unsigned>(p - p0)) { return fmt_adjusted(p0, p, s, fmt); }
        return s.append(p0, p);
    }

    fp_exp10_format fp10;
    fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    int prec = fmt.prec;

    if (mantissa != 0 || exp > 0) {
        bool optimal = false;
        unsigned log = fp_traits<Ty>::kBitsPerMantissa;

        // Shift binary mantissa so the MSB bit is `1`
        if (exp > 0) {
            mantissa <<= 63 - fp_traits<Ty>::kBitsPerMantissa;
            mantissa |= 1ull << 63;
        } else {  // handle denormalized form
            log = ulog2(mantissa);
            mantissa <<= 63 - log;
            exp -= fp_traits<Ty>::kBitsPerMantissa - 1 - log;
        }

        if (prec < 0) {
            prec = g_default_prec[log];
            optimal = fp_fmt == fmt_flags::kDefault;
        } else {
            prec &= 0xffff;
        }

        // Obtain decimal power
        fp10.exp = g_pow_tbl.exp2to10[pow_table_t::kPow2Max - fp_traits<Ty>::kExpBias + exp];

        // Evaluate desired decimal mantissa length (its integer part)
        // Note: integer part of decimal mantissa is the digits to output,
        // fractional part of decimal mantissa is to be rounded and dropped
        if (fp_fmt == fmt_flags::kDefault) { prec = std::max(prec - 1, 0); }
        int n_digs = 1 + prec;                                    // desired digit count for scientific format
        if (fp_fmt == fmt_flags::kFixed) { n_digs += fp10.exp; }  // for fixed format

        if (n_digs >= 0) {
            n_digs = std::min<int>(n_digs, pow_table_t::kPrecLimit);

            // Calculate decimal mantissa representation :
            // Note: coefficients in `coef10to2` are normalized and belong [1, 2) range
            // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
            // but we drop the lowest 32 bits
            // To get the desired count of digits we move up the `coef10to2` table, it's equivalent to
            // multiplying the result by a certain power of 10
            const fp_m96_t& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp + n_digs - 1];
            uint128_t res128 = mul96x64_hi128(coef.m, mantissa);
            res128.hi += mantissa;  // apply implicit 1 term of normalized coefficient

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
            const uint64_t higher_bit = res128.hi < mantissa ? 1ull : 0ull;

            // Store the shift needed to align integer part of decimal mantissa with 64-bit boundary
            const unsigned shift = 63 + fp_traits<Ty>::kExpBias - exp - coef.exp;  // sum coefficient exponent as well
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
                fp10.mantissa = div64 + (res128.hi + mod64) / 10;
                unsigned mod = static_cast<unsigned>(res128.hi - 10 * fp10.mantissa);
                if (mod > 5 || (mod == 5 && (res128.lo != 0 || (fp10.mantissa & 1) != 0))) { ++fp10.mantissa; }
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

                if (fp_fmt != fmt_flags::kFixed && res128.hi >= g_pow_tbl.ten_pows[n_digs]) {
                    ++fp10.exp, err_mul += 10;  // one excess digit
                    // Remove one excess digit for scientific format, do 'nearest even' rounding
                    fp10.mantissa = res128.hi / 10;
                    err = res128.hi - 10 * fp10.mantissa;
                    if (err > 5 || (err == 5 && (res128.lo != 0 || (fp10.mantissa & 1) != 0))) {
                        ++fp10.mantissa, err -= 10;
                    }
                } else {
                    // Do 'nearest even' rounding
                    const uint64_t half = 1ull << 63;
                    const uint64_t frac = (res128.hi & 1) == 0 ? res128.lo + half - 1 : res128.lo + half;
                    fp10.mantissa = res128.hi;
                    if (frac < res128.lo) { ++fp10.mantissa, err = -1; }
                    if (fp10.mantissa >= g_pow_tbl.ten_pows[n_digs]) {
                        ++fp10.exp;  // one excess digit
                        if (fp_fmt != fmt_flags::kFixed) {
                            // Remove one excess digit for scientific format
                            // Note: `fp10.mantissa` is exact power of 10 in this case
                            fp10.mantissa /= 10;
                        }
                    }
                }
            }

            if (fp10.mantissa != 0) {
                if (optimal) {
                    // Evaluate acceptable error range to pass roundtrip test
                    assert(log + shift >= 30);
                    const unsigned shift2 = log + shift - 30;
                    int64_t delta_minus = (coef.m.hi >> shift2) | (1ull << (64 - shift2));
                    int64_t delta_plus = delta_minus;
                    if (exp > 1 && mantissa == 1ull << 63) { delta_plus >>= 1; }
                    err = (err << 32) | (res128.lo >> 32);

                    // Trim trailing unsignificant digits
                    const int64_t max_err_mul = delta_minus << 1;
                    while (true) {
                        uint64_t t = fp10.mantissa / 10;
                        unsigned mod = static_cast<unsigned>(fp10.mantissa - 10 * t);
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
                        --prec, fp10.mantissa = t;
                        if (*err_mul >= max_err_mul) {
                            prec -= remove_trailing_zeros(fp10.mantissa);
                            break;
                        }
                    }
                    if (prec < 0) { ++fp10.exp, prec = 0; }
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec + 4) {
                        fp_fmt = fmt_flags::kFixed, prec = std::max(prec - fp10.exp, 0);
                    }
                } else if (fp_fmt == fmt_flags::kDefault) {
                    const int prec0 = prec;
                    prec = n_digs - 1 - remove_trailing_zeros(fp10.mantissa);
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec0) {
                        fp_fmt = fmt_flags::kFixed, prec = std::max(prec - fp10.exp, 0);
                    }
                }

                goto not_zero_result;
            }
        }

        fp10.exp = 0;
    }

    if (fp_fmt == fmt_flags::kDefault) {
        fp_fmt = fmt_flags::kFixed, prec = 0;
    } else if (prec < 0) {
        prec = 0;
    } else {
        prec &= 0xffff;
    }

not_zero_result:
    if (fmt.width > 0) {
        unsigned len = show_sign;
        if (fp_fmt == fmt_flags::kFixed) {
            len += fmt_fp_exp10_fixed_len(fp10, fmt.flags, prec);
        } else {
            len += fmt_fp_exp10_len(fp10, fmt.flags, prec);
        }
        if (fmt.width > len) {
            if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
                unsigned left = 0, right = 0;
                switch (fmt.flags & fmt_flags::kAdjustField) {
                    case fmt_flags::kLeft: right = fmt.width - len; break;
                    case fmt_flags::kInternal: {
                        right = fmt.width - len, left = right >> 1;
                        right -= left;
                    } break;
                    default: left = fmt.width - len; break;
                }
                s.append(left, fmt.fill);
                if (show_sign) { s.push_back(sign); }
                if (fp_fmt == fmt_flags::kFixed) {
                    fmt_fp_exp10_fixed(fp10, s, fmt.flags, prec);
                } else {
                    fmt_fp_exp10(fp10, s, fmt.flags, prec);
                }
                s.append(right, fmt.fill);
                return s;
            }
            if (show_sign) { s.push_back(sign); }
            s.append(fmt.width - len, '0');
        } else if (show_sign) {
            s.push_back(sign);
        }
    } else if (show_sign) {
        s.push_back(sign);
    }

    if (fp_fmt == fmt_flags::kFixed) { return fmt_fp_exp10_fixed(fp10, s, fmt.flags, prec); }
    return fmt_fp_exp10(fp10, s, fmt.flags, prec);
}

}  // namespace scvt

#define SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(ty, to_string_func) \
    /*static*/ template<typename StrTy> \
    StrTy& string_converter<ty>::to_string(ty val, StrTy& s, const fmt_state& fmt) { \
        return scvt::to_string_func(val, s, fmt); \
    }

SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(int8_t, fmt_signed)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(int16_t, fmt_signed)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(int32_t, fmt_signed)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(int64_t, fmt_signed)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(uint8_t, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(uint16_t, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(uint32_t, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(uint64_t, fmt_unsigned)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(float, fmt_float)
SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER(double, fmt_float)
#undef SCVT_IMPLEMENT_STANDARD_TO_STRING_CONVERTER

template<typename StrTy>
/*static*/ StrTy& string_converter<char>::to_string(char val, StrTy& s, const fmt_state& fmt) {
    if (fmt.width > 1) { return scvt::fmt_adjusted(&val, &val + 1, s, fmt); }
    s.push_back(val);
    return s;
}

template<typename StrTy>
/*static*/ StrTy& string_converter<bool>::to_string(bool val, StrTy& s, const fmt_state& fmt) {
    std::string_view sval = !(fmt.flags & fmt_flags::kUpperCase) ?
                                (val ? std::string_view("true", 4) : std::string_view("false", 5)) :
                                (val ? std::string_view("TRUE", 4) : std::string_view("FALSE", 5));
    if (sval.size() < fmt.width) { return scvt::fmt_adjusted(sval.data(), sval.data() + sval.size(), s, fmt); }
    return s.append(sval.data(), sval.data() + sval.size());
}

}  // namespace util
