#include "util/string_cvt.h"

#include <array>

#if defined(_MSC_VER)
#    include <intrin.h>
#elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#endif  // defined(_MSC_VER)

using namespace util;

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

inline uint64_t lo32(uint64_t x) { return x & 0xffffffff; }

inline uint64_t hi32(uint64_t x) { return x >> 32; }

template<typename TyH, typename TyL>
uint64_t make64(TyH hi, TyL lo) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

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

#if __cplusplus < 201703L
static const ulog2_table_t g_ulog2_tbl;
#else   // __cplusplus < 201703L
constexpr ulog2_table_t g_ulog2_tbl{};
#endif  // __cplusplus < 201703L

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

inline unsigned ulog2(uint128_t x) { return x.hi != 0 ? 64 + ulog2(x.hi) : ulog2(x.lo); }

inline uint128_t shl(uint128_t x, unsigned shift) {
    return uint128_t{(x.hi << shift) | (x.lo >> (64 - shift)), x.lo << shift};
}

inline uint128_t shr(uint128_t x, unsigned shift) {
    return uint128_t{x.hi >> shift, (x.lo >> shift) | (x.hi << (64 - shift))};
}

inline uint96_t mul64x32(uint64_t x, uint32_t y, uint32_t bias = 0) {
#if defined(_MSC_VER) && defined(_M_AMD64)
    uint128_t result;
    result.lo = _umul128(x, y, &result.hi) + bias;
    if (result.lo < bias) { ++result.hi; }
    return uint96_t{make64(lo32(result.hi), hi32(result.lo)), static_cast<uint32_t>(lo32(result.lo))};
#elif defined(__GNUC__) && defined(__x86_64__)
    gcc_ints::uint128 p = static_cast<gcc_ints::uint128>(x) * static_cast<gcc_ints::uint128>(y) + bias;
    return uint96_t{static_cast<uint64_t>(p >> 32), static_cast<uint32_t>(p)};
#else
    uint64_t lower = lo32(x) * y + bias;
    return uint96_t{hi32(x) * y + hi32(lower), static_cast<uint32_t>(lo32(lower))};
#endif
}

