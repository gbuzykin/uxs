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
    for (uint32_t n = 0; n < ten_pows.size(); ++n, mul *= 10) { ten_pows[n] = mul; }

    // decimal multipliers
    mul = 1ull;
    for (uint32_t n = 0; n < decimal_mul.size(); n += 10, mul *= 10) {
        decimal_mul[n] = mul << 32;
        for (uint32_t k = 1; k < 10; ++k) { decimal_mul[n + k] = (mul * k) << 32; }
    }
}

const pow_table_t g_pow_tbl;

const int g_default_prec[] = {2,  2,  2,  3,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  7,  7,
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

}  // namespace scvt

#define SCVT_INSTANTIATE_STANDARD_STRING_CONVERTER(ty) \
    template const char* string_converter<ty>::from_string(const char* first, const char* last, ty& val); \
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
    template const wchar_t* string_converter<ty>::from_string(const wchar_t* first, const wchar_t* last, ty& val); \
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

template basic_dynbuf_appender<char>::~basic_dynbuf_appender();
template void basic_dynbuf_appender<char>::grow(size_t extra);
template basic_dynbuf_appender<wchar_t>::~basic_dynbuf_appender();
template void basic_dynbuf_appender<wchar_t>::grow(size_t extra);

}  // namespace util
