#include "util/string_ext.h"

#include <array>
#include <cmath>

using namespace util;

std::wstring util::from_utf8_to_wide(std::string_view s) {
    uint32_t code;
    std::wstring result;
    result.reserve(s.size());
    for (auto p = s.begin(), p1 = p; (p1 = from_utf8(p, s.end(), &code)) > p; p = p1) {
        to_utf16(code, std::back_inserter(result));
    }
    return result;
}

std::string util::from_wide_to_utf8(std::wstring_view s) {
    uint32_t code;
    std::string result;
    result.reserve(s.size());
    for (auto p = s.begin(), p1 = p; (p1 = from_utf16(p, s.end(), &code)) > p; p = p1) {
        to_utf8(code, std::back_inserter(result));
    }
    return result;
}

std::string_view util::trim_string(std::string_view s) {
    auto p1 = s.begin(), p2 = s.end();
    while ((p1 < p2) && std::isblank(*p1)) { ++p1; }
    while ((p1 < p2) && std::isblank(*(p2 - 1))) { --p2; }
    return s.substr(p1 - s.begin(), p2 - p1);
}

std::vector<std::string> util::unpack_strings(std::string_view s, char sep) {
    std::vector<std::string> result;
    unpack_strings(s, sep, std::back_inserter(result));
    return result;
}

std::string util::encode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    std::string result;
    result.reserve(s.size());
    auto p = s.begin(), p0 = p;
    for (; p < s.end(); ++p) {
        auto pos = symb.find(*p);
        if (pos != std::string_view::npos) {
            result.append(p0, p);
            result += '\\';
            result += code[pos];
            p0 = p + 1;
        }
    }
    result.append(p0, p);
    return result;
}

std::string util::decode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    std::string result;
    result.reserve(s.size());
    auto p = s.begin(), p0 = p;
    for (; p < s.end(); ++p) {
        if (*p != '\\') { continue; }
        result.append(p0, p);
        p0 = p + 1;
        if (++p == s.end()) { break; }
        auto pos = code.find(*p);
        if (pos != std::string_view::npos) {
            result += symb[pos];
            p0 = p + 1;
        }
    }
    result.append(p0, p);
    return result;
}

std::pair<unsigned, unsigned> util::parse_flag_string(
    std::string_view s, const std::vector<std::pair<std::string_view, unsigned>>& flag_tbl) {
    std::pair<unsigned, unsigned> flags(0, 0);
    separate_words(s, ' ', function_caller([&](std::string_view flag) {
                       bool add_flag = (flag[0] != '-');
                       if ((flag[0] == '+') || (flag[0] == '-')) { flag = flag.substr(1); }
                       auto it = std::find_if(flag_tbl.begin(), flag_tbl.end(),
                                              [flag](decltype(*flag_tbl.begin()) el) { return el.first == flag; });
                       if (it == flag_tbl.end()) {
                       } else if (add_flag) {
                           flags.first |= it->second;
                       } else {
                           flags.second |= it->second;
                       }
                   }));
    return flags;
}

int util::compare_strings_nocase(std::string_view lhs, std::string_view rhs) {
    auto p1_end = lhs.begin() + std::min(lhs.size(), rhs.size());
    for (auto p1 = lhs.begin(), p2 = rhs.begin(); p1 < p1_end; ++p1, ++p2) {
        auto ch1 = std::toupper(*p1), ch2 = std::toupper(*p2);
        if (std::string_view::traits_type::lt(ch1, ch2)) {
            return -1;
        } else if (std::string_view::traits_type::lt(ch2, ch1)) {
            return 1;
        }
    }
    if (lhs.size() < rhs.size()) {
        return -1;
    } else if (rhs.size() < lhs.size()) {
        return 1;
    }
    return 0;
}

std::string util::to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

//---------------------------------------------------------------------------------
// String converter implementation

