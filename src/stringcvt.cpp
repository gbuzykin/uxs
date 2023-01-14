#include "uxs/stringcvt_impl.h"

namespace uxs {
#if __cplusplus < 201703L
namespace detail {
UXS_EXPORT const char_tbl_t g_char_tbl;
}
#endif  // __cplusplus < 201703L
namespace scvt {

#if __cplusplus < 201703L && !(defined(_MSC_VER) && defined(_M_X64)) && !(defined(__GNUC__) && defined(__x86_64__))
const UXS_EXPORT ulog2_table_t g_ulog2_tbl;
#endif

const uint64_t msb64 = 1ull << 63;

struct uint128_t {
    uint64_t hi;
    uint64_t lo;
};

struct uint96_t {
    uint64_t hi;
    uint32_t lo;
};

struct fp_m96_t {
    uint96_t m;
    int exp;
};

template<typename TyH, typename TyL>
uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

inline uint128_t mul64x64(uint64_t x, uint64_t y, uint64_t bias = 0) {
#if defined(_MSC_VER) && defined(_M_X64)
    uint128_t result;
    result.lo = _umul128(x, y, &result.hi);
#    if _MSC_VER > 1800
    result.hi += _addcarry_u64(0, result.lo, bias, &result.lo);
#    else  // VS2013 compiler bug workaround
    result.lo += bias;
    if (result.lo < bias) { ++result.hi; }
#    endif
    return result;
#elif defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * static_cast<gcc_ints::uint128>(y) + bias;
    return uint128_t{static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    uint64_t mid1 = lo32(x) * hi32(y) + hi32(bias), mid2 = hi32(x) * lo32(y) + hi32(lower);
    uint64_t t = lo32(mid1) + lo32(mid2);
    return uint128_t{higher + hi32(mid1) + hi32(mid2) + hi32(t), make64(lo32(t), lo32(lower))};
#endif
}

inline uint128_t mul64x32(uint64_t x, uint32_t y, uint32_t bias = 0) {
#if (defined(_MSC_VER) && defined(_M_X64)) || (defined(__GNUC__) && defined(__x86_64__))
    return mul64x64(x, y, bias);
#else
    const uint64_t lower = lo32(x) * y + bias;
    const uint64_t higher = hi32(x) * y + hi32(lower);
    return uint128_t{hi32(higher), make64(lo32(higher), lo32(lower))};
#endif
}

inline uint128_t mul96x64_hi128(uint96_t x, uint64_t y) {
#if defined(_MSC_VER) && defined(_M_X64)
    uint64_t mid;
    uint64_t lo = _umul128(x.lo, y, &mid);
    return mul64x64(x.hi, y, (mid << 32) | (lo >> 32));
#elif defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x.lo) * static_cast<gcc_ints::uint128>(y);
    p = static_cast<gcc_ints::uint128>(x.hi) * static_cast<gcc_ints::uint128>(y) + (p >> 32);
    return uint128_t{static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    const uint64_t lower = lo32(y) * x.lo;
    return mul64x64(x.hi, y, hi32(y) * x.lo + hi32(lower));
#endif
}

#if defined(_MSC_VER) && defined(_M_X64)
using one_bit_t = unsigned char;
inline void add64_carry(uint64_t& a, uint64_t b, one_bit_t& carry) { carry = _addcarry_u64(carry, a, b, &a); }
inline void sub64_borrow(uint64_t& a, uint64_t b, one_bit_t& borrow) { borrow = _subborrow_u64(borrow, a, b, &a); }
#elif SCVT_HAS_BUILTIN(__builtin_addcll) && SCVT_HAS_BUILTIN(__builtin_subcll)
using one_bit_t = unsigned long long;
inline void add64_carry(uint64_t& a, uint64_t b, one_bit_t& carry) { a = __builtin_addcll(a, b, carry, &carry); }
inline void sub64_borrow(uint64_t& a, uint64_t b, one_bit_t& borrow) { a = __builtin_subcll(a, b, borrow, &borrow); }
#else
using one_bit_t = uint8_t;
inline void add64_carry(uint64_t& a, uint64_t b, one_bit_t& carry) {
    b += carry;
    a += b;
    carry = a < b || b < carry ? 1 : 0;
}
inline void sub64_borrow(uint64_t& a, uint64_t b, one_bit_t& borrow) {
    b += borrow;
    borrow = a < b || b < borrow ? 1 : 0;
    a -= b;
}
#endif

// --------------------------

inline unsigned bignum_multiply_by(const uint64_t* x, uint64_t* y, unsigned sz, unsigned mul) {
    uint128_t t{0, 0};
    for (unsigned k = sz; k > 0; --k) {
        t = mul64x32(x[k - 1], mul, static_cast<uint32_t>(lo32(t.hi)));
        y[k - 1] = t.lo;
    }
    return static_cast<unsigned>(lo32(t.hi));
}

inline bool bignum_less(const uint64_t* x, const uint64_t* y, unsigned sz) {
    for (unsigned k = 0; k < sz; ++k) {
        if (x[k] != y[k]) { return x[k] < y[k]; }
    }
    return false;
}

inline int bignum_2x_compare(const uint64_t* x, const uint64_t* y, unsigned sz) {
    if (!(x[0] & msb64)) { return -1; }
    for (unsigned k = 0; k < sz - 1; ++k) {
        uint64_t x2x = (x[k] << 1) | (x[k + 1] >> 63);
        if (x2x != y[k]) { return x2x > y[k] ? 1 : -1; }
    }
    uint64_t x2x = x[sz - 1] << 1;
    return x2x != y[sz - 1] ? (x2x > y[sz - 1] ? 1 : -1) : 0;
}

inline unsigned bignum_subtract(uint64_t* x, const uint64_t* y, unsigned sz) {
    one_bit_t borrow = 0;
    for (unsigned k = sz; k > 0; --k) { sub64_borrow(x[k - 1], y[k - 1], borrow); }
    return borrow;
}

inline unsigned bignum_subtract_2_p(uint64_t* x, const uint64_t* y, unsigned sz, unsigned p) {
    one_bit_t borrow = 0;
    sub64_borrow(x[sz - 1], y[sz - 1] << p, borrow);
    for (unsigned k = sz - 1; k > 0; --k) { sub64_borrow(x[k - 1], y[k - 1] << p | (y[k] >> (64 - p)), borrow); }
    return borrow;
}

static unsigned bignum_divmod(unsigned& integral, uint64_t* x, const uint64_t* y, unsigned sz) {
    assert(integral > 0);
    unsigned quotient = 0;
    if (integral > 4) {
        // Try to subtract 2^p-multiples of denominator from the numerator
        unsigned p = ulog2(integral - 1) - 1;  // estimate maximum `p`
        unsigned n_delta = (1 << p) + static_cast<unsigned>(y[0] >> (64 - p));
        do {
            integral -= n_delta + bignum_subtract_2_p(x, y, sz, p), quotient += 1 << p;
            while (integral <= n_delta) {
                if (--p == 0) { goto finish; }
                n_delta = (1 << p) + static_cast<unsigned>(y[0] >> (64 - p));
            }
        } while (p > 0);
    }

finish:
    while (integral > 1) { integral -= 1 + bignum_subtract(x, y, sz), ++quotient; }
    if (integral == 1 && !bignum_less(x, y, sz)) { integral -= 1 + bignum_subtract(x, y, sz), ++quotient; }
    return quotient;
}

static unsigned bignum_multiply_by_uint64(uint64_t* result, const uint64_t* x, unsigned sz, uint64_t mul) {
    uint128_t t = mul64x64(x[sz - 1], mul);
    uint64_t lower = t.lo;
    for (unsigned k = sz - 1; k > 0; --k) {
        t = mul64x64(x[k - 1], mul, t.hi);
        result[k] = t.lo;
    }
    result[0] = t.hi;
    if (lower != 0) {
        result[sz++] = lower;
    } else {
        while (sz > 0 && x[sz - 1] == 0) { --sz; }
    }
    return sz;
}

inline unsigned bignum_shift_left(uint64_t* x, unsigned sz, unsigned shift) {
    unsigned higher = static_cast<unsigned>(x[0] >> (64 - shift));
    for (unsigned n = 0; n < sz - 1; ++n) { x[n] = (x[n] << shift) | (x[n + 1] >> (64 - shift)); }
    x[sz - 1] <<= shift;
    return higher;
}

inline uint64_t bignum_shift_right(uint64_t* x, unsigned sz, unsigned shift, uint64_t higher) {
    uint64_t lower = x[sz - 1] << (64 - shift);
    for (unsigned k = sz; k > 1; --k) { x[k - 1] = (x[k - 1] >> shift) | (x[k - 2] << (64 - shift)); }
    x[0] = (x[0] >> shift) | (higher << (64 - shift));
    return lower;
}

// --------------------------

static uint128_t uint128_multiply_by_10(const uint128_t& v, int& exp) {
    const uint32_t mul = 10;
    uint128_t t = mul64x32(v.lo, mul);
    uint128_t result{0, t.lo};
    t = mul64x32(v.hi, mul, static_cast<uint32_t>(lo32(t.hi)));
    const uint64_t higher = lo32(t.hi);
    result.hi = t.lo;
    // re-normalize
    const unsigned shift = higher >= 8 ? 4 : 3;
    exp += shift;
    result.lo = (result.lo >> shift) | (result.hi << (64 - shift));
    result.hi = (result.hi >> shift) | (higher << (64 - shift));
    return result;
}

static uint128_t uint128_multiply_by_0_1(const uint128_t& v, int& exp) {
    const uint64_t mul = 0xccccccccccccccccull;
    one_bit_t carry = 0;
    uint128_t lower = mul64x64(v.lo, mul);
    uint128_t t = mul64x64(v.hi, mul, lower.hi);
    uint128_t higher{0, t.hi};
    lower.hi = t.lo;
    add64_carry(lower.hi, lower.lo, carry);
    add64_carry(higher.lo, t.lo, carry);
    add64_carry(higher.hi, t.hi, carry);
    exp -= 3;
    if (!(higher.hi & msb64)) {  // re-normalize
        higher.hi = (higher.hi << 1) | (higher.lo >> 63);
        higher.lo = (higher.lo << 1) | (lower.hi >> 63);
        --exp;
    }
    return higher;
}

static fp_m96_t uint128_to_fp_m96(const uint128_t& v, int exp) {
    one_bit_t carry = 0;
    uint128_t result{(v.hi << 1) | (v.lo >> 63), v.lo << 1};
    add64_carry(result.lo, 0x80000000, carry);
    return fp_m96_t{{result.hi + carry, static_cast<uint32_t>(hi32(result.lo))}, exp};
}

// --------------------------

struct pow_table_t {
    enum : int { kPrecLimit = 19, kShortMantissaLimit = 18 };
    enum : int { kPow10Max = 324 + kPrecLimit - 1 };
    enum : int { kBigPow10Max = 324 };
    enum : int { kPow2Bias = 1074, kPow2Max = 1023 };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, kPow2Bias + kPow2Max + 1> exp2to10;
    std::array<uint64_t, 2070> bigpow10;
    std::array<unsigned, kBigPow10Max + 1> bigpow10_offset;
    std::array<int64_t, 70> decimal_mul;
    pow_table_t();
};

pow_table_t::pow_table_t() {
    // 10^N -> 2^M power conversion table
    int exp = 0;
    uint128_t v{msb64, 0};
    coef10to2[kPow10Max] = fp_m96_t{{0, 0}, 0};
    for (unsigned n = 0; n < kPow10Max; ++n) {
        v = uint128_multiply_by_10(v, exp);
        coef10to2[kPow10Max + n + 1] = uint128_to_fp_m96(v, exp);
    }

    exp = 0, v = uint128_t{msb64, 0};
    for (unsigned n = 0; n < kPow10Max; ++n) {
        v = uint128_multiply_by_0_1(v, exp);
        coef10to2[kPow10Max - n - 1] = uint128_to_fp_m96(v, exp);
    }

    // 10^N big numbers and its offsets
    unsigned sz = 1, prev_sz = 0, total_sz = 1;
    bigpow10_offset[0] = 0;
    bigpow10[0] = 0x4000000000000000ull;
    for (unsigned n = 1; n < kBigPow10Max; ++n) {
        uint64_t* x = &bigpow10[total_sz];
        bigpow10_offset[n] = total_sz;
        unsigned integral = 10 + bignum_multiply_by(&bigpow10[prev_sz], x, sz, 10);
        unsigned shift = integral >= 16 ? 4 : 3;
        uint64_t lower = bignum_shift_right(x, sz, shift, integral);
        if (lower != 0) { x[sz++] = lower; }
        prev_sz = total_sz, total_sz += sz;
    }
    bigpow10_offset[kBigPow10Max] = total_sz;

    // 2^N -> 10^M power conversion index table
    for (int exp = -kPow2Bias; exp <= kPow2Max; ++exp) {
        auto it = std::lower_bound(coef10to2.begin(), coef10to2.end(), -exp,
                                   [](const fp_m96_t& el, int exp) { return el.exp < exp; });
        exp2to10[kPow2Bias + exp] = kPow10Max - static_cast<int>(it - coef10to2.begin());
    }

    // decimal-binary multipliers
    uint64_t mul = 1ull;
    for (uint32_t n = 0; n < decimal_mul.size(); n += 10, mul *= 10u) {
        decimal_mul[n] = mul << 32;
        for (uint32_t k = 1; k < 10; ++k) { decimal_mul[n + k] = (mul * k) << 32; }
    }
}

static const pow_table_t g_pow_tbl;

// maximal default precisions for each mantissa length
static const int g_default_prec[] = {2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,
                                     7,  8,  8,  8,  8,  9,  9,  9,  10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
                                     13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 17};

// minimal digit count for numbers 2^N <= x < 2^(N+1), N = 0, 1, 2, ...
UXS_EXPORT const unsigned g_exp2_digs[] = {1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,
                                           6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9,  10, 10, 10, 10,
                                           11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16,
                                           16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 19, 20};

#define UXS_SCVT_POWERS_OF_10(base) \
    base, (base)*10, (base)*100, (base)*1000, (base)*10000, (base)*100000, (base)*1000000, (base)*10000000, \
        (base)*100000000, (base)*1000000000
// powers of ten 10^N, N = 0, 1, 2, ...
UXS_EXPORT const uint64_t g_ten_pows[] = {UXS_SCVT_POWERS_OF_10(1ull), UXS_SCVT_POWERS_OF_10(10000000000ull)};

UXS_EXPORT const char g_digits[][2] = {
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

// --------------------------

uint64_t fp10_to_fp2(fp_m64_t fp10, bool zero_tail, const unsigned bpm, const int exp_max) NOEXCEPT {
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
    // It seems that exact halves detection is not possible without exact integral numbers tracking.
    // So we use some heuristic to detect exact halves. But there are reasons to believe that this approach can
    // be theoretically justified. If we take long enough range of bits after the point where we need to break the
    // series then analyzing this bits we can make the decision in which direction to round. The following bits:
    //               x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
    //               x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
    //              | needed    | to be truncated   | unknown   |
    // In our case we need `n_bits` left bits in `res128.hi`, all other bits are rounded and dropped.
    // To decide in which direction to round we use reliable bits after `n_bits`. Totally 96 bits are
    // reliable, because `coef10to2` has 96-bit precision. So, we use only `res128.hi` and higher 32-bit
    // part of `res128.lo`.
    one_bit_t carry = 0;
    const uint64_t half = 1ull << (63 - n_bits);
    // Drop unreliable bits and resolve the case of periodical `1`
    // Note: we do not need to reset lower 32-bit part of `res128.lo`, because it is ignored further
    add64_carry(res128.lo, 0x80000000, carry);
    // Do banker's rounding
    add64_carry(res128.hi,
                zero_tail && hi32(res128.lo) == 0 && ((res128.hi + carry) & (half << 1)) == 0 ? half - 1 : half, carry);
    if (carry) {  // overflow while rounding
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

// --------------------------

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
        if (q > std::numeric_limits<uint32_t>::max() / 100u) { break; }
        s += 2, n = q;
    }
    uint32_t q = rotr1(static_cast<uint32_t>(n) * mod_inv_5);
    if (q <= std::numeric_limits<uint32_t>::max() / 10u) { ++s, n = q; }
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
        if (q > std::numeric_limits<uint64_t>::max() / 100u) { break; }
        s += 2, n = q;
    }
    uint64_t q = rotr1(n * mod_inv_5);
    if (q <= std::numeric_limits<uint64_t>::max() / 10u) { ++s, n = q; }
    return s;
}

fp_dec_fmt_t::fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, const int exp_bias) NOEXCEPT {
    const int default_prec = 6;
    const fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    int prec = fmt.prec;
    fixed_ = fp_fmt == fmt_flags::kFixed;
    alternate_ = !!(fmt.flags & fmt_flags::kAlternate);
    if (fp2.m == 0 && fp2.exp == 0) {
        if (fp_fmt == fmt_flags::kDefault && prec < 0) {
            fixed_ = true, prec = alternate_ ? 1 : 0;
        } else {
            prec = prec < 0 ? default_prec : (prec & 0xffff);
            if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
                fixed_ = true, prec = alternate_ ? std::max(prec - 1, 1) : 0;
            }
        }
        significand_ = 0, digs_ = digs_buf_, exp_ = 0, prec_ = n_zeroes_ = prec, digs_buf_[0] = '0';
        return;
    }

    bool optimal = false;

    // Shift binary mantissa so the MSB bit is `1`
    if (fp2.exp > 0) {
        fp2.m <<= 63 - bpm;
        fp2.m |= msb64;
    } else {  // handle denormalized form
        const unsigned bpm0 = bpm;
        bpm = ulog2(fp2.m);
        fp2.m <<= 63 - bpm, fp2.exp -= bpm0 - bpm - 1;
    }

    if (fp_fmt == fmt_flags::kDefault && prec < 0) {
        prec = g_default_prec[bpm] - 1, optimal = true;
    } else {
        prec = prec < 0 ? default_prec : (prec & 0xffff);
        if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) { prec = std::max(prec - 1, 0); }
    }

