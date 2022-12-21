#pragma once

#include "uxs/stringcvt.h"

#include <array>
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER) && defined(_M_X64)
#    include <intrin.h>
#elif defined(__GNUC__) && defined(__x86_64__)
namespace gcc_ints {
__extension__ typedef unsigned __int128 uint128;
}  // namespace gcc_ints
#endif

#ifdef __has_builtin
#    define SCVT_HAS_BUILTIN(x) __has_builtin(x)
#else
#    define SCVT_HAS_BUILTIN(x) 0
#endif

namespace uxs {
namespace scvt {

struct fp_m64_t {
    uint64_t m;
    int exp;
};

inline uint64_t lo32(uint64_t x) { return x & 0xffffffff; }
inline uint64_t hi32(uint64_t x) { return x >> 32; }

extern UXS_EXPORT const unsigned g_exp2_digs[];
extern UXS_EXPORT const uint64_t g_ten_pows[];
extern UXS_EXPORT const char g_digits[][2];

#if defined(_MSC_VER) && defined(_M_X64)
inline unsigned ulog2(uint32_t x) {
    unsigned long ret;
    _BitScanReverse(&ret, x | 1);
    return ret;
}
inline unsigned ulog2(uint64_t x) {
    unsigned long ret;
    _BitScanReverse64(&ret, x | 1);
    return ret;
}
#elif defined(__GNUC__) && defined(__x86_64__)
inline unsigned ulog2(uint32_t x) { return __builtin_clz(x | 1) ^ 31; }
inline unsigned ulog2(uint64_t x) { return __builtin_clzll(x | 1) ^ 63; }
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
extern UXS_EXPORT const ulog2_table_t g_ulog2_tbl;
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

// ---- from string to value

template<typename CharT>
const CharT* starts_with(const CharT* p, const CharT* end, const char* s, const size_t len) NOEXCEPT {
    if (static_cast<size_t>(end - p) < len) { return p; }
    for (const CharT* p1 = p; p1 < end; ++p1, ++s) {
        if (to_lower(*p1) != *s) { return p; }
    }
    return p + len;
}

template<typename Ty, typename CharT>
Ty to_integer_limited(const CharT* p, const CharT* end, const CharT*& last, Ty pos_limit) NOEXCEPT {
    bool neg = false;
    last = p;
    if (p == end) {
        return 0;
    } else if (*p == '+') {
        ++p;  // skip positive sign
    } else if (*p == '-') {
        ++p, neg = true;  // negative sign
    }
    unsigned dig = 0;
    if (p == end || (dig = dig_v(*p)) >= 10) { return 0; }
    Ty val = dig;
    while (++p < end && (dig = dig_v(*p)) < 10) {
        Ty val0 = val;
        val = 10u * val + dig;
        if (val < val0) { return 0; }  // too big integer
    }
    // Note: resulting number must be in range [-(1 + pos_limit / 2), pos_limit]
    if (neg) {
        if (val > 1 + (pos_limit >> 1)) { return 0; }  // negative integer is out of range
        val = ~val + 1;                                // apply sign
    } else if (val > pos_limit) {
        return 0;  // positive integer is out of range
    }
    last = p;
    return val;
}

template<typename CharT>
const CharT* accum_mantissa(const CharT* p, const CharT* end, uint64_t& m, int& exp) NOEXCEPT {
    const uint64_t max_mantissa10 = 1000000000000000000ull;
    for (unsigned dig = 0; p < end && (dig = dig_v(*p)) < 10; ++p) {
        if (m < max_mantissa10) {  // decimal mantissa can hold up to 19 digits
            m = 10u * m + dig;
        } else {
            ++exp;
        }
    }
    return p;
}

template<typename CharT>
const CharT* chars_to_fp10(const CharT* p, const CharT* end, fp_m64_t& fp10) NOEXCEPT {
    unsigned dig = 0;
    const CharT dec_point = '.';
    if (p == end) { return p; }
    if ((dig = dig_v(*p)) < 10) {  // integral part
        fp10.m = dig;
        p = accum_mantissa(++p, end, fp10.m, fp10.exp);
        if (p < end && *p == dec_point) { ++p; }  // skip decimal point
    } else if (*p == dec_point && p + 1 < end && (dig = dig_v(*(p + 1))) < 10) {
        fp10.m = dig, fp10.exp = -1, p += 2;  // tenth
    } else {
        return p;
    }
    const CharT* p1 = accum_mantissa(p, end, fp10.m, fp10.exp);  // fractional part
    fp10.exp -= static_cast<unsigned>(p1 - p);
    if (p1 < end && (*p1 == 'e' || *p1 == 'E')) {  // optional exponent
        int exp_optional = to_integer<int>(p1 + 1, end, p);
        if (p > p1 + 1) { fp10.exp += exp_optional, p1 = p; }
    }
    return p1;
}

UXS_EXPORT uint64_t fp10_to_fp2(fp_m64_t fp10, const unsigned bpm, const int exp_max) NOEXCEPT;

template<typename CharT>
uint64_t to_float_common(const CharT* p, const CharT* end, const CharT*& last, const unsigned bpm,
                         const int exp_max) NOEXCEPT {
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
    const CharT* p1 = chars_to_fp10(p, end, fp10);
    if (p1 > p) {
        fp2 |= fp10_to_fp2(fp10, bpm, exp_max);
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
Ty to_bool(const CharT* p, const CharT* end, const CharT*& last) NOEXCEPT {
    unsigned dig = 0;
    Ty val = false;
    const CharT* p0 = p;
    if ((p = scvt::starts_with(p, end, "true", 4)) > p0) {
        val = true;
    } else if ((p = scvt::starts_with(p, end, "false", 5)) > p0) {
    } else if (p < end && (dig = dig_v(*p)) < 10) {
        do {
            if (dig) { val = true; }
        } while (++p < end && (dig = dig_v(*p)) < 10);
    }
    last = p;
    return val;
}

// ---- from value to string

template<typename StrTy, typename Func>
StrTy& adjust_numeric(StrTy& s, Func fn, unsigned len, const unsigned n_prefix, const fmt_opts& fmt) {
    unsigned left = fmt.width - len, right = left;
    int fill = fmt.fill;
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        switch (fmt.flags & fmt_flags::kAdjustField) {
            case fmt_flags::kLeft: left = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            case fmt_flags::kRight:
            default: right = 0; break;
        }
    } else {
        fill = '0';
    }
    s.append(left, fill);
    fn(s);
    if (!(fmt.flags & fmt_flags::kLeadingZeroes)) {
        s.append(right, fmt.fill);
    } else if (n_prefix != 0) {
        using CharT = typename StrTy::value_type;
        CharT *p = &s.back() - fmt.width, *p_end = p + n_prefix, *p_src = &s.back() - len;
        do {
            *++p = *++p_src;
            *p_src = '0';
        } while (p != p_end);
    }
    return s;
}

template<typename StrTy>
struct common_str_impl_enabler : std::true_type {};

template<typename CharT>
struct common_str_impl_enabler<basic_unlimbuf_appender<CharT>> : std::false_type {};

template<typename CharT, typename Alloc>
struct common_str_impl_enabler<basic_dynbuffer<CharT, Alloc>> : std::false_type {};

// ---- binary

template<typename CharT, typename Ty>
void fmt_gen_bin(CharT* p, Ty val, char sign, const fmt_opts& fmt) NOEXCEPT {
    do {
        *--p = '0' + static_cast<unsigned>(val & 1);
        val >>= 1;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'b' : 'B';
        *--p = '0';
    }
    if (sign) { *--p = sign; }
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_gen_bin(basic_unlimbuf_appender<CharT>& s, Ty val, char sign, unsigned len,
                                            const fmt_opts& fmt) NOEXCEPT {
    fmt_gen_bin(s.curr() + len, val, sign, fmt);
    return s.advance(len);
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_gen_bin(basic_dynbuffer<CharT, Alloc>& s, Ty val, char sign, unsigned len,
                                           const fmt_opts& fmt) {
    fmt_gen_bin(s.reserve_at_curr(len) + len, val, sign, fmt);
    return s.advance(len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<common_str_impl_enabler<StrTy>::value>>
StrTy& fmt_gen_bin(StrTy& s, Ty val, char sign, unsigned len, const fmt_opts& fmt) {
    std::array<typename StrTy::value_type, 72> buf;
    fmt_gen_bin(buf.data() + len, val, sign, fmt);
    return s.append(buf.data(), buf.data() + len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_bin(StrTy& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = (sign != '\0' ? 1 : 0) + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0);
    const unsigned len = 1 + n_prefix + ulog2(val);
    const auto fn = [val, sign, len, &fmt](StrTy& s) -> StrTy& { return fmt_gen_bin(s, val, sign, len, fmt); };
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt) : fn(s);
}

// ---- octal

template<typename CharT, typename Ty>
void fmt_gen_oct(CharT* p, Ty val, char sign, const fmt_opts& fmt) NOEXCEPT {
    do {
        *--p = '0' + static_cast<unsigned>(val & 7);
        val >>= 3;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) { *--p = '0'; }
    if (sign) { *--p = sign; }
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_gen_oct(basic_unlimbuf_appender<CharT>& s, Ty val, char sign, unsigned len,
                                            const fmt_opts& fmt) NOEXCEPT {
    fmt_gen_oct(s.curr() + len, val, sign, fmt);
    return s.advance(len);
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_gen_oct(basic_dynbuffer<CharT, Alloc>& s, Ty val, char sign, unsigned len,
                                           const fmt_opts& fmt) {
    fmt_gen_oct(s.reserve_at_curr(len) + len, val, sign, fmt);
    return s.advance(len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<common_str_impl_enabler<StrTy>::value>>
StrTy& fmt_gen_oct(StrTy& s, Ty val, char sign, unsigned len, const fmt_opts& fmt) {
    std::array<typename StrTy::value_type, 24> buf;
    fmt_gen_oct(buf.data() + len, val, sign, fmt);
    return s.append(buf.data(), buf.data() + len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_oct(StrTy& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = sign != '\0' ? 1 : 0;
    const unsigned len = n_prefix + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 1) + ulog2(val) / 3;
    const auto fn = [val, sign, len, &fmt](StrTy& s) -> StrTy& { return fmt_gen_oct(s, val, sign, len, fmt); };
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt) : fn(s);
}

// ---- hexadecimal

template<typename CharT, typename Ty>
void fmt_gen_hex(CharT* p, Ty val, char sign, const fmt_opts& fmt) NOEXCEPT {
    const char* digs = !!(fmt.flags & fmt_flags::kUpperCase) ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        *--p = digs[val & 0xf];
        val >>= 4;
    } while (val != 0);
    if (!!(fmt.flags & fmt_flags::kAlternate)) {
        *--p = !(fmt.flags & fmt_flags::kUpperCase) ? 'x' : 'X';
        *--p = '0';
    }
    if (sign) { *--p = sign; }
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_gen_hex(basic_unlimbuf_appender<CharT>& s, Ty val, char sign, unsigned len,
                                            const fmt_opts& fmt) NOEXCEPT {
    fmt_gen_hex(s.curr() + len, val, sign, fmt);
    return s.advance(len);
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_gen_hex(basic_dynbuffer<CharT, Alloc>& s, Ty val, char sign, unsigned len,
                                           const fmt_opts& fmt) {
    fmt_gen_hex(s.reserve_at_curr(len) + len, val, sign, fmt);
    return s.advance(len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<common_str_impl_enabler<StrTy>::value>>
StrTy& fmt_gen_hex(StrTy& s, Ty val, char sign, unsigned len, const fmt_opts& fmt) {
    std::array<typename StrTy::value_type, 24> buf;
    fmt_gen_hex(buf.data() + len, val, sign, fmt);
    return s.append(buf.data(), buf.data() + len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_hex(StrTy& s, Ty val, bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = (sign != '\0' ? 1 : 0) + (!!(fmt.flags & fmt_flags::kAlternate) ? 2 : 0);
    const unsigned len = 1 + n_prefix + (ulog2(val) >> 2);
    const auto fn = [val, sign, len, &fmt](StrTy& s) -> StrTy& { return fmt_gen_hex(s, val, sign, len, fmt); };
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt) : fn(s);
}

// ---- decimal

template<typename Ty>
unsigned fmt_dec_unsigned_len(Ty val) NOEXCEPT {
    const unsigned pow = g_exp2_digs[ulog2(val)];
    return val >= g_ten_pows[pow] ? pow + 1 : pow;
}

template<typename CharT>
void copy2(CharT* tgt, const char* src) {
    *tgt++ = *src++;
    *tgt = *src;
}

inline void copy2(char* tgt, const char* src) { std::memcpy(tgt, src, 2); }

template<typename CharT, typename Ty>
CharT* gen_digits(CharT* p, Ty v) NOEXCEPT {
    while (v >= 100u) {
        const Ty t = v / 100u;
        copy2(p -= 2, g_digits[static_cast<unsigned>(v - 100u * t)]);
        v = t;
    }
    if (v >= 10u) {
        copy2(p -= 2, g_digits[v]);
        return p;
    }
    *--p = '0' + static_cast<unsigned>(v);
    return p;
}

template<typename CharT, typename Ty>
Ty gen_digits_n(CharT* p, Ty v, unsigned n) NOEXCEPT {
    CharT* p0 = p - (n & ~1);
    while (p != p0) {
        const Ty t = v / 100u;
        copy2(p -= 2, g_digits[static_cast<unsigned>(v - 100u * t)]);
        v = t;
    }
    if (!(n & 1)) { return v; }
    const Ty t = v / 10u;
    *--p = '0' + static_cast<unsigned>(v - 10u * t);
    return t;
}

template<typename CharT, typename Ty>
void fmt_gen_dec(CharT* p, Ty val, char sign) NOEXCEPT {
    p = gen_digits(p, val);
    if (sign) { *--p = sign; }
}

template<typename CharT, typename Ty>
basic_unlimbuf_appender<CharT>& fmt_gen_dec(basic_unlimbuf_appender<CharT>& s, Ty val, char sign,
                                            unsigned len) NOEXCEPT {
    fmt_gen_dec(s.curr() + len, val, sign);
    return s.advance(len);
}

template<typename CharT, typename Alloc, typename Ty>
basic_dynbuffer<CharT, Alloc>& fmt_gen_dec(basic_dynbuffer<CharT, Alloc>& s, Ty val, char sign, unsigned len) {
    fmt_gen_dec(s.reserve_at_curr(len) + len, val, sign);
    return s.advance(len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<common_str_impl_enabler<StrTy>::value>>
StrTy& fmt_gen_dec(StrTy& s, Ty val, char sign, unsigned len) {
    std::array<typename StrTy::value_type, 24> buf;
    fmt_gen_dec(buf.data() + len, val, sign);
    return s.append(buf.data(), buf.data() + len);
}

template<typename StrTy, typename Ty, typename = std::enable_if_t<std::is_unsigned<Ty>::value>>
StrTy& fmt_dec(StrTy& s, Ty val, const bool is_signed, const fmt_opts& fmt) {
    char sign = '\0';
    if (is_signed) {
        if (val & (static_cast<Ty>(1) << (8 * sizeof(Ty) - 1))) {
            sign = '-', val = ~val + 1;  // negative value
        } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
            sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
        }
    }
    const unsigned n_prefix = sign != '\0' ? 1 : 0;
    const unsigned len = n_prefix + fmt_dec_unsigned_len(val);
    const auto fn = [val, sign, len](StrTy& s) -> StrTy& { return fmt_gen_dec(s, val, sign, len); };
    return fmt.width > len ? adjust_numeric(s, fn, len, n_prefix, fmt) : fn(s);
}

// --------------------------

template<typename StrTy, typename Ty>
StrTy& fmt_integral(StrTy& s, Ty val, const fmt_opts& fmt) {
    using UTy = typename std::make_unsigned<Ty>::type;
    const bool is_signed = std::is_signed<Ty>::value;
    switch (fmt.flags & fmt_flags::kBaseField) {
        case fmt_flags::kDec: return fmt_dec(s, static_cast<UTy>(val), is_signed, fmt);
        case fmt_flags::kBin: return fmt_bin(s, static_cast<UTy>(val), is_signed, fmt);
        case fmt_flags::kOct: return fmt_oct(s, static_cast<UTy>(val), is_signed, fmt);
        case fmt_flags::kHex: return fmt_hex(s, static_cast<UTy>(val), is_signed, fmt);
        default: UNREACHABLE_CODE;
    }
}

// ---- char

template<typename StrTy, typename Ty>
StrTy& fmt_char(StrTy& s, Ty val, const fmt_opts& fmt) {
    const auto fn = [val](StrTy& s) -> StrTy& {
        s.push_back(val);
        return s;
    };
    return fmt.width > 1 ? append_adjusted(s, fn, 1, fmt) : fn(s);
}

// ---- float

class UXS_EXPORT fp_dec_fmt_t {
 public:
    fp_dec_fmt_t(fp_m64_t fp2, const fmt_opts& fmt, unsigned bpm, const int exp_bias) NOEXCEPT;
    ~fp_dec_fmt_t() {
        if (digs_ && digs_ != digs_buf_) { std::free(digs_); }
    }

    unsigned get_len(char sign) const NOEXCEPT {
        return (sign != '\0' ? 2 : 1) + (fixed_ ? std::max(exp_, 0) : (exp_ <= -100 || exp_ >= 100 ? 5 : 4)) +
               (prec_ > 0 || alternate_ ? prec_ + 1 : 0);
    }

    template<typename CharT>
    void generate(CharT* p, const fmt_opts& fmt) const NOEXCEPT;

    template<typename CharT>
    basic_unlimbuf_appender<CharT>& generate(basic_unlimbuf_appender<CharT>& s, char sign, unsigned len,
                                             const fmt_opts& fmt) const NOEXCEPT {
        CharT* p = s.curr();
        if (sign != '\0') { *p = sign; }  // generate sign
        generate(p + len, fmt);
        return s.advance(len);
    }

    template<typename CharT, typename Alloc>
    basic_dynbuffer<CharT, Alloc>& generate(basic_dynbuffer<CharT, Alloc>& s, char sign, unsigned len,
                                            const fmt_opts& fmt) const {
        CharT* p = s.reserve_at_curr(len);
        if (sign != '\0') { *p = sign; }  // generate sign
        generate(p + len, fmt);
        return s.advance(len);
    }

    template<typename StrTy, typename = std::enable_if_t<common_str_impl_enabler<StrTy>::value>>
    StrTy& generate(StrTy& s, char sign, unsigned len, const fmt_opts& fmt) const {
        using CharT = typename StrTy::value_type;
        basic_inline_dynbuffer<CharT> buf;
        CharT* p = buf.reserve_at_curr(len);
        if (sign != '\0') { *p = sign; }  // generate sign
        generate(p + len, fmt);
        return s.append(buf.data(), buf.data() + len);
    }

 private:
    uint64_t significand_;
    char* digs_;
    int exp_;
    int prec_;
    bool fixed_;
    bool alternate_;
    int n_zeroes_;
    char digs_buf_[64];
};

template<typename CharT>
void fp_dec_fmt_t::generate(CharT* p, const fmt_opts& fmt) const NOEXCEPT {
    const CharT dec_point = '.';
    const CharT e_symb = !(fmt.flags & fmt_flags::kUpperCase) ? 'e' : 'E';

    if (!fixed_) {  // generate scientific representation
        // generate exponent
        int exp10 = exp_;
        char exp_sign = '+';
        if (exp10 < 0) { exp_sign = '-', exp10 = -exp10; }
        if (exp10 < 100) {
            copy2(p -= 2, g_digits[exp10]);
        } else {
            const int t = (656 * exp10) >> 16;
            copy2(p -= 2, g_digits[exp10 - 100 * t]);
            *--p = '0' + t;
        }
        *--p = exp_sign;
        *--p = e_symb;

        if (prec_ > 0) {   // has fractional part
            if (!digs_) {  // generate from significand
                p = gen_digits(p, significand_);
            } else {  // generate from chars
                p -= prec_ + 1;
                std::fill_n(std::copy_n(digs_, prec_ + 1 - n_zeroes_, p), n_zeroes_, '0');
            }
            // insert decimal point
            *(p - 1) = *p;
            *p = dec_point;
        } else {  // only one digit
            if (alternate_) { *--p = dec_point; }
            *--p = '0' + static_cast<unsigned>(significand_);
        }

        return;
    }

    // generate fixed representation

    uint64_t m = significand_;
    int k = 1 + exp_, n_zeroes = n_zeroes_;
    if (prec_ > 0) {       // has fractional part
        if (k > 0) {       // fixed form [1-9]+.[0-9]+
            if (!digs_) {  // generate fractional part from significand
                m = gen_digits_n(p, m, prec_);
            } else {  // generate from chars
                if (n_zeroes < prec_) {
                    std::fill_n(std::copy_n(digs_ + k, prec_ - n_zeroes, p - prec_), n_zeroes, '0');
                } else {  // all zeroes
                    std::fill_n(p - prec_, prec_, '0');
                }
                n_zeroes -= prec_;
            }
            *(p -= 1 + prec_) = dec_point;
        } else {  // fixed form 0.0*[1-9][0-9]*
            // append trailing fractional part later as is was integral part
            std::fill_n(p - prec_ - 2, 2 - k, '0');  // one `0` will be replaced with decimal point later
            *(p - prec_ - 1) = dec_point;            // put decimal point into reserved cell
            k += prec_;
        }
    } else if (alternate_) {
        *--p = dec_point;
    }
    if (!digs_) {  // generate integral part from significand
        gen_digits(p, m);
    } else if (n_zeroes > 0) {  // generate from chars
        std::fill_n(std::copy_n(digs_, k - n_zeroes, p - k), n_zeroes, '0');
    } else {
        std::copy_n(digs_, k, p - k);
    }
}

template<typename StrTy>
StrTy& fmt_float_common(StrTy& s, uint64_t u64, const fmt_opts& fmt, const unsigned bpm, const int exp_max) {
    char sign = '\0';
    if (u64 & (static_cast<uint64_t>(1 + exp_max) << bpm)) {
        sign = '-';  // negative value
    } else if ((fmt.flags & fmt_flags::kSignField) != fmt_flags::kSignNeg) {
        sign = (fmt.flags & fmt_flags::kSignField) == fmt_flags::kSignPos ? '+' : ' ';
    }

    // Binary exponent and mantissa
    const fp_m64_t fp2{u64 & ((1ull << bpm) - 1), static_cast<int>((u64 >> bpm) & exp_max)};

    if (fp2.exp == exp_max) {
        const char* p0 = fp2.m == 0 ? (!(fmt.flags & fmt_flags::kUpperCase) ? "inf" : "INF") :
                                      (!(fmt.flags & fmt_flags::kUpperCase) ? "nan" : "NAN");
        const unsigned len = sign != '\0' ? 4 : 3;
        const auto fn = [p0, sign](StrTy& s) -> StrTy& {
            if (sign != '\0') { s.push_back(sign); }
            return s.append(p0, p0 + 3);
        };
        return fmt.width > len ? append_adjusted(s, fn, len, fmt) : fn(s);
    }

    fp_dec_fmt_t fp(fp2, fmt, bpm, exp_max >> 1);
    const unsigned len = fp.get_len(sign);
    const auto fn = [&fp, sign, len, &fmt](StrTy& s) -> StrTy& { return fp.generate(s, sign, len, fmt); };
    return fmt.width > len ? adjust_numeric(s, fn, len, sign != '\0' ? 1 : 0, fmt) : fn(s);
}

// ---- float

template<typename StrTy, typename Ty>
StrTy& fmt_bool(StrTy& s, Ty val, const fmt_opts& fmt) {
    const std::string_view sval = val ? std::string_view(!(fmt.flags & fmt_flags::kUpperCase) ? "true" : "TRUE", 4) :
                                        std::string_view(!(fmt.flags & fmt_flags::kUpperCase) ? "false" : "FALSE", 5);
    const auto fn = [sval](StrTy& s) -> StrTy& { return s.append(sval.begin(), sval.end()); };
    return fmt.width > sval.size() ? append_adjusted(s, fn, static_cast<unsigned>(sval.size()), fmt) : fn(s);
}

}  // namespace scvt

}  // namespace uxs