inline uint128_t mul64x64(uint64_t x, uint64_t y, uint64_t bias = 0) {
#if defined(_MSC_VER) && defined(_M_AMD64)
    uint128_t result;
    result.lo = _umul128(x, y, &result.hi) + bias;
    if (result.lo < bias) { ++result.hi; }
    return result;
#elif defined(__GNUC__) && defined(__x86_64__)
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

inline uint128_t mul96x64_hi128(uint96_t x, uint64_t y) {
    if (lo32(y) != 0) { return mul64x64(x.hi, y, mul64x32(y, x.lo).hi); }
    y >>= 32;
    uint64_t lower = y * x.lo;
    uint96_t res96 = mul64x32(x.hi, static_cast<uint32_t>(y), static_cast<uint32_t>(hi32(lower)));
    return uint128_t{res96.hi, make64(res96.lo, lo32(lower))};
}

struct fp_m96_t {
    uint96_t m;
    int exp;
};

template<unsigned MaxWords>
struct large_int {
    enum : unsigned { kMaxWords = MaxWords };
    unsigned count = 0;
    std::array<uint64_t, kMaxWords> x;
    large_int() { x.fill(0); }
    explicit large_int(uint32_t val) : large_int() { count = 1, x[0] = val; }

    bool less(const large_int& rhv) const {
        if (count != rhv.count) { return count < rhv.count; };
        for (unsigned n = count; n > 0; --n) {
            if (x[n - 1] != rhv.x[n - 1]) { return x[n - 1] < rhv.x[n - 1]; }
        }
        return false;
    }

    large_int& subtract(const large_int& rhv) {
        unsigned n = 0;
        uint64_t carry = 0;
        for (; n < rhv.count; ++n) {
            uint64_t tmp = x[n] + carry;
            carry = tmp > x[n] || tmp < rhv.x[n] ? ~0ull : 0ull;
            x[n] = tmp - rhv.x[n];
        }
        for (; carry != 0 && n < kMaxWords; ++n) { carry = x[n]-- == 0 ? ~0ull : 0ull; }
        if (n > count) {
            count = n;
        } else {  // track zero words
            while (count > 0 && x[count - 1] == 0) { --count; }
        }
        return *this;
    }

    large_int& negate() {
        unsigned n = 0;
        uint64_t carry = 0;
        for (; n < count; ++n) {
            x[n] = carry - x[n];
            carry = carry != 0 || x[n] != 0 ? ~0ull : 0ull;
        }
        if (carry != 0 && n < kMaxWords) {
            do { --x[n++]; } while (n < kMaxWords);
        } else {  // track zero words
            while (count > 0 && x[count - 1] == 0) { --count; }
        }
        return *this;
    }

    large_int& multiply(uint32_t val) {
        uint96_t mul{0, 0};
        for (unsigned n = 0; n < count; ++n) {
            mul = mul64x32(x[n], val, static_cast<uint32_t>(hi32(mul.hi)));
            x[n] = make64(mul.hi, mul.lo);
        }
        if (hi32(mul.hi) != 0) { x[count++] = hi32(mul.hi); }
        return *this;
    }

    large_int& shr1() {
        for (unsigned n = 1; n < count; ++n) { x[n - 1] = (x[n - 1] >> 1) | (x[n] << 63); }
        if ((x[count - 1] >>= 1) == 0) { --count; }
        return *this;
    }

    large_int& invert(unsigned word_limit = kMaxWords) {
        large_int a{*this}, div{*this};
        a.negate();
        div.shr1();
        div.x[kMaxWords - 1] |= 1ull << 63;
        count = kMaxWords;
        for (unsigned n = kMaxWords; n > kMaxWords - word_limit; --n) {
            uint64_t mask = 1ull << 63;
            x[n - 1] = 0;
            do {
                if (!a.less(div)) { a.subtract(div), x[n - 1] |= mask; }
                div.shr1();
            } while (mask >>= 1);
            if (x[n - 1] == 0) { --count; }
        }
        return *this;
    }

    template<unsigned MaxWords2>
    std::pair<large_int<MaxWords2>, int> get_normalized() const {
        large_int<MaxWords2> norm;
        unsigned shift = ulog2(x[count - 1]);
        if (count <= MaxWords2) {
            if (shift > 0) {
                norm.x[MaxWords2 - count] = x[0] << (64 - shift);
                for (unsigned n = 1; n < count; ++n) {
                    norm.x[MaxWords2 - count + n] = (x[n] << (64 - shift)) | (x[n - 1] >> shift);
                }
            } else {
                norm.x[MaxWords2 - count] = 0;
                for (unsigned n = 1; n < count; ++n) { norm.x[MaxWords2 - count + n] = x[n - 1]; }
            }
        } else if (shift > 0) {
            for (unsigned n = count - MaxWords2; n < count; ++n) {
                norm.x[MaxWords2 - count + n] = (x[n] << (64 - shift)) | (x[n - 1] >> shift);
            }
        } else {
            for (unsigned n = count - MaxWords2; n < count; ++n) { norm.x[MaxWords2 - count + n] = x[n - 1]; }
        }
        norm.count = MaxWords2;
        while (norm.count > 0 && norm.x[norm.count - 1] == 0) { --norm.count; }
        return {norm, shift + 64 * (count - 1)};
    }

    fp_m96_t make_fp_m96(int exp) const {
        uint128_t v{x[kMaxWords - 1], x[kMaxWords - 2]};
        const uint64_t half = 0x80000000;
        v.lo += half;
        if (v.lo < half) { ++v.hi; }
        return fp_m96_t{v.hi, static_cast<uint32_t>(hi32(v.lo)), exp};
    }
};

struct pow_table_t {
    enum : int { kPow10Max = 400, kPow2Max = 1100, kPrecLimit = 19 };
    enum : uint64_t { kMaxMantissa10 = 10000000000000000000ull };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, 2 * kPow2Max + 1> exp2to10;
    std::array<uint64_t, 20> ten_pows;
    std::array<uint64_t, 190> decimal_mul;
    pow_table_t() {
        // 10^N -> 2^M power conversion table
        large_int<24> lrg{10};
        coef10to2[kPow10Max] = fp_m96_t{0, 0, 0};
        for (unsigned n = 0; n < kPow10Max; ++n) {
            auto norm = lrg.get_normalized<4>();
            lrg.multiply(10);
            coef10to2[kPow10Max + n + 1] = norm.first.make_fp_m96(norm.second);
            if (norm.first.count != 0) {
                coef10to2[kPow10Max - n - 1] = norm.first.invert(2).make_fp_m96(-norm.second - 1);
            } else {
                coef10to2[kPow10Max - n - 1] = fp_m96_t{0, 0, -norm.second};
            }
        }

        // 2^N -> 10^M power conversion index table
        for (int exp = -kPow2Max; exp <= kPow2Max; ++exp) {
            auto it = std::lower_bound(coef10to2.begin(), coef10to2.end(), -exp,
                                       [](decltype(*coef10to2.begin()) el, int exp) { return el.exp < exp; });
            exp2to10[kPow2Max + exp] = kPow10Max - static_cast<int>(it - coef10to2.begin());
        }

        // powers of ten 10^N, N = 0, 1, 2, ...
        uint64_t mul = 1ull;
        for (uint32_t n = 0; n < ten_pows.size(); ++n, mul *= 10) { ten_pows[n] = mul; }

        // deciman multipliers
        mul = 1ull;
        for (uint32_t n = 0; n < decimal_mul.size(); n += 10, mul *= 10) {
            decimal_mul[n] = mul;
            for (uint32_t k = 1; k < 10; ++k) { decimal_mul[n + k] = mul * k; }
        }
    }
};

static const pow_table_t g_pow_tbl;

static const int g_default_prec[] = {2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,
                                     7,  8,  8,  8,  8,  9,  9,  9,  10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
                                     13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 17};

static const char g_digits[][2] = {
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

struct fp_exp10_format {
    uint64_t mantissa = 0;
    int exp = 0;
};

static const char* starts_with(const char* p, const char* end, const char* s, size_t len) {
    if (static_cast<size_t>(end - p) < len) { return p; }
    for (const char *p1 = p, *p2 = s; p1 < end; ++p1, ++p2) {
        char ch1 = std::tolower(static_cast<unsigned char>(*p1));
        char ch2 = std::tolower(static_cast<unsigned char>(*p2));
        if (ch1 != ch2) { return p; }
    }
    return p + len;
}

inline const char* skip_spaces(const char* p, const char* end) {
    while (p < end && std::isspace(static_cast<unsigned char>(*p))) { ++p; }
    return p;
}

template<typename Ty>
unsigned get_rest_and_div100(Ty& v) {
    Ty t = v;
    v /= 100;
    return static_cast<unsigned>(t - 100 * v);
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
    if (p == end || !std::isdigit(static_cast<unsigned char>(*p))) { return p0; }
    val = static_cast<Ty>(*p++ - '0');
    while (p < end && std::isdigit(static_cast<unsigned char>(*p))) { val = 10 * val + static_cast<Ty>(*p++ - '0'); }
    if (neg) { val = ~val + 1; }  // apply sign
    return p;
}

static const char* accum_mantissa(const char* p, const char* end, uint64_t& m, int& exp) {
    for (; p < end && std::isdigit(static_cast<unsigned char>(*p)); ++p) {
        if (m < pow_table_t::kMaxMantissa10 / 10) {  // decimal mantissa can hold up to 19 digits
            m = 10 * m + static_cast<uint64_t>(*p - '0');
        } else {
            ++exp;
        }
    }
    return p;
}

static const char* to_fp_exp10(const char* p, const char* end, fp_exp10_format& fp10) {
    if (p == end) {
        return p;
    } else if (std::isdigit(static_cast<unsigned char>(*p))) {  // integer part
        fp10.mantissa = static_cast<uint64_t>(*p++ - '0');
        p = accum_mantissa(p, end, fp10.mantissa, fp10.exp);
        if (p < end && *p == '.') { ++p; }  // skip decimal point
    } else if (*p == '.' && p + 1 < end && std::isdigit(static_cast<unsigned char>(*(p + 1)))) {
        fp10.mantissa = static_cast<uint64_t>(*(p + 1) - '0');  // tenth
        fp10.exp = -1, p += 2;
    } else {
        return p;
    }
    const char* p1 = accum_mantissa(p, end, fp10.mantissa, fp10.exp);  // fractional part
    fp10.exp -= static_cast<unsigned>(p1 - p);
    if (p1 < end && (*p1 == 'e' || *p1 == 'E')) {  // optional exponent
        int exp_optional = 0;
        if ((p = to_integer(p1 + 1, end, exp_optional)) > p1 + 1) { fp10.exp += exp_optional, p1 = p; }
    }
    return p1;
}

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
            // Note: multiplication of 64-bit mantissa by 96-bit coefficient gives 128+32-bit result
            // but we drop the lowest 32 bits
            const auto& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max + fp10.exp];
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
StrTy& fmt_bin(StrTy& s, Ty val, const fmt_state& fmt) {
    std::array<char, 65> buf;
    char *last = buf.data() + buf.size(), *p = last;
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'b' : 'B'; }
    do {
        *--p = '0' + static_cast<unsigned>(val & 0x1);
        val >>= 1;
    } while (val != 0);
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return detail::fmt_adjusted(s, fmt, p, last); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_oct(StrTy& s, Ty val, const fmt_state& fmt) {
    std::array<char, 23> buf;
    char *last = buf.data() + buf.size(), *p = last;
    do {
        *--p = '0' + static_cast<unsigned>(val & 0x7);
        val >>= 3;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kShowBase)) { *--p = '0'; }
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return detail::fmt_adjusted(s, fmt, p, last); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_hex(StrTy& s, Ty val, const fmt_state& fmt) {
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
        if (fmt.width > len && !!(fmt.flags & fmt_flags::kLeadingZeroes)) {
            s.append(p, p + 2).append(fmt.width - len, '0');
            p += 2;
        } else if (fmt.width > len) {
            return detail::fmt_adjusted(s, fmt, p, last);
        }
    } else if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return detail::fmt_adjusted(s, fmt, p, last); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec_unsigned(StrTy& s, Ty val, const fmt_state& fmt) {
    std::array<char, 20> buf;
    char *last = buf.data() + buf.size(), *p = last;
    if (val < 10) {
        *--p = '0' + static_cast<unsigned>(val);
    } else {
        do {
            const char* d = &g_digits[get_rest_and_div100(val)][0];
            p -= 2;
            p[0] = d[0], p[1] = d[1];
        } while (val >= 10);
        if (val > 0) { *--p = '0' + static_cast<unsigned>(val); }
    }
    unsigned len = static_cast<unsigned>(last - p);
    if (fmt.width > len) {
        if (!(fmt.flags & fmt_flags::kLeadingZeroes)) { return detail::fmt_adjusted(s, fmt, p, last); }
        s.append(fmt.width - len, '0');
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_dec_signed(StrTy& s, Ty val, const fmt_state& fmt) {
    char sign = '+', show_sign = 0;
    if (val < 0) {
        sign = '-', show_sign = 1, val = -val;  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos) {
        show_sign = 1;
    } else if ((fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignAlign) {
        show_sign = 1, sign = ' ';
    }

    std::array<char, 21> buf;
    char *last = buf.data() + buf.size(), *p = last;
    auto uval = static_cast<typename std::make_unsigned<Ty>::type>(val);
    if (uval < 10) {
        *--p = '0' + static_cast<unsigned>(uval);
    } else {
        do {
            const char* d = &g_digits[get_rest_and_div100(uval)][0];
            p -= 2;
            p[0] = d[0], p[1] = d[1];
        } while (uval >= 10);
        if (uval > 0) { *--p = '0' + static_cast<unsigned>(uval); }
    }
    unsigned len = static_cast<unsigned>(last - p) + show_sign;
    if (fmt.width > len && !!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        if (show_sign) { s += sign; }
        s.append(fmt.width - len, '0');
    } else {
        if (show_sign) { *--p = sign; }
        if (fmt.width > len) { return detail::fmt_adjusted(s, fmt, p, last); }
    }
    return s.append(p, last);
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_unsigned(StrTy& s, Ty val, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kBin: fmt_bin(s, val, fmt); break;
        case fmt_flags::kOct: fmt_oct(s, val, fmt); break;
        case fmt_flags::kHex: fmt_hex(s, val, fmt); break;
        default: fmt_dec_unsigned(s, val, fmt); break;
    }
    return s;
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_signed<Ty>::value>>
StrTy& fmt_signed(StrTy& s, Ty val, const fmt_state& fmt) {
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kBin: {
            fmt_bin(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        } break;
        case fmt_flags::kOct: {
            fmt_oct(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        } break;
        case fmt_flags::kHex: {
            fmt_hex(s, static_cast<typename std::make_unsigned<Ty>::type>(val), fmt);
        } break;
        default: fmt_dec_signed(s, val, fmt); break;
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
StrTy& fmt_fp_exp10(StrTy& s, const fp_exp10_format& fp10, fmt_flags flags, int prec) {
    std::array<char, 25> buf;
    char *p1 = buf.data() + 20, *p = p1;
    uint64_t m = fp10.mantissa;
    if (m < 10) {
        *--p = '0' + static_cast<unsigned>(m);
    } else {
        do {
            const char* d = &g_digits[get_rest_and_div100(m)][0];
            p -= 2;
            p[0] = d[0], p[1] = d[1];
        } while (m >= 10);
        if (m > 0) { *--p = '0' + static_cast<unsigned>(m); }
    }

    // exponent
    int exp10 = fp10.exp;
    char* p_exp = p1;
    p_exp[0] = !(flags & fmt_flags::kUpperCase) ? 'e' : 'E';
    if (exp10 < 0) {
        exp10 = -exp10, p_exp[1] = '-';
    } else {
        p_exp[1] = '+';
    }
    if (exp10 >= 100) {
        const char* d = &g_digits[get_rest_and_div100(exp10)][0];
        p_exp[2] = '0' + static_cast<unsigned>(exp10), p_exp[3] = d[0], p_exp[4] = d[1];
        p_exp += 5;
    } else {
        p_exp[2] = g_digits[exp10][0], p_exp[3] = g_digits[exp10][1];
        p_exp += 4;
    }

    // integer part
    s += *p;
    if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s += '.'; }

    // fractional part + exponent
    int n_digs = static_cast<int>(p1 - p);
    if (n_digs < 1 + prec) {
        s.append(p + 1, p1).append(1 + prec - n_digs, '0').append(p1, p_exp);
    } else {
        s.append(p + 1, p_exp);
    }
    return s;
}

template<typename StrTy>
StrTy& fmt_fp_exp10_fixed(StrTy& s, const fp_exp10_format& fp10, fmt_flags flags, int prec) {
    std::array<char, 20> buf;
    char *p1 = buf.data() + 20, *p = p1;
    uint64_t m = fp10.mantissa;
    while (m >= 10) {
        const char* d = &g_digits[get_rest_and_div100(m)][0];
        p -= 2;
        p[0] = d[0], p[1] = d[1];
    }
    if (m > 0) { *--p = '0' + static_cast<unsigned>(m); }

    if (fp10.exp >= 0) {
        // integer part
        int n_digs = static_cast<int>(p1 - p);
        if (n_digs <= fp10.exp + 1) {
            s.append(p, p1).append(fp10.exp + 1 - n_digs, '0');
            if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s += '.'; }
            s.append(prec, '0');
            return s;
        }

        s.append(p, p + fp10.exp + 1);
        p += fp10.exp + 1;
        if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s += '.'; }
        s.append(p, p1).append(prec - static_cast<int>(p1 - p), '0');
        return s;
    }

    s += '0';
    if (prec > 0 || !!(flags & fmt_flags::kShowPoint)) { s += '.'; }
    s.append(-fp10.exp - 1, '0').append(p, p1);
    return s;
}

template<typename Ty, typename StrTy, typename = std::enable_if_t<std::is_floating_point<Ty>::value>>
StrTy& fmt_float(StrTy& s, Ty val, const fmt_state& fmt) {
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
    int exp = static_cast<int>((mantissa & fp_traits<Ty>::kExpMask) >> fp_traits<Ty>::kBitsPerMantissa) -
              fp_traits<Ty>::kExpBias;
    mantissa &= fp_traits<Ty>::kMantissaMask;

    if (fp_traits<Ty>::kExpBias + exp == fp_traits<Ty>::kExpMax) {
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
        if (fmt.width > static_cast<unsigned>(p - p0)) { return detail::fmt_adjusted(s, fmt, p0, p); }
        return s.append(p0, p);
    }

    fp_exp10_format fp10;
    fmt_flags fp_fmt = fmt.flags & fmt_flags::kFloatField;
    int prec = fmt.prec;
    bool optimal = false;

    // Shift binary mantissa so the MSB bit is `1`
    unsigned log = fp_traits<Ty>::kBitsPerMantissa;
    if (exp == -fp_traits<Ty>::kExpBias) {  // handle denormalized form
        log = ulog2(mantissa);
        mantissa <<= 63 - log;
        exp -= fp_traits<Ty>::kBitsPerMantissa - log - 1;
    } else {
        mantissa <<= 63 - fp_traits<Ty>::kBitsPerMantissa;
        mantissa |= 1ull << 63;
    }

    if (prec < 0) {
        prec = g_default_prec[log];
        optimal = fp_fmt == fmt_flags::kDefault;
    }

    if (mantissa != 0) {
        // Obtain decimal power
        fp10.exp = g_pow_tbl.exp2to10[pow_table_t::kPow2Max + exp];

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
            const auto& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp + n_digs - 1];
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
            const unsigned shift = 63 - exp - coef.exp;  // sum coefficient exponent as well
            // Note: resulting decimal mantissa has the form `C * 10^n_digs`, where `C` belongs [1, 20) range.
            // So, if `C >= 10` we get decimal mantissa with one excess digit. We should detect these cases
            // and divide decimal mantissa by 10 for scientific format

            int64_t err = 0;
            unsigned err_shift = 0;
            const uint64_t* err_mul = g_pow_tbl.decimal_mul.data();
            if (shift == 0 && higher_bit != 0) {
                assert(n_digs == pow_table_t::kPrecLimit);
                // Decimal mantissa contains one excess digit, which doesn't fit 64 bits.
                // We can't handle more than 64 bits while outputting the number, so remove it.
                // Note: if `higher_bit == 1` decimal mantissa has 65 significant bits
                ++fp10.exp;
                // Divide 65-bit value by 10.
                // We know the division result for 10^64 summand, so we just use it.
                const uint64_t div64 = 1844674407370955161, mod64 = 6;
                fp10.mantissa = div64 + (res128.hi + mod64) / 10;
                int64_t mod = res128.hi - 10 * fp10.mantissa;
                if (mod > 5 || (mod == 5 && (res128.lo != 0 || (fp10.mantissa & 1) != 0))) { ++fp10.mantissa; }
            } else {
                // Align fractional part of with 64-bit boundary, calculate initial error
                if (shift >= 46) {
                    res128.lo = (res128.lo >> 32) | (res128.hi << 32);
                    res128.hi = (res128.hi >> 32) | (higher_bit << 32);
                    err_shift = shift - 32;
                    err = res128.hi & ((1ull << err_shift) - 1);
                    res128.hi >>= err_shift, log += 32;
                } else {
                    err_shift = shift + 4;
                    err = ((res128.hi << 4) | (res128.lo >> 60)) & ((1ull << err_shift) - 1);
                    res128.hi = (res128.hi >> shift) | (higher_bit << (64 - shift));
                    res128.lo <<= 4, log -= 4;
                }

                if (fp_fmt != fmt_flags::kFixed && res128.hi >= g_pow_tbl.ten_pows[n_digs]) {
                    ++fp10.exp;  // one excess digit
                    // Remove one excess digit for scientific format, do 'nearest even' rounding
                    fp10.mantissa = res128.hi / 10;
                    int64_t mod = res128.hi - 10 * fp10.mantissa;
                    if (mod > 5 || (mod == 5 && (err != 0 || res128.lo != 0 || (fp10.mantissa & 1) != 0))) {
                        ++fp10.mantissa, mod -= 10;
                    }
                    err += mod << err_shift, err_mul += 10;
                } else {
                    // Do 'nearest even' rounding
                    const int64_t half = 1ull << (err_shift - 1);
                    fp10.mantissa = res128.hi;
                    if (err > half || (err == half && (res128.lo != 0 || (res128.hi & 1) != 0))) {
                        ++fp10.mantissa, err -= half << 1;
                    }
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
                    uint64_t delta_minus = (coef.m.hi >> (log + 1)) + (1ull << (63 - log));
                    uint64_t delta_plus = delta_minus;
                    if (exp > -fp_traits<Ty>::kExpBias + 1 && mantissa == 1ull << 63) {
                        delta_plus = (delta_plus >> 1) + (delta_plus & 1);
                    }
                    err <<= 1, ++err_shift;
                    if (res128.lo >= 0x80000000) { ++err; }

                    // Trim trailing unsignificant digits
                    const uint64_t max_err_mul = delta_minus << 1;
                    while (true) {
                        uint64_t t = fp10.mantissa / 10;
                        int64_t mod = fp10.mantissa - 10 * t;
                        if (mod > 0) {
                            err += err_mul[mod] << err_shift;
                            err_mul += 10;
                            int64_t err2 = (err_mul[0] << err_shift) - err;
                            assert(err >= 0 && err2 >= 0);
                            // If both round directions are acceptable, use banker's rounding
                            if (static_cast<uint64_t>(err) < delta_plus) {
                                if (static_cast<uint64_t>(err2) < delta_minus &&
                                    (err > err2 || (err == err2 && (t & 1) != 0))) {
                                    ++t, err = -err2;
                                }
                            } else if (static_cast<uint64_t>(err2) < delta_minus) {
                                ++t, err = -err2;
                            } else {
                                break;
                            }
                        } else {
                            err_mul += 10;
                        }
                        --prec, fp10.mantissa = t;
                        if ((err_mul[0] << err_shift) >= max_err_mul) {
                            // Trim trailing zeroes and break
                            while (true) {
                                uint64_t t = fp10.mantissa / 10;
                                if (fp10.mantissa > 10 * t) { break; }
                                --prec, fp10.mantissa = t;
                            }
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
                    prec = n_digs - 1;
                    // Trim trailing zeroes
                    while (true) {
                        uint64_t t = fp10.mantissa / 10;
                        if (fp10.mantissa > 10 * t) { break; }
                        --prec, fp10.mantissa = t;
                    }
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec0) {
                        fp_fmt = fmt_flags::kFixed, prec = std::max(prec - fp10.exp, 0);
                    }
                }
            }
        }
    }

    if (fp10.mantissa == 0) {
        fp10.exp = 0;
        if (fp_fmt == fmt_flags::kDefault) { fp_fmt = fmt_flags::kFixed, prec = 0; }
    }

    if (fmt.width > 0) {
        unsigned len = show_sign;
        if (fp_fmt == fmt_flags::kFixed) {
            len += fmt_fp_exp10_fixed_len(fp10, fmt.flags, prec);
        } else {
            len += fmt_fp_exp10_len(fp10, fmt.flags, prec);
        }
        if (fmt.width > len) {
            if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
                size_t left = 0, right = 0;
                switch (fmt.flags & fmt_flags::kAdjustField) {
                    case fmt_flags::kLeft: right = static_cast<size_t>(fmt.width) - len; break;
                    case fmt_flags::kInternal: {
                        right = static_cast<size_t>(fmt.width) - len, left = right >> 1;
                        right -= left;
                    } break;
                    default: left = static_cast<size_t>(fmt.width) - len; break;
                }
                s.append(left, fmt.fill);
                if (show_sign) { s += sign; }
                if (fp_fmt == fmt_flags::kFixed) {
                    fmt_fp_exp10_fixed(s, fp10, fmt.flags, prec);
                } else {
                    fmt_fp_exp10(s, fp10, fmt.flags, prec);
                }
                s.append(right, fmt.fill);
                return s;
            }
            if (show_sign) { s += sign; }
            s.append(fmt.width - len, '0');
        } else if (show_sign) {
            s += sign;
        }
    } else if (show_sign) {
        s += sign;
    }

    if (fp_fmt == fmt_flags::kFixed) { return fmt_fp_exp10_fixed(s, fp10, fmt.flags, prec); }
    return fmt_fp_exp10(s, fp10, fmt.flags, prec);
}

}  // namespace scvt

#define IMPLEMENT_STANDARD_STRING_CONVERTERS(ty, from_string_func, to_string_func) \
    /*static*/ const char* string_converter<ty>::from_string(const char* first, const char* last, ty& val) { \
        const char* p = scvt::skip_spaces(first, last); \
        last = scvt::from_string_func(p, last, val); \
        return last > p ? last : first; \
    } \
    /*static*/ std::string& string_converter<ty>::to_string(std::string& s, ty val, const fmt_state& fmt) { \
        return scvt::to_string_func(s, val, fmt); \
    } \
    /*static*/ char_buf_appender& string_converter<ty>::to_string(char_buf_appender& s, ty val, const fmt_state& fmt) { \
        return scvt::to_string_func(s, val, fmt); \
    } \
    /*static*/ char_n_buf_appender& string_converter<ty>::to_string(char_n_buf_appender& s, ty val, \
                                                                    const fmt_state& fmt) { \
        return scvt::to_string_func(s, val, fmt); \
    }

IMPLEMENT_STANDARD_STRING_CONVERTERS(int8_t, to_integer, fmt_signed)
IMPLEMENT_STANDARD_STRING_CONVERTERS(int16_t, to_integer, fmt_signed)
IMPLEMENT_STANDARD_STRING_CONVERTERS(int32_t, to_integer, fmt_signed)
IMPLEMENT_STANDARD_STRING_CONVERTERS(int64_t, to_integer, fmt_signed)
IMPLEMENT_STANDARD_STRING_CONVERTERS(uint8_t, to_integer, fmt_unsigned)
IMPLEMENT_STANDARD_STRING_CONVERTERS(uint16_t, to_integer, fmt_unsigned)
IMPLEMENT_STANDARD_STRING_CONVERTERS(uint32_t, to_integer, fmt_unsigned)
IMPLEMENT_STANDARD_STRING_CONVERTERS(uint64_t, to_integer, fmt_unsigned)
IMPLEMENT_STANDARD_STRING_CONVERTERS(float, to_float, fmt_float)
IMPLEMENT_STANDARD_STRING_CONVERTERS(double, to_float, fmt_float)

/*static*/ const char* string_converter<char>::from_string(const char* first, const char* last, char& val) {
    const char* p = scvt::skip_spaces(first, last);
    if (p == last) { return first; }
    val = *p;
    return ++p;
}

/*static*/ const char* string_converter<bool>::from_string(const char* first, const char* last, bool& val) {
    const char *p = scvt::skip_spaces(first, last), *p0 = p;
    if ((p = scvt::starts_with(p, last, "true", 4)) > p0) {
        val = true;
    } else if ((p = scvt::starts_with(p, last, "false", 5)) > p0) {
        val = false;
    } else if (p < last && std::isdigit(static_cast<unsigned char>(*p))) {
        val = false;
        do {
            if (*p++ != '0') { val = true; }
        } while (p < last && std::isdigit(static_cast<unsigned char>(*p)));
    } else {
        return first;
    }
    return p;
}
