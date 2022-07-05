#pragma once

#include "utf.h"

#include "uxs/format.h"

namespace uxs {
namespace detail {

enum class fmt_parse_flags : unsigned {
    kDefault = 0,
    kDynamicWidth = 1,
    kDynamicPrec = 2,
    kArgNumSpecified = 0x10,
    kWidthArgNumSpecified = 0x20,
    kPrecArgNumSpecified = 0x40,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_parse_flags, unsigned);

struct fmt_arg_specs {
    fmt_state fmt;
    fmt_parse_flags flags = fmt_parse_flags::kDefault;
    unsigned n_arg = 0;
    unsigned n_width_arg = 0;
    unsigned n_prec_arg = 0;
};

struct fmt_meta_tbl_t {
    enum code_t {
        kDefault = 0,
        kLeft,
        kMid,
        kRight,
        kOpenBr,
        kCloseBr,
        kDot,
        kDig19,
        kPlus,
        kSpace,
        kMinus,
        kSharp,
        kZero,
        kDec,
        kBin,
        kBinCap,
        kOct,
        kHex,
        kHexCap,
        kFloat,
        kFloatCap,
        kSci,
        kSciCap,
        kGen,
        kGenCap,
        kPtr,
        kPtrCap,
        kStr,
        kChar
    };
    std::array<uint8_t, 128> tbl{};
    CONSTEXPR fmt_meta_tbl_t() {
        for (unsigned ch = 0; ch < tbl.size(); ++ch) { tbl[ch] = code_t::kDefault; }
        tbl['<'] = code_t::kLeft, tbl['^'] = code_t::kMid, tbl['>'] = code_t::kRight;
        tbl['{'] = code_t::kOpenBr, tbl['}'] = code_t::kCloseBr, tbl['.'] = code_t::kDot;
        for (unsigned ch = '1'; ch <= '9'; ++ch) { tbl[ch] = code_t::kDig19; }
        tbl['+'] = code_t::kPlus, tbl[' '] = code_t::kSpace, tbl['-'] = code_t::kMinus;
        tbl['#'] = code_t::kSharp, tbl['0'] = code_t::kZero;
        tbl['d'] = code_t::kDec, tbl['b'] = code_t::kBin, tbl['o'] = code_t::kOct, tbl['x'] = code_t::kHex;
        tbl['B'] = code_t::kBinCap, tbl['X'] = code_t::kHexCap;
        tbl['f'] = code_t::kFloat, tbl['e'] = code_t::kSci, tbl['g'] = code_t::kGen;
        tbl['F'] = code_t::kFloatCap, tbl['E'] = code_t::kSciCap, tbl['G'] = code_t::kGenCap;
        tbl['s'] = code_t::kStr, tbl['c'] = code_t::kChar, tbl['p'] = code_t::kPtr, tbl['P'] = code_t::kPtrCap;
    }
};

#if __cplusplus < 201703L
static const fmt_meta_tbl_t g_meta_tbl;
#else   // __cplusplus < 201703L
static constexpr fmt_meta_tbl_t g_meta_tbl{};
#endif  // __cplusplus < 201703L

template<typename CharT, typename Ty>
const CharT* fmt_accum_num(const CharT* p, const CharT* last, Ty& num) {
    while (p != last && is_digit(*p)) { num = 10 * num + static_cast<Ty>(*p++ - '0'); }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_arg_spec(const CharT* p, const CharT* last, fmt_arg_specs& specs) {
    assert(p != last);

    if (is_digit(*p)) {
        specs.flags |= fmt_parse_flags::kArgNumSpecified;
        specs.n_arg = static_cast<size_t>(*p++ - '0');
        p = fmt_accum_num(p, last, specs.n_arg);
        if (p == last) { return nullptr; }
        if (*p == '}') { return p + 1; }
    }
    if (*p != ':' || ++p == last) { return nullptr; }

    if (p + 1 != last) {
        switch (*(p + 1)) {  // adjustment with fill character
            case '<': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::kLeft, p += 2; break;
            case '^': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::kInternal, p += 2; break;
            case '>': specs.fmt.fill = *p, p += 2; break;
            default: break;
        }
    }

    enum class state_t { kStart = 0, kSign, kAlternate, kLeadingZeroes, kWidth, kPrecision, kType, kFinish };

    state_t state = state_t::kStart;
    while (p != last) {
        auto ch = static_cast<typename std::make_unsigned<CharT>::type>(*p++);
        if (ch >= g_meta_tbl.tbl.size()) { return nullptr; }
        switch (g_meta_tbl.tbl[ch]) {
            // end of format specifier
            case fmt_meta_tbl_t::kDefault: return nullptr;
            case fmt_meta_tbl_t::kCloseBr: return p;

#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < state_t::next_state) { \
        state = state_t::next_state; \
        action; \
        break; \
    } \
    return nullptr;

            // adjustment
            case fmt_meta_tbl_t::kLeft: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kLeft);
            case fmt_meta_tbl_t::kMid: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kInternal);
            case fmt_meta_tbl_t::kRight: UXS_FMT_SPECIFIER_CASE(kSign, {});

            // sign specifiers
            case fmt_meta_tbl_t::kPlus: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignPos);
            case fmt_meta_tbl_t::kSpace: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignAlign);
            case fmt_meta_tbl_t::kMinus: UXS_FMT_SPECIFIER_CASE(kAlternate, {});

