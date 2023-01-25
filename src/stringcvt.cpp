#include "uxs/stringcvt_impl.h"

namespace uxs {
#if __cplusplus < 201703L
namespace detail {
UXS_EXPORT const char_tbl_t g_char_tbl;
}
#endif  // __cplusplus < 201703L
namespace scvt {

UXS_EXPORT const fmt_opts g_default_opts;

#if __cplusplus < 201703L && \
    (SCVT_USE_COMPILER_128BIT_EXTENSIONS == 0 || \
     (!(defined(_MSC_VER) && defined(_M_X64)) && !(defined(__GNUC__) && defined(__x86_64__))))
const UXS_EXPORT ulog2_table_t g_ulog2_tbl;
#endif

inline uint64_t umul128(uint64_t x, uint64_t y, uint64_t bias, uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    uint64_t result_lo = _umul128(x, y, &result_hi);
#    if _MSC_VER <= 1800  // VS2013 compiler bug workaround
    result_lo += bias;
    if (result_lo < bias) { ++result_hi; }
#    else
    result_hi += _addcarry_u64(0, result_lo, bias, &result_lo);
#    endif
    return result_lo;
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * y + bias;
    result_hi = static_cast<uint64_t>(p >> 64);
    return static_cast<uint64_t>(p);
#else
    const uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    const uint64_t mid1 = lo32(x) * hi32(y) + hi32(bias), mid2 = hi32(x) * lo32(y) + hi32(lower);
    const uint64_t t = lo32(mid1) + lo32(mid2);
    result_hi = higher + hi32(mid1) + hi32(mid2) + hi32(t);
    return make64(lo32(t), lo32(lower));
#endif
}

inline uint64_t umul128(uint64_t x, uint64_t y, uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return _umul128(x, y, &result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * y;
    result_hi = static_cast<uint64_t>(p >> 64);
    return static_cast<uint64_t>(p);
#else
    return umul128(x, y, 0, result_hi);
#endif
}

inline uint64_t umul64x32(uint64_t x, uint32_t y, uint32_t bias, uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && \
    ((defined(_MSC_VER) && defined(_M_X64)) || (defined(__GNUC__) && defined(__x86_64__)))
    return umul128(x, y, bias, result_hi);
#else
    const uint64_t lower = lo32(x) * y + bias;
    const uint64_t higher = hi32(x) * y + hi32(lower);
    result_hi = hi32(higher);
    return make64(lo32(higher), lo32(lower));
#endif
}

#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && _MSC_VER > 1800 && defined(_M_X64)
// VS2013 compiler has a bug concerning these intrinsics
using one_bit_t = unsigned char;
inline one_bit_t add64_carry(uint64_t a, uint64_t b, uint64_t& c, one_bit_t carry = 0) {
    return _addcarry_u64(carry, a, b, &c);
}
inline one_bit_t sub64_borrow(uint64_t a, uint64_t b, uint64_t& c, one_bit_t borrow = 0) {
    return _subborrow_u64(borrow, a, b, &c);
}
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && SCVT_HAS_BUILTIN(__builtin_addcll) && \
    SCVT_HAS_BUILTIN(__builtin_subcll)
using one_bit_t = unsigned long long;
inline one_bit_t add64_carry(uint64_t a, uint64_t b, uint64_t& c, one_bit_t carry = 0) {
    c = __builtin_addcll(a, b, carry, &carry);
    return carry;
}
inline one_bit_t sub64_borrow(uint64_t a, uint64_t b, uint64_t& c, one_bit_t borrow = 0) {
    c = __builtin_subcll(a, b, borrow, &borrow);
    return borrow;
}
#else
using one_bit_t = uint8_t;
inline one_bit_t add64_carry(uint64_t a, uint64_t b, uint64_t& c, one_bit_t carry = 0) {
    b += carry;
    c = a + b;
    return c < b || b < carry;
}
inline one_bit_t sub64_borrow(uint64_t a, uint64_t b, uint64_t& c, one_bit_t borrow = 0) {
    b += borrow;
    c = a - b;
    return a < b || b < borrow;
}
#endif

struct uint128_t {
    uint64_t hi;
    uint64_t lo;
};

inline uint64_t udiv128(uint128_t x, uint64_t y) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && _MSC_VER >= 1920 && defined(_M_X64)
    uint64_t r;
    return _udiv128(x.hi, x.lo, y, &r);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    return static_cast<uint64_t>(((static_cast<gcc_ints::uint128>(x.hi) << 64) | x.lo) / y);
#else
    uint128_t denominator{y, 0};
    uint64_t quotient = 0, bit = msb64;
    for (int i = 0; i < 64; ++i, bit >>= 1) {
        denominator.lo = (denominator.hi << 63) | (denominator.lo >> 1), denominator.hi >>= 1;
        if (x.hi > denominator.hi || (x.hi == denominator.hi && x.lo >= denominator.lo)) {
            sub64_borrow(x.hi, denominator.hi, x.hi, sub64_borrow(x.lo, denominator.lo, x.lo));
            quotient |= bit;
        }
    }
    return quotient;
#endif
}

// --------------------------

// Is used by `accum_mantissa<>` template
uint64_t bignum_mul32(uint64_t* x, unsigned sz, uint32_t mul, uint32_t bias) {
    assert(sz > 0);
    uint64_t higher = bias;
    uint64_t* x0 = x;
    x += sz;
    do { --x, *x = umul64x32(*x, mul, static_cast<uint32_t>(higher), higher); } while (x != x0);
    return higher;
}

inline uint64_t bignum_mul(uint64_t* x, unsigned sz, uint64_t mul) {
    assert(sz > 0);
    uint64_t higher;
    uint64_t* x0 = x;
    x += sz - 1;
    *x = umul128(*x, mul, higher);
    while (x != x0) { --x, *x = umul128(*x, mul, higher, higher); }
    return higher;
}

inline uint64_t bignum_mul(uint64_t* x, const uint64_t* y, unsigned sz, uint64_t mul) {
    assert(sz > 0);
    uint64_t higher;
    uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    *x = umul128(*y, mul, higher);
    while (x != x0) { --x, --y, *x = umul128(*y, mul, higher, higher); }
    return higher;
}

inline uint64_t bignum_addmul(uint64_t* x, const uint64_t* y, unsigned sz, uint64_t mul) {
    assert(sz > 0);
    uint64_t higher;
    uint64_t* x0 = x;
    x += sz - 1, y += sz;
    one_bit_t carry = add64_carry(*x, umul128(*--y, mul, higher), *x);
    while (x != x0) { --x, carry = add64_carry(*x, umul128(*--y, mul, higher, higher), *x, carry); }
    return higher + carry;
}

inline void bignum_mul_vec(uint64_t* x, const uint64_t* y, unsigned sz_x, unsigned sz_y) {
    uint64_t* x0 = x;
    x += sz_x - 1;
    *x = bignum_mul(x + 1, y, sz_y, *x);
    while (x != x0) { --x, *x = bignum_addmul(x + 1, y, sz_y, *x); }
}

inline unsigned bignum_trim_unused(const uint64_t* x, unsigned sz) {
    while (sz > 0 && !x[sz - 1]) { --sz; }
    return sz;
}

inline uint64_t bignum_submul(uint64_t* x, const uint64_t* y, unsigned sz_x, unsigned sz, uint64_t mul) {
    // returns: x[0]...x[sz_x-1]000... - mul * y[0]...y[sz-1], MSB of y[0] is 1
    assert(sz > 0);
    one_bit_t borrow = 0;
    uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    uint64_t higher, lower = umul128(*y, mul, higher);
    if (sz > sz_x) {
        uint64_t* x1 = x0 + sz_x;
        borrow = sub64_borrow(0, lower, *x);
        while (x != x1) { --x, --y, borrow = sub64_borrow(0, umul128(*y, mul, higher, higher), *x, borrow); }
    } else {
        borrow = sub64_borrow(*x, lower, *x);
    }
    while (x != x0) { --x, --y, borrow = sub64_borrow(*x, umul128(*y, mul, higher, higher), *x, borrow); }
    return higher + borrow;
}

inline one_bit_t bignum_add(uint64_t* x, const uint64_t* y, unsigned sz) {
    assert(sz > 0);
    uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    one_bit_t carry = add64_carry(*x, *y, *x);
    while (x != x0) { --x, --y, carry = add64_carry(*x, *y, *x, carry); }
    return carry;
}

inline uint64_t bignum_divmod(uint64_t integral, uint64_t* x, const uint64_t* y, unsigned sz_x, unsigned sz) {
    // integral.x[0]...x[sz_x-1]000... / 0.y[0]...y[sz-1], MSB of y[0] is 1
    // returns: quotient, 0.x[0]...x[sz-1] - remainder
    assert(sz > 0);
    uint128_t num128{integral, sz_x ? x[0] : 0};
    if (sz_x > 1) { num128.hi += add64_carry(num128.lo, 1, num128.lo); }
    // Calculate quotient upper estimation.
    // In case if `num128.hi < 2^62` it is guaranteed, that q_est - q <= 1
    uint64_t quotient = udiv128(num128, y[0]);
    if (!quotient) { return 0; }
    // Subtract scaled denominator from numerator
    if (integral < bignum_submul(x, y, sz_x, sz, quotient)) {
        // In case of overflow just add one denominator
        bignum_add(x, y, sz), --quotient;
    }
    return quotient;
}

inline uint64_t shl128(uint64_t x_hi, uint64_t x_lo, unsigned shift) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return __shiftleft128(x_lo, x_hi, shift);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 x = ((static_cast<gcc_ints::uint128>(x_hi) << 64) | x_lo) << shift;
    return static_cast<uint64_t>(x >> 64);
#else
    return (x_hi << shift) | (x_lo >> (64 - shift));
#endif
}

inline uint64_t bignum_shift_left(uint64_t* x, unsigned sz, unsigned shift) {
    assert(sz > 0);
    uint64_t* x0 = x + sz - 1;
    uint64_t higher = *x >> (64 - shift);
    while (x != x0) { *x = shl128(*x, *(x + 1), shift), ++x; }
    *x <<= shift;
    return higher;
}

inline uint64_t shr128(uint64_t x_hi, uint64_t x_lo, unsigned shift) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return __shiftright128(x_lo, x_hi, shift);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 x = ((static_cast<gcc_ints::uint128>(x_hi) << 64) | x_lo) >> shift;
    return static_cast<uint64_t>(x);
#else
    return (x_hi << (64 - shift)) | (x_lo >> shift);
#endif
}

