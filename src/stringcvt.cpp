#include "util/stringcvt_impl.h"

namespace util {
namespace scvt {

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
        return fp_m96_t{{v.hi, static_cast<uint32_t>(hi32(v.lo))}, exp};
    }
};

struct pow_table_t {
    enum : int { kPow10Max = 400, kPow2Max = 1100, kPrecLimit = 19 };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, 2 * kPow2Max + 1> exp2to10;
    std::array<uint64_t, 20> ten_pows;
    std::array<int64_t, 70> decimal_mul;
    pow_table_t();
};

pow_table_t::pow_table_t() {
    // 10^N -> 2^M power conversion table
    large_int<24> lrg{10};
    coef10to2[kPow10Max] = fp_m96_t{{0, 0}, 0};
    for (unsigned n = 0; n < kPow10Max; ++n) {
        auto norm = lrg.get_normalized<4>();
        lrg.multiply(10);
        coef10to2[kPow10Max + n + 1] = norm.first.make_fp_m96(norm.second);
        if (norm.first.count != 0) {
            coef10to2[kPow10Max - n - 1] = norm.first.invert(2).make_fp_m96(-norm.second - 1);
        } else {
            coef10to2[kPow10Max - n - 1] = fp_m96_t{{0, 0}, -norm.second};
        }
    }

    // 2^N -> 10^M power conversion index table
    for (int exp = -kPow2Max; exp <= kPow2Max; ++exp) {
        auto it = std::lower_bound(coef10to2.begin(), coef10to2.end(), -exp,
                                   [](const fp_m96_t& el, int exp) { return el.exp < exp; });
        exp2to10[kPow2Max + exp] = kPow10Max - static_cast<int>(it - coef10to2.begin());
    }

    // powers of ten 10^N, N = 0, 1, 2, ...
    uint64_t mul = 1ull;
    for (uint32_t n = 0; n < ten_pows.size(); ++n, mul *= 10u) { ten_pows[n] = mul; }

    // decimal multipliers
    mul = 1ull;
    for (uint32_t n = 0; n < decimal_mul.size(); n += 10, mul *= 10u) {
        decimal_mul[n] = mul << 32;
        for (uint32_t k = 1; k < 10; ++k) { decimal_mul[n + k] = (mul * k) << 32; }
    }
}

static const pow_table_t g_pow_tbl;

static const int g_default_prec[] = {2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,
                                     7,  8,  8,  8,  8,  9,  9,  9,  10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
                                     13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 17};