    // Obtain decimal power
    assert(fp2.exp - exp_bias >= -pow_table_t::kPow2Bias && fp2.exp - exp_bias <= pow_table_t::kPow2Max);
    fp_m64_t fp10{0, g_pow_tbl.exp2to10[pow_table_t::kPow2Bias + fp2.exp - exp_bias]};

    // Evaluate desired decimal mantissa length (its integer part)
    // Note: integer part of decimal mantissa is the digits to output,
    // fractional part of decimal mantissa is to be rounded and dropped
    int n_digs = 1 + prec;                                    // desired digit count for scientific format
    if (fp_fmt == fmt_flags::kFixed) { n_digs += fp10.exp; }  // for fixed format

    if (n_digs < 0) {
        significand_ = 0, digs_ = digs_buf_, exp_ = 0, prec_ = n_zeroes_ = prec, digs_buf_[0] = '0';
        return;
    }

    if (n_digs > pow_table_t::kShortMantissaLimit) {  // Long decimal mantissa
        static std::array<unsigned, 10> dig_base = {UXS_SCVT_POWERS_OF_10(1)};

        // 12 uint64-s are enough to hold all (normalized!) 10^n, n <= 324, +1 to multiply by uint64
        std::array<uint64_t, 16> buf;  // so, 16 are enough for sure

        // The first digit can belong [1, 20) range, reserve one additional cell to store it.
        char* const p0 = n_digs + 1 > sizeof(digs_buf_) ? static_cast<char*>(std::malloc(n_digs + 1)) : digs_buf_;
        char* p = p0;

        uint64_t* num = &buf[0];  // numerator
        unsigned sz = 1;
        bool round_to_upper = false;
        const unsigned shift = 1 + g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp].exp + fp2.exp - exp_bias;
        if (fp10.exp <= 0) {
            unsigned dig = 0;
            if (fp10.exp < 0) {
                const unsigned* poffset = &g_pow_tbl.bigpow10_offset[-fp10.exp - 1];
                const uint64_t* mul = &g_pow_tbl.bigpow10[*poffset];  // multiplier bits
                sz = *(poffset + 1) - *poffset;                       // multiplier size

                one_bit_t carry = 0;
                sz = bignum_multiply_by_uint64(num, mul, sz, fp2.m);
                add64_carry(num[0], fp2.m, carry);  // handle implicit `1`
                dig = (carry << shift) | bignum_shift_left(num, sz, shift);
            } else {
                dig = static_cast<unsigned>(fp2.m >> (64 - shift)), num[0] = fp2.m << shift;
            }

            if (dig < 10u) {
                *p++ = '0' + dig;
            } else {
                ++fp10.exp, p = std::copy_n(g_digits[dig], 2, p);
                if (fp_fmt != fmt_flags::kFixed) { --n_digs; }
            }

            --n_digs;
            while (n_digs > 0) {
                unsigned n_dig_base = std::min(n_digs, 9);
                dig = bignum_multiply_by(num, num, sz, dig_base[n_dig_base]);
                char* p_next = p + n_dig_base;
                std::fill(p, gen_digits(p_next, dig), '0');
                p = p_next, n_digs -= n_dig_base;
                while (sz > 0 && num[sz - 1] == 0) { --sz; }
                if (sz == 0) { goto finish; }  // tail of zeroes
            }

            round_to_upper = num[0] > msb64 || (sz == 1 && num[0] == msb64 && (dig & 1) != 0);  // nearest even
        } else {
            const unsigned* poffset = &g_pow_tbl.bigpow10_offset[fp10.exp - 1];
            const uint64_t* denom = &g_pow_tbl.bigpow10[*poffset];  // denominator bits
            sz = *(poffset + 1) - *poffset;                         // denominator size

            unsigned integral = static_cast<unsigned>(fp2.m >> (63 - shift));
            num[0] = fp2.m << (shift + 1);
            std::fill_n(num + 1, sz - 1, 0);

            unsigned dig = bignum_divmod(integral, num, denom, sz);
            if (dig < 10u) {
                *p++ = '0' + dig;
            } else {
                ++fp10.exp, p = std::copy_n(g_digits[dig], 2, p);
                if (fp_fmt != fmt_flags::kFixed) { --n_digs; }
            }

            --n_digs;
            while (n_digs > 0) {
                unsigned n_dig_base = std::min(n_digs, 9);
                integral = (integral > 0 ? dig_base[n_dig_base] : 0) +
                           bignum_multiply_by(num, num, sz, dig_base[n_dig_base]);
                if (integral > 0) {
                    dig = bignum_divmod(integral, num, denom, sz);
                    char* p_next = p + n_dig_base;
                    std::fill(p, gen_digits(p_next, dig), '0');
                    p = p_next, n_digs -= n_dig_base;
                } else {
                    if (std::all_of(num, num + sz, [](uint64_t x) { return x == 0; })) {
                        goto finish;  // tail of zeroes
                    }
                    dig = 0, n_digs -= n_dig_base;
                    p = std::fill_n(p, n_dig_base, '0');
                }
            }

            if (integral == 0) {
                int result = bignum_2x_compare(num, denom, sz);
                round_to_upper = result > 0 || (result == 0 && (dig & 1) != 0);  // nearest even
            } else {
                round_to_upper = true;
            }
        }