inline uint64_t bignum_shift_right(uint64_t higher, uint64_t* x, unsigned sz, unsigned shift) {
    assert(sz > 0);
    uint64_t* x0 = x;
    x += sz - 1;
    uint64_t lower = *x << (64 - shift);
    while (x != x0) { *x = shr128(*(x - 1), *x, shift), --x; }
    *x = shr128(higher, *x, shift);
    return lower;
}

// --------------------------

struct bignum_t {
    const uint64_t* x;
    unsigned sz;
    unsigned exp;
};

SCVT_CONSTEXPR_DATA int kBigpow10TblSize = 19;
inline bignum_t get_bigpow10(unsigned index) NOEXCEPT {
    static SCVT_CONSTEXPR_DATA uint64_t bigpow10[] = {
        0xde0b6b3a76400000, 0xc097ce7bc90715b3, 0x4b9f100000000000, 0xa70c3c40a64e6c51, 0x999090b65f67d924,
        0x90e40fbeea1d3a4a, 0xbc8955e946fe31cd, 0xcf66f634e1000000, 0xfb5878494ace3a5f, 0x04ab48a04065c723,
        0xe64cd7818d59b87f, 0xcddc800000000000, 0xda01ee641a708de9, 0xe80e6f4820cc9495, 0xd74baad03bc1d8d3,
        0xdffef8f2564c1a20, 0xbd176620a501fbff, 0xb650e5a93bc3d898, 0x54bea8f289011b2a, 0x4c7fb4df0948eb64,
        0x2be5f25148000000, 0xa402b9c5a8d3a6e7, 0x5f16206c9c6209a6, 0x39caef6ed62f905b, 0x618ba0b12f00d2f8,
        0x96516cc9f995eeb0, 0x5b82000000000000, 0x8e41ade9fbebc27d, 0x14588f13be847307, 0x23bd6a2059c002f5,
        0xcd10a54139faf1c0, 0x795ce6703d77ec02, 0xb333f64b07b8cc80, 0xf6c69a72a3989f5b, 0x8aad549e57273d45,
        0x0e4709b7db8059dc, 0xa12d31943dcb7f8b, 0x66e452a76f19d6c3, 0xc17402306dd3000b, 0xde51e2ec40000000,
        0xd60b3bd56a5586f1, 0x8a71e223d8d3b074, 0xcd9ad624ee401914, 0xbe07bacc405e71ea, 0x262cd4bda217dae6,
        0xcbdc138b54a044fe, 0x3cdb3fafb37e3b58, 0x2090000000000000, 0xb9a74a0637ce2ee1, 0x6d953e2bd7173692,
        0x88efb0037ac08bde, 0x64bd540844336e0e, 0xd9e8d18961ec51d5, 0xd6b92fc3f211b0ae, 0xb4afab8df71b51e2,
        0x4f169cad05aa8400, 0xa1075a24e4421730, 0xb24cf65b8612f81f, 0x9a37d253f9394bb6, 0xc373981b368c7a18,
        0x5fc328be2e4dd3f6, 0x9f9f25600cb180dc, 0x979dd3488adaa89f, 0x801d84bdedfd1f3c, 0x25abeb7900000000,
        0x8bab8eefb6409c1a, 0x1ad089b6c2f7548e, 0x25c7b885ba466e37, 0x71d7e631e70524e4, 0x06e597a9b2ce192a,
        0xd8226c00400b5974, 0x83e5c42032a16cf0, 0xac1c797472a3d307, 0xf2d617a2f8f895ee, 0xa440000000000000,
        0xf24a01a73cf2dccf, 0xbc633b39673c8cec, 0x3d9c44cd2f36917c, 0x74d896e89de4c050, 0xbca660710fe28056,
        0x0ca6e49c81a6228c, 0x81dc4118ac7ddf6a, 0x682cdb2a8844aae2, 0xe466cc72886c4435, 0x745c3172e3bd2000,
        0xd226fc195c6a2f8c, 0x73832eec6fff3111, 0xe2228cbf49612182, 0x75e7c7e8f2099e01, 0x9c349dc1c3c70f53,
        0xa136affbe9dcf0d8, 0x6f9e207c13bc9de8, 0x367c9c158d628df8, 0x123e3a841ff86cb5, 0x59c1c11857fb99ef,
        0x56eb5c0800000000, 0xb6472e511c81471d, 0xe0133fe4adf8e952, 0x5dede5861d36aec1, 0x27824ce2b2eeb577,
        0x648cc0f46f5d1171, 0x7d957a1ec7ca51f6, 0x7b9741e759686ce7, 0xc7141c08562bf86c, 0x67b45f53126287c5,
        0x8b1dce977ea8a308, 0xc1a4b754c3086912, 0xb200000000000000, 0x9e19db92b4e31ba9, 0x6c07a2c26a8346d1,
        0x4944d9f52cd0dec2, 0xaefc86c50710cdc9, 0x5c6828c645cf642e, 0x349925d58abedce8, 0xd0a64682c7e0a4e7,
        0xb0d307330110b52c, 0x6f3046d2181f9e82, 0x9c712a8d39f55391, 0x41e82749e529f8e2, 0x5512e72b36b88000,
        0x892179be91d43a43, 0x88083f8943a1148c, 0xd69283788730f71b, 0x5a7dd3dd4576e805, 0x981e98226ca211b8,
        0x75341fde13cd0ade, 0x7bdb2dc9c078d6e6, 0x17f150e1d35cf5e7, 0xda153ab760d85d36, 0x4d55f7b5df842d22,
        0xf5ed5e5a949d844e, 0x5023ac542102c3de, 0xb953b92000000000};
    static SCVT_CONSTEXPR_DATA unsigned bigpow10_offset[kBigpow10TblSize + 1] = {
        0, 1, 3, 5, 8, 12, 16, 21, 27, 33, 40, 48, 56, 65, 75, 85, 96, 108, 120, 133};
    static SCVT_CONSTEXPR_DATA unsigned bigpow10_exp[kBigpow10TblSize] = {
        59, 119, 179, 239, 298, 358, 418, 478, 538, 597, 657, 717, 777, 837, 896, 956, 1016, 1076, 1136};
    assert(index < kBigpow10TblSize);
    const unsigned* const offset = &bigpow10_offset[index];
    return bignum_t{bigpow10 + offset[0], offset[1] - offset[0], bigpow10_exp[index]};
}