            // alternate
            case fmt_meta_tbl_t::kSharp:
                UXS_FMT_SPECIFIER_CASE(kLeadingZeroes, specs.fmt.flags |= fmt_flags::kAlternate);

            // leading zeroes
            case fmt_meta_tbl_t::kZero: UXS_FMT_SPECIFIER_CASE(kWidth, specs.fmt.flags |= fmt_flags::kLeadingZeroes);

            // width
            case fmt_meta_tbl_t::kOpenBr:
                UXS_FMT_SPECIFIER_CASE(kPrecision, {
                    if (p == last) { return nullptr; }
                    specs.flags |= detail::fmt_parse_flags::kDynamicWidth;
                    if (*p == '}') {
                        ++p;
                        break;
                    } else if (is_digit(*p)) {
                        specs.flags |= detail::fmt_parse_flags::kWidthArgNumSpecified;
                        specs.n_width_arg = static_cast<unsigned>(*p++ - '0');
                        p = fmt_accum_num(p, last, specs.n_width_arg);
                        if (p != last && *p++ == '}') { break; }
                    }
                    return nullptr;
                });
            case fmt_meta_tbl_t::kDig19:
                UXS_FMT_SPECIFIER_CASE(kPrecision, {
                    specs.fmt.width = static_cast<unsigned>(*(p - 1) - '0');
                    p = fmt_accum_num(p, last, specs.fmt.width);
                });

            // precision
            case fmt_meta_tbl_t::kDot:
                UXS_FMT_SPECIFIER_CASE(kType, {
                    if (p == last) { return nullptr; }
                    if (is_digit(*p)) {
                        specs.fmt.prec = static_cast<int>(*p++ - '0');
                        p = fmt_accum_num(p, last, specs.fmt.prec);
                        break;
                    } else if (*p == '{' && ++p != last) {
                        specs.flags |= detail::fmt_parse_flags::kDynamicPrec;
                        if (*p == '}') {
                            ++p;
                            break;
                        } else if (is_digit(*p)) {
                            specs.flags |= detail::fmt_parse_flags::kPrecArgNumSpecified;
                            specs.n_prec_arg = static_cast<unsigned>(*p++ - '0');
                            p = fmt_accum_num(p, last, specs.n_prec_arg);
                            if (p != last && *p++ == '}') { break; }
                        }
                    }
                    return nullptr;
                });

            // types
            case fmt_meta_tbl_t::kBin: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kBin);
            case fmt_meta_tbl_t::kOct: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kOct);
            case fmt_meta_tbl_t::kHex: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kHex);

            case fmt_meta_tbl_t::kBinCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kBin | fmt_flags::kUpperCase);
            case fmt_meta_tbl_t::kHexCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kHex | fmt_flags::kUpperCase);

            case fmt_meta_tbl_t::kFloat:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kFixed;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case fmt_meta_tbl_t::kFloatCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kFixed | fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case fmt_meta_tbl_t::kSci:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kScientific;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case fmt_meta_tbl_t::kSciCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kScientific | fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case fmt_meta_tbl_t::kGen:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case fmt_meta_tbl_t::kGenCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case fmt_meta_tbl_t::kDec:
            case fmt_meta_tbl_t::kStr:
            case fmt_meta_tbl_t::kChar:
            case fmt_meta_tbl_t::kPtr: UXS_FMT_SPECIFIER_CASE(kFinish, {});
            case fmt_meta_tbl_t::kPtrCap: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kUpperCase);

            default: UNREACHABLE_CODE;
#undef UXS_FMT_SPECIFIER_CASE
        }
    }

    return nullptr;
}

template<typename StrTy, typename CharT>
std::pair<const CharT*, bool> fmt_parse_next(StrTy& s, const CharT* p, const CharT* last, fmt_arg_specs& specs) {
    const CharT* p0 = p;
    do {
        if (*p == '{' || *p == '}') {
            s.append(p0, p);
            p0 = ++p;
            if (p == last) { break; }
            if (*(p - 1) == '{' && *p != '{') {
                specs.fmt = fmt_state();
                specs.flags = fmt_parse_flags::kDefault;
                if (*p == '}') {
                    ++p;
                } else if (!(p = fmt_parse_arg_spec(p, last, specs))) {
                    throw format_error("invalid format specifier");
                }
                return {p, true};
            }
        }
    } while (++p != last);
    s.append(p0, p);
    return {p, false};
}