const char g_digits[][2] = {
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

uint64_t fp_exp10_to_exp2(fp_m64_t fp10, const unsigned bpm, const int exp_max) {
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
    // So we use some heuristic to detect exact halves. But there are reasons to believe that this approach is
    // theoretically justified. If we take long enough range of bits after the point where we need to break the
    // series then analyzing this bits we can make the decision in which direction to round. The following bits:
    //               x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
    //               x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
    //              | needed    | to be truncated   | unknown   |
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

// --------------------------

fp_m64_t fp_exp2_to_exp10(fp_m64_t fp2, fmt_flags& flags, int& prec, unsigned bpm, const int exp_bias) {
    if (fp2.m != 0 || fp2.exp > 0) {
        bool optimal = false;

        // Shift binary mantissa so the MSB bit is `1`
        if (fp2.exp > 0) {
            fp2.m <<= 63 - bpm;
            fp2.m |= 1ull << 63;
        } else {  // handle denormalized form
            const unsigned bpm0 = bpm;
            bpm = ulog2(fp2.m);
            fp2.m <<= 63 - bpm, fp2.exp -= bpm0 - bpm - 1;
        }

        if (prec < 0) {
            prec = g_default_prec[bpm], optimal = !flags;
        } else {
            prec &= 0xffff;
        }

        // Obtain decimal power
        fp_m64_t fp10{0, g_pow_tbl.exp2to10[pow_table_t::kPow2Max - exp_bias + fp2.exp]};

        // Evaluate desired decimal mantissa length (its integer part)
        // Note: integer part of decimal mantissa is the digits to output,
        // fractional part of decimal mantissa is to be rounded and dropped
        if (!flags) { prec = std::max(prec - 1, 0); }
        int n_digs = 1 + prec;                                   // desired digit count for scientific format
        if (flags == fmt_flags::kFixed) { n_digs += fp10.exp; }  // for fixed format

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
            // It seems that exact halves detection is not possible without exact integral numbers tracking.
            // So we use some heuristic to detect exact halves. But there are reasons to believe that this approach is
            // theoretically justified. If we take long enough range of bits after the point where we need to break the
            // series then analyzing this bits we can make the decision in which direction to round. The following bits:
            //               x x x x x x 1 0 0 0 0 0 0 0 0 0 . . . . . . we consider as exact half
            //               x x x x x x 0 1 1 1 1 1 1 1 1 1 . . . . . . and this is exact half too
            //              | needed    | to be truncated   | unknown   |
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
                fp10.m = div64 + (res128.hi + mod64) / 10u;
                unsigned mod = static_cast<unsigned>(res128.hi - 10u * fp10.m);
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

                if (flags != fmt_flags::kFixed && res128.hi >= g_pow_tbl.ten_pows[n_digs]) {
                    ++fp10.exp, err_mul += 10;  // one excess digit
                    // Remove one excess digit for scientific format, do 'nearest even' rounding
                    fp10.m = res128.hi / 10u;
                    err = res128.hi - 10u * fp10.m;
                    if (err > 5 || (err == 5 && (res128.lo != 0 || (fp10.m & 1) != 0))) { ++fp10.m, err -= 10; }
                } else {
                    // Do 'nearest even' rounding
                    const uint64_t half = 1ull << 63;
                    const uint64_t frac = (res128.hi & 1) == 0 ? res128.lo + half - 1 : res128.lo + half;
                    fp10.m = res128.hi;
                    if (frac < res128.lo) { ++fp10.m, err = -1; }
                    if (fp10.m >= g_pow_tbl.ten_pows[n_digs]) {
                        ++fp10.exp;  // one excess digit
                        if (flags != fmt_flags::kFixed) {
                            // Remove one excess digit for scientific format
                            // Note: `fp10.m` is exact power of 10 in this case
                            fp10.m /= 10u;
                        }
                    }
                }
            }

            if (fp10.m != 0) {
                if (optimal) {
                    // Evaluate acceptable error range to pass roundtrip test
                    assert(bpm + shift >= 30);
                    const unsigned shift2 = bpm + shift - 30;
                    int64_t delta_minus = (coef.m.hi >> shift2) | (1ull << (64 - shift2));
                    int64_t delta_plus = delta_minus;
                    if (fp2.exp > 1 && fp2.m == 1ull << 63) { delta_plus >>= 1; }
                    err = (err << 32) | (res128.lo >> 32);

                    // Trim trailing insignificant digits
                    while (true) {
                        uint64_t t = fp10.m / 10u;
                        unsigned mod = static_cast<unsigned>(fp10.m - 10u * t);
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
                        if (*err_mul + err >= delta_plus && *err_mul - err >= delta_minus) {
                            prec -= remove_trailing_zeros(fp10.m);
                            break;
                        }
                    }
                    if (prec < 0) { ++fp10.exp, prec = 0; }
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec) { flags = fmt_flags::kFixed, prec -= fp10.exp; }
                } else if (!flags) {
                    const int prec0 = prec;
                    prec = n_digs - 1 - remove_trailing_zeros(fp10.m);
                    // Select format for number representation
                    if (fp10.exp >= -4 && fp10.exp <= prec0) {
                        flags = fmt_flags::kFixed, prec = std::max(prec - fp10.exp, 0);
                    }
                }

                return fp10;
            }
        }
    }

    if (!flags) {
        flags = fmt_flags::kFixed, prec = 0;
    } else if (prec < 0) {
        prec = 0;
    } else {
        prec &= 0xffff;
    }

    return fp_m64_t{0, 0};
}

}  // namespace scvt

#define SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(ty) \
    template size_t string_converter<ty>::from_string(std::string_view s, ty& val); \
    template unlimbuf_appender& string_converter<ty>::to_string(unlimbuf_appender& s, ty val, const fmt_state& fmt); \
    template limbuf_appender& string_converter<ty>::to_string(limbuf_appender& s, ty val, const fmt_state& fmt); \
    template dynbuf_appender& string_converter<ty>::to_string(dynbuf_appender& s, ty val, const fmt_state& fmt);
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(int8_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(int16_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(int32_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(int64_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(uint8_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(uint16_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(uint32_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(uint64_t)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(float)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(double)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(char)
SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(bool)
#undef SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER

#define SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(ty) \
    template size_t string_converter<ty>::from_string(std::wstring_view s, ty& val); \
    template wunlimbuf_appender& string_converter<ty>::to_string(wunlimbuf_appender& s, ty val, const fmt_state& fmt); \
    template wlimbuf_appender& string_converter<ty>::to_string(wlimbuf_appender& s, ty val, const fmt_state& fmt); \
    template wdynbuf_appender& string_converter<ty>::to_string(wdynbuf_appender& s, ty val, const fmt_state& fmt);
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(int8_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(int16_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(int32_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(int64_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(uint8_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(uint16_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(uint32_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(uint64_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(float)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(double)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(wchar_t)
SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER(bool)
#undef SCVT_INSTANTIATE_STANDARD_WSTRING_CONVERTER

template class basic_dynbuf_appender<char>;
template class basic_dynbuf_appender<wchar_t>;

}  // namespace util