// --------------------------

SCVT_CONSTEXPR_DATA int kPow10Max = 328;

struct uint96_t {
    uint64_t hi;
    uint32_t lo;
};

struct coef10to2_t {
    uint96_t m[2 * kPow10Max + 1];
    coef10to2_t();
};

static const coef10to2_t g_coef10to2;

inline uint128_t umul128_2(uint64_t x, uint64_t y, uint64_t bias) {
    uint128_t result;
    result.lo = umul128(x, y, bias, result.hi);
    return result;
}

inline uint128_t umul64x32_2(uint64_t x, uint32_t y, uint32_t bias) {
    uint128_t result;
    result.lo = umul64x32(x, y, bias, result.hi);
    return result;
}

inline uint128_t umul96x32(uint96_t x, uint32_t y) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    uint128_t result;
    const uint64_t lower = static_cast<uint64_t>(x.lo) * mul;
    result.lo = make64(umul128(x.hi, static_cast<uint64_t>(y) << 32, lower & (~0ull << 32), result.hi), lo32(lower));
    return result;
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = ((static_cast<gcc_ints::uint128>(x.hi) << 32) | x.lo) * y;
    return uint128_t{static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    uint96_t result;
    const uint64_t lower = static_cast<uint64_t>(x.lo) * mul;
    const uint64_t mid = lo32(x.hi) * y + hi32(lower);
    return uint96_t{hi32(x.hi) * y + hi32(mid), make64(lo32(mid), lo32(lower))};
#endif
}

