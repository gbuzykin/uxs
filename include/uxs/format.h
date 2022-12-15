#pragma once

#include "io/iobuf.h"
#include "stringcvt.h"

#include <array>

namespace uxs {

class format_error : public std::runtime_error {
 public:
    explicit format_error(const char* message) : std::runtime_error(message) {}
    explicit format_error(const std::string& message) : std::runtime_error(message) {}
};

namespace fmt {

enum class arg_type_id {
    kInt8 = 0,
    kUInt8,
    kInt16,
    kUInt16,
    kInt32,
    kUInt32,
    kInt64,
    kUInt64,
    kChar,
    kWChar,
    kBool,
    kFloat,
    kDouble,
    kPointer,
    kString,
    kCustom
};

template<typename Ty, typename = void>
struct type_id_t : std::integral_constant<arg_type_id, arg_type_id::kCustom> {};

#define UXS_FMT_DECLARE_TYPE_ID(ty, type_id) \
    template<> \
    struct type_id_t<ty, void> : std::integral_constant<arg_type_id, arg_type_id::type_id> {};
UXS_FMT_DECLARE_TYPE_ID(int8_t, kInt8)
UXS_FMT_DECLARE_TYPE_ID(uint8_t, kUInt8)
UXS_FMT_DECLARE_TYPE_ID(int16_t, kInt16)
UXS_FMT_DECLARE_TYPE_ID(uint16_t, kUInt16)
UXS_FMT_DECLARE_TYPE_ID(int32_t, kInt32)
UXS_FMT_DECLARE_TYPE_ID(uint32_t, kUInt32)
UXS_FMT_DECLARE_TYPE_ID(int64_t, kInt64)
UXS_FMT_DECLARE_TYPE_ID(uint64_t, kUInt64)
UXS_FMT_DECLARE_TYPE_ID(char, kChar)
UXS_FMT_DECLARE_TYPE_ID(wchar_t, kWChar)
UXS_FMT_DECLARE_TYPE_ID(bool, kBool)
UXS_FMT_DECLARE_TYPE_ID(float, kFloat)
UXS_FMT_DECLARE_TYPE_ID(double, kDouble)
#undef UXS_FMT_DECLARE_TYPE_ID

template<typename Ty>
struct type_id_t<Ty*, std::enable_if_t<!is_character<Ty>::value>>
    : std::integral_constant<arg_type_id, arg_type_id::kPointer> {};

template<typename CharT>
struct type_id_t<CharT*, std::enable_if_t<is_character<CharT>::value>>
    : std::integral_constant<arg_type_id, arg_type_id::kString> {};

template<typename CharT>
struct type_id_t<std::basic_string_view<CharT>, void> : std::integral_constant<arg_type_id, arg_type_id::kString> {};

template<typename CharT>
struct type_id_t<std::basic_string<CharT>, void> : std::integral_constant<arg_type_id, arg_type_id::kString> {};

template<typename StrTy, typename Ty, typename = void>
struct arg_fmt_func_t;

template<typename StrTy, typename Ty>
struct arg_fmt_func_t<StrTy, Ty, std::void_t<typename string_converter<Ty>::is_string_converter>> {
    static StrTy& func(StrTy& s, const void* val, fmt_state& fmt) {
        return to_basic_string(s, *reinterpret_cast<const Ty*>(val), fmt);
    }
};

template<typename StrTy, typename Ty>
struct arg_fmt_func_t<StrTy, Ty*, std::enable_if_t<!is_character<Ty>::value>> {
    static StrTy& func(StrTy& s, const void* val, fmt_state& fmt) {
        fmt.flags &= ~fmt_flags::kBaseField;
        fmt.flags |= fmt_flags::kHex | fmt_flags::kAlternate;
        return to_basic_string(s, reinterpret_cast<uintptr_t>(val), fmt);
    }
};

template<typename StrTy>
UXS_EXPORT StrTy& format_string(StrTy& s, std::basic_string_view<typename StrTy::value_type> val, fmt_state& fmt);

template<typename StrTy>
struct arg_fmt_func_t<StrTy, typename StrTy::value_type*, void> {
    static StrTy& func(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return format_string(s, std::basic_string_view<CharT>(static_cast<const CharT*>(val)), fmt);
    }
};

template<typename StrTy>
struct arg_fmt_func_t<StrTy, std::basic_string_view<typename StrTy::value_type>, void> {
    static StrTy& func(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return format_string(s, *reinterpret_cast<const std::basic_string_view<CharT>*>(val), fmt);
    }
};

template<typename StrTy>
struct arg_fmt_func_t<StrTy, std::basic_string<typename StrTy::value_type>, void> {
    static StrTy& func(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return format_string(s, *reinterpret_cast<const std::basic_string<CharT>*>(val), fmt);
    }
};

template<typename Ty>
const void* get_arg_ptr(const Ty& arg) {
    return reinterpret_cast<const void*>(&arg);
}

template<typename Ty>
const void* get_arg_ptr(Ty* arg) {
    return arg;
}

template<typename Ty, typename = void>
struct arg_type {
    using type = Ty;
};

template<typename Ty>
struct arg_type<Ty, std::enable_if_t<std::is_array<Ty>::value>> {
    using type = typename std::add_pointer<std::remove_cv_t<typename std::remove_extent<Ty>::type>>::type;
};

template<typename Ty>
struct arg_type<Ty*, void> {
    using type = typename std::add_pointer<std::remove_cv_t<Ty>>::type;
};

template<typename StrTy>
struct arg_list_item {
#if __cplusplus < 201703L
    arg_list_item() = default;
    arg_list_item(const void* p, StrTy& (*fn)(StrTy&, const void*, fmt_state&), arg_type_id id)
        : p_arg(p), fmt_func(fn), type_id(id) {}
#endif  // __cplusplus < 201703L
    const void* p_arg = nullptr;
    StrTy& (*fmt_func)(StrTy&, const void*, fmt_state&) = nullptr;
    arg_type_id type_id = arg_type_id::kInt8;
};

template<typename StrTy, typename... Ts>
std::array<const arg_list_item<StrTy>, sizeof...(Ts)> make_args(const Ts&... args) NOEXCEPT {
    return {{{get_arg_ptr(args), arg_fmt_func_t<StrTy, typename arg_type<Ts>::type>::func,
              type_id_t<typename arg_type<Ts>::type>::value}...}};
}

enum class parse_flags : unsigned {
    kDefault = 0,
    kFmtTypeMask = 0xff,
    kDynamicWidth = 0x100,
    kDynamicPrec = 0x200,
    kArgNumSpecified = 0x1000,
    kWidthArgNumSpecified = 0x2000,
    kPrecArgNumSpecified = 0x4000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(parse_flags, unsigned);

struct arg_specs {
    fmt_state fmt;
    parse_flags flags = parse_flags::kDefault;
    unsigned n_arg = 0;
    unsigned n_width_arg = 0;
    unsigned n_prec_arg = 0;
};

struct meta_tbl_t {
    enum code_t {
        kLeft = 0,
        kMid,
        kRight,
        kOpenBr,
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
        kChar,
        kFixed,
        kFixedCap,
        kScientific,
        kScientificCap,
        kGeneral,
        kGeneralCap,
        kPtr,
        kPtrCap,
        kString,
        kCloseBr,
        kOther
    };
    std::array<uint8_t, 128> tbl{};
    CONSTEXPR meta_tbl_t() {
        for (unsigned ch = 0; ch < tbl.size(); ++ch) { tbl[ch] = code_t::kOther; }
        tbl['<'] = code_t::kLeft, tbl['^'] = code_t::kMid, tbl['>'] = code_t::kRight;
        tbl['{'] = code_t::kOpenBr, tbl['}'] = code_t::kCloseBr, tbl['.'] = code_t::kDot;
        for (unsigned ch = '1'; ch <= '9'; ++ch) { tbl[ch] = code_t::kDig19; }
        tbl['+'] = code_t::kPlus, tbl[' '] = code_t::kSpace, tbl['-'] = code_t::kMinus;
        tbl['#'] = code_t::kSharp, tbl['0'] = code_t::kZero;
        tbl['d'] = code_t::kDec, tbl['b'] = code_t::kBin, tbl['o'] = code_t::kOct, tbl['x'] = code_t::kHex;
        tbl['B'] = code_t::kBinCap, tbl['X'] = code_t::kHexCap, tbl['c'] = code_t::kChar;
        tbl['f'] = code_t::kFixed, tbl['e'] = code_t::kScientific, tbl['g'] = code_t::kGeneral;
        tbl['F'] = code_t::kFixedCap, tbl['E'] = code_t::kScientificCap, tbl['G'] = code_t::kGeneralCap;
        tbl['s'] = code_t::kString, tbl['p'] = code_t::kPtr, tbl['P'] = code_t::kPtrCap;
    }
};

#if __cplusplus < 201703L
extern const meta_tbl_t g_meta_tbl;
extern const std::array<const char*, static_cast<unsigned>(arg_type_id::kCustom) + 1> g_allowed_types;
#else   // __cplusplus < 201703L
static constexpr meta_tbl_t g_meta_tbl{};
static constexpr std::array<const char*, static_cast<unsigned>(arg_type_id::kCustom) + 1> g_allowed_types{
    "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod",
    "c",      "c",      "",       "fFeEgG", "fFeEgG", "pP",     "s",      ""};
#endif  // __cplusplus < 201703L

template<typename CharT, typename Ty>
CONSTEXPR const CharT* accum_num(const CharT* p, const CharT* last, Ty& num) {
    while (p != last && is_digit(*p)) { num = 10 * num + static_cast<Ty>(*p++ - '0'); }
    return p;
}

template<typename CharT>
CONSTEXPR const CharT* parse_arg_spec(const CharT* p, const CharT* last, arg_specs& specs) {
    assert(p != last && *p != '}');

    if (is_digit(*p)) {
        specs.flags |= parse_flags::kArgNumSpecified;
        specs.n_arg = static_cast<unsigned>(*p++ - '0');
        p = accum_num(p, last, specs.n_arg);
        if (p == last) { return nullptr; }
        if (*p == '}') { return p; }
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
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < state_t::next_state) { \
        state = state_t::next_state; \
        if CONSTEXPR_IF (state_t::next_state == state_t::kFinish) { \
            specs.flags |= static_cast<parse_flags>(static_cast<uint8_t>(ch)); \
        } \
        action; \
        break; \
    } \
    return nullptr;

            // adjustment
            case meta_tbl_t::kLeft: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kLeft);
            case meta_tbl_t::kMid: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kInternal);
            case meta_tbl_t::kRight: UXS_FMT_SPECIFIER_CASE(kSign, {});