namespace scvt {

//-----------------------------------------------------------------------------
// Converter utility functions

template<typename Ty>
struct fp_format_traits;

template<>
struct fp_format_traits<double> {
    enum : int { kTotalBits = 64, kBitsPerMantissa = 52 };
    enum : uint64_t {
        kSignBit = 1ull << (kTotalBits - 1),
        kMantissaMask = (1ull << kBitsPerMantissa) - 1,
        kExpMask = ~kMantissaMask & ~kSignBit
    };
    enum : int { kExpMax = kExpMask >> kBitsPerMantissa, kExpBias = kExpMax >> 1 };
    static uint64_t to_u64(const double& f) { return *reinterpret_cast<const uint64_t*>(&f); }
    static double from_u64(const uint64_t& u64) { return *reinterpret_cast<const double*>(&u64); }
};

template<>
struct fp_format_traits<float> {
    enum : int { kTotalBits = 32, kBitsPerMantissa = 23 };
    enum : uint64_t {
        kSignBit = 1ull << (kTotalBits - 1),
        kMantissaMask = (1ull << kBitsPerMantissa) - 1,
        kExpMask = ~kMantissaMask & ~kSignBit & ((1ull << kTotalBits) - 1)
    };
    enum : int { kExpMax = kExpMask >> kBitsPerMantissa, kExpBias = kExpMax >> 1 };
    static uint64_t to_u64(const float& f) { return *reinterpret_cast<const uint32_t*>(&f); }
    static float from_u64(const uint64_t& u64) { return *reinterpret_cast<const float*>(&u64); }
};

struct uint128_t {
    uint128_t() = default;
    uint128_t(uint64_t in_hi, uint64_t in_lo) : hi(in_hi), lo(in_lo) {}
    uint64_t hi = 0;
    uint64_t lo = 0;
};

inline uint64_t lo32(uint64_t x) { return x & 0xffffffff; }

inline uint64_t hi32(uint64_t x) { return x >> 32; }

template<typename Ty>
uint64_t make64(Ty hi, Ty lo) {
    return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

static struct ulog2_table_t {
    std::array<int, 256> n_bit;
    ulog2_table_t() {
        for (uint32_t n = 0; n < n_bit.size(); ++n) {
            uint32_t u8 = n;
            n_bit[n] = 0;
            while (u8 >>= 1) { ++n_bit[n]; }
        }
    }
} g_ulog2_tbl;

inline int ulog2(uint32_t x) {
    int bias = 0;
    if (x >= (1 << 16)) { x >>= 16, bias += 16; }
    if (x >= (1 << 8)) { x >>= 8, bias += 8; }
    return bias + g_ulog2_tbl.n_bit[x];
}

inline int ulog2(uint64_t x) {
    if (x >= (1ull << 32)) { return 32 + ulog2(static_cast<uint32_t>(hi32(x))); }
    return ulog2(static_cast<uint32_t>(lo32(x)));
}

inline int ulog2(uint128_t x) { return (x.hi != 0) ? 64 + ulog2(x.hi) : ulog2(x.lo); }

inline uint64_t shr_lo64(uint128_t x, int shift) {
    if (shift < 64) { return (x.lo >> shift) | (x.hi << (64 - shift)); }
    if (shift < 128) { return x.hi >> (shift - 64); }
    return 0;
}

inline uint128_t mul64x32(uint64_t x, uint32_t y, uint32_t bias = 0) {
    uint64_t lower = lo32(x) * y + bias;
    uint64_t mid = hi32(x) * y + hi32(lower);
    return uint128_t{hi32(mid), make64(mid, lo32(lower))};
}

inline uint64_t mul64x32_hi64(uint64_t x, uint32_t y, uint32_t bias = 0) {
    return hi32(x) * y + hi32(lo32(x) * y + bias);
}

inline uint128_t mul64x64(uint64_t x, uint64_t y, uint64_t bias = 0) {
    uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    uint64_t mid = lo32(x) * hi32(y) + hi32(bias), mid0 = mid;
    mid += hi32(x) * lo32(y) + hi32(lower);
    if (mid < mid0) { higher += 0x100000000; }
    return uint128_t{higher + hi32(mid), make64(lo32(mid), lo32(lower))};
}

inline uint64_t mul64x64_hi64(uint64_t x, uint64_t y, uint64_t bias = 0) {
    uint64_t lower = lo32(x) * lo32(y) + lo32(bias), higher = hi32(x) * hi32(y);
    uint64_t mid = lo32(x) * hi32(y) + hi32(bias), mid0 = mid;
    mid += hi32(x) * lo32(y) + hi32(lower);
    if (mid < mid0) { higher += 0x100000000; }
    return higher + hi32(mid);
}

struct fp_m96_t {
    fp_m96_t() = default;
    fp_m96_t(uint64_t in_m, uint32_t in_m2, int in_exp) : m(in_m), m2(in_m2), exp(in_exp) {}
    uint64_t m = 0;
    uint32_t m2 = 0;
    int exp = 0;
};

template<int MaxWords>
struct large_int {
    enum : int { kMaxWords = MaxWords };
    int count = 0;
    std::array<uint64_t, kMaxWords> x;
    large_int() { x.fill(0); }
    explicit large_int(uint32_t val) : large_int() { count = 1, x[0] = val; }

    bool less(const large_int& rhv) const {
        if (count != rhv.count) { return count < rhv.count; };
        for (int n = count; n > 0; --n) {
            if (x[n - 1] != rhv.x[n - 1]) { return x[n - 1] < rhv.x[n - 1]; }
        }
        return false;
    }

    void subtract(const large_int& rhv) {
        int n = 0;
        uint64_t carry = 0;
        assert(count >= rhv.count);
        for (; n < rhv.count; ++n) {
            uint64_t tmp = x[n] + carry;
            carry = ((tmp > x[n]) || (tmp < rhv.x[n])) ? ~0ull : 0ull;
            x[n] = tmp - rhv.x[n];
        }
        for (; (carry != 0) && (n < count); ++n) { carry = (x[n]-- == 0) ? ~0ull : 0ull; }
        if (n == count) {  // trim zeroes
            while ((count > 0) && (x[count - 1] == 0)) { --count; }
        }
    }

    void negate() {
        int n = 0;
        uint64_t carry = 0;
        for (; n < count; ++n) {
            x[n] = carry - x[n];
            carry = ((carry != 0) || (x[n] != 0)) ? ~0ull : 0ull;
        }
        if (carry != 0) {
            for (; n < kMaxWords; ++n) { --x[n]; }
            count = kMaxWords;
        } else {  // trim zeroes
            while ((count > 0) && (x[count - 1] == 0)) { --count; }
        }
    }

    void multiply(uint32_t val) {
        uint128_t mul;
        for (int n = 0; n < count; ++n) {
            mul = mul64x32(x[n], val, static_cast<uint32_t>(mul.hi));
            x[n] = mul.lo;
        }
        if (mul.hi != 0) { x[count++] = mul.hi; }
    }

    void shr1() {
        for (int n = 1; n < count; ++n) { x[n - 1] = (x[n - 1] >> 1) | (x[n] << 63); }
        if ((x[count - 1] >>= 1) == 0) { --count; }
    }

    bool invert(int word_limit = kMaxWords) {
        if (count == 0) { return false; }
        large_int a{*this}, div{*this};
        a.negate();
        div.shr1();
        div.x[kMaxWords - 1] |= 1ull << 63;
        count = kMaxWords;
        for (int n = kMaxWords; n > kMaxWords - word_limit; --n) {
            uint64_t mask = 1ull << 63;
            x[n - 1] = 0;
            do {
                if (!a.less(div)) { a.subtract(div), x[n - 1] |= mask; }
                div.shr1();
            } while (mask >>= 1);
            if (x[n - 1] == 0) { --count; }
        }
        return true;
    }

    template<int MaxWords2>
    int get_normalized(large_int<MaxWords2>& norm) const {
        int shift = ulog2(x[count - 1]);
        for (int n = 0; n < MaxWords2 - count; ++n) { norm.x[n] = 0; }
        if (count <= MaxWords2) {
            if (shift > 0) {
                norm.x[MaxWords2 - count] = x[0] << (64 - shift);
                for (int n = 1; n < count; ++n) {
                    norm.x[MaxWords2 - count + n] = (x[n] << (64 - shift)) | (x[n - 1] >> shift);
                }
            } else {
                norm.x[MaxWords2 - count] = 0;
                for (int n = 1; n < count; ++n) { norm.x[MaxWords2 - count + n] = x[n - 1]; }
            }
        } else {
            if (shift > 0) {
                for (int n = count - MaxWords2; n < count; ++n) {
                    norm.x[MaxWords2 - count + n] = (x[n] << (64 - shift)) | (x[n - 1] >> shift);
                }
            } else {
                for (int n = count - MaxWords2; n < count; ++n) { norm.x[MaxWords2 - count + n] = x[n - 1]; }
            }
        }
        norm.count = MaxWords2;
        while ((norm.count > 0) && (norm.x[norm.count - 1] == 0)) { --norm.count; }
        return shift + 64 * (count - 1);
    }

    fp_m96_t make_fp_m96(int exp) const {
        fp_m96_t fp{x[kMaxWords - 1], static_cast<uint32_t>(hi32(x[kMaxWords - 2])) + 1, exp};
        if (fp.m2 == 0) { ++fp.m; }
        if (fp.m == 0) { ++fp.exp; }
        return fp;
    }
};

static struct pow_table_t {
    enum : int { kPow10Max = 400, kPow2Max = 1100, kPrecLimit = 18 };
    enum : uint64_t { kMaxMantissa10 = 1000000000000000000ull };
    std::array<fp_m96_t, 2 * kPow10Max + 1> coef10to2;
    std::array<int, 2 * kPow2Max + 1> exp_inv10;
    std::array<uint64_t, kPrecLimit> decimal_mul;
    pow_table_t() {
        // 10^N -> 2^M power conversion table
        large_int<24> lrg{10};
        for (int n = 0; n < kPow10Max; ++n) {
            large_int<4> norm;
            int exp = lrg.get_normalized(norm);
            lrg.multiply(10);
            coef10to2[kPow10Max + n + 1] = norm.make_fp_m96(exp);
            if (norm.invert(2)) {
                coef10to2[kPow10Max - n - 1] = norm.make_fp_m96(-exp - 1);
            } else {
                coef10to2[kPow10Max - n - 1] = fp_m96_t{0, 0, -exp};
            }
        }

        // 2^N -> 10^M power conversion index table
        for (int n = -kPow2Max; n <= kPow2Max; ++n) {
            auto it = std::lower_bound(coef10to2.begin(), coef10to2.end(), -n,
                                       [](decltype(*coef10to2.begin()) el, int exp) { return el.exp < exp; });
            exp_inv10[kPow2Max + n] = static_cast<int>(it - coef10to2.begin()) - kPow10Max;
        }

        // decimal multipliers 10^N, N = 1, 2, ...
        uint64_t mul = 10ull;
        for (uint32_t n = 0; n < decimal_mul.size(); ++n, mul *= 10) { decimal_mul[n] = mul; }
    }
} g_pow_tbl;

struct fp_exp10_format {
    enum { kNeg = 1, kInf = 2, kNan = 4 };
    char flags = 0;
    int exp = 0;
    uint64_t mantissa = 0;
};

static const char* starts_with(const char* p, const char* end, std::string_view s) {
    if (static_cast<size_t>(end - p) < s.size()) { return p; }
    for (const char *p1 = p, *p2 = s.data(); p1 < end; ++p1, ++p2) {
        if (std::toupper(*p1) != std::toupper(*p2)) { return p; }
    }
    return p + s.size();
}

inline const char* skip_spaces(const char* p, const char* end) {
    while ((p < end) && std::isspace(*p)) { ++p; }
    return p;
}

template<typename Ty>
char get_dig_and_div(Ty& v) {
    Ty t = v;
    v /= 10;
    return '0' + static_cast<int>(t - 10 * v);
}

// ---- from string to value

template<typename Ty>
const char* to_unsigned(const char* p, const char* end, Ty& val) {
    if ((p == end) || !std::isdigit(*p)) { return p; }
    val = static_cast<Ty>(*p++ - '0');
    while ((p < end) && std::isdigit(*p)) { val = 10 * val + static_cast<Ty>(*p++ - '0'); }
    return p;
}

template<typename Ty>
const char* to_signed(const char* p, const char* end, Ty& val) {
    const char *p0 = p, *p1 = p;
    bool neg = false;
    if (p == end) {
        return p0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        neg = true;  // negative sign
        ++p;
    }
    if ((p1 = to_unsigned(p, end, val)) == p) { return p0; }
    if (neg) { val = -val; }  // apply sign
    return p1;
}

static const char* accum_mantissa(const char* p, const char* end, uint64_t& m, int& exp) {
    for (; (p < end) && std::isdigit(*p); ++p) {
        if (m < (pow_table_t::kMaxMantissa10 / 10)) {  // decimal mantissa can hold up to 18 digits
            m = 10 * m + static_cast<uint64_t>(*p - '0');
        } else {
            ++exp;
        }
    }
    return p;
}

static const char* to_exp10_float(const char* p, const char* end, fp_exp10_format& fp10) {
    const char *p0 = p, *p1 = p;
    if (p == end) {
        return p0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        fp10.flags |= fp_exp10_format::kNeg;  // negative sign
        ++p;
    }
    if (p == end) {
        return p0;
    } else if (std::isdigit(*p)) {  // integer part
        fp10.mantissa = static_cast<uint64_t>(*p++ - '0');
        p = accum_mantissa(p, end, fp10.mantissa, fp10.exp);
        if ((p < end) && (*p == '.')) { ++p; }  // skip decimal point
    } else if ((*p == '.') && ((p + 1) < end) && std::isdigit(*(p + 1))) {
        fp10.mantissa = static_cast<uint64_t>(*(p + 1) - '0');  // tenth
        fp10.exp = -1;
        p += 2;
    } else if ((p1 = starts_with(p, end, "inf")) > p) {
        fp10.flags |= fp_exp10_format::kInf;  // infinity
        return p1;
    } else if ((p1 = starts_with(p, end, "nan")) > p) {
        fp10.flags = fp_exp10_format::kNan;  // NaN
        return p1;
    } else {
        return p0;
    }
    p1 = accum_mantissa(p, end, fp10.mantissa, fp10.exp);  // fractional part
    fp10.exp -= static_cast<int>(p1 - p);
    if ((p1 < end) && ((*p1 == 'e') || (*p1 == 'E'))) {  // optional exponent
        int exp_optional = 0;
        if ((p = to_signed(p1 + 1, end, exp_optional)) > p1 + 1) {
            p1 = p;
            fp10.exp += exp_optional;
        }
    }
    return p1;
}

template<typename Ty>
const char* to_float(const char* p, const char* end, Ty& val) {
    fp_exp10_format fp10;
    const char* p1 = to_exp10_float(p, end, fp10);
    if (p1 == p) { return p; }
    uint64_t mantissa2 = 0;
    int exp = 0;
    if (fp10.flags & (fp_exp10_format::kInf | fp_exp10_format::kNan)) {  // infinity or NaN
        exp = fp_format_traits<Ty>::kExpMax;
        if (fp10.flags & fp_exp10_format::kNan) { mantissa2 = fp_format_traits<Ty>::kMantissaMask; }
    } else if ((fp10.mantissa == 0) || (fp10.exp < -pow_table_t::kPow10Max)) {  // zero
    } else if (fp10.exp > pow_table_t::kPow10Max) {
        exp = fp_format_traits<Ty>::kExpMax;  // convert to infinity
    } else {
        // convert decimal mantissa to binary
        const auto& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max + fp10.exp];
        auto res128 = mul64x64(fp10.mantissa, coef.m);
        res128.hi += fp10.mantissa;  // apply implicit 1 term of normalized coefficient
        int shift = ulog2(res128.hi);
        exp = fp_format_traits<Ty>::kExpBias + shift + coef.exp;
        if (exp >= fp_format_traits<Ty>::kExpMax) {
            exp = fp_format_traits<Ty>::kExpMax;  // convert to infinity
        } else {
            // obtain +1 mantissa bits for rounding
            mantissa2 = shr_lo64(res128, shift + 63 - fp_format_traits<Ty>::kBitsPerMantissa);
            if (~mantissa2 & ((1ull << (fp_format_traits<Ty>::kBitsPerMantissa + 1)) - 1)) {
                mantissa2 = (mantissa2 + 1) >> 1;  // round mantissa
            } else {                               // overflow while rounding, the value can become infinity
                ++exp, mantissa2 = 0;
            }

            if (exp <= 0) {  // denormalize value or the value will be converted to zero
                if (exp > -static_cast<int>(fp_format_traits<Ty>::kBitsPerMantissa)) {  // denormalize
                    mantissa2 = (mantissa2 | (1ull << fp_format_traits<Ty>::kBitsPerMantissa)) >> (1 - exp);
                }
                exp = 0;
            }
        }
    }

    // compose floating point value
    val = fp_format_traits<Ty>::from_u64(((fp10.flags & fp_exp10_format::kNeg) ? fp_format_traits<Ty>::kSignBit : 0) |
                                         (static_cast<uint64_t>(exp) << fp_format_traits<Ty>::kBitsPerMantissa) |
                                         (mantissa2 & fp_format_traits<Ty>::kMantissaMask));
    return p1;
}

// ---- from value to string

template<typename Ty>
char* from_unsigned(char* p, Ty val, scvt_base base) {
    switch (base) {
        case scvt_base::kBase8:
            do {
                *--p = '0' + static_cast<int>(val & 0x7);
                val >>= 3;
            } while (val != 0);
            break;
        case scvt_base::kBase10:  //
            do { *--p = get_dig_and_div(val); } while (val != 0);
            break;
        case scvt_base::kBase16:
            do {
                *--p = "0123456789abcdef"[val & 0xf];
                val >>= 4;
            } while (val != 0);
            break;
    }
    return p;
}

template<typename Ty>
char* from_signed(char* p, Ty val, scvt_base base) {
    bool neg = false;
    if ((base == scvt_base::kBase10) && (val < 0)) {
        val = -val;  // negative value
        neg = true;
    }
    do { *--p = get_dig_and_div(val); } while (val != 0);
    if (neg) { *--p = '-'; }  // add negative sign
    return p;
}

static char* from_exp10_float(char* p, const fp_exp10_format& fp10, scvt_fp fmt, int prec) {
    if (fp10.flags & fp_exp10_format::kInf) {
        p -= 3;
        p[0] = 'i', p[1] = 'n', p[2] = 'f';
        if (fp10.flags & fp_exp10_format::kNeg) { *--p = '-'; }  // add negative sign
        return p;
    } else if (fp10.flags & fp_exp10_format::kNan) {
        p -= 3;
        p[0] = 'n', p[1] = 'a', p[2] = 'n';
        return p;
    }

    bool trim_zeroes = false;
    if (fmt == scvt_fp::kGeneral) {
        trim_zeroes = true;
        prec = std::max(prec - 1, 0);
        if ((fp10.exp >= -4) && (fp10.exp <= prec)) { fmt = scvt_fp::kFixed, prec -= fp10.exp; }
    }

    uint64_t m = fp10.mantissa;
    int n_zeroes = prec;
    if (m != 0) {
        int n_digs = 1 + prec;
        if (fmt == scvt_fp::kFixed) { n_digs += fp10.exp; }
        // count trailing zeroes
        n_zeroes = std::max(n_digs - pow_table_t::kPrecLimit, 0);
        for (;;) {
            uint64_t t = m / 10;
            if (m > 10 * t) { break; }
            ++n_zeroes, m = t;
        }
    }

    if (fmt != scvt_fp::kFixed) {  // add exponent
        int exp10 = fp10.exp;
        if (fp10.exp < 0) { exp10 = -exp10; }
        if (exp10 >= 10) {
            do { *--p = get_dig_and_div(exp10); } while (exp10 != 0);
        } else {
            *--p = '0' + exp10;
            *--p = '0';
        }
        *--p = (fp10.exp < 0) ? '-' : '+';
        *--p = 'e';
    }

    // fractional part
    char* p0 = p;
    if (trim_zeroes) {
        if (n_zeroes < prec) {
            prec -= n_zeroes;
            n_zeroes = 0;
            for (; prec > 0; --prec) { *--p = get_dig_and_div(m); }
        } else {
            n_zeroes -= prec;
        }
    } else if (n_zeroes < prec) {
        prec -= n_zeroes;
        for (; n_zeroes; --n_zeroes) { *--p = '0'; }
        for (; prec > 0; --prec) { *--p = get_dig_and_div(m); }
    } else {
        n_zeroes -= prec;
        for (; prec > 0; --prec) { *--p = '0'; }
    }

    // integer part
    if (p < p0) { *--p = '.'; }
    for (; n_zeroes; --n_zeroes) { *--p = '0'; }
    do { *--p = get_dig_and_div(m); } while (m != 0);

    if (fp10.flags & fp_exp10_format::kNeg) { *--p = '-'; }  // add negative sign
    return p;
}

template<typename Ty>
char* from_float(char* p, Ty val, scvt_fp fmt, int prec) {
    fp_exp10_format fp10;
    if (prec < 0) { prec = 6; }
    uint64_t mantissa = fp_format_traits<Ty>::to_u64(val);
    if (mantissa & fp_format_traits<Ty>::kSignBit) { fp10.flags |= fp_exp10_format::kNeg; }
    int exp = static_cast<int>((mantissa & fp_format_traits<Ty>::kExpMask) >> fp_format_traits<Ty>::kBitsPerMantissa) -
              fp_format_traits<Ty>::kExpBias;
    mantissa &= fp_format_traits<Ty>::kMantissaMask;
    if ((fp_format_traits<Ty>::kExpBias + exp) == fp_format_traits<Ty>::kExpMax) {
        if (mantissa == 0) {  // infinity
            fp10.flags |= fp_exp10_format::kInf;
        } else {  // NaN
            fp10.flags = fp_exp10_format::kNan;
        }
    } else if ((exp > -fp_format_traits<Ty>::kExpBias) || (mantissa != 0)) {
        if (exp == -fp_format_traits<Ty>::kExpBias) {  // denormalized form
            int shift = fp_format_traits<Ty>::kBitsPerMantissa - ulog2(mantissa);
            mantissa <<= shift;
            exp -= shift - 1;
        } else {
            mantissa |= 1ull << fp_format_traits<Ty>::kBitsPerMantissa;
        }

        fp10.exp = -g_pow_tbl.exp_inv10[pow_table_t::kPow2Max + exp];  // inverted power of 10

        int n_digs = 1 + prec;
        if (fmt == scvt_fp::kFixed) {  // fixed format
            n_digs += fp10.exp;
        } else if ((fmt == scvt_fp::kGeneral) && (n_digs > 1)) {  // general format
            --n_digs;
        }

        if (n_digs > 0) {
            n_digs = std::min<int>(n_digs, pow_table_t::kPrecLimit);

            // calculate decimal mantissa representation
            const auto& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp + n_digs - 1];
            auto res128 = mul64x64(mantissa, coef.m, mul64x32_hi64(mantissa, coef.m2));
            res128.hi += mantissa;  // apply implicit 1 term of normalized coefficient
            exp += coef.exp;        // sum exponents as well

            // obtain +1 mantissa bits for rounding
            fp10.mantissa = shr_lo64(res128, 63 + fp_format_traits<Ty>::kBitsPerMantissa - exp);

            // round mantissa
            ++fp10.mantissa;
            if (fp10.mantissa >= (g_pow_tbl.decimal_mul[n_digs - 1] << 1)) {
                ++fp10.exp;
                if ((fmt != scvt_fp::kFixed) || (n_digs == pow_table_t::kPrecLimit)) {
                    // formula for removing one digit: fp10.mantissa = (fp10.mantissa + 10) / 20
                    // but we've just added 1 and will divide by 2 after that
                    fp10.mantissa = (fp10.mantissa + 9) / 10;  // remove one digit
                }
            }
            fp10.mantissa >>= 1;
        } else if (n_digs == 0) {
            // special case for fixed format when mantissa is almost hidden (unsignificant),
            // but we can still get one digit
            const auto& coef = g_pow_tbl.coef10to2[pow_table_t::kPow10Max - fp10.exp];
            fp10.mantissa = mantissa + mul64x64_hi64(mantissa, coef.m, mul64x32_hi64(mantissa, coef.m2));
            exp += coef.exp;
            fp10.mantissa >>= fp_format_traits<Ty>::kBitsPerMantissa - exp - 1;
            fp10.mantissa = (fp10.mantissa + 10) / 20;
            if (fp10.mantissa >= 1) { ++fp10.exp; }
        }
    }