inline uint128_t umul96x64_higher128(uint96_t x, uint64_t y) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    uint64_t mid;
    uint64_t lo = _umul128(x.lo, y, &mid);
    return umul128_2(x.hi, y, (mid << 32) | (lo >> 32));
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x.lo) * y;
    p = static_cast<gcc_ints::uint128>(x.hi) * y + (p >> 32);
    return uint128_t{static_cast<uint64_t>(p >> 64), static_cast<uint64_t>(p)};
#else
    const uint64_t lower = lo32(y) * x.lo;
    return umul128_2(x.hi, y, hi32(y) * x.lo + hi32(lower));
#endif
}

static uint128_t uint128_multiply_by_10(const uint128_t& v) {
    const uint32_t mul = 10;
    uint128_t t = umul64x32_2(v.lo, mul, 0);
    uint128_t result{0, t.lo};
    t = umul64x32_2(v.hi, mul, static_cast<uint32_t>(lo32(t.hi)));
    const uint64_t higher = lo32(t.hi);
    result.hi = t.lo;
    // re-normalize
    const unsigned shift = higher >= 8 ? 4 : 3;
    result.lo = (result.lo >> shift) | (result.hi << (64 - shift));
    result.hi = (result.hi >> shift) | (higher << (64 - shift));
    return result;
}

static uint128_t uint128_multiply_by_0_1(const uint128_t& v) {
    const uint64_t mul = 0xccccccccccccccccull;
    uint128_t lower = umul128_2(v.lo, mul, 0);
    uint128_t t = umul128_2(v.hi, mul, lower.hi);
    uint128_t higher{0, t.hi};
    lower.hi = t.lo;
    one_bit_t carry = add64_carry(lower.hi, lower.lo, lower.hi);
    carry = add64_carry(higher.lo, t.lo, higher.lo, carry);
    add64_carry(higher.hi, t.hi, higher.hi, carry);
    if (!(higher.hi & msb64)) {  // re-normalize
        higher.hi = (higher.hi << 1) | (higher.lo >> 63);
        higher.lo = (higher.lo << 1) | (lower.hi >> 63);
    }
    return higher;
}

static uint96_t uint128_to_uint96(const uint128_t& v) {
    uint128_t result{(v.hi << 1) | (v.lo >> 63), v.lo << 1};
    one_bit_t carry = add64_carry(result.lo, 0x80000000, result.lo);
    return uint96_t{result.hi + carry, static_cast<uint32_t>(hi32(result.lo))};
}

coef10to2_t::coef10to2_t() {
    // 10^N -> 2^M power conversion table
    uint128_t v{msb64, 0};
    m[kPow10Max] = uint96_t{0, 0};
    for (unsigned n = 0; n < kPow10Max; ++n) {
        v = uint128_multiply_by_10(v);
        m[kPow10Max + n + 1] = uint128_to_uint96(v);
    }
    v = uint128_t{msb64, 0};
    for (unsigned n = 0; n < kPow10Max; ++n) {
        v = uint128_multiply_by_0_1(v);
        m[kPow10Max - n - 1] = uint128_to_uint96(v);
    }
}

// --------------------------

inline int exp10to2(int exp) {
    SCVT_CONSTEXPR_DATA int64_t ln10_ln2 = 0x35269e12f;  // 2^32 * ln(10) / ln(2)
    return static_cast<int>(hi32(ln10_ln2 * exp));
}