            // sign specifiers
            case meta_tbl_t::kPlus: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignPos);
            case meta_tbl_t::kSpace: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignAlign);
            case meta_tbl_t::kMinus: UXS_FMT_SPECIFIER_CASE(kAlternate, {});

            // alternate
            case meta_tbl_t::kSharp: UXS_FMT_SPECIFIER_CASE(kLeadingZeroes, specs.fmt.flags |= fmt_flags::kAlternate);

            // leading zeroes
            case meta_tbl_t::kZero: UXS_FMT_SPECIFIER_CASE(kWidth, specs.fmt.flags |= fmt_flags::kLeadingZeroes);

            // width
            case meta_tbl_t::kOpenBr:
                UXS_FMT_SPECIFIER_CASE(kPrecision, {
                    if (p == last) { return nullptr; }
                    specs.flags |= parse_flags::kDynamicWidth;
                    if (*p == '}') {
                        ++p;
                        break;
                    } else if (is_digit(*p)) {
                        specs.flags |= parse_flags::kWidthArgNumSpecified;
                        specs.n_width_arg = static_cast<unsigned>(*p++ - '0');
                        p = accum_num(p, last, specs.n_width_arg);
                        if (p != last && *p++ == '}') { break; }
                    }
                    return nullptr;
                });
            case meta_tbl_t::kDig19:
                UXS_FMT_SPECIFIER_CASE(kPrecision, {
                    specs.fmt.width = static_cast<unsigned>(*(p - 1) - '0');
                    p = accum_num(p, last, specs.fmt.width);
                });

            // precision
            case meta_tbl_t::kDot:
                UXS_FMT_SPECIFIER_CASE(kType, {
                    if (p == last) { return nullptr; }
                    if (is_digit(*p)) {
                        specs.fmt.prec = static_cast<int>(*p++ - '0');
                        p = accum_num(p, last, specs.fmt.prec);
                        break;
                    } else if (*p == '{' && ++p != last) {
                        specs.flags |= parse_flags::kDynamicPrec;
                        if (*p == '}') {
                            ++p;
                            break;
                        } else if (is_digit(*p)) {
                            specs.flags |= parse_flags::kPrecArgNumSpecified;
                            specs.n_prec_arg = static_cast<unsigned>(*p++ - '0');
                            p = accum_num(p, last, specs.n_prec_arg);
                            if (p != last && *p++ == '}') { break; }
                        }
                    }
                    return nullptr;
                });

            // types
            case meta_tbl_t::kBin: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kBin);
            case meta_tbl_t::kOct: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kOct);
            case meta_tbl_t::kHex: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kHex);

            case meta_tbl_t::kBinCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kBin | fmt_flags::kUpperCase);
            case meta_tbl_t::kHexCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kHex | fmt_flags::kUpperCase);

            case meta_tbl_t::kFixed:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kFixed;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case meta_tbl_t::kFixedCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kFixed | fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case meta_tbl_t::kScientific:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kScientific;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case meta_tbl_t::kScientificCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kScientific | fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case meta_tbl_t::kGeneral:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });
            case meta_tbl_t::kGeneralCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.fmt.flags |= fmt_flags::kUpperCase;
                    if (specs.fmt.prec < 0) { specs.fmt.prec = 6; }
                });

            case meta_tbl_t::kDec:
            case meta_tbl_t::kString:
            case meta_tbl_t::kChar:
            case meta_tbl_t::kPtr: UXS_FMT_SPECIFIER_CASE(kFinish, {});
            case meta_tbl_t::kPtrCap: UXS_FMT_SPECIFIER_CASE(kFinish, specs.fmt.flags |= fmt_flags::kUpperCase);