        // `n_digs` is trailing zero count now, which are to be appended to digit buffer
        // Note: some trailing zeroes can already be in digit buffer
        // Invariant: `static_cast<unsigned>(p - p0) + n_digs = <total needed digit count>`

        if (round_to_upper) {  // round to upper value
            while (p != p0) {
                if (*(p - 1) != '9') {
                    ++*(p - 1);
                    break;
                }
                *--p = '0', ++n_digs;  // add trailing zero
            }
            if (p == p0) {  // overflow: the number is exact power of 10
                ++fp10.exp, *p = '1';
                if (fp_fmt != fmt_flags::kFixed) { --n_digs; }
            }
        }

    finish:
        // Note: `n_digs` is trailing zero count
        if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
            // Select format for number representation
            if (fp10.exp >= -4 && fp10.exp <= prec) { fixed_ = true, prec -= fp10.exp; }
            if (!alternate_) {  // trim trailing zeroes
                if (n_digs < prec) {
                    prec -= n_digs, n_digs = 0;
                    while (*(p - 1) == '0' && prec > 0) { --p, --prec; }
                } else {
                    n_digs -= prec, prec = 0;
                }
            }
        }

        significand_ = 0, digs_ = p0, exp_ = fp10.exp, prec_ = prec, n_zeroes_ = n_digs;
        return;
    }

    // Calculate decimal mantissa representation :
    // Note: coefficients in `coef10to2` are normalized and belong [1, 2) range
    // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
    // but we drop the lowest 32 bits
    // To get the desired count of digits we move up the `coef10to2` table, it's equivalent to
    // multiplying the result by a certain power of 10
    assert(n_digs - fp10.exp - 1 >= -pow_table_t::kPow10Max && n_digs - fp10.exp - 1 <= pow_table_t::kPow10Max);
    fp_m96_t coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp + n_digs - 1];
    uint128_t res128 = mul96x64_hi128(coef.m, fp2.m);

    // Do banker's or `nearest even` rounding :
    // It seems that exact halves detection is not possible without exact integral numbers tracking.
    // So we use some heuristic to detect exact halves. But there are reasons to believe that this approach can
    // be theoretically justified. If we take long enough range of bits after the point where we need to break the
    // series then analyzing this bits we can make the decision in which direction to round. The following bits:
    //               x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
    //               x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
    //              | needed    | to be truncated   | unknown   |
    // In our case we need known count of left bits `res128.hi`, which represent integer part of
    // decimal mantissa, all other bits are rounded and dropped. To decide in which direction to round
    // we use reliable bits after decimal mantissa. Totally 96 bits are reliable, because `coef10to2`
    // has 96-bit precision. So, we use only `res128.hi` and higher 32-bit part of `res128.lo`.
    one_bit_t carry = 0;
    // Drop unreliable bits and resolve the case of periodical `1`
    add64_carry(res128.lo, 0x80000000, carry);
    add64_carry(res128.hi, fp2.m, carry);  // apply implicit 1 term of normalized coefficient
    res128.lo &= ~((1ull << 32) - 1);      // zero lower 32-bit part of `res128.lo`

    // Store the shift needed to align integer part of decimal mantissa with 64-bit boundary
    const unsigned shift = 63 + exp_bias - fp2.exp - coef.exp;  // sum coefficient exponent as well
    // Note: resulting decimal mantissa has the form `C * 10^n_digs`, where `C` belongs [1, 20) range.
    // So, if `C >= 10` we get decimal mantissa with one excess digit. We should detect these cases
    // and divide decimal mantissa by 10 for scientific format

    int64_t err = 0;
    const int64_t* err_mul = g_pow_tbl.decimal_mul.data();
    // Align integer part of decimal mantissa with 64-bit boundary
    assert(shift != 0);
    if (shift < 64) {
        res128.lo = (res128.lo >> shift) | (res128.hi << (64 - shift));
        res128.hi = (res128.hi >> shift) | (static_cast<uint64_t>(carry) << (64 - shift));
    } else if (shift > 64) {
        res128.lo = (res128.hi >> (shift - 64)) | (static_cast<uint64_t>(carry) << (128 - shift));
        res128.hi = 0;
    } else {
        res128.lo = res128.hi;
        res128.hi = carry;
    }

    if (fp_fmt != fmt_flags::kFixed && res128.hi >= g_ten_pows[n_digs]) {
        ++fp10.exp, err_mul += 10;  // one excess digit
        // Remove one excess digit for scientific format, do 'nearest even' rounding
        fp10.m = res128.hi / 10u;
        err = res128.hi - 10u * fp10.m;
        if (err > 5 || (err == 5 && (res128.lo != 0 || (fp10.m & 1) != 0))) { ++fp10.m, err -= 10; }
    } else {
        // Do 'nearest even' rounding
        one_bit_t carry = 0;
        uint64_t frac = res128.lo;
        add64_carry(frac, msb64 - 1 + (res128.hi & 1), carry);
        fp10.m = res128.hi + carry, err = -static_cast<int64_t>(carry);
        if (fp10.m >= g_ten_pows[n_digs]) {
            ++fp10.exp;  // one excess digit
            if (fp_fmt != fmt_flags::kFixed) {
                // Remove one excess digit for scientific format
                // Note: `fp10.m` is exact power of 10 in this case
                fp10.m /= 10u;
            } else {
                ++n_digs;
            }
        }
    }

    if (fp10.m == 0) {
        significand_ = 0, digs_ = digs_buf_, exp_ = 0, prec_ = n_zeroes_ = prec, digs_buf_[0] = '0';
        return;
    }

    digs_ = nullptr, n_zeroes_ = 0;

    if (optimal) {
        // Select format for number representation
        if (fp10.exp >= -4 && fp10.exp <= prec) { fixed_ = true, prec -= fp10.exp; }

        // Evaluate acceptable error range to pass roundtrip test
        assert(bpm + shift >= 30);
        const unsigned shift2 = bpm + shift - 30;
        int64_t delta_minus = (coef.m.hi >> shift2) | (1ull << (64 - shift2));
        int64_t delta_plus = delta_minus;
        if (fp2.exp > 1 && fp2.m == msb64) { delta_plus >>= 1; }
        err = (err << 32) | (res128.lo >> 32);

        // Trim trailing insignificant digits
        int prec0 = prec;
        if (prec > 0) {
            do {
                uint64_t t = fp10.m / 10u;
                unsigned mod = static_cast<unsigned>(fp10.m - 10u * t);
                if (mod > 0) {
                    err += err_mul[mod];
                    err_mul += 10;
                    int64_t err2 = *err_mul - err;
                    assert(err >= 0 && err2 >= 0);
                    // If both round directions are acceptable, use banker's rounding
                    if (err < delta_plus) {
                        if (err2 < delta_minus && (err2 < err || (err2 == err && (t & 1) != 0))) { ++t, err = -err2; }
                    } else if (err2 < delta_minus) {
                        ++t, err = -err2;
                    } else {
                        significand_ = fp10.m, exp_ = fp10.exp, prec_ = prec;
                        return;
                    }
                } else {
                    err_mul += 10;
                }
                --prec, fp10.m = t;
            } while (prec > 0 && (*err_mul + err < delta_plus || *err_mul - err < delta_minus));
        }

        prec -= remove_trailing_zeros(fp10.m);
        if (n_digs + prec == prec0) {  // overflow while rounding
            ++fp10.exp;
            if (!fixed_) { ++prec; }
        }

        if (prec >= 0) {
        } else if (prec >= -4) {
            fp10.m *= g_ten_pows[-prec], prec = 0;
        } else {  // move back to scientific representation if it is shorter
            fixed_ = false, prec += fp10.exp;
        }

        if (alternate_ && prec == 0) { fp10.m *= 10u, prec = 1; }
        significand_ = fp10.m, exp_ = fp10.exp, prec_ = prec;
        return;
    }

    if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
        // Select format for number representation
        if (fp10.exp >= -4 && fp10.exp <= prec) { fixed_ = true, prec -= fp10.exp; }
        if (!alternate_) {
            prec -= remove_trailing_zeros(fp10.m);
            if (prec < 0) { fp10.m *= g_ten_pows[-prec], prec = 0; }
        } else if (prec == 0) {
            fp10.m *= 10u, prec = 1;
        }
    }

    significand_ = fp10.m, exp_ = fp10.exp, prec_ = prec;
}