uint64_t fp10_to_fp2(fp10_t& fp10, const unsigned bpm, const int exp_max) NOEXCEPT {
    uint64_t* m10 = &fp10.bits[kMaxFp10MantissaSize - fp10.bits_used];
    // Note, that decimal mantissa can contain up to 772 digits. So, all numbers with
    // powers less than -772 - 324 = -1096 are zeroes in fact. We round this power to -1100
    if (*m10 == 0 || fp10.exp < -1100) {
        return 0;                                      // zero
    } else if (fp10.exp > 310) {                       // too big power even for one specified digit
        return static_cast<uint64_t>(exp_max) << bpm;  // infinity
    }

    const int exp_aligned = fp10.exp - ((kDigsPer64 * 1000000 + fp10.exp) %
                                        kDigsPer64);  // lower kDigsPer64-aligned decimal exponent

    unsigned sz_num = fp10.bits_used;
    const uint64_t mul10 = get_pow10(fp10.exp - exp_aligned);
    if (mul10 != 1) {
        const uint64_t higher = bignum_mul(m10, sz_num, mul10);
        if (higher) { *--m10 = higher, ++sz_num; }
    }

    // Obtain binary exponent
    const int exp_bias = exp_max >> 1;
    const int log = ulog2(*m10);
    int exp2 = exp_bias + log + (static_cast<int>(sz_num - 1) << 6 /* *64 */);

    // Align numerator so 2 left bits are zero (reserved)
    if (log < 61) {
        bignum_shift_left(m10, sz_num, 61 - log);
    } else if (log > 61) {
        const uint64_t lower = bignum_shift_right(0, m10, sz_num, log - 61);
        if (lower) { m10[sz_num++] = lower; }
    }

    uint64_t m;
    --sz_num;  // count only numerator uint64_t-s with index > 0
    if (exp_aligned < 0) {
        const int index0 = (-exp_aligned / kDigsPer64) - 1;
        int index = std::min<int>(index0, kBigpow10TblSize - 1);
        bignum_t denominator = get_bigpow10(index);

        uint64_t big_denominator[kMaxFp10MantissaSize + 1];  // all powers >= -1100 (aligned to -1116) will fit
        if (index < index0) {
            // Calculate big denominator multiplying powers of 10 from table
            std::memcpy(big_denominator, denominator.x, denominator.sz * sizeof(uint64_t));
            do {
                const int index2 = std::min<int>(index0 - index - 1, kBigpow10TblSize - 1);
                const bignum_t denominator2 = get_bigpow10(index2);
                bignum_mul_vec(big_denominator, denominator2.x, denominator.sz, denominator2.sz);
                denominator.sz += denominator2.sz;
                if (!big_denominator[denominator.sz - 1]) { --denominator.sz; }
                denominator.exp += 1 + denominator2.exp, index += index2 + 1;
            } while (index < index0);
            const unsigned shift = 63 - ulog2(big_denominator[0]);
            if (shift > 0) {
                bignum_shift_left(big_denominator, denominator.sz, shift);
                if (!big_denominator[denominator.sz - 1]) { --denominator.sz; }
                denominator.exp -= shift;
            }
            denominator.x = big_denominator;
        }
        // After `bignum_divmod` the result can have 1 or 2 left zero bits
        m = bignum_divmod(m10[0], m10 + 1, denominator.x, sz_num, denominator.sz);
        sz_num = std::max(sz_num, denominator.sz), exp2 -= denominator.exp;
    } else {
        if (exp_aligned > 0) {
            const int index = (exp_aligned / kDigsPer64) - 1;
            if (index >= kBigpow10TblSize) { return static_cast<uint64_t>(exp_max) << bpm; }  // infinity
            const bignum_t multiplier = get_bigpow10(index);
            sz_num = bignum_trim_unused(m10 + 1, sz_num);
            bignum_mul_vec(m10, multiplier.x, sz_num + 1, multiplier.sz);
            sz_num += multiplier.sz, exp2 += 1 + multiplier.exp;
        }
        // After multiplication the result can have 2 or 3 left zero bits,
        // so shift it left by 1 bit
        m = m10[0] << 1;
    }

    // Align mantissa so only 1 left bit is zero
    if (!(m & (1ull << 62))) { m <<= 1, --exp2; }

    if (exp2 >= exp_max) {
        return static_cast<uint64_t>(exp_max) << bpm;  // infinity
    } else if (exp2 < -static_cast<int>(bpm)) {
        return 0;  // zero
    }

    // When `exp2 <= 0` mantissa will be denormalized further, so store the real mantissa length
    const unsigned n_bits = exp2 > 0 ? 1 + bpm : bpm + exp2;

    // Do banker's or `nearest even` rounding
    const uint64_t lsb = 1ull << (63 - n_bits);
    const uint64_t half = lsb >> 1;
    const uint64_t frac = m & (lsb - 1);
    m >>= 63 - n_bits;  // shift mantissa to the right position
    if (frac > half || (frac == half && (!fp10.zero_tail || (m & 1) || bignum_trim_unused(m10 + 1, sz_num)))) {
        ++m;                         // round to upper
        if (m & (1ull << n_bits)) {  // overflow
            // Note: the value can become normalized if `exp == 0` or infinity if `exp == exp_max - 1`
            // Note: in case of overflow mantissa will be zero
            ++exp2;
        }
    }

    // Compose floating point value
    if (exp2 <= 0) { return m; }                                              // denormalized
    return (static_cast<uint64_t>(exp2) << bpm) | (m & ((1ull << bpm) - 1));  // normalized
}

// --------------------------

inline int exp2to10(int exp) {
    SCVT_CONSTEXPR_DATA int64_t ln2_ln10 = 0x4d104d42;  // 2^32 * ln(2) / ln(10)
    return static_cast<int>(hi32(ln2_ln10 * exp));
}

// Compilers should be able to optimize this into the ror instruction
inline uint64_t rotr1(uint64_t n) { return (n >> 1) | (n << 63); }
inline uint64_t rotr2(uint64_t n) { return (n >> 2) | (n << 62); }

// Removes trailing zeros and returns the number of zeros removed
inline int remove_trailing_zeros(uint64_t& n) {
    int s = 0;
    SCVT_CONSTEXPR_DATA uint64_t mod_inv_5 = 0xcccccccccccccccd;
    SCVT_CONSTEXPR_DATA uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;
    while (true) {
        uint64_t q = rotr2(n * mod_inv_25);
        if (q > std::numeric_limits<uint64_t>::max() / 100u) { break; }
        s += 2, n = q;
    }
    uint64_t q = rotr1(n * mod_inv_5);
    if (q <= std::numeric_limits<uint64_t>::max() / 10u) { ++s, n = q; }
    return s;
}