    return from_exp10_float(p, fp10, fmt, prec);
}

}  // namespace scvt

/*static*/ const char* string_converter<int16_t>::from_string(std::string_view s, int16_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_signed(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<int16_t>::to_string(int16_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_signed(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<int32_t>::from_string(std::string_view s, int32_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_signed(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<int32_t>::to_string(int32_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_signed(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<int64_t>::from_string(std::string_view s, int64_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_signed(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<int64_t>::to_string(int64_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_signed(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<uint16_t>::from_string(std::string_view s, uint16_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_unsigned(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<uint16_t>::to_string(uint16_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_unsigned(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<uint32_t>::from_string(std::string_view s, uint32_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_unsigned(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<uint32_t>::to_string(uint32_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_unsigned(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<uint64_t>::from_string(std::string_view s, uint64_t& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_unsigned(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<uint64_t>::to_string(uint64_t val, scvt_base base) {
    std::array<char, 32> s;
    return std::string(scvt::from_unsigned(s.data() + s.size(), val, base), s.data() + s.size());
}

/*static*/ const char* string_converter<float>::from_string(std::string_view s, float& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_float(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<float>::to_string(float val, scvt_fp fmt, int prec) {
    std::array<char, 128> s;
    return std::string(scvt::from_float(s.data() + s.size(), val, fmt, prec), s.data() + s.size());
}

/*static*/ const char* string_converter<double>::from_string(std::string_view s, double& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end);
    const char* p1 = scvt::to_float(p, end, val);
    if (ok) { *ok = p1 > p; }
    return p1;
}

/*static*/ std::string string_converter<double>::to_string(double val, scvt_fp fmt, int prec) {
    std::array<char, 512> s;
    return std::string(scvt::from_float(s.data() + s.size(), val, fmt, prec), s.data() + s.size());
}

/*static*/ const char* string_converter<bool>::from_string(std::string_view s, bool& val, bool* ok) {
    const char *end = s.data() + s.size(), *p = scvt::skip_spaces(s.data(), end), *p1 = p;
    if ((p1 = scvt::starts_with(p, end, "true")) > p) {
        val = true;
    } else if ((p1 = scvt::starts_with(p, end, "false")) > p) {
        val = false;
    } else if ((p < end) && std::isdigit(*p)) {
        val = false;
        do {
            if (*p++ != '0') {
                val = true;
                break;
            }
        } while ((p < end) && std::isdigit(*p));
        p1 = p;
    }
    if (ok) { *ok = p1 > p; }
    return p1;
}

//---------------------------------------------------------------------------------
// String formatter implementation

std::string sformat::str() const {
    std::string result;
    auto p = fmt_.begin(), p0 = p;
    while (p < fmt_.end()) {
        if (*p != '%') {
            ++p;
            continue;
        }
        result.append(p0, p++);
        if (p == fmt_.end()) {
            return result;
        } else if (std::isdigit(*p)) {
            size_t n = static_cast<size_t>(*p++ - '0');
            while ((p < fmt_.end()) && std::isdigit(*p)) { n = 10 * n + static_cast<size_t>(*p++ - '0'); }
            if ((n > 0) && (n <= args_.size())) { result.append(args_[n - 1]); }
            p0 = p;
        } else {
            p0 = p++;
        }
    }
    result.append(p0, p);
    return result;
}
