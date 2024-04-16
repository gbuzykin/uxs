#include "uxs/stringcvt_impl.h"

namespace uxs {
#if __cplusplus < 201703L
namespace detail {
const char_tbl_t g_char_tbl;
}
#endif  // __cplusplus < 201703L
namespace scvt {

const fmt_opts g_default_opts;

#if __cplusplus < 201703L && \
    (SCVT_USE_COMPILER_128BIT_EXTENSIONS == 0 || \
     (!(defined(_MSC_VER) && defined(_M_X64)) && !(defined(__GNUC__) && defined(__x86_64__))))
const ulog2_table_t g_ulog2_tbl;
#endif

inline std::uint64_t umul128(std::uint64_t x, std::uint64_t y, std::uint64_t bias, std::uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    std::uint64_t result_lo = _umul128(x, y, &result_hi);
    result_hi += _addcarry_u64(0, result_lo, bias, &result_lo);
    return result_lo;
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * y + bias;
    result_hi = static_cast<std::uint64_t>(p >> 64);
    return static_cast<std::uint64_t>(p);
#else
    const std::uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    const std::uint64_t mid1 = lo32(x) * hi32(y) + hi32(bias), mid2 = hi32(x) * lo32(y) + hi32(lower);
    const std::uint64_t t = lo32(mid1) + lo32(mid2);
    result_hi = higher + hi32(mid1) + hi32(mid2) + hi32(t);
    return make64(lo32(t), lo32(lower));
#endif
}

inline std::uint64_t umul128(std::uint64_t x, std::uint64_t y, std::uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return _umul128(x, y, &result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * y;
    result_hi = static_cast<std::uint64_t>(p >> 64);
    return static_cast<std::uint64_t>(p);
#else
    return umul128(x, y, 0, result_hi);
#endif
}

inline std::uint64_t umul64x32(std::uint64_t x, std::uint32_t y, std::uint32_t bias, std::uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && \
    ((defined(_MSC_VER) && defined(_M_X64)) || (defined(__GNUC__) && defined(__x86_64__)))
    return umul128(x, y, bias, result_hi);
#else
    const std::uint64_t lower = lo32(x) * y + bias;
    const std::uint64_t higher = hi32(x) * y + hi32(lower);
    result_hi = hi32(higher);
    return make64(lo32(higher), lo32(lower));
#endif
}

#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_M_X64)
// VS2013 compiler has a bug concerning these intrinsics
using one_bit_t = unsigned char;
inline one_bit_t add64_carry(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t carry = 0) {
    return _addcarry_u64(carry, a, b, &c);
}
inline one_bit_t sub64_borrow(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t borrow = 0) {
    return _subborrow_u64(borrow, a, b, &c);
}
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && SCVT_HAS_BUILTIN(__builtin_addcll) && \
    SCVT_HAS_BUILTIN(__builtin_subcll)
using one_bit_t = unsigned long long;
inline one_bit_t add64_carry(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t carry = 0) {
    c = __builtin_addcll(a, b, carry, &carry);
    return carry;
}
inline one_bit_t sub64_borrow(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t borrow = 0) {
    c = __builtin_subcll(a, b, borrow, &borrow);
    return borrow;
}
#else
using one_bit_t = std::uint8_t;
inline one_bit_t add64_carry(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t carry = 0) {
    b += carry;
    c = a + b;
    return c < b || b < carry;
}
inline one_bit_t sub64_borrow(std::uint64_t a, std::uint64_t b, std::uint64_t& c, one_bit_t borrow = 0) {
    b += borrow;
    c = a - b;
    return a < b || b < borrow;
}
#endif

struct uint128_t {
    std::uint64_t hi;
    std::uint64_t lo;
};

inline std::uint64_t udiv128(uint128_t x, std::uint64_t y) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && _MSC_VER >= 1920 && defined(_M_X64)
    std::uint64_t r;
    return _udiv128(x.hi, x.lo, y, &r);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    return static_cast<std::uint64_t>(((static_cast<gcc_ints::uint128>(x.hi) << 64) | x.lo) / y);
#else
    uint128_t denominator{y, 0};
    std::uint64_t quotient = 0, bit = msb64;
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
std::uint64_t bignum_mul32(std::uint64_t* x, unsigned sz, std::uint32_t mul, std::uint32_t bias) {
    assert(sz > 0);
    std::uint64_t higher = bias;
    std::uint64_t* x0 = x;
    x += sz;
    do { --x, *x = umul64x32(*x, mul, static_cast<std::uint32_t>(higher), higher); } while (x != x0);
    return higher;
}

inline std::uint64_t bignum_mul(std::uint64_t* x, unsigned sz, std::uint64_t mul) {
    assert(sz > 0);
    std::uint64_t higher;
    std::uint64_t* x0 = x;
    x += sz - 1;
    *x = umul128(*x, mul, higher);
    while (x != x0) { --x, *x = umul128(*x, mul, higher, higher); }
    return higher;
}

inline std::uint64_t bignum_mul(std::uint64_t* x, const std::uint64_t* y, unsigned sz, std::uint64_t mul) {
    assert(sz > 0);
    std::uint64_t higher;
    std::uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    *x = umul128(*y, mul, higher);
    while (x != x0) { --x, --y, *x = umul128(*y, mul, higher, higher); }
    return higher;
}

inline std::uint64_t bignum_addmul(std::uint64_t* x, const std::uint64_t* y, unsigned sz, std::uint64_t mul) {
    assert(sz > 0);
    std::uint64_t higher;
    std::uint64_t* x0 = x;
    x += sz - 1, y += sz;
    one_bit_t carry = add64_carry(*x, umul128(*--y, mul, higher), *x);
    while (x != x0) { --x, carry = add64_carry(*x, umul128(*--y, mul, higher, higher), *x, carry); }
    return higher + carry;
}

inline void bignum_mul_vec(std::uint64_t* x, const std::uint64_t* y, unsigned sz_x, unsigned sz_y) {
    std::uint64_t* x0 = x;
    x += sz_x - 1;
    *x = bignum_mul(x + 1, y, sz_y, *x);
    while (x != x0) { --x, *x = bignum_addmul(x + 1, y, sz_y, *x); }
}

inline unsigned bignum_trim_unused(const std::uint64_t* x, unsigned sz) {
    while (sz > 0 && !x[sz - 1]) { --sz; }
    return sz;
}

inline std::uint64_t bignum_submul(std::uint64_t* x, const std::uint64_t* y, unsigned sz_x, unsigned sz,
                                   std::uint64_t mul) {
    // returns: x[0]...x[sz_x-1]000... - mul * y[0]...y[sz-1], MSB of y[0] is 1
    assert(sz > 0);
    one_bit_t borrow = 0;
    std::uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    std::uint64_t higher, lower = umul128(*y, mul, higher);
    if (sz > sz_x) {
        std::uint64_t* x1 = x0 + sz_x;
        borrow = sub64_borrow(0, lower, *x);
        while (x != x1) { --x, --y, borrow = sub64_borrow(0, umul128(*y, mul, higher, higher), *x, borrow); }
    } else {
        borrow = sub64_borrow(*x, lower, *x);
    }
    while (x != x0) { --x, --y, borrow = sub64_borrow(*x, umul128(*y, mul, higher, higher), *x, borrow); }
    return higher + borrow;
}

inline one_bit_t bignum_add(std::uint64_t* x, const std::uint64_t* y, unsigned sz) {
    assert(sz > 0);
    std::uint64_t* x0 = x;
    x += sz - 1, y += sz - 1;
    one_bit_t carry = add64_carry(*x, *y, *x);
    while (x != x0) { --x, --y, carry = add64_carry(*x, *y, *x, carry); }
    return carry;
}

inline std::uint64_t bignum_divmod(std::uint64_t integral, std::uint64_t* x, const std::uint64_t* y, unsigned sz_x,
                                   unsigned sz) {
    // integral.x[0]...x[sz_x-1]000... / 0.y[0]...y[sz-1], MSB of y[0] is 1
    // returns: quotient, 0.x[0]...x[sz-1] - remainder
    assert(sz > 0);
    uint128_t num128{integral, sz_x ? x[0] : 0};
    if (sz_x > 1) { num128.hi += add64_carry(num128.lo, 1, num128.lo); }
    // Calculate quotient upper estimation.
    // In case if `num128.hi < 2^62` it is guaranteed, that q_est - q <= 1
    std::uint64_t quotient = udiv128(num128, y[0]);
    if (!quotient) { return 0; }
    // Subtract scaled denominator from numerator
    if (integral < bignum_submul(x, y, sz_x, sz, quotient)) {
        // In case of overflow just add one denominator
        bignum_add(x, y, sz), --quotient;
    }
    return quotient;
}

inline std::uint64_t shl128(std::uint64_t x_hi, std::uint64_t x_lo, unsigned shift) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return __shiftleft128(x_lo, x_hi, shift);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 x = ((static_cast<gcc_ints::uint128>(x_hi) << 64) | x_lo) << shift;
    return static_cast<std::uint64_t>(x >> 64);
#else
    return (x_hi << shift) | (x_lo >> (64 - shift));
#endif
}

inline std::uint64_t bignum_shift_left(std::uint64_t* x, unsigned sz, unsigned shift) {
    assert(sz > 0);
    std::uint64_t* x0 = x + sz - 1;
    std::uint64_t higher = *x >> (64 - shift);
    while (x != x0) { *x = shl128(*x, *(x + 1), shift), ++x; }
    *x <<= shift;
    return higher;
}

inline std::uint64_t shr128(std::uint64_t x_hi, std::uint64_t x_lo, unsigned shift) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return __shiftright128(x_lo, x_hi, shift);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 x = ((static_cast<gcc_ints::uint128>(x_hi) << 64) | x_lo) >> shift;
    return static_cast<std::uint64_t>(x);
#else
    return (x_hi << (64 - shift)) | (x_lo >> shift);
#endif
}

inline std::uint64_t bignum_shift_right(std::uint64_t higher, std::uint64_t* x, unsigned sz, unsigned shift) {
    assert(sz > 0);
    std::uint64_t* x0 = x;
    x += sz - 1;
    std::uint64_t lower = *x << (64 - shift);
    while (x != x0) { *x = shr128(*(x - 1), *x, shift), --x; }
    *x = shr128(higher, *x, shift);
    return lower;
}

// --------------------------

struct bignum_t {
    const std::uint64_t* x;
    unsigned sz;
    unsigned exp;
};

SCVT_CONSTEXPR_DATA int bigpow10_tbl_size = 19;
SCVT_FORCE_INLINE bignum_t get_bigpow10(unsigned index) noexcept {
    static SCVT_CONSTEXPR_DATA std::uint64_t bigpow10[] = {
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
    static SCVT_CONSTEXPR_DATA unsigned bigpow10_offset[bigpow10_tbl_size + 1] = {
        0, 1, 3, 5, 8, 12, 16, 21, 27, 33, 40, 48, 56, 65, 75, 85, 96, 108, 120, 133};
    static SCVT_CONSTEXPR_DATA unsigned bigpow10_exp[bigpow10_tbl_size] = {
        59, 119, 179, 239, 298, 358, 418, 478, 538, 597, 657, 717, 777, 837, 896, 956, 1016, 1076, 1136};
    assert(index < bigpow10_tbl_size);
    const unsigned* const offset = &bigpow10_offset[index];
    return bignum_t{bigpow10 + offset[0], offset[1] - offset[0], bigpow10_exp[index]};
}

// --------------------------

struct uint96_t {
    std::uint64_t hi;
    std::uint32_t lo;
};

inline std::uint64_t umul96x32(uint96_t x, std::uint32_t y, std::uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return umul128(x.hi, static_cast<std::uint64_t>(y) << 32, static_cast<std::uint64_t>(x.lo) * y, result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = ((static_cast<gcc_ints::uint128>(x.hi) << 32) | x.lo) * y;
    result_hi = static_cast<std::uint64_t>(p >> 64);
    return static_cast<std::uint64_t>(p);
#else
    const std::uint64_t lower = static_cast<std::uint64_t>(x.lo) * y;
    const std::uint64_t mid = lo32(x.hi) * y + hi32(lower);
    result_hi = hi32(x.hi) * y + hi32(mid);
    return make64(lo32(mid), lo32(lower));
#endif
}

inline std::uint64_t umul96x64_higher128(uint96_t x, std::uint64_t y, std::uint64_t& result_hi) {
#if SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(_MSC_VER) && defined(_M_X64)
    return umul128(x.hi, y, __umulh(static_cast<std::uint64_t>(x.lo) << 32, y), result_hi);
#elif SCVT_USE_COMPILER_128BIT_EXTENSIONS != 0 && defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(y) * (static_cast<std::uint64_t>(x.lo) << 32);
    p = static_cast<gcc_ints::uint128>(y) * x.hi + (p >> 64);
    result_hi = static_cast<std::uint64_t>(p >> 64);
    return static_cast<std::uint64_t>(p);
#else
    const std::uint64_t lower = lo32(y) * x.lo;
    return umul128(x.hi, y, hi32(y) * x.lo + hi32(lower), result_hi);
#endif
}

SCVT_CONSTEXPR_DATA int pow10_max = 344;
SCVT_FORCE_INLINE uint96_t get_cached_pow10(int pow) noexcept {
    assert(pow >= -pow10_max && pow <= pow10_max);
    static SCVT_CONSTEXPR_DATA std::uint64_t higher64[] = {
        0x98ee4a22ecf3188b, 0xe3e27a444d8d98b7, 0xa9c98d8ccb009506, 0xfd00b897478238d0, 0xbc807527ed3e12bc,
        0x8c71dcd9ba0b4925, 0xd1476e2c07286faa, 0x9becce62836ac577, 0xe858ad248f5c22c9, 0xad1c8eab5ee43b66,
        0x80fa687f881c7f8e, 0xc0314325637a1939, 0x8f31cc0937ae58d2, 0xd5605fcdcf32e1d6, 0x9efa548d26e5a6e1,
        0xece53cec4a314ebd, 0xb080392cc4349dec, 0x8380dea93da4bc60, 0xc3f490aa77bd60fc, 0x91ff83775423cc06,
        0xd98ddaee19068c76, 0xa21727db38cb002f, 0xf18899b1bc3f8ca1, 0xb3f4e093db73a093, 0x8613fd0145877585,
        0xc7caba6e7c5382c8, 0x94db483840b717ef, 0xddd0467c64bce4a0, 0xa54394fe1eedb8fe, 0xf64335bcf065d37d,
        0xb77ada0617e3bbcb, 0x88b402f7fd75539b, 0xcbb41ef979346bca, 0x97c560ba6b0919a5, 0xe2280b6c20dd5232,
        0xa87fea27a539e9a5, 0xfb158592be068d2e, 0xbb127c53b17ec159, 0x8b61313bbabce2c6, 0xcfb11ead453994ba,
        0x9abe14cd44753b52, 0xe69594bec44de15b, 0xabcc77118461cefc, 0x8000000000000000, 0xbebc200000000000,
        0x8e1bc9bf04000000, 0xd3c21bcecceda100, 0x9dc5ada82b70b59d, 0xeb194f8e1ae525fd, 0xaf298d050e4395d6,
        0x82818f1281ed449f, 0xc2781f49ffcfa6d5, 0x90e40fbeea1d3a4a, 0xd7e77a8f87daf7fb, 0xa0dc75f1778e39d6,
        0xefb3ab16c59b14a2, 0xb2977ee300c50fe7, 0x850fadc09923329e, 0xc646d63501a1511d, 0x93ba47c980e98cdf,
        0xdc21a1171d42645d, 0xa402b9c5a8d3a6e7, 0xf46518c2ef5b8cd1, 0xb616a12b7fe617aa, 0x87aa9aff79042286,
        0xca28a291859bbf93, 0x969eb7c47859e743, 0xe070f78d3927556a, 0xa738c6bebb12d16c, 0xf92e0c3537826145,
        0xb9a74a0637ce2ee1, 0x8a5296ffe33cc92f, 0xce1de40642e3f4b9, 0x9991a6f3d6bf1765, 0xe4d5e82392a40515,
        0xaa7eebfb9df9de8d, 0xfe0efb53d30dd4d7, 0xbd49d14aa79dbc82, 0x8d07e33455637eb2, 0xd226fc195c6a2f8c,
        0x9c935e00d4b9d8d2, 0xe950df20247c83fd, 0xadd57a27d29339f6, 0x81842f29f2cce375, 0xc0fe908895cf3b44,
        0x8fcac257558ee4e6, 0xd6444e39c3db9b09};
    static SCVT_CONSTEXPR_DATA std::uint32_t lower32[] = {
        0x9028bed3, 0xfd1b1b23, 0x680efdaf, 0x8920b099, 0xc6050837, 0x9ff0c08b, 0x1af5af66, 0x4ee367f9, 0xd1b34010,
        0xda324365, 0x7ce66635, 0xfa911156, 0xd1b2ecb9, 0xfb1e4a9b, 0xc47bc501, 0xa4f8bf56, 0xbd8d794e, 0x4247cb9e,
        0xbedbfc44, 0x7b6306a3, 0x3badd625, 0xb8ada00e, 0xdc44e6c4, 0x59ed2167, 0xbd06742d, 0xfe64a52f, 0xa8c2a44f,
        0xac7cb3f7, 0xc2974eb5, 0x4d4617b6, 0x09ce6ebb, 0x11dbcb02, 0x4f2b40a0, 0xdccd87a0, 0x25c6da64, 0x3f2398d7,
        0xeed6e2f1, 0x5560c018, 0x2323ac4b, 0x67de18ee, 0xc4926a96, 0x4c2ebe68, 0xfdc20d2b, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0xf0200000, 0x5dcfab08, 0x9670b12b, 0xbff8f10e, 0x3cbf6b72, 0xbc8955e9, 0xdc33745f,
        0x696361ae, 0xc5cfe94f, 0x58edec92, 0x03e2cf6c, 0xb281e1fd, 0xc66f336c, 0x76707544, 0x5f16206d, 0x7eb25866,
        0x577b986b, 0x90fb44d3, 0x7d7b8f75, 0x9f644ae6, 0x85bbe254, 0xb428f8ac, 0xa7709a57, 0x6d953e2c, 0x82bd6b71,
        0x36251261, 0xacca6da2, 0x0fabaf40, 0xddbb901c, 0xed238cd4, 0x4b2d8645, 0xdb0b487b, 0x73832eec, 0x6ed1bf9a,
        0x47c6b82f, 0x79c5db9b, 0xe6a11583, 0x505f522e, 0x213a4f0b, 0x848ce346};
    SCVT_CONSTEXPR_DATA int step_pow = 3;
    int n = (pow10_max + pow) >> step_pow, k = pow & ((1 << step_pow) - 1);
    uint96_t result{higher64[n], lower32[n]};
    if (!k) { return result; }
    static SCVT_CONSTEXPR_DATA std::uint32_t mul10[] = {0,          0xa0000000, 0xc8000000, 0xfa000000,
                                                        0x9c400000, 0xc3500000, 0xf4240000, 0x98968000};
    std::uint64_t t = umul96x32(result, mul10[k], result.hi);
    if (!(result.hi & msb64)) {
        result.hi = shl128(result.hi, t, 1);
        t <<= 1;
    }
    result.lo = static_cast<std::uint32_t>(hi32(t + (1ull << 31)));
    return result;
}

// --------------------------

inline int exp10to2(int exp) {
    SCVT_CONSTEXPR_DATA std::int64_t ln10_ln2 = 0x35269e12f;  // 2^32 * ln(10) / ln(2)
    return static_cast<int>(hi32(ln10_ln2 * exp));
}

static std::uint64_t fp10_to_fp2_slow(fp10_t& fp10, unsigned bpm, int exp_max) noexcept {
    unsigned sz_num = fp10.bits_used;
    std::uint64_t* m10 = &fp10.bits[max_fp10_mantissa_size - sz_num];

    // Calculate lower digs_per_64-aligned decimal exponent
    int index = digs_per_64 * 1000000 + fp10.exp;
    const std::uint64_t mul10 = get_pow10(divmod<digs_per_64>(index));
    index -= 1000000;
    if (mul10 != 1) {
        const std::uint64_t higher = bignum_mul(m10, sz_num, mul10);
        if (higher) { *--m10 = higher, ++sz_num; }
    }

    // Obtain binary exponent
    const int exp_bias = exp_max >> 1;
    const int log = ulog2(m10[0]);
    int exp2 = exp_bias + log + (static_cast<int>(sz_num - 1) << 6 /* *64 */);

    // Align numerator so 2 left bits are zero (reserved)
    if (log < 61) {
        bignum_shift_left(m10, sz_num, 61 - log);
    } else if (log > 61) {
        const std::uint64_t lower = bignum_shift_right(0, m10, sz_num, log - 61);
        if (lower) { m10[sz_num++] = lower; }
    }

    std::uint64_t m;
    --sz_num;  // count only m10[n] where n > 0
    if (index < 0) {
        const int index0 = -1 - index;
        index = std::min<int>(index0, bigpow10_tbl_size - 1);
        bignum_t denominator = get_bigpow10(index);

        std::uint64_t big_denominator[max_fp10_mantissa_size + 1];  // all powers >= -1100 (aligned to -1116) will fit
        if (index < index0) {
            // Calculate big denominator multiplying powers of 10 from table
            std::memcpy(big_denominator, denominator.x, denominator.sz * sizeof(std::uint64_t));
            do {
                const int index2 = std::min<int>(index0 - index - 1, bigpow10_tbl_size - 1);
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
        if (index > 0) {
            if (--index >= bigpow10_tbl_size) { return static_cast<std::uint64_t>(exp_max) << bpm; }  // infinity
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
        return static_cast<std::uint64_t>(exp_max) << bpm;  // infinity
    } else if (exp2 < -static_cast<int>(bpm)) {
        return 0;  // zero
    }

    // When `exp2 <= 0` mantissa will be denormalized further, so store the real mantissa length
    const unsigned n_bits = exp2 > 0 ? 1 + bpm : bpm + exp2;

    // Do banker's or `nearest even` rounding
    const std::uint64_t lsb = 1ull << (63 - n_bits);
    const std::uint64_t half = lsb >> 1;
    const std::uint64_t frac = m & (lsb - 1);
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
    if (exp2 <= 0) { return m; }                                                   // denormalized
    return (static_cast<std::uint64_t>(exp2) << bpm) | (m & ((1ull << bpm) - 1));  // normalized
}

std::uint64_t fp10_to_fp2(fp10_t& fp10, unsigned bpm, int exp_max) noexcept {
    unsigned sz_num = fp10.bits_used;
    std::uint64_t m = fp10.bits[max_fp10_mantissa_size - sz_num];

    // Note, that decimal mantissa can contain up to 772 digits. So, all numbers with
    // powers less than -772 - 324 = -1096 are zeroes in fact. We round this power to -1100
    if (m == 0 || fp10.exp < -1100) {
        return 0;                                           // zero
    } else if (fp10.exp > 310) {                            // too big power even for one specified digit
        return static_cast<std::uint64_t>(exp_max) << bpm;  // infinity
    }

    // If too many digits are specified or decimal power is too great use slow algorithm
    if (sz_num > 1 || fp10.exp < -pow10_max || fp10.exp > pow10_max) { return fp10_to_fp2_slow(fp10, bpm, exp_max); }

    // Obtain binary exponent
    const int exp_bias = exp_max >> 1;
    const int log = ulog2(m);
    int exp2 = 1 + exp_bias + log + exp10to2(fp10.exp);
    if (log < 63) { m <<= 63 - log; }

    // Obtain binary mantissa
    unsigned shift = 64;
    const uint96_t coef = get_cached_pow10(fp10.exp);
    std::uint64_t frac = umul96x64_higher128(coef, m, m);
    if (!(m & msb64)) { --shift, --exp2; }

    if (exp2 >= exp_max) {
        return static_cast<std::uint64_t>(exp_max) << bpm;  // infinity
    } else if (exp2 < -static_cast<int>(bpm)) {
        return 0;  // zero
    }

    // When `exp2 <= 0` mantissa will be denormalized further, so store the real mantissa length
    const unsigned n_bits = exp2 > 0 ? 1 + bpm : bpm + exp2;

    // Shift mantissa to the right position
    const std::uint64_t half = 1ull << 31;
    shift -= n_bits;
    if (shift < 64) {
        frac = shr128(m, frac, shift), m >>= shift;
        m += add64_carry(frac, half, frac);  // round mantissa
    } else {                                 // shift == 64
        m = add64_carry(m, half, frac);      // round mantissa
    }

    frac >>= 32;  // drop lower 32 bits
    if (frac > half) {
        ++m;                        // round to upper
    } else if (frac >= half - 1) {  // round direction is undefined: use slow algorithm
        return fp10_to_fp2_slow(fp10, bpm, exp_max);
    }
    if (m & (1ull << n_bits)) {  // overflow
        // Note: the value can become normalized if `exp == 0` or infinity if `exp == exp_max - 1`
        // Note: in case of overflow mantissa will be zero
        ++exp2;
    }

    // Compose floating point value
    if (exp2 <= 0) { return m; }                                                   // denormalized
    return (static_cast<std::uint64_t>(exp2) << bpm) | (m & ((1ull << bpm) - 1));  // normalized
}

// --------------------------

fp_hex_fmt_t::fp_hex_fmt_t(const fp_m64_t& fp2, const fmt_opts& fmt, unsigned bpm, int exp_bias) noexcept
    : significand_(fp2.m), exp_(fp2.exp), prec_(fmt.prec), n_zeroes_(0),
      alternate_(!!(fmt.flags & fmt_flags::alternate)) {
    if (significand_ == 0 && exp_ == 0) {  // real zero
        exp_ = 0, prec_ = n_zeroes_ = prec_ < 0 ? 0 : (prec_ & 0xffff);
        return;
    }

    // Align, so 4 left bits are an integer part:
    // 1 (or 2 after round) for normalized, 0 for denormalized
    significand_ <<= 60 - bpm;
    if (exp_ > 0) {
        significand_ |= 1ull << 60, exp_ -= exp_bias;
    } else {
        exp_ = 1 - exp_bias;
    }
    if (prec_ < 0) {
        prec_ = 15;
        while (!(significand_ & 0xf)) { significand_ >>= 4, --prec_; }
    } else if (prec_ < 15) {  // round
        const std::uint64_t half = 1ull << (59 - 4 * prec_);
        const std::uint64_t frac = significand_ & ((half << 1) - 1);
        significand_ >>= 60 - 4 * prec_;
        if (frac > half || (frac == half && (significand_ & 1))) { ++significand_; }
    } else {
        prec_ &= 0xffff;
        n_zeroes_ = prec_ - 15;
    }
}

// --------------------------

inline int exp2to10(int exp) {
    SCVT_CONSTEXPR_DATA std::int64_t ln2_ln10 = 0x4d104d42;  // 2^32 * ln(2) / ln(10)
    return static_cast<int>(hi32(ln2_ln10 * exp));
}

// Compilers should be able to optimize this into the ror instruction
inline std::uint64_t rotr1(std::uint64_t n) { return (n >> 1) | (n << 63); }
inline std::uint64_t rotr2(std::uint64_t n) { return (n >> 2) | (n << 62); }

// Removes trailing zeros and returns the number of zeros removed
SCVT_FORCE_INLINE int remove_trailing_zeros(std::uint64_t& n, int max_remove) {
    int s = max_remove;
    SCVT_CONSTEXPR_DATA std::uint64_t mod_inv_5 = 0xcccccccccccccccd;
    SCVT_CONSTEXPR_DATA std::uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;
    while (s > 1) {
        const std::uint64_t q = rotr2(n * mod_inv_25);
        if (q > std::numeric_limits<std::uint64_t>::max() / 100u) { break; }
        s -= 2, n = q;
    }
    if (s > 0) {
        const std::uint64_t q = rotr1(n * mod_inv_5);
        if (q <= std::numeric_limits<std::uint64_t>::max() / 10u) { --s, n = q; }
    }
    return max_remove - s;
}

fp_dec_fmt_t::fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, int exp_bias) noexcept
    : significand_(0), prec_(fmt.prec), n_zeroes_(0), alternate_(!!(fmt.flags & fmt_flags::alternate)) {
    const int default_prec = 6;
    const fmt_flags fp_fmt = fmt.flags & fmt_flags::float_field;
    fixed_ = fp_fmt == fmt_flags::fixed;
    if (fp2.m == 0 && fp2.exp == 0) {  // real zero
        if (fp_fmt == fmt_flags::none) {
            fixed_ = true, prec_ = prec_ < 0 && !!(fmt.flags & fmt_flags::json_compat) ? 1 : 0;
        } else {
            prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
            if (fp_fmt == fmt_flags::general) { fixed_ = true, prec_ = alternate_ ? std::max(prec_ - 1, 0) : 0; }
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

    if (fp_fmt != fmt_flags::none || prec_ >= 0) {
        prec_ = prec_ < 0 ? default_prec : (prec_ & 0xffff);
        if (fp_fmt == fmt_flags::none || fp_fmt == fmt_flags::general) { prec_ = std::max(prec_ - 1, 0); }

        // Evaluate desired digit count
        const int n_digs = 1 + prec_ + (fp_fmt == fmt_flags::fixed ? exp_ : 0);

        if (n_digs >= 0 && n_digs < digs_per_64) {  // short decimal mantissa
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
    const std::uint64_t half = 1ull << 31;
    const int cached_exp = prec_ - exp_;
    const uint96_t coef = get_cached_pow10(cached_exp);
    std::uint64_t frac = umul96x64_higher128(coef, fp2.m, significand_);
    const unsigned shift = 62 - fp2.exp - exp10to2(cached_exp);
    assert(shift > 0 && shift < 64);
    frac = shr128(significand_, frac, shift), significand_ >>= shift;
    significand_ += add64_carry(frac, half, frac);  // round mantissa
    frac >>= 32;                                    // drop lower 32 bits

    // Evaluate acceptable error range to pass roundtrip test
    assert(bpm + shift >= 30);
    const std::uint64_t delta_minus = coef.hi >> (bpm + shift - 30);
    const std::uint64_t delta_plus = fp2.m == msb64 && fp2.exp > 1 - exp_bias ? (delta_minus >> 1) : delta_minus;

    // Try to remove two digits at once
    const std::uint64_t significand0 = significand_;
    const std::uint64_t err100_p = frac + (divmod<100u>(significand_) << 32), err100_m = (100ull << 32) - err100_p;
    if (err100_p < delta_plus) {
        if (err100_m < err100_p || (err100_m == err100_p && (significand_ & 1))) { ++significand_; }
        prec_ -= 2;
    } else if (err100_m < delta_minus) {
        ++significand_, prec_ -= 2;
    } else {
        // Try to remove only one digit
        significand_ = significand0;
        const std::uint64_t err10_p = frac + (divmod<10u>(significand_) << 32), err10_m = (10ull << 32) - err10_p;
        if (err10_p < delta_plus) {
            if (err10_m < err10_p || (err10_m == err10_p && (significand_ & 1))) { ++significand_; }
            --prec_;
        } else if (err10_m < delta_minus) {
            ++significand_, --prec_;
        } else {
            significand_ = significand0;
            if (frac > half || (frac == half && (significand_ & 1))) { ++significand_; }
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
    if (!!(fmt.flags & fmt_flags::json_compat) && prec_ == 0) { significand_ *= 10u, prec_ = 1; }
}

void fp_dec_fmt_t::format_short_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept {
    assert(n_digs < digs_per_64);
    ++n_digs;  // one additional digit for rounding

    // Calculate decimal mantissa representation :
    // Note: powers of 10 are normalized and belong [0.5, 1) range
    // To get the desired count of digits we get greater power of 10, it's equivalent to
    // multiplying the result by a certain power of 10
    const int cached_exp = n_digs - exp_ - 1;
    const uint96_t coef = get_cached_pow10(cached_exp);
    std::uint64_t frac = umul96x64_higher128(coef, fp2.m, significand_);
    const unsigned shift = 62 - fp2.exp - exp10to2(cached_exp);
    assert(shift > 0 && shift < 64);
    frac = shr128(significand_, frac, shift), significand_ >>= shift;
    significand_ += add64_carry(frac, 1ull << 31, frac);  // round mantissa
    frac >>= 32;                                          // drop lower 32 bits

    // Note, that the first digit formally can belong [1, 20) range, so we can get one digit more
    if (fp_fmt != fmt_flags::fixed && significand_ >= get_pow10(n_digs)) {  // one excess digit
        // Remove one excess digit for scientific format
        const std::uint64_t err = frac + (divmod<100u>(significand_) << 32);
        if (err > (50ull << 32)) {
            ++significand_;
        } else if (err >= (50ull << 32) - 1) {
            format_short_decimal_slow(fp2, n_digs, fp_fmt);
            goto finish;
        }
        ++exp_;
    } else {
        const std::uint64_t err = frac + (divmod<10u>(significand_) << 32);
        if (err > (5ull << 32)) {
            ++significand_;
        } else if (err >= (5ull << 32) - 1) {
            format_short_decimal_slow(fp2, n_digs, fp_fmt);
            goto finish;
        }
        if (significand_ >= get_pow10(n_digs - 1)) {
            ++exp_;  // one excess digit
            if (fp_fmt != fmt_flags::fixed) {
                // Remove one excess digit for scientific format
                // Note: `significand` is exact power of 10 in this case
                significand_ /= 10u;
            }
        }
    }

finish:
    if (significand_ == 0) {
        exp_ = 0, n_zeroes_ = prec_ + 1;
    } else if (fp_fmt == fmt_flags::none || fp_fmt == fmt_flags::general) {
        // Select format for number representation
        if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }
        if (!alternate_ || fp_fmt == fmt_flags::none) { prec_ -= remove_trailing_zeros(significand_, prec_); }
    }
}

void fp_dec_fmt_t::format_short_decimal_slow(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept {
    // `max_pow10_size` uint64-s are enough to hold all (normalized!) 10^n, n <= 324 + digs_per_64
    std::uint64_t num[max_pow10_size + 1];  // +1 to multiply by uint64
    unsigned sz_num = 1;

    // Calculate upper digs_per_64-aligned decimal exponent and multiplier for correction
    int index = digs_per_64 * 1000000 + exp_ - n_digs;
    const std::uint64_t mul10 = get_pow10(digs_per_64 - 1 - divmod<digs_per_64>(index));
    index -= 999999;

    auto mul_and_shift = [&num, &sz_num](std::uint64_t m, std::uint64_t mul, int shift) {
        std::uint64_t digs;
        num[0] = umul128(m, mul, digs);
        if (shift > 0) {
            digs = shl128(digs, num[0], shift), num[0] <<= shift;
        } else if (shift < 0) {
            num[1] = num[0] << (64 + shift), num[0] = shr128(digs, num[0], -shift), digs >>= -shift, ++sz_num;
        }
        return digs;
    };

    if (index > 0) {
        const bignum_t denominator = get_bigpow10(index - 1);
        significand_ = bignum_divmod(mul_and_shift(fp2.m, mul10, fp2.exp - denominator.exp), num, denominator.x, sz_num,
                                     denominator.sz);
        if (significand_ && sz_num < denominator.sz) { sz_num = denominator.sz; }
    } else if (index < 0) {
        const bignum_t multiplier = get_bigpow10(-1 - index);
        const int shift = 2 + fp2.exp + multiplier.exp;
        sz_num += multiplier.sz;
        num[0] = bignum_mul(num + 1, multiplier.x, sz_num - 1, fp2.m);
        significand_ = bignum_mul(num, sz_num, mul10);
        if (shift > 0) {
            significand_ = (significand_ << shift) | bignum_shift_left(num, sz_num, shift);
        } else if (shift < 0) {
            num[sz_num] = bignum_shift_right(significand_, num, sz_num, -shift);
            significand_ >>= -shift, ++sz_num;
        }
    } else {
        significand_ = mul_and_shift(fp2.m, mul10, 1 + fp2.exp);
    }

    // Note, that the first digit formally can belong [1, 20) range, so we can get one digit more
    if (fp_fmt != fmt_flags::fixed && significand_ >= get_pow10(n_digs)) {
        ++exp_;  // one excess digit
        // Remove one excess digit for scientific format
        const unsigned r = static_cast<unsigned>(divmod<100u>(significand_));
        if (r > 50u || (r == 50u && ((significand_ & 1) || bignum_trim_unused(num, sz_num) /* nearest even */))) {
            ++significand_;
        }
    } else {
        const unsigned r = static_cast<unsigned>(divmod<10u>(significand_));
        if (r > 5u || (r == 5u && ((significand_ & 1) || bignum_trim_unused(num, sz_num) /* nearest even */))) {
            ++significand_;
        }
        if (significand_ >= get_pow10(n_digs - 1)) {
            ++exp_;  // one excess digit
            if (fp_fmt != fmt_flags::fixed) {
                // Remove one excess digit for scientific format
                // Note: `significand` is exact power of 10 in this case
                significand_ /= 10u;
            }
        }
    }
}

void fp_dec_fmt_t::format_long_decimal(const fp_m64_t& fp2, int n_digs, fmt_flags fp_fmt) noexcept {
    assert(n_digs >= digs_per_64);
    ++n_digs;  // one additional digit for rounding

    // `max_pow10_size` uint64-s are enough to hold all (normalized!) 10^n, n <= 324 + digs_per_64
    std::uint64_t num[max_pow10_size + 1];  // +1 to multiply by uint64
    unsigned sz_num = 1;

    // Digits are generated by packs: the first pack length can be in range
    // [1, digs_per_64], all next are of digs_per_64 length

    // Calculate lower digs_per_64-aligned decimal exponent
    int index = digs_per_64 * 1000000 + exp_;
    const unsigned digs_first_len = 1 + divmod<digs_per_64>(index);  // first digit pack length
    assert(digs_first_len <= digs_per_64);
    index -= 1000000;

    char* p = digs_buf_;
    auto gen_first_digit_pack = [this, fp_fmt, digs_first_len, &p, &n_digs](std::uint64_t digs) {
        // Note, that the first digit formally can belong [1, 20) range,
        // so we can get one digit more while calculating the first pack
        if (digs < get_pow10(digs_first_len)) {
            gen_digits(p + digs_first_len, digs);
        } else {
            ++exp_, gen_digits(++p + digs_first_len, digs);
            if (fp_fmt != fmt_flags::fixed) { --n_digs; }
        }
        p += digs_first_len, n_digs -= digs_first_len;
    };

    if (index > 0) {
        bignum_t denominator = get_bigpow10(--index);
        const unsigned shift = fp2.exp - denominator.exp;

        num[0] = fp2.m << shift;
        std::uint64_t digs = bignum_divmod(fp2.m >> (64 - shift), num, denominator.x, 1, denominator.sz);
        assert(digs);

        gen_first_digit_pack(digs);

        sz_num = denominator.sz;
        while (n_digs > 0 && (sz_num = bignum_trim_unused(num, sz_num))) {
            assert(index >= 0);
            const unsigned digs_len = std::min(n_digs, digs_per_64);
            if (digs_len < digs_per_64) {
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
        std::uint64_t digs;
        if (index < 0) {
            const bignum_t multiplier = get_bigpow10(-1 - index);
            const unsigned shift = 2 + fp2.exp + multiplier.exp;
            sz_num += multiplier.sz;
            num[0] = bignum_mul(num + 1, multiplier.x, sz_num - 1, fp2.m);
            digs = bignum_shift_left(num, sz_num, shift);
        } else {
            digs = fp2.m >> (63 - fp2.exp), num[0] = fp2.m << (1 + fp2.exp);
        }

        gen_first_digit_pack(digs);

        while (n_digs > 0 && (sz_num = bignum_trim_unused(num, sz_num))) {
            const unsigned digs_len = std::min(n_digs, digs_per_64);
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
                if (fp_fmt != fmt_flags::fixed) { --n_digs; }
                break;
            }
        }
        ++*(p - 1);
    }

    // Note: `n_digs` is trailing zero count
    if (fp_fmt == fmt_flags::none || fp_fmt == fmt_flags::general) {
        // Select format for number representation
        if (exp_ >= -4 && exp_ <= prec_) { fixed_ = true, prec_ -= exp_; }
        if (!alternate_ || fp_fmt == fmt_flags::none) {  // trim trailing zeroes
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

template UXS_EXPORT std::uint32_t to_integral_common(const char*, const char*, const char*&, std::uint32_t) noexcept;
template UXS_EXPORT std::uint64_t to_integral_common(const char*, const char*, const char*&, std::uint64_t) noexcept;
template UXS_EXPORT std::uint64_t to_float_common(const char*, const char*, const char*& last, const unsigned,
                                                  const int) noexcept;
template UXS_EXPORT bool to_boolean(const char*, const char*, const char*& last) noexcept;

template UXS_EXPORT std::uint32_t to_integral_common(const wchar_t*, const wchar_t*, const wchar_t*&,
                                                     std::uint32_t) noexcept;
template UXS_EXPORT std::uint64_t to_integral_common(const wchar_t*, const wchar_t*, const wchar_t*&,
                                                     std::uint64_t) noexcept;
template UXS_EXPORT std::uint64_t to_float_common(const wchar_t*, const wchar_t*, const wchar_t*& last, const unsigned,
                                                  const int) noexcept;
template UXS_EXPORT bool to_boolean(const wchar_t*, const wchar_t*, const wchar_t*& last) noexcept;

template UXS_EXPORT void fmt_integral_common(membuffer&, std::int32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(membuffer&, std::int64_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(membuffer&, std::uint32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(membuffer&, std::uint64_t, const fmt_opts&);
template UXS_EXPORT void fmt_character(membuffer&, char, const fmt_opts&);
template UXS_EXPORT void fmt_float_common(membuffer&, std::uint64_t, const fmt_opts&, const unsigned, const int);
template UXS_EXPORT void fmt_boolean(membuffer&, bool, const fmt_opts&);

template UXS_EXPORT void fmt_integral_common(wmembuffer&, std::int32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(wmembuffer&, std::int64_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(wmembuffer&, std::uint32_t, const fmt_opts&);
template UXS_EXPORT void fmt_integral_common(wmembuffer&, std::uint64_t, const fmt_opts&);
template UXS_EXPORT void fmt_character(wmembuffer&, wchar_t, const fmt_opts&);
template UXS_EXPORT void fmt_float_common(wmembuffer&, std::uint64_t, const fmt_opts&, const unsigned, const int);
template UXS_EXPORT void fmt_boolean(wmembuffer&, bool, const fmt_opts&);

}  // namespace scvt

}  // namespace uxs