fp_dec_fmt_t::fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, const int exp_bias) NOEXCEPT
    : significand_(0),
      prec_(fmt.prec),
      n_zeroes_(0),
      alternate_(!!(fmt.flags & fmt_flags::kAlternate)) {
    const int default_prec = 6;
    const fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    fixed_ = fp_fmt == fmt_flags::kFixed;
    if (fp2.m == 0 && fp2.exp == 0) {
        if (fp_fmt == fmt_flags::kDefault && prec_ < 0) {
            fixed_ = true, prec_ = alternate_ ? 1 : 0;
        } else {
            prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
            if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
                fixed_ = true, prec_ = alternate_ ? std::max(prec_ - 1, 1) : 0;
            }
        }
        exp_ = 0, n_zeroes_ = prec_ + 1;
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

    // maximal default precisions for each mantissa length
    static SCVT_CONSTEXPR_DATA int g_default_prec[] = {
        2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9, 10,
        10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 17};

    if (fp_fmt == fmt_flags::kDefault && prec_ < 0) {
        prec_ = g_default_prec[bpm] - 1, optimal = true;
    } else {
        prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
        if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) { prec_ = std::max(prec_ - 1, 0); }
    }

    // Obtain decimal power
    fp2.exp -= exp_bias;
    exp_ = exp2to10(fp2.exp);

    // Evaluate desired decimal mantissa length (its integer part)
    // Note: integer part of decimal mantissa is the digits to output,
    // fractional part of decimal mantissa is to be rounded and dropped
    int n_digs = 1 + prec_;                               // desired digit count for scientific format
    if (fp_fmt == fmt_flags::kFixed) { n_digs += exp_; }  // for fixed format

    if (n_digs < 0) {
        exp_ = 0, n_zeroes_ = prec_ + 1;
        return;
    } else if (n_digs >= kDigsPer64) {  // long decimal mantissa
        format_long_decimal(fp2, n_digs, fp_fmt);
        return;
    } else if (!optimal) {  // short decimal mantissa
        format_short_decimal(fp2, n_digs, fp_fmt);
        return;
    }

    // Calculate decimal mantissa representation :
    // Note: coefficients in `coef10to2_t` are normalized and belong [1, 2) range
    // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 160-bit result,
    // but we drop the lowest 32 bits
    // To get the desired count of digits we move up the `coef10to2_t` table, it's equivalent to
    // multiplying the result by a certain power of 10
    assert(n_digs - exp_ - 1 >= -kPow10Max && n_digs - exp_ - 1 <= kPow10Max);
    uint96_t coef = g_coef10to2.m[kPow10Max - exp_ + n_digs - 1];
    uint128_t res128 = umul96x64_higher128(coef, fp2.m);

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
    // we use reliable bits after decimal mantissa. Totally 96 bits are reliable, because `coef10to2_t`
    // has 96-bit precision. So, we use only `res128.hi` and higher 32-bit part of `res128.lo`.
    // Drop unreliable bits and resolve the case of periodical `1`
    one_bit_t carry = add64_carry(res128.lo, 0x80000000, res128.lo);
    carry = add64_carry(res128.hi, fp2.m, res128.hi, carry);  // apply implicit 1 term of normalized coefficient
    res128.lo &= ~((1ull << 32) - 1);                         // zero lower 32-bit part of `res128.lo`

    // Store the shift needed to align integer part of decimal mantissa with 64-bit boundary
    const unsigned shift = 63 - fp2.exp - exp10to2(n_digs - exp_ - 1);  // sum coefficient exponent as well
    // Note: resulting decimal mantissa has the form `C * 10^n_digs`, where `C` belongs [1, 20) range.
    // So, if `C >= 10` we get decimal mantissa with one excess digit. We should detect these cases
    // and divide decimal mantissa by 10 for scientific format

    // decimal-binary multipliers
    static SCVT_CONSTEXPR_DATA int64_t g_decimal_mul[70] = {
        0x100000000,      0x100000000,      0x200000000,      0x300000000,      0x400000000,      0x500000000,
        0x600000000,      0x700000000,      0x800000000,      0x900000000,      0xa00000000,      0xa00000000,
        0x1400000000,     0x1e00000000,     0x2800000000,     0x3200000000,     0x3c00000000,     0x4600000000,
        0x5000000000,     0x5a00000000,     0x6400000000,     0x6400000000,     0xc800000000,     0x12c00000000,
        0x19000000000,    0x1f400000000,    0x25800000000,    0x2bc00000000,    0x32000000000,    0x38400000000,
        0x3e800000000,    0x3e800000000,    0x7d000000000,    0xbb800000000,    0xfa000000000,    0x138800000000,
        0x177000000000,   0x1b5800000000,   0x1f4000000000,   0x232800000000,   0x271000000000,   0x271000000000,
        0x4e2000000000,   0x753000000000,   0x9c4000000000,   0xc35000000000,   0xea6000000000,   0x1117000000000,
        0x1388000000000,  0x15f9000000000,  0x186a000000000,  0x186a000000000,  0x30d4000000000,  0x493e000000000,
        0x61a8000000000,  0x7a12000000000,  0x927c000000000,  0xaae6000000000,  0xc350000000000,  0xdbba000000000,
        0xf424000000000,  0xf424000000000,  0x1e848000000000, 0x2dc6c000000000, 0x3d090000000000, 0x4c4b4000000000,
        0x5b8d8000000000, 0x6acfc000000000, 0x7a120000000000, 0x89544000000000};

    int64_t err = 0;
    const int64_t* err_mul = g_decimal_mul;
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

    if (fp_fmt != fmt_flags::kFixed && res128.hi >= get_pow10(n_digs)) {
        ++exp_, err_mul += 10;  // one excess digit
        // Remove one excess digit for scientific format, do 'nearest even' rounding
        significand_ = res128.hi / 10u;
        err = res128.hi - 10u * significand_;
        if (err > 5 || (err == 5 && (res128.lo != 0 || (significand_ & 1) != 0))) { ++significand_, err -= 10; }
    } else {
        // Do 'nearest even' rounding
        uint64_t frac = res128.lo;
        one_bit_t carry = add64_carry(frac, msb64 - 1 + (res128.hi & 1), frac);
        significand_ = res128.hi + carry, err = -static_cast<int64_t>(carry);
        if (significand_ >= get_pow10(n_digs)) {
            ++exp_;  // one excess digit
            if (fp_fmt != fmt_flags::kFixed) {
                // Remove one excess digit for scientific format
                // Note: `significand_` is exact power of 10 in this case
                significand_ /= 10u;
            } else {
                ++n_digs;
            }
        }
    }

    if (significand_ == 0) {
        exp_ = 0, n_zeroes_ = prec_ + 1;
        return;
    }

    // Select format for number representation
    if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }

    // Evaluate acceptable error range to pass roundtrip test
    assert(bpm + shift >= 30);
    const unsigned shift2 = bpm + shift - 30;
    int64_t delta_minus = (coef.hi >> shift2) | (1ull << (64 - shift2));
    int64_t delta_plus = delta_minus;
    if (fp2.exp > 1 - exp_bias && fp2.m == msb64) { delta_plus >>= 1; }
    err = (err << 32) | (res128.lo >> 32);

    // Trim trailing insignificant digits
    int prec0 = prec_;
    if (prec_ > 0) {
        do {
            uint64_t t = significand_ / 10u;
            unsigned mod = static_cast<unsigned>(significand_ - 10u * t);
            if (mod > 0) {
                err += err_mul[mod];
                err_mul += 10;
                int64_t err2 = *err_mul - err;
                assert(err >= 0 && err2 >= 0);
                if (err < delta_plus) {
                    if (err2 < err) { ++t, err = -err2; }
                } else if (err2 < delta_minus) {
                    ++t, err = -err2;
                } else {
                    return;
                }
            } else {
                err_mul += 10;
            }
            --prec_, significand_ = t;
        } while (prec_ > 0 && (*err_mul + err < delta_plus || *err_mul - err < delta_minus));
    }

    prec_ -= remove_trailing_zeros(significand_);
    if (n_digs + prec_ == prec0) {  // overflow while rounding
        ++exp_;
        if (!fixed_) { ++prec_; }
    }

    if (prec_ >= 0) {
    } else if (prec_ >= -4) {
        significand_ *= get_pow10(-prec_), prec_ = 0;
    } else {  // move back to scientific representation if it is shorter
        fixed_ = false, prec_ += exp_;
    }

    if (alternate_ && prec_ == 0) { significand_ *= 10u, prec_ = 1; }
}

