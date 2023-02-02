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

struct uint96_t {
    uint64_t hi;
    uint32_t lo;
};

inline uint64_t umul96x32(uint96_t x, uint32_t y, uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return umul128(x.hi, static_cast<uint64_t>(y) << 32, static_cast<uint64_t>(x.lo) * y, result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = ((static_cast<gcc_ints::uint128>(x.hi) << 32) | x.lo) * y;
    result_hi = static_cast<uint64_t>(p >> 64);
    return static_cast<uint64_t>(p);
#else
    const uint64_t lower = static_cast<uint64_t>(x.lo) * y;
    const uint64_t mid = lo32(x.hi) * y + hi32(lower);
    result_hi = hi32(x.hi) * y + hi32(mid);
    return make64(lo32(mid), lo32(lower));
#endif
}

inline uint64_t umul96x64_higher128(uint96_t x, uint64_t y, uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return umul128(x.hi, y, __umulh(static_cast<uint64_t>(x.lo) << 32, y), result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(y) * (static_cast<uint64_t>(x.lo) << 32);
    p = static_cast<gcc_ints::uint128>(y) * x.hi + (p >> 64);
    result_hi = static_cast<uint64_t>(p >> 64);
    return static_cast<uint64_t>(p);
#else
    const uint64_t lower = lo32(y) * x.lo;
    return umul128(x.hi, y, hi32(y) * x.lo + hi32(lower), result_hi);
#endif
}

SCVT_CONSTEXPR_DATA int kPow10Max = 328;
inline uint96_t get_cached_pow10(int pow) NOEXCEPT {
    assert(pow >= -kPow10Max && pow <= kPow10Max);
#define FULL_CACHE 0
#if FULL_CACHE == 0
    static SCVT_CONSTEXPR_DATA uint64_t higher64[] = {
        0xa9c98d8ccb009506, 0xfd00b897478238d0, 0xbc807527ed3e12bc, 0x8c71dcd9ba0b4925, 0xd1476e2c07286faa,
        0x9becce62836ac577, 0xe858ad248f5c22c9, 0xad1c8eab5ee43b66, 0x80fa687f881c7f8e, 0xc0314325637a1939,
        0x8f31cc0937ae58d2, 0xd5605fcdcf32e1d6, 0x9efa548d26e5a6e1, 0xece53cec4a314ebd, 0xb080392cc4349dec,
        0x8380dea93da4bc60, 0xc3f490aa77bd60fc, 0x91ff83775423cc06, 0xd98ddaee19068c76, 0xa21727db38cb002f,
        0xf18899b1bc3f8ca1, 0xb3f4e093db73a093, 0x8613fd0145877585, 0xc7caba6e7c5382c8, 0x94db483840b717ef,
        0xddd0467c64bce4a0, 0xa54394fe1eedb8fe, 0xf64335bcf065d37d, 0xb77ada0617e3bbcb, 0x88b402f7fd75539b,
        0xcbb41ef979346bca, 0x97c560ba6b0919a5, 0xe2280b6c20dd5232, 0xa87fea27a539e9a5, 0xfb158592be068d2e,
        0xbb127c53b17ec159, 0x8b61313bbabce2c6, 0xcfb11ead453994ba, 0x9abe14cd44753b52, 0xe69594bec44de15b,
        0xabcc77118461cefc, 0x8000000000000000, 0xbebc200000000000, 0x8e1bc9bf04000000, 0xd3c21bcecceda100,
        0x9dc5ada82b70b59d, 0xeb194f8e1ae525fd, 0xaf298d050e4395d6, 0x82818f1281ed449f, 0xc2781f49ffcfa6d5,
        0x90e40fbeea1d3a4a, 0xd7e77a8f87daf7fb, 0xa0dc75f1778e39d6, 0xefb3ab16c59b14a2, 0xb2977ee300c50fe7,
        0x850fadc09923329e, 0xc646d63501a1511d, 0x93ba47c980e98cdf, 0xdc21a1171d42645d, 0xa402b9c5a8d3a6e7,
        0xf46518c2ef5b8cd1, 0xb616a12b7fe617aa, 0x87aa9aff79042286, 0xca28a291859bbf93, 0x969eb7c47859e743,
        0xe070f78d3927556a, 0xa738c6bebb12d16c, 0xf92e0c3537826145, 0xb9a74a0637ce2ee1, 0x8a5296ffe33cc92f,
        0xce1de40642e3f4b9, 0x9991a6f3d6bf1765, 0xe4d5e82392a40515, 0xaa7eebfb9df9de8d, 0xfe0efb53d30dd4d7,
        0xbd49d14aa79dbc82, 0x8d07e33455637eb2, 0xd226fc195c6a2f8c, 0x9c935e00d4b9d8d2, 0xe950df20247c83fd,
        0xadd57a27d29339f6, 0x81842f29f2cce375, 0xc0fe908895cf3b44};
    static SCVT_CONSTEXPR_DATA uint32_t lower32[] = {
        0x680efdaf, 0x8920b099, 0xc6050837, 0x9ff0c08b, 0x1af5af66, 0x4ee367f9, 0xd1b34010, 0xda324365, 0x7ce66635,
        0xfa911156, 0xd1b2ecb9, 0xfb1e4a9b, 0xc47bc501, 0xa4f8bf56, 0xbd8d794e, 0x4247cb9e, 0xbedbfc44, 0x7b6306a3,
        0x3badd625, 0xb8ada00e, 0xdc44e6c4, 0x59ed2167, 0xbd06742d, 0xfe64a52f, 0xa8c2a44f, 0xac7cb3f7, 0xc2974eb5,
        0x4d4617b6, 0x09ce6ebb, 0x11dbcb02, 0x4f2b40a0, 0xdccd87a0, 0x25c6da64, 0x3f2398d7, 0xeed6e2f1, 0x5560c018,
        0x2323ac4b, 0x67de18ee, 0xc4926a96, 0x4c2ebe68, 0xfdc20d2b, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0xf0200000, 0x5dcfab08, 0x9670b12b, 0xbff8f10e, 0x3cbf6b72, 0xbc8955e9, 0xdc33745f, 0x696361ae, 0xc5cfe94f,
        0x58edec92, 0x03e2cf6c, 0xb281e1fd, 0xc66f336c, 0x76707544, 0x5f16206d, 0x7eb25866, 0x577b986b, 0x90fb44d3,
        0x7d7b8f75, 0x9f644ae6, 0x85bbe254, 0xb428f8ac, 0xa7709a57, 0x6d953e2c, 0x82bd6b71, 0x36251261, 0xacca6da2,
        0x0fabaf40, 0xddbb901c, 0xed238cd4, 0x4b2d8645, 0xdb0b487b, 0x73832eec, 0x6ed1bf9a, 0x47c6b82f, 0x79c5db9b,
        0xe6a11583, 0x505f522e};
    SCVT_CONSTEXPR_DATA int step_pow = 3;
    int n = (kPow10Max + pow) >> step_pow, k = pow & ((1 << step_pow) - 1);
    uint96_t result{higher64[n], lower32[n]};
    if (!k) { return result; }
    static SCVT_CONSTEXPR_DATA uint32_t mul10[] = {0,          0xa0000000, 0xc8000000, 0xfa000000,
                                                   0x9c400000, 0xc3500000, 0xf4240000, 0x98968000};
    uint64_t t = umul96x32(result, mul10[k], result.hi);
    if (!(result.hi & msb64)) {
        result.hi = shl128(result.hi, t, 1);
        t <<= 1;
    }
    result.lo = static_cast<uint32_t>(hi32(t + (1ull << 31)));
    return result;
#else
    static SCVT_CONSTEXPR_DATA uint64_t higher64[] = {
        0xa9c98d8ccb009506, 0xd43bf0effdc0ba48, 0x84a57695fe98746d, 0xa5ced43b7e3e9188, 0xcf42894a5dce35ea,
        0x818995ce7aa0e1b2, 0xa1ebfb4219491a1f, 0xca66fa129f9b60a6, 0xfd00b897478238d0, 0x9e20735e8cb16382,
        0xc5a890362fddbc62, 0xf712b443bbd52b7b, 0x9a6bb0aa55653b2d, 0xc1069cd4eabe89f8, 0xf148440a256e2c76,
        0x96cd2a865764dbca, 0xbc807527ed3e12bc, 0xeba09271e88d976b, 0x93445b8731587ea3, 0xb8157268fdae9e4c,
        0xe61acf033d1a45df, 0x8fd0c16206306bab, 0xb3c4f1ba87bc8696, 0xe0b62e2929aba83c, 0x8c71dcd9ba0b4925,
        0xaf8e5410288e1b6f, 0xdb71e91432b1a24a, 0x892731ac9faf056e, 0xab70fe17c79ac6ca, 0xd64d3d9db981787d,
        0x85f0468293f0eb4e, 0xa76c582338ed2621, 0xd1476e2c07286faa, 0x82cca4db847945ca, 0xa37fce126597973c,
        0xcc5fc196fefd7d0c, 0xff77b1fcbebcdc4f, 0x9faacf3df73609b1, 0xc795830d75038c1d, 0xf97ae3d0d2446f25,
        0x9becce62836ac577, 0xc2e801fb244576d5, 0xf3a20279ed56d48a, 0x9845418c345644d6, 0xbe5691ef416bd60c,
        0xedec366b11c6cb8f, 0x94b3a202eb1c3f39, 0xb9e08a83a5e34f07, 0xe858ad248f5c22c9, 0x91376c36d99995be,
        0xb58547448ffffb2d, 0xe2e69915b3fff9f9, 0x8dd01fad907ffc3b, 0xb1442798f49ffb4a, 0xdd95317f31c7fa1d,
        0x8a7d3eef7f1cfc52, 0xad1c8eab5ee43b66, 0xd863b256369d4a40, 0x873e4f75e2224e68, 0xa90de3535aaae202,
        0xd3515c2831559a83, 0x8412d9991ed58091, 0xa5178fff668ae0b6, 0xce5d73ff402d98e3, 0x80fa687f881c7f8e,
        0xa139029f6a239f72, 0xc987434744ac874e, 0xfbe9141915d7a922, 0x9d71ac8fada6c9b5, 0xc4ce17b399107c22,
        0xf6019da07f549b2b, 0x99c102844f94e0fb, 0xc0314325637a1939, 0xf03d93eebc589f88, 0x96267c7535b763b5,
        0xbbb01b9283253ca2, 0xea9c227723ee8bcb, 0x92a1958a7675175f, 0xb749faed14125d36, 0xe51c79a85916f484,
        0x8f31cc0937ae58d2, 0xb2fe3f0b8599ef07, 0xdfbdcece67006ac9, 0x8bd6a141006042bd, 0xaecc49914078536d,
        0xda7f5bf590966848, 0x888f99797a5e012d, 0xaab37fd7d8f58178, 0xd5605fcdcf32e1d6, 0x855c3be0a17fcd26,
        0xa6b34ad8c9dfc06f, 0xd0601d8efc57b08b, 0x823c12795db6ce57, 0xa2cb1717b52481ed, 0xcb7ddcdda26da268,
        0xfe5d54150b090b02, 0x9efa548d26e5a6e1, 0xc6b8e9b0709f109a, 0xf867241c8cc6d4c0, 0x9b407691d7fc44f8,
        0xc21094364dfb5636, 0xf294b943e17a2bc4, 0x979cf3ca6cec5b5a, 0xbd8430bd08277231, 0xece53cec4a314ebd,
        0x940f4613ae5ed136, 0xb913179899f68584, 0xe757dd7ec07426e5, 0x9096ea6f3848984f, 0xb4bca50b065abe63,
        0xe1ebce4dc7f16dfb, 0x8d3360f09cf6e4bd, 0xb080392cc4349dec, 0xdca04777f541c567, 0x89e42caaf9491b60,
        0xac5d37d5b79b6239, 0xd77485cb25823ac7, 0x86a8d39ef77164bc, 0xa8530886b54dbdeb, 0xd267caa862a12d66,
        0x8380dea93da4bc60, 0xa46116538d0deb78, 0xcd795be870516656, 0x806bd9714632dff6, 0xa086cfcd97bf97f3,
        0xc8a883c0fdaf7df0, 0xfad2a4b13d1b5d6c, 0x9cc3a6eec6311a63, 0xc3f490aa77bd60fc, 0xf4f1b4d515acb93b,
        0x991711052d8bf3c5, 0xbf5cd54678eef0b6, 0xef340a98172aace4, 0x9580869f0e7aac0e, 0xbae0a846d2195712,
        0xe998d258869facd7, 0x91ff83775423cc06, 0xb67f6455292cbf08, 0xe41f3d6a7377eeca, 0x8e938662882af53e,
        0xb23867fb2a35b28d, 0xdec681f9f4c31f31, 0x8b3c113c38f9f37e, 0xae0b158b4738705e, 0xd98ddaee19068c76,
        0x87f8a8d4cfa417c9, 0xa9f6d30a038d1dbc, 0xd47487cc8470652b, 0x84c8d4dfd2c63f3b, 0xa5fb0a17c777cf09,
        0xcf79cc9db955c2cc, 0x81ac1fe293d599bf, 0xa21727db38cb002f, 0xca9cf1d206fdc03b, 0xfd442e4688bd304a,
        0x9e4a9cec15763e2e, 0xc5dd44271ad3cdba, 0xf7549530e188c128, 0x9a94dd3e8cf578b9, 0xc13a148e3032d6e7,
        0xf18899b1bc3f8ca1, 0x96f5600f15a7b7e5, 0xbcb2b812db11a5de, 0xebdf661791d60f56, 0x936b9fcebb25c995,
        0xb84687c269ef3bfb, 0xe65829b3046b0afa, 0x8ff71a0fe2c2e6dc, 0xb3f4e093db73a093, 0xe0f218b8d25088b8,
        0x8c974f7383725573, 0xafbd2350644eeacf, 0xdbac6c247d62a583, 0x894bc396ce5da772, 0xab9eb47c81f5114f,
        0xd686619ba27255a2, 0x8613fd0145877585, 0xa798fc4196e952e7, 0xd17f3b51fca3a7a0, 0x82ef85133de648c4,
        0xa3ab66580d5fdaf5, 0xcc963fee10b7d1b3, 0xffbbcfe994e5c61f, 0x9fd561f1fd0f9bd3, 0xc7caba6e7c5382c8,
        0xf9bd690a1b68637b, 0x9c1661a651213e2d, 0xc31bfa0fe5698db8, 0xf3e2f893dec3f126, 0x986ddb5c6b3a76b7,
        0xbe89523386091465, 0xee2ba6c0678b597f, 0x94db483840b717ef, 0xba121a4650e4ddeb, 0xe896a0d7e51e1566,
        0x915e2486ef32cd60, 0xb5b5ada8aaff80b8, 0xe3231912d5bf60e6, 0x8df5efabc5979c8f, 0xb1736b96b6fd83b3,
        0xddd0467c64bce4a0, 0x8aa22c0dbef60ee4, 0xad4ab7112eb3929d, 0xd89d64d57a607744, 0x87625f056c7c4a8b,
        0xa93af6c6c79b5d2d, 0xd389b47879823479, 0x843610cb4bf160cb, 0xa54394fe1eedb8fe, 0xce947a3da6a9273e,
        0x811ccc668829b887, 0xa163ff802a3426a8, 0xc9bcff6034c13052, 0xfc2c3f3841f17c67, 0x9d9ba7832936edc0,
        0xc5029163f384a931, 0xf64335bcf065d37d, 0x99ea0196163fa42e, 0xc06481fb9bcf8d39, 0xf07da27a82c37088,
        0x964e858c91ba2655, 0xbbe226efb628afea, 0xeadab0aba3b2dbe5, 0x92c8ae6b464fc96f, 0xb77ada0617e3bbcb,
        0xe55990879ddcaabd, 0x8f57fa54c2a9eab6, 0xb32df8e9f3546564, 0xdff9772470297ebd, 0x8bfbea76c619ef36,
        0xaefae51477a06b03, 0xdab99e59958885c4, 0x88b402f7fd75539b, 0xaae103b5fcd2a881, 0xd59944a37c0752a2,
        0x857fcae62d8493a5, 0xa6dfbd9fb8e5b88e, 0xd097ad07a71f26b2, 0x825ecc24c873782f, 0xa2f67f2dfa90563b,
        0xcbb41ef979346bca, 0xfea126b7d78186bc, 0x9f24b832e6b0f436, 0xc6ede63fa05d3143, 0xf8a95fcf88747d94,
        0x9b69dbe1b548ce7c, 0xc24452da229b021b, 0xf2d56790ab41c2a2, 0x97c560ba6b0919a5, 0xbdb6b8e905cb600f,
        0xed246723473e3813, 0x9436c0760c86e30b, 0xb94470938fa89bce, 0xe7958cb87392c2c2, 0x90bd77f3483bb9b9,
        0xb4ecd5f01a4aa828, 0xe2280b6c20dd5232, 0x8d590723948a535f, 0xb0af48ec79ace837, 0xdcdb1b2798182244,
        0x8a08f0f8bf0f156b, 0xac8b2d36eed2dac5, 0xd7adf884aa879177, 0x86ccbb52ea94baea, 0xa87fea27a539e9a5,
        0xd29fe4b18e88640e, 0x83a3eeeef9153e89, 0xa48ceaaab75a8e2b, 0xcdb02555653131b6, 0x808e17555f3ebf11,
        0xa0b19d2ab70e6ed6, 0xc8de047564d20a8b, 0xfb158592be068d2e, 0x9ced737bb6c4183d, 0xc428d05aa4751e4c,
        0xf53304714d9265df, 0x993fe2c6d07b7fab, 0xbf8fdb78849a5f96, 0xef73d256a5c0f77c, 0x95a8637627989aad,
        0xbb127c53b17ec159, 0xe9d71b689dde71af, 0x9226712162ab070d, 0xb6b00d69bb55c8d1, 0xe45c10c42a2b3b05,
        0x8eb98a7a9a5b04e3, 0xb267ed1940f1c61c, 0xdf01e85f912e37a3, 0x8b61313bbabce2c6, 0xae397d8aa96c1b77,
        0xd9c7dced53c72255, 0x881cea14545c7575, 0xaa242499697392d2, 0xd4ad2dbfc3d07787, 0x84ec3c97da624ab4,
        0xa6274bbdd0fadd61, 0xcfb11ead453994ba, 0x81ceb32c4b43fcf4, 0xa2425ff75e14fc31, 0xcad2f7f5359a3b3e,
        0xfd87b5f28300ca0d, 0x9e74d1b791e07e48, 0xc612062576589dda, 0xf79687aed3eec551, 0x9abe14cd44753b52,
        0xc16d9a0095928a27, 0xf1c90080baf72cb1, 0x971da05074da7bee, 0xbce5086492111aea, 0xec1e4a7db69561a5,
        0x9392ee8e921d5d07, 0xb877aa3236a4b449, 0xe69594bec44de15b, 0x901d7cf73ab0acd9, 0xb424dc35095cd80f,
        0xe12e13424bb40e13, 0x8cbccc096f5088cb, 0xafebff0bcb24aafe, 0xdbe6fecebdedd5be, 0x89705f4136b4a597,
        0xabcc77118461cefc, 0xd6bf94d5e57a42bc, 0x8637bd05af6c69b5, 0xa7c5ac471b478423, 0xd1b71758e219652b,
        0x83126e978d4fdf3b, 0xa3d70a3d70a3d70a, 0xcccccccccccccccc, 0x8000000000000000, 0xa000000000000000,
        0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000, 0xc350000000000000, 0xf424000000000000,
        0x9896800000000000, 0xbebc200000000000, 0xee6b280000000000, 0x9502f90000000000, 0xba43b74000000000,
        0xe8d4a51000000000, 0x9184e72a00000000, 0xb5e620f480000000, 0xe35fa931a0000000, 0x8e1bc9bf04000000,
        0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000, 0xad78ebc5ac620000, 0xd8d726b7177a8000,
        0x878678326eac9000, 0xa968163f0a57b400, 0xd3c21bcecceda100, 0x84595161401484a0, 0xa56fa5b99019a5c8,
        0xcecb8f27f4200f3a, 0x813f3978f8940984, 0xa18f07d736b90be5, 0xc9f2c9cd04674ede, 0xfc6f7c4045812296,
        0x9dc5ada82b70b59d, 0xc5371912364ce305, 0xf684df56c3e01bc6, 0x9a130b963a6c115c, 0xc097ce7bc90715b3,
        0xf0bdc21abb48db20, 0x96769950b50d88f4, 0xbc143fa4e250eb31, 0xeb194f8e1ae525fd, 0x92efd1b8d0cf37be,
        0xb7abc627050305ad, 0xe596b7b0c643c719, 0x8f7e32ce7bea5c6f, 0xb35dbf821ae4f38b, 0xe0352f62a19e306e,
        0x8c213d9da502de45, 0xaf298d050e4395d6, 0xdaf3f04651d47b4c, 0x88d8762bf324cd0f, 0xab0e93b6efee0053,
        0xd5d238a4abe98068, 0x85a36366eb71f041, 0xa70c3c40a64e6c51, 0xd0cf4b50cfe20765, 0x82818f1281ed449f,
        0xa321f2d7226895c7, 0xcbea6f8ceb02bb39, 0xfee50b7025c36a08, 0x9f4f2726179a2245, 0xc722f0ef9d80aad6,
        0xf8ebad2b84e0d58b, 0x9b934c3b330c8577, 0xc2781f49ffcfa6d5, 0xf316271c7fc3908a, 0x97edd871cfda3a56,
        0xbde94e8e43d0c8ec, 0xed63a231d4c4fb27, 0x945e455f24fb1cf8, 0xb975d6b6ee39e436, 0xe7d34c64a9c85d44,
        0x90e40fbeea1d3a4a, 0xb51d13aea4a488dd, 0xe264589a4dcdab14, 0x8d7eb76070a08aec, 0xb0de65388cc8ada8,
        0xdd15fe86affad912, 0x8a2dbf142dfcc7ab, 0xacb92ed9397bf996, 0xd7e77a8f87daf7fb, 0x86f0ac99b4e8dafd,
        0xa8acd7c0222311bc, 0xd2d80db02aabd62b, 0x83c7088e1aab65db, 0xa4b8cab1a1563f52, 0xcde6fd5e09abcf26,
        0x80b05e5ac60b6178, 0xa0dc75f1778e39d6, 0xc913936dd571c84c, 0xfb5878494ace3a5f, 0x9d174b2dcec0e47b,
        0xc45d1df942711d9a, 0xf5746577930d6500, 0x9968bf6abbe85f20, 0xbfc2ef456ae276e8, 0xefb3ab16c59b14a2,
        0x95d04aee3b80ece5, 0xbb445da9ca61281f, 0xea1575143cf97226, 0x924d692ca61be758, 0xb6e0c377cfa2e12e,
        0xe498f455c38b997a, 0x8edf98b59a373fec, 0xb2977ee300c50fe7, 0xdf3d5e9bc0f653e1, 0x8b865b215899f46c,
        0xae67f1e9aec07187, 0xda01ee641a708de9, 0x884134fe908658b2, 0xaa51823e34a7eede, 0xd4e5e2cdc1d1ea96,
        0x850fadc09923329e, 0xa6539930bf6bff45, 0xcfe87f7cef46ff16, 0x81f14fae158c5f6e, 0xa26da3999aef7749,
        0xcb090c8001ab551c, 0xfdcb4fa002162a63, 0x9e9f11c4014dda7e, 0xc646d63501a1511d, 0xf7d88bc24209a565,
        0x9ae757596946075f, 0xc1a12d2fc3978937, 0xf209787bb47d6b84, 0x9745eb4d50ce6332, 0xbd176620a501fbff,
        0xec5d3fa8ce427aff, 0x93ba47c980e98cdf, 0xb8a8d9bbe123f017, 0xe6d3102ad96cec1d, 0x9043ea1ac7e41392,
        0xb454e4a179dd1877, 0xe16a1dc9d8545e94, 0x8ce2529e2734bb1d, 0xb01ae745b101e9e4, 0xdc21a1171d42645d,
        0x899504ae72497eba, 0xabfa45da0edbde69, 0xd6f8d7509292d603, 0x865b86925b9bc5c2, 0xa7f26836f282b732,
        0xd1ef0244af2364ff, 0x8335616aed761f1f, 0xa402b9c5a8d3a6e7, 0xcd036837130890a1, 0x802221226be55a64,
        0xa02aa96b06deb0fd, 0xc83553c5c8965d3d, 0xfa42a8b73abbf48c, 0x9c69a97284b578d7, 0xc38413cf25e2d70d,
        0xf46518c2ef5b8cd1, 0x98bf2f79d5993802, 0xbeeefb584aff8603, 0xeeaaba2e5dbf6784, 0x952ab45cfa97a0b2,
        0xba756174393d88df, 0xe912b9d1478ceb17, 0x91abb422ccb812ee, 0xb616a12b7fe617aa, 0xe39c49765fdf9d94,
        0x8e41ade9fbebc27d, 0xb1d219647ae6b31c, 0xde469fbd99a05fe3, 0x8aec23d680043bee, 0xada72ccc20054ae9,
        0xd910f7ff28069da4, 0x87aa9aff79042286, 0xa99541bf57452b28, 0xd3fa922f2d1675f2, 0x847c9b5d7c2e09b7,
        0xa59bc234db398c25, 0xcf02b2c21207ef2e, 0x8161afb94b44f57d, 0xa1ba1ba79e1632dc, 0xca28a291859bbf93,
        0xfcb2cb35e702af78, 0x9defbf01b061adab, 0xc56baec21c7a1916, 0xf6c69a72a3989f5b, 0x9a3c2087a63f6399,
        0xc0cb28a98fcf3c7f, 0xf0fdf2d3f3c30b9f, 0x969eb7c47859e743, 0xbc4665b596706114, 0xeb57ff22fc0c7959,
        0x9316ff75dd87cbd8, 0xb7dcbf5354e9bece, 0xe5d3ef282a242e81, 0x8fa475791a569d10, 0xb38d92d760ec4455,
        0xe070f78d3927556a, 0x8c469ab843b89562, 0xaf58416654a6babb, 0xdb2e51bfe9d0696a, 0x88fcf317f22241e2,
        0xab3c2fddeeaad25a, 0xd60b3bd56a5586f1, 0x85c7056562757456, 0xa738c6bebb12d16c, 0xd106f86e69d785c7,
        0x82a45b450226b39c, 0xa34d721642b06084, 0xcc20ce9bd35c78a5, 0xff290242c83396ce, 0x9f79a169bd203e41,
        0xc75809c42c684dd1, 0xf92e0c3537826145, 0x9bbcc7a142b17ccb, 0xc2abf989935ddbfe, 0xf356f7ebf83552fe,
        0x98165af37b2153de, 0xbe1bf1b059e9a8d6, 0xeda2ee1c7064130c, 0x9485d4d1c63e8be7, 0xb9a74a0637ce2ee1,
        0xe8111c87c5c1ba99, 0x910ab1d4db9914a0, 0xb54d5e4a127f59c8, 0xe2a0b5dc971f303a, 0x8da471a9de737e24,
        0xb10d8e1456105dad, 0xdd50f1996b947518, 0x8a5296ffe33cc92f, 0xace73cbfdc0bfb7b, 0xd8210befd30efa5a,
        0x8714a775e3e95c78, 0xa8d9d1535ce3b396, 0xd31045a8341ca07c, 0x83ea2b892091e44d, 0xa4e4b66b68b65d60,
        0xce1de40642e3f4b9, 0x80d2ae83e9ce78f3, 0xa1075a24e4421730, 0xc94930ae1d529cfc, 0xfb9b7cd9a4a7443c,
        0x9d412e0806e88aa5, 0xc491798a08a2ad4e, 0xf5b5d7ec8acb58a2, 0x9991a6f3d6bf1765, 0xbff610b0cc6edd3f,
        0xeff394dcff8a948e, 0x95f83d0a1fb69cd9, 0xbb764c4ca7a4440f, 0xea53df5fd18d5513, 0x92746b9be2f8552c,
        0xb7118682dbb66a77, 0xe4d5e82392a40515, 0x8f05b1163ba6832d, 0xb2c71d5bca9023f8, 0xdf78e4b2bd342cf6,
        0x8bab8eefb6409c1a, 0xae9672aba3d0c320, 0xda3c0f568cc4f3e8, 0x8865899617fb1871, 0xaa7eebfb9df9de8d,
        0xd51ea6fa85785631, 0x8533285c936b35de, 0xa67ff273b8460356, 0xd01fef10a657842c, 0x8213f56a67f6b29b,
        0xa298f2c501f45f42, 0xcb3f2f7642717713, 0xfe0efb53d30dd4d7, 0x9ec95d1463e8a506, 0xc67bb4597ce2ce48,
        0xf81aa16fdc1b81da, 0x9b10a4e5e9913128, 0xc1d4ce1f63f57d72, 0xf24a01a73cf2dccf, 0x976e41088617ca01,
        0xbd49d14aa79dbc82, 0xec9c459d51852ba2, 0x93e1ab8252f33b45, 0xb8da1662e7b00a17, 0xe7109bfba19c0c9d,
        0x906a617d450187e2, 0xb484f9dc9641e9da, 0xe1a63853bbd26451, 0x8d07e33455637eb2, 0xb049dc016abc5e5f,
        0xdc5c5301c56b75f7, 0x89b9b3e11b6329ba, 0xac2820d9623bf429, 0xd732290fbacaf133, 0x867f59a9d4bed6c0,
        0xa81f301449ee8c70, 0xd226fc195c6a2f8c, 0x83585d8fd9c25db7, 0xa42e74f3d032f525, 0xcd3a1230c43fb26f,
        0x80444b5e7aa7cf85, 0xa0555e361951c366, 0xc86ab5c39fa63440, 0xfa856334878fc150, 0x9c935e00d4b9d8d2,
        0xc3b8358109e84f07, 0xf4a642e14c6262c8, 0x98e7e9cccfbd7dbd, 0xbf21e44003acdd2c, 0xeeea5d5004981478,
        0x95527a5202df0ccb, 0xbaa718e68396cffd, 0xe950df20247c83fd, 0x91d28b7416cdd27e, 0xb6472e511c81471d,
        0xe3d8f9e563a198e5, 0x8e679c2f5e44ff8f, 0xb201833b35d63f73, 0xde81e40a034bcf4f, 0x8b112e86420f6191,
        0xadd57a27d29339f6, 0xd94ad8b1c7380874, 0x87cec76f1c830548, 0xa9c2794ae3a3c69a, 0xd433179d9c8cb841,
        0x849feec281d7f328, 0xa5c7ea73224deff3, 0xcf39e50feae16bef, 0x81842f29f2cce375, 0xa1e53af46f801c53,
        0xca5e89b18b602368, 0xfcf62c1dee382c42, 0x9e19db92b4e31ba9, 0xc5a05277621be293, 0xf70867153aa2db38,
        0x9a65406d44a5c903, 0xc0fe908895cf3b44};
    static SCVT_CONSTEXPR_DATA uint32_t lower32[] = {
        0x680efdaf, 0x0212bd1b, 0x014bb631, 0x419ea3bd, 0x52064cad, 0x7343efec, 0x1014ebe7, 0xd41a26e0, 0x8920b099,
        0x55b46e5f, 0xeb2189f7, 0xa5e9ec75, 0x47b233c9, 0x999ec0bb, 0xc00670ea, 0x38040692, 0xc6050837, 0xf7864a45,
        0x7ab3ee6b, 0x5960ea06, 0x6fb92487, 0xa5d3b6d4, 0x8f48a48a, 0x331acdac, 0x9ff0c08b, 0x07ecf0ae, 0xc9e82cda,
        0xbe311c08, 0x6dbd630a, 0x092cbbcd, 0x25bbf560, 0xaf2af2b8, 0x1af5af66, 0x50d98da0, 0xe50ff108, 0x1e53ed4a,
        0x25e8e89c, 0x77b19162, 0xd59df5ba, 0x4b057328, 0x4ee367f9, 0x229c41f8, 0x6b435275, 0x830a1389, 0x23cc986c,
        0x2cbfbe87, 0x7bf7d714, 0xdaf5ccd9, 0xd1b34010, 0x2310080a, 0xabd40a0c, 0x16c90c8f, 0xae3da7d9, 0x99cd11d0,
        0x40405644, 0x482835ea, 0xda324365, 0x90bed43e, 0x5a7744a7, 0x711515d1, 0x0d5a5b45, 0xe858790b, 0x626e974e,
        0xfb0a3d21, 0x7ce66635, 0x1c1fffc2, 0xa327ffb2, 0x4bf1ff9f, 0x6f773fc3, 0xcb550fb4, 0x7e2a53a1, 0x2eda7445,
        0xfa911156, 0x793555ab, 0x4bc1558b, 0x9eb1aaee, 0x465e15a9, 0x0bfacd8a, 0xcef980ec, 0x82b7e128, 0xd1b2ecb9,
        0x861fa7e7, 0x67a791e1, 0xe0c8bb2c, 0x58fae9f7, 0xaf39a475, 0x6d8406c9, 0xc8e5087c, 0xfb1e4a9b, 0x5cf2eea1,
        0xf42faa49, 0xf13b94db, 0x76c53d09, 0x54768c4b, 0xa9942f5e, 0xd3f93b35, 0xc47bc501, 0x359ab642, 0xc30163d2,
        0x79e0de63, 0x985915fc, 0x3e6f5b7b, 0xa705992d, 0x50c6ff78, 0xa4f8bf56, 0x871b7796, 0x28e2557b, 0x331aeada,
        0x3ff0d2c8, 0x0fed077a, 0xd3e84959, 0x64712dd8, 0xbd8d794e, 0xecf0d7a1, 0xf41686c5, 0x311c2876, 0x7d633293,
        0xae5dff9c, 0xd9f57f83, 0xd072df64, 0x4247cb9e, 0x52d9be86, 0x67902e27, 0x00ba1cd9, 0x80e8a40f, 0x6122cd13,
        0x796b8057, 0xcbe33036, 0xbedbfc44, 0xee92fb55, 0x751bdd15, 0xd262d45a, 0x86fb8971, 0xd45d35e7, 0x89748360,
        0x2bd1a438, 0x7b6306a3, 0x1a3bc84c, 0x20caba5f, 0x547eb47b, 0xe99e619a, 0x6405fa01, 0xde83bc41, 0x9624ab51,
        0x3badd625, 0xe54ca5d7, 0x5e9fcf4d, 0x7647c320, 0x29ecd9f4, 0xf4681071, 0x7182148d, 0xc6f14cd8, 0xb8ada00e,
        0xa6d90812, 0x908f4a16, 0x9a598e4e, 0x40eff1e2, 0xd12bee5a, 0x82bb74f8, 0xe36a5236, 0xdc44e6c4, 0x29ab103a,
        0x7415d449, 0x111b495b, 0xcab10dd9, 0x3d5d514f, 0x0cb4a5a3, 0x47f0e786, 0x59ed2167, 0x306869c1, 0x1e414219,
        0xe5d1929f, 0xdf45f747, 0x6b8bba8c, 0x066ea92f, 0xc80a537b, 0xbd06742d, 0x2c481138, 0xf75a1586, 0x9a984d74,
        0xc13e60d1, 0x318df905, 0xfdf17746, 0xfeb6ea8c, 0xfe64a52f, 0x3dfdce7b, 0x06bea10d, 0x486e4950, 0x5a89dba4,
        0xf8962946, 0xf6bbb398, 0x746aa07e, 0xa8c2a44f, 0x92f34d62, 0x77b020bb, 0x0ace1475, 0x0d819992, 0x10e1fff7,
        0xca8d3ffa, 0xbd308ff9, 0xac7cb3f7, 0x6bcdf07a, 0x86c16c99, 0xe871c7bf, 0x11471cd7, 0xd598e40d, 0x4aff1d11,
        0xcedf722a, 0xc2974eb5, 0x733d2262, 0x0806357d, 0xca07c2dd, 0xfc89b394, 0xbbac2079, 0xd54b944c, 0x0a9e795e,
        0x4d4617b6, 0x504bced2, 0xe45ec286, 0x5d767328, 0x3a6a07f9, 0x890489f7, 0x2b45ac75, 0x3b0b8bc9, 0x09ce6ebb,
        0xcc420a6a, 0x9fa94682, 0x47939823, 0x59787e2c, 0x57eb4edb, 0xede62292, 0xe95fab37, 0x11dbcb02, 0xd652bdc3,
        0x4be76d33, 0x6f70a440, 0xcb4ccd50, 0x7e2000a4, 0x8ed40067, 0x72890080, 0x4f2b40a0, 0xe2f610c8, 0x0dd9ca7d,
        0x91503d1c, 0x75a44c64, 0xc986afbe, 0xfbe85bae, 0xfae27299, 0xdccd87a0, 0x5400e988, 0x290123ea, 0xf9a0b672,
        0xf808e40f, 0xb60b1d12, 0xb1c6f22b, 0x1e38aeb6, 0x25c6da64, 0x579c487e, 0x2d835a9e, 0xf8e43145, 0x1b8e9ecb,
        0xe272467e, 0x5b0ed81e, 0x98e94713, 0x3f2398d7, 0x8eec7f0d, 0x1953cf68, 0x5fa8c342, 0x3792f413, 0xe2bbd88c,
        0x5b6aceaf, 0xf245825a, 0xeed6e2f1, 0x55464dd7, 0xaa97e14c, 0xd53dd99f, 0xe546a804, 0xde985204, 0x963e6686,
        0xdde70013, 0x5560c018, 0xaab8f01e, 0xcab39613, 0x3d607b98, 0x8cb89a7e, 0x77f3608f, 0x55f038b2, 0x6b6c46df,
        0x2323ac4b, 0xabec975e, 0x96e7bd36, 0x7e50d641, 0xdde50bd2, 0x955e4ec6, 0xbd5af13c, 0xecb1ad8b, 0x67de18ee,
        0x80eacf95, 0xa125837a, 0x096ee458, 0x8bca9d6e, 0x775ea265, 0x95364afe, 0x3a83ddbe, 0xc4926a96, 0x75b7053c,
        0x5324c68b, 0xd3f6fc17, 0x88f4bb1d, 0x2b31e9e4, 0x3aff322e, 0x09befeba, 0x4c2ebe68, 0x0f9d3701, 0x538484c2,
        0x2865a5f2, 0xf93f87b7, 0xf78f69a5, 0xb573440e, 0x31680a89, 0xfdc20d2b, 0x3d329076, 0xa63f9a4a, 0x0fcf80dc,
        0xd3c36113, 0x645a1cac, 0x3d70a3d7, 0xcccccccd, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x50000000, 0xa4000000, 0x4d000000,
        0xf0200000, 0x6c280000, 0xc7320000, 0x3c7f4000, 0x4b9f1000, 0x1e86d400, 0x13144480, 0x17d955a0, 0x5dcfab08,
        0x5aa1cae5, 0xf14a3d9e, 0x6d9ccd06, 0xe4820024, 0xdda2802d, 0xd50b2038, 0x4526f423, 0x9670b12b, 0x3c0cdd76,
        0xa5880a6a, 0x8eea0d04, 0x72a49046, 0x47a6da2b, 0x999090b6, 0xfff4b4e4, 0xbff8f10e, 0xaff72d52, 0x9bf4f8a7,
        0x02f236d0, 0x01d76242, 0x424d3ad3, 0xd2e08987, 0x63cc55f5, 0x3cbf6b72, 0x8bef464e, 0x97758bf1, 0x3d52eeed,
        0x4ca7aaa8, 0x8fe8caa9, 0xb3e2fd54, 0x60dbbca8, 0xbc8955e9, 0x6babab64, 0xc696963c, 0xfc1e1de6, 0x3b25a55f,
        0x49ef0eb7, 0x6e356932, 0x49c2c37f, 0xdc33745f, 0x69a028bb, 0xc40832ea, 0xf50a3fa5, 0x792667c7, 0x577001b9,
        0xed4c0227, 0x544f8158, 0x696361ae, 0x03bc3a1a, 0x04ab48a0, 0x62eb0d64, 0x3ba5d0bd, 0xca8f44ec, 0x7e998b14,
        0x9e3fedd9, 0xc5cfe94f, 0xbba1f1d1, 0x2a8a6e46, 0xf52d09d7, 0x593c2626, 0x6f8b2fb0, 0x0b6dfb9c, 0x4724bd42,
        0x58edec92, 0x2f2967b6, 0xbd79e0d2, 0xecd85907, 0xe80e6f48, 0x3109058d, 0xbd4b46f0, 0x6c9e18ac, 0x03e2cf6c,
        0x84db8347, 0xe6126418, 0x4fcb7e8f, 0xe3be5e33, 0x5cadf5c0, 0x73d97330, 0x2867e7fe, 0xb281e1fd, 0x1f225a7d,
        0x3375788e, 0x0052d6b1, 0xc0678c5e, 0xf840b7bb, 0xb650e5a9, 0xa3e51f14, 0xc66f336c, 0xb80b0047, 0xa60dc059,
        0x87c89838, 0x29babe46, 0xf4296dd7, 0x1899e4a6, 0x5ec05dd0, 0x76707544, 0x6a06494a, 0x0487db9d, 0x45a9d284,
        0x0b8a2393, 0x8e6cac77, 0x3207d795, 0x7f44e6bd, 0x5f16206d, 0x36dba888, 0xc2494955, 0xf2db9baa, 0x6f928295,
        0xcb77233a, 0xff2a7604, 0xfef51385, 0x7eb25866, 0xef2f7740, 0xaafb5510, 0x95ba2a54, 0xdd945a74, 0x94f97112,
        0x7a37cd56, 0xac62e056, 0x577b986b, 0xed5a7e86, 0x14588f14, 0x596eb2d9, 0x6fca5f8f, 0x25de7bb9, 0xaf561aa8,
        0x1b2ba152, 0x90fb44d3, 0x353a1608, 0x42889b8a, 0x69956136, 0x43fab983, 0x94f967e4, 0x1d1be0ef, 0x6462d92a,
        0x7d7b8f75, 0x5cda7352, 0x3a088813, 0x088aaa18, 0x8aad549e, 0x36ac54e3, 0x84576a1c, 0x656d44a3, 0x9f644ae6,
        0x873d5d9f, 0xa90cb507, 0x09a7f124, 0x0c11ed6d, 0x8f1668c9, 0xf96e017d, 0x37c981dd, 0x85bbe254, 0x93956d74,
        0x387ac8d2, 0x06997b06, 0x441fece4, 0xd527e81d, 0x8a71e224, 0xf6872d56, 0xb428f8ac, 0xe13336d7, 0xecc00246,
        0x27f002d8, 0x31ec038e, 0x7e670471, 0x0f0062c7, 0x52c07b79, 0xa7709a57, 0x88a66076, 0x6acff894, 0x0583f6b9,
        0xc3727a33, 0x744f18c0, 0x1162def0, 0x8addcb56, 0x6d953e2c, 0xc8fa8db7, 0x1d9c9892, 0x2503beb7, 0x2e44ae65,
        0x5ceaecff, 0x7425a83f, 0xd12f124e, 0x82bd6b71, 0x636cc64d, 0x3c47f7e0, 0x65acfaec, 0x7f1839a7, 0x1ede4811,
        0x934aed0b, 0xf81da84d, 0x36251261, 0xc1d72b7c, 0xb24cf65c, 0xdee033f2, 0x169840ef, 0x8e1f2895, 0xf1a6f2bb,
        0xae10af69, 0xacca6da2, 0x17fd090a, 0xddfc4b4d, 0x4abdaf10, 0x9d6d1ad4, 0x84c86189, 0x32fd3cf6, 0x3fbc8c33,
        0x0fabaf40, 0x29cb4d88, 0x743e20ea, 0x914da924, 0x1ad089b7, 0xa184ac24, 0xc9e5d72e, 0x7e2fa67c, 0xddbb901c,
        0x552a7422, 0xd53a8896, 0x8a892abb, 0x2d2b756a, 0x9c3b2962, 0x8349f3bb, 0x241c70a9, 0xed238cd4, 0xf4363804,
        0xb143c605, 0xdd94b787, 0xca7cf2b4, 0xfd1c2f61, 0xbc633b39, 0xd5be0504, 0x4b2d8645, 0xddf8e7d6, 0xcabb90e6,
        0x3d6a751f, 0x0cc51267, 0x27fb2b80, 0xb1f9f661, 0x5e7873f9, 0xdb0b487b, 0x91ce1a9a, 0x7641a141, 0xa9e904c8,
        0x546345fb, 0xa97c1779, 0x49ed8eac, 0x5c68f257, 0x73832eec, 0xc831fd54, 0xba3e7ca9, 0x28ce1bd3, 0x7980d164,
        0xd7e105bd, 0x8dd9472c, 0xb14f98f7, 0x6ed1bf9a, 0x0a862f81, 0xcd27bb61, 0x8038d51d, 0xe0470a64, 0x1858ccfd,
        0x0f37801e, 0xd3056026, 0x47c6b82f, 0x4cdc331d, 0xe0133fe5, 0x58180fde, 0x570f09eb, 0x2cd2cc65, 0xf8077f7f,
        0xfb04afaf, 0x79c5db9b, 0x18375282, 0x8f229391, 0xb2eb3875, 0x5fa60693, 0xdbc7c41c, 0x12b9b523, 0xd768226b,
        0xe6a11583, 0x60495ae4, 0x385bb19d, 0x46729e04, 0x6c07a2c2, 0xc7098b73, 0xb8cbee50, 0x737f74f2, 0x505f522e};
    return {higher64[kPow10Max + pow], lower32[kPow10Max + pow]};
#endif
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
inline int remove_trailing_zeros(uint64_t& n, int max_remove) {
    int s = max_remove;
    SCVT_CONSTEXPR_DATA uint64_t mod_inv_5 = 0xcccccccccccccccd;
    SCVT_CONSTEXPR_DATA uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;
    while (s > 1) {
        const uint64_t q = rotr2(n * mod_inv_25);
        if (q > std::numeric_limits<uint64_t>::max() / 100u) { break; }
        s -= 2, n = q;
    }
    if (s > 0) {
        const uint64_t q = rotr1(n * mod_inv_5);
        if (q <= std::numeric_limits<uint64_t>::max() / 10u) { --s, n = q; }
    }
    return max_remove - s;
}

fp_dec_fmt_t::fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, const int exp_bias) NOEXCEPT
    : significand_(0),
      prec_(fmt.prec),
      n_zeroes_(0),
      alternate_(!!(fmt.flags & fmt_flags::kAlternate)) {
    const int default_prec = 6;
    const fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    fixed_ = fp_fmt == fmt_flags::kFixed;
    if (fp2.m == 0 && fp2.exp == 0) {  // real zero
        if (fp_fmt == fmt_flags::kDefault && prec_ < 0) {
            fixed_ = true, prec_ = alternate_ ? 1 : 0;
        } else {
            prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
            if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) {
                fixed_ = true, prec_ = alternate_ ? std::max(prec_ - 1, 0) : 0;
            }
        }
        exp_ = 0, n_zeroes_ = prec_ + 1;
        return;
    }

    // Shift binary mantissa so the MSB bit is `1`
    if (fp2.exp > 0) {
        fp2.m <<= 63 - bpm;
        fp2.m |= msb64;
    } else {  // handle denormalized form
        const unsigned bpm0 = bpm;
        bpm = ulog2(fp2.m);
        fp2.m <<= 63 - bpm, fp2.exp -= bpm0 - bpm - 1;
    }

    // Obtain decimal power
    fp2.exp -= exp_bias;
    exp_ = exp2to10(fp2.exp);

    if (fp_fmt != fmt_flags::kDefault || prec_ >= 0) {
        prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
        if (fp_fmt == fmt_flags::kDefault || fp_fmt == fmt_flags::kGeneral) { prec_ = std::max(prec_ - 1, 0); }

        // Evaluate desired digit count
        const int n_digs = 1 + prec_ + (fp_fmt == fmt_flags::kFixed ? exp_ : 0);

        if (n_digs >= 0 && n_digs < kDigsPer64) {  // short decimal mantissa
            format_short_decimal(fp2, n_digs, fp_fmt);
        } else if (n_digs >= 0) {  // long decimal mantissa
            format_long_decimal(fp2, n_digs, fp_fmt);
        } else {  // zero
            exp_ = 0, n_zeroes_ = prec_ + 1;
        }

        return;
    }

    prec_ = get_exp2_dig_count(bpm + 1);  // maximal needed precision for each mantissa length

    // Calculate decimal mantissa representation :
    // Note: powers of 10 are normalized and belong [0.5, 1) range
    // To get the desired count of digits we get greater power of 10, it's equivalent to
    // multiplying the result by a certain power of 10
    const int cached_exp = prec_ - exp_;
    const uint96_t coef = get_cached_pow10(cached_exp);
    uint64_t frac = umul96x64_higher128(coef, fp2.m, significand_);
    const unsigned shift = 62 - fp2.exp - exp10to2(cached_exp);
    assert(shift > 0 && shift < 64);
    frac = shr128(significand_, frac, shift), significand_ >>= shift;
    significand_ += add64_carry(frac, 1ull << 31, frac);  // round mantissa

    // Evaluate acceptable error range to pass roundtrip test
    assert(bpm + shift >= 30);
    const int64_t delta_minus = coef.hi >> (bpm + shift - 30);
    const int64_t delta_plus = fp2.exp > 1 - exp_bias && fp2.m == msb64 ? (delta_minus >> 1) : delta_minus;

    // Try to remove two digits at once
    const int64_t err0 = hi32(frac);
    const uint64_t div100 = significand_ / 100u;
    const int64_t err100_p = make64(significand_ - 100u * div100, err0), err100_m = (100ll << 32) - err100_p;
    if (err100_p < delta_plus) {
        significand_ = div100, prec_ -= 2;
        if (err100_m < err100_p || (err100_m == err100_p && (significand_ & 1))) { ++significand_; }
    } else if (err100_m < delta_minus) {
        significand_ = div100 + 1, prec_ -= 2;
    } else {
        // Try to remove only one digit
        const uint64_t div10 = significand_ / 10u;
        const int64_t err10_p = make64(significand_ - 10u * div10, err0), err10_m = (10ll << 32) - err10_p;
        if (err10_p < delta_plus) {
            significand_ = div10, --prec_;
            if (err10_m < err10_p || (err10_m == err10_p && (significand_ & 1))) { ++significand_; }
        } else if (err10_m < delta_minus) {
            significand_ = div10 + 1, --prec_;
        } else {
            const int64_t half = 1ll << 31;
            if (err0 > half || (err0 == half && (significand_ & 1))) { ++significand_; }
        }
    }

    // Reevaluate real digit count: it can be one more than expected
    if (significand_ >= get_pow10(1 + prec_)) { ++prec_, ++exp_; }

    // Get rid of redundant trailing zeroes
    prec_ -= remove_trailing_zeros(significand_, prec_ - (exp_ <= 4 ? exp_ : 0));

    // Select format for number representation
    if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }

    // Put mandatory digit after decimal point in alternate mode:
    // it is not needed by standard, but very useful for JSON formatter
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
        if (!alternate_) { prec_ -= remove_trailing_zeros(significand_, prec_); }
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