#undef UXS_FMT_SPECIFIER_CASE

            // end of format specifier
            case meta_tbl_t::kCloseBr: return p - 1;
            case meta_tbl_t::kOther: return nullptr;

            default: UNREACHABLE_CODE;
        }
    }

    return nullptr;
}

enum class parse_format_error_code { kSuccess = 0, kInvalidFormat, kOutOfArgList };

template<typename CharT, typename AppendFn, typename AppendArgFn, typename GetUIntArgFn>
CONSTEXPR parse_format_error_code parse_format(std::basic_string_view<CharT> fmt, const size_t arg_count,
                                               const AppendFn& append_fn, const AppendArgFn& append_arg_fn,
                                               const GetUIntArgFn& get_uint_arg_fn) {
    unsigned n_arg_auto = 0;
    const CharT *first0 = fmt.data(), *first = first0, *last = first0 + fmt.size();
    while (first != last) {
        if (*first == '{' || *first == '}') {
            append_fn(first0, first);
            first0 = ++first;
            if (first == last) { return parse_format_error_code::kInvalidFormat; }
            if (*(first - 1) == '{' && *first != '{') {
                arg_specs specs;
                if (*first != '}' && !(first = parse_arg_spec(first, last, specs))) {
                    return parse_format_error_code::kInvalidFormat;
                }
                // obtain argument number
                if (!(specs.flags & parse_flags::kArgNumSpecified)) { specs.n_arg = n_arg_auto++; }
                if (specs.n_arg >= arg_count) { return parse_format_error_code::kOutOfArgList; }
                if (!!(specs.flags & parse_flags::kDynamicWidth)) {
                    // obtain argument number for width
                    if (!(specs.flags & parse_flags::kWidthArgNumSpecified)) { specs.n_width_arg = n_arg_auto++; }
                    if (specs.n_width_arg >= arg_count) { return parse_format_error_code::kOutOfArgList; }
                    specs.fmt.width = get_uint_arg_fn(specs.n_width_arg);
                }
                if (!!(specs.flags & parse_flags::kDynamicPrec)) {
                    // obtain argument number for precision
                    if (!(specs.flags & parse_flags::kPrecArgNumSpecified)) { specs.n_prec_arg = n_arg_auto++; }
                    if (specs.n_prec_arg >= arg_count) { return parse_format_error_code::kOutOfArgList; }
                    specs.fmt.prec = get_uint_arg_fn(specs.n_prec_arg);
                }
                append_arg_fn(specs.n_arg, specs);
                first0 = first + 1;
            } else if (*(first - 1) != *first) {
                return parse_format_error_code::kInvalidFormat;
            }
        }
        ++first;
    }
    append_fn(first0, last);
    return parse_format_error_code::kSuccess;
}