template UXS_EXPORT uint32_t to_integer_limited(const char*, const char*, const char*&, uint32_t) NOEXCEPT;
template UXS_EXPORT uint64_t to_integer_limited(const char*, const char*, const char*&, uint64_t) NOEXCEPT;
template UXS_EXPORT uint64_t to_float_common(const char*, const char*, const char*& last, const unsigned,
                                             const int) NOEXCEPT;
template UXS_EXPORT bool to_bool(const char*, const char*, const char*& last) NOEXCEPT;

template UXS_EXPORT uint32_t to_integer_limited(const wchar_t*, const wchar_t*, const wchar_t*&, uint32_t) NOEXCEPT;
template UXS_EXPORT uint64_t to_integer_limited(const wchar_t*, const wchar_t*, const wchar_t*&, uint64_t) NOEXCEPT;
template UXS_EXPORT uint64_t to_float_common(const wchar_t*, const wchar_t*, const wchar_t*& last, const unsigned,
                                             const int) NOEXCEPT;
template UXS_EXPORT bool to_bool(const wchar_t*, const wchar_t*, const wchar_t*& last) NOEXCEPT;

template UXS_EXPORT void fmt_integral(membuffer&, int32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(membuffer&, int64_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(membuffer&, uint32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(membuffer&, uint64_t, const fmt_opts&);
template UXS_EXPORT void fmt_char(membuffer&, char, const fmt_opts&);
template UXS_EXPORT void fmt_float_common(membuffer&, uint64_t, const fmt_opts&, const unsigned, const int);
template UXS_EXPORT void fmt_bool(membuffer&, bool, const fmt_opts&);

template UXS_EXPORT void fmt_integral(wmembuffer&, int32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(wmembuffer&, int64_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(wmembuffer&, uint32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral(wmembuffer&, uint64_t, const fmt_opts&);
template UXS_EXPORT void fmt_char(wmembuffer&, char, const fmt_opts&);
template UXS_EXPORT void fmt_char(wmembuffer&, wchar_t, const fmt_opts&);
template UXS_EXPORT void fmt_float_common(wmembuffer&, uint64_t, const fmt_opts&, const unsigned, const int);
template UXS_EXPORT void fmt_bool(wmembuffer&, bool, const fmt_opts&);

}  // namespace scvt

}  // namespace uxs