template<typename StrTy>
unsigned get_fmt_arg_integer_value(const detail::fmt_arg_list_item<StrTy>& arg, const char* msg_not_integer,
                                   const char* msg_negative) {
#define UXS_FMT_ARG_INTEGER_VALUE_CASE(ty) \
    if (arg.second == detail::fmt_arg_appender<StrTy, ty>::append) { \
        ty val = *reinterpret_cast<const ty*>(arg.first); \
        if (val < 0) { throw format_error(msg_negative); } \
        return static_cast<unsigned>(val); \
    }
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    if (arg.second == detail::fmt_arg_appender<StrTy, ty>::append) { \
        return static_cast<unsigned>(*reinterpret_cast<const ty*>(arg.first)); \
    }
    UXS_FMT_ARG_INTEGER_VALUE_CASE(int32_t)
    UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint32_t)
    UXS_FMT_ARG_INTEGER_VALUE_CASE(int8_t)
    UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint8_t)
    UXS_FMT_ARG_INTEGER_VALUE_CASE(int16_t)
    UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint16_t)
    UXS_FMT_ARG_INTEGER_VALUE_CASE(int64_t)
    UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint64_t)
#undef UXS_FMT_ARG_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
    throw format_error(msg_not_integer);
}

template<typename CharT>
struct utf_count_chars;

template<>
struct utf_count_chars<char> {
    unsigned operator()(uint8_t ch) const { return get_utf8_byte_count(ch); }
};

#if defined(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct utf_count_chars<wchar_t> {
    unsigned operator()(uint32_t ch) const { return 1; }
};
#else   // define(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct utf_count_chars<wchar_t> {
    unsigned operator()(uint16_t ch) const { return get_utf16_word_count(ch); }
};
#endif  // define(WCHAR_MAX) && WCHAR_MAX > 0xffff

template<typename StrTy>
StrTy& fmt_append_string(StrTy& s, std::basic_string_view<typename StrTy::value_type> val, fmt_state& fmt) {
    using CharT = typename StrTy::value_type;
    const CharT *first = val.data(), *last = first + val.size();
    size_t len = 0;
    unsigned count = 0;
    const CharT* p = first;
    if (fmt.prec >= 0) {
        unsigned prec = fmt.prec;
        len = prec;
        while (prec > 0 && static_cast<size_t>(last - p) > count) {
            p += count;
            count = utf_count_chars<CharT>()(*p), --prec;
        }
        if (prec > 0) {
            len -= prec;
        } else if (static_cast<size_t>(last - p) > count) {
            last = p + count;
        }
    } else if (fmt.width > 0) {
        while (static_cast<size_t>(last - p) > count) {
            p += count;
            count = utf_count_chars<CharT>()(*p), ++len;
        }
    }

    if (fmt.width > len) {
        unsigned left = fmt.width - static_cast<unsigned>(len), right = left;
        switch (fmt.flags & fmt_flags::kAdjustField) {
            case fmt_flags::kLeft: left = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            default: right = 0; break;
        }
        return s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
    }
    return s.append(first, last);
}

}  // namespace detail

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt,
                     span<const detail::fmt_arg_list_item<StrTy>> args) {
    unsigned n = 0;
    auto check_arg_idx = [&args](unsigned idx) {
        if (idx >= args.size()) { throw format_error("out of argument list"); }
    };
    detail::fmt_arg_specs specs;
    using CharT = typename StrTy::value_type;
    const CharT *first = fmt.data(), *last = fmt.data() + fmt.size();
    while (first != last) {
        bool put_arg = false;
        std::tie(first, put_arg) = fmt_parse_next(s, first, last, specs);
        if (put_arg) {
            // obtain argument number
            if (!(specs.flags & detail::fmt_parse_flags::kArgNumSpecified)) { specs.n_arg = n++; }
            check_arg_idx(specs.n_arg);

            if (!!(specs.flags & detail::fmt_parse_flags::kDynamicWidth)) {
                // obtain argument number for width
                if (!(specs.flags & detail::fmt_parse_flags::kWidthArgNumSpecified)) { specs.n_width_arg = n++; }
                check_arg_idx(specs.n_width_arg);
                specs.fmt.width = detail::get_fmt_arg_integer_value(args[specs.n_width_arg], "width is not an integer",
                                                                    "negative width specified");
            }
            if (!!(specs.flags & detail::fmt_parse_flags::kDynamicPrec)) {
                // obtain argument number for precision
                if (!(specs.flags & detail::fmt_parse_flags::kPrecArgNumSpecified)) { specs.n_prec_arg = n++; }
                check_arg_idx(specs.n_prec_arg);
                specs.fmt.prec = detail::get_fmt_arg_integer_value(
                    args[specs.n_prec_arg], "precision is not an integer", "negative precision specified");
            }
            args[specs.n_arg].second(s, args[specs.n_arg].first, specs.fmt);
        }
    }
    return s;
}

}  // namespace uxs