CONSTEXPR bool check_type(const arg_specs& specs, arg_type_id type_id) {
    const char type = static_cast<char>(specs.flags & parse_flags::kFmtTypeMask);
    if (type) {
        const char* p_types = g_allowed_types[static_cast<unsigned>(type_id)];
        while (*p_types && *p_types != type) { ++p_types; }
        if (!*p_types) { return false; }
    }
    return true;
}

}  // namespace fmt

template<typename Char>
struct basic_runtime_string {
    std::basic_string_view<Char> s;
};

using runtime_string = basic_runtime_string<char>;
using wruntime_string = basic_runtime_string<wchar_t>;

inline void format_string_error(const char*) {}

template<typename CharT, typename... Ts>
struct basic_format_string {
    std::basic_string_view<CharT> checked;
    template<typename Ty,
             typename = std::enable_if_t<std::is_convertible<const Ty&, std::basic_string_view<CharT>>::value>>
    CONSTEVAL basic_format_string(const Ty& fmt) : checked(fmt) {
#if defined(HAS_CONSTEVAL)
        const std::array<fmt::arg_type_id, sizeof...(Ts)> arg_type_ids{
            fmt::type_id_t<typename fmt::arg_type<Ts>::type>::value...};
        const auto error_code = fmt::parse_format(
            checked, sizeof...(Ts), [](const CharT*, const CharT*) constexpr {},
            [&arg_type_ids](unsigned n_arg, fmt::arg_specs& specs) constexpr {
                if (!fmt::check_type(specs, arg_type_ids[n_arg])) {
                    format_string_error("invalid argument format specifier");
                }
            },
            [&arg_type_ids](unsigned n_arg) constexpr->unsigned {
                if (arg_type_ids[n_arg] > fmt::arg_type_id::kUInt64) {
                    format_string_error("argument is not an integer");
                }
                return 0;
            });
        if (error_code == fmt::parse_format_error_code::kSuccess) {
        } else if (error_code == fmt::parse_format_error_code::kOutOfArgList) {
            format_string_error("out of argument list");
        } else {
            format_string_error("invalid specifier syntax");
        }
#endif  // defined(HAS_CONSTEVAL)
    }
    basic_format_string(basic_runtime_string<CharT> fmt) : checked(fmt.s) {}
};