void fp_dec_fmt_t::format_short_decimal(const fp_m64_t& fp2, int n_digs, const fmt_flags fp_fmt) NOEXCEPT {
    assert(n_digs < kDigsPer64);
    ++n_digs;  // one additional digit for rounding

    // `kMaxPow10Size` uint64-s are enough to hold all (normalized!) 10^n, n <= 324 + kDigsPer64
    uint64_t num[kMaxPow10Size + 1];  // +1 to multiply by uint64
    unsigned sz_num = 1;

    const int exp_aligned = kDigsPer64 + exp_ - n_digs -
                            ((kDigsPer64 * 1000000 + exp_ - n_digs) %
                             kDigsPer64);  // upper kDigsPer64-aligned decimal exponent
    const uint64_t mul10 = get_pow10(exp_aligned - exp_ + n_digs - 1);

    auto mul_and_shift = [&num, &sz_num](uint64_t m, uint64_t mul, int shift) {
        uint64_t digs;
        num[0] = umul128(m, mul, digs);
        if (shift > 0) {
            digs = shl128(digs, num[0], shift), num[0] <<= shift;
        } else if (shift < 0) {
            num[1] = num[0] << (64 + shift), num[0] = shr128(digs, num[0], -shift), digs >>= -shift, ++sz_num;
        }
        return digs;
    };

    uint64_t digs;
    if (exp_aligned > 0) {
        const bignum_t denominator = get_bigpow10((exp_aligned / kDigsPer64) - 1);
        digs = bignum_divmod(mul_and_shift(fp2.m, mul10, fp2.exp - denominator.exp), num, denominator.x, sz_num,
                             denominator.sz);
        if (digs && sz_num < denominator.sz) { sz_num = denominator.sz; }
    } else if (exp_aligned < 0) {
        const bignum_t multiplier = get_bigpow10((-exp_aligned / kDigsPer64) - 1);
        const int shift = 2 + fp2.exp + multiplier.exp;
        sz_num += multiplier.sz;
        num[0] = bignum_mul(num + 1, multiplier.x, sz_num - 1, fp2.m);
        digs = bignum_mul(num, sz_num, mul10);
        if (shift > 0) {
            digs = (digs << shift) | bignum_shift_left(num, sz_num, shift);
        } else if (shift < 0) {
            num[sz_num] = bignum_shift_right(digs, num, sz_num, -shift);
            digs >>= -shift, ++sz_num;
        }
    } else {
        digs = mul_and_shift(fp2.m, mul10, 1 + fp2.exp);
    }

    // Note, that the first digit formally can belong [1, 20) range, so we can get one digit more
    if (fp_fmt != fmt_flags::kFixed && digs >= get_pow10(n_digs)) {
        ++exp_, significand_ = digs / 100u;  // one excess digit
        // Remove one excess digit for scientific format
        const unsigned r = static_cast<unsigned>(digs - 100u * significand_);
        if (r > 50u || (r == 50u && ((significand_ & 1) || bignum_trim_unused(num, sz_num) /* nearest even */))) {
            ++significand_;
        }
    } else {
        significand_ = digs / 10u;
        const unsigned r = static_cast<unsigned>(digs - 10u * significand_);
        if (r > 5u || (r == 5u && ((significand_ & 1) || bignum_trim_unused(num, sz_num) /* nearest even */))) {
            ++significand_;
        }
        if (significand_ >= get_pow10(n_digs - 1)) {
            ++exp_;  // one excess digit
            if (fp_fmt != fmt_flags::kFixed) {
                // Remove one excess digit for scientific format
                // Note: `significand_` is exact power of 10 in this case
                significand_ /= 10u;
            }
        }
    }

    if (significand_ == 0) {
        exp_ = 0, n_zeroes_ = prec_ + 1;
        return;
    }

    if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
        // Select format for number representation
        if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }
        if (!alternate_) {
            prec_ -= remove_trailing_zeros(significand_);
            if (prec_ < 0) { significand_ *= get_pow10(-prec_), prec_ = 0; }
        } else if (prec_ == 0) {
            significand_ *= 10u, prec_ = 1;
        }
    }
}