template<typename StrTy>
UXS_EXPORT StrTy& basic_vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt,
                                span<const fmt::arg_list_item<StrTy>> args);

#if defined(_MSC_VER) && _MSC_VER <= 1800
template<typename... Ts>
using format_string = basic_format_string<char>;
template<typename... Ts>
using wformat_string = basic_format_string<wchar_t>;
template<typename StrTy, typename... Ts>
StrTy& basic_format(StrTy& s, basic_format_string<typename StrTy::value_type> fmt, const Ts&... args) {
    return basic_vformat<StrTy>(s, fmt.checked, fmt::make_args<StrTy>(args...));
}
#else   // defined(_MSC_VER) && _MSC_VER <= 1800
template<typename... Ts>
using format_string = basic_format_string<char, type_identity_t<Ts>...>;
template<typename... Ts>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Ts>...>;
template<typename StrTy, typename... Ts>
StrTy& basic_format(StrTy& s, basic_format_string<typename StrTy::value_type, Ts...> fmt, const Ts&... args) {
    return basic_vformat<StrTy>(s, fmt.checked, fmt::make_args<StrTy>(args...));
}
#endif  // defined(_MSC_VER) && _MSC_VER <= 1800

template<typename... Ts>
NODISCARD std::string format(format_string<Ts...> fmt, const Ts&... args) {
    inline_dynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    return std::string(buf.data(), buf.size());
}

template<typename... Ts>
NODISCARD std::wstring format(wformat_string<Ts...> fmt, const Ts&... args) {
    inline_wdynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    return std::wstring(buf.data(), buf.size());
}

template<typename... Ts>
char* format_to(char* buf, format_string<Ts...> fmt, const Ts&... args) {
    unlimbuf_appender appender(buf);
    return basic_format(appender, fmt, args...).curr();
}

template<typename... Ts>
wchar_t* format_to(wchar_t* buf, wformat_string<Ts...> fmt, const Ts&... args) {
    wunlimbuf_appender appender(buf);
    return basic_format(appender, fmt, args...).curr();
}

template<typename... Ts>
char* format_to_n(char* buf, size_t n, format_string<Ts...> fmt, const Ts&... args) {
    limbuf_appender appender(buf, n);
    return basic_format(appender, fmt, args...).curr();
}

template<typename... Ts>
wchar_t* format_to_n(wchar_t* buf, size_t n, wformat_string<Ts...> fmt, const Ts&... args) {
    wlimbuf_appender appender(buf, n);
    return basic_format(appender, fmt, args...).curr();
}

template<typename... Ts>
iobuf& fprint(iobuf& out, format_string<Ts...> fmt, const Ts&... args) {
    inline_dynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    return out.write(as_span(buf.data(), buf.size()));
}

template<typename... Ts>
wiobuf& fprint(wiobuf& out, wformat_string<Ts...> fmt, const Ts&... args) {
    inline_wdynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    return out.write(as_span(buf.data(), buf.size()));
}

template<typename... Ts>
iobuf& fprintln(iobuf& out, format_string<Ts...> fmt, const Ts&... args) {
    inline_dynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    buf.push_back('\n');
    return out.write(as_span(buf.data(), buf.size())).flush();
}

template<typename... Ts>
wiobuf& fprintln(wiobuf& out, wformat_string<Ts...> fmt, const Ts&... args) {
    inline_wdynbuffer buf;
    basic_format(buf.base(), fmt, args...);
    buf.push_back('\n');
    return out.write(as_span(buf.data(), buf.size())).flush();
}

template<typename... Ts>
iobuf& print(format_string<Ts...> fmt, const Ts&... args) {
    return fprint(stdbuf::out, fmt, args...);
}

template<typename... Ts>
iobuf& println(format_string<Ts...> fmt, const Ts&... args) {
    return fprintln(stdbuf::out, fmt, args...);
}

}  // namespace uxs