void fp_dec_fmt_t::format_long_decimal(const fp_m64_t& fp2, int n_digs, const fmt_flags fp_fmt) NOEXCEPT {
    assert(n_digs >= kDigsPer64);
    ++n_digs;  // one additional digit for rounding

    // `kMaxPow10Size` uint64-s are enough to hold all (normalized!) 10^n, n <= 324 + kDigsPer64
    uint64_t num[kMaxPow10Size + 1];  // +1 to multiply by uint64
    unsigned sz_num = 1;

    // Digits are generated by packs: the first pack length can be in range
    // [1, kDigsPer64], all next are of kDigsPer64 length
    const int exp_aligned = exp_ -
                            ((kDigsPer64 * 1000000 + exp_) % kDigsPer64);  // lower kDigsPer64-aligned decimal exponent

    char* p = digs_buf_;
    auto gen_first_digit_pack = [this, fp_fmt, exp_aligned, &p, &n_digs](uint64_t digs) {
        const unsigned digs_len = 1 + exp_ - exp_aligned;  // first digit pack length
        assert(digs_len <= kDigsPer64);
        // Note, that the first digit formally can belong [1, 20) range,
        // so we can get one digit more while calculating the first pack
        if (digs < get_pow10(digs_len)) {
            gen_digits(p + digs_len, digs);
        } else {
            ++exp_, gen_digits(++p + digs_len, digs);
            if (fp_fmt != fmt_flags::kFixed) { --n_digs; }
        }
        p += digs_len, n_digs -= digs_len;
    };

    if (exp_aligned > 0) {
        unsigned index = (exp_aligned / kDigsPer64) - 1;
        bignum_t denominator = get_bigpow10(index);
        const unsigned shift = fp2.exp - denominator.exp;
        uint64_t digs = fp2.m >> (64 - shift);
        num[0] = fp2.m << shift;

        digs = bignum_divmod(digs, num, denominator.x, 1, denominator.sz);
        assert(digs);

        gen_first_digit_pack(digs);

        sz_num = denominator.sz;
        while (n_digs > 0 && (sz_num = bignum_trim_unused(num, sz_num))) {
            assert(index >= 0);
            const unsigned digs_len = std::min(n_digs, kDigsPer64);
            if (digs_len < kDigsPer64) {
                digs = bignum_divmod(bignum_mul(num, sz_num, get_pow10(digs_len)), num, denominator.x, sz_num,
                                     denominator.sz);
            } else if (index > 0) {
                const unsigned prev_denominator_exp = denominator.exp;
                denominator = get_bigpow10(index - 1);
                digs = bignum_divmod(bignum_shift_left(num, sz_num, prev_denominator_exp - denominator.exp), num,
                                     denominator.x, sz_num, denominator.sz);
                if (digs && sz_num < denominator.sz) { sz_num = denominator.sz; }
            } else {  // division will be not needed in this case (division by 1)
                digs = bignum_shift_left(num, sz_num, 1 + denominator.exp);
            }
            std::fill(p, gen_digits(p + digs_len, digs), '0');
            p += digs_len, n_digs -= digs_len, --index;
        }
    } else {
        uint64_t digs;
        if (exp_aligned < 0) {
            const bignum_t multiplier = get_bigpow10((-exp_aligned / kDigsPer64) - 1);
            const unsigned shift = 2 + fp2.exp + multiplier.exp;
            sz_num += multiplier.sz;
            num[0] = bignum_mul(num + 1, multiplier.x, sz_num - 1, fp2.m);
            digs = bignum_shift_left(num, sz_num, shift);
        } else {
            digs = fp2.m >> (63 - fp2.exp), num[0] = fp2.m << (1 + fp2.exp);
        }

        gen_first_digit_pack(digs);

        while (n_digs > 0 && (sz_num = bignum_trim_unused(num, sz_num))) {
            const unsigned digs_len = std::min(n_digs, kDigsPer64);
            digs = bignum_mul(num, sz_num, get_pow10(digs_len));
            std::fill(p, gen_digits(p + digs_len, digs), '0');
            p += digs_len, n_digs -= digs_len;
        }
    }

    // `n_digs` is trailing zero count now, which are to be appended to digit buffer
    // Note: some trailing zeroes can already be in digit buffer
    // Invariant: `static_cast<int>(p - digs_buf_) + n_digs = <total needed digit count>`

    if (n_digs) {
        --n_digs;
    } else if (*--p > '5' ||
               (*p == '5' && (((*(p - 1) - '0') & 1) || bignum_trim_unused(num, sz_num) /* nearest even */))) {
        // round to upper value
        while (*(p - 1) == '9') {
            --p, ++n_digs;  // trailing zero
            if (p == digs_buf_) {
                ++exp_, *p++ = '0';
                if (fp_fmt != fmt_flags::kFixed) { --n_digs; }
                break;
            }
        }
        ++*(p - 1);
    }

    // Note: `n_digs` is trailing zero count
    if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
        // Select format for number representation
        if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }
        if (!alternate_) {  // trim trailing zeroes
            if (n_digs < prec_) {
                prec_ -= n_digs, n_digs = 0;
                while (*(p - 1) == '0' && prec_ > 0) { --p, --prec_; }
            } else {
                n_digs -= prec_, prec_ = 0;
            }
        }
    }

    n_zeroes_ = n_digs;
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
