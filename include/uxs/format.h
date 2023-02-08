#pragma once

#include "alignment.h"
#include "io/iobuf.h"
#include "stringcvt.h"

#include <array>

namespace uxs {

class format_error : public std::runtime_error {
 public:
    explicit format_error(const char* message) : std::runtime_error(message) {}
    explicit format_error(const std::string& message) : std::runtime_error(message) {}
};

namespace sfmt {

enum class arg_type_id {
    kUnsignedChar = 0,
    kUnsignedShort,
    kUnsigned,
    kUnsignedLong,
    kUnsignedLongLong,
    kSignedChar,
    kSignedShort,
    kSigned,
    kSignedLong,
    kSignedLongLong,
    kChar,
    kWChar,
    kBool,
    kFloat,
    kDouble,
    kLongDouble,
    kPointer,
    kString,
    kCustom
};

template<typename Ty, typename = void>
struct type_id_t : std::integral_constant<arg_type_id, arg_type_id::kCustom> {};

#define UXS_FMT_DECLARE_TYPE_ID(ty, type_id) \
    template<> \
    struct type_id_t<ty, void> : std::integral_constant<arg_type_id, arg_type_id::type_id> {};
UXS_FMT_DECLARE_TYPE_ID(unsigned char, kUnsignedChar)
UXS_FMT_DECLARE_TYPE_ID(unsigned short, kUnsignedShort)
UXS_FMT_DECLARE_TYPE_ID(unsigned, kUnsigned)
UXS_FMT_DECLARE_TYPE_ID(unsigned long, kUnsignedLong)
UXS_FMT_DECLARE_TYPE_ID(unsigned long long, kUnsignedLongLong)
UXS_FMT_DECLARE_TYPE_ID(signed char, kSignedChar)
UXS_FMT_DECLARE_TYPE_ID(signed short, kSignedShort)
UXS_FMT_DECLARE_TYPE_ID(signed, kSigned)
UXS_FMT_DECLARE_TYPE_ID(signed long, kSignedLong)
UXS_FMT_DECLARE_TYPE_ID(signed long long, kSignedLongLong)
UXS_FMT_DECLARE_TYPE_ID(char, kChar)
UXS_FMT_DECLARE_TYPE_ID(wchar_t, kWChar)
UXS_FMT_DECLARE_TYPE_ID(bool, kBool)
UXS_FMT_DECLARE_TYPE_ID(float, kFloat)
UXS_FMT_DECLARE_TYPE_ID(double, kDouble)
UXS_FMT_DECLARE_TYPE_ID(long double, kLongDouble)
#undef UXS_FMT_DECLARE_TYPE_ID

template<typename Ty>
struct type_id_t<Ty*, std::enable_if_t<!is_character<Ty>::value>>
    : std::integral_constant<arg_type_id, arg_type_id::kPointer> {};

template<typename CharT>
struct type_id_t<CharT*, std::enable_if_t<is_character<CharT>::value>>
    : std::integral_constant<arg_type_id, arg_type_id::kString> {};

template<typename CharT, typename Traits>
struct type_id_t<std::basic_string_view<CharT, Traits>, void>
    : std::integral_constant<arg_type_id, arg_type_id::kString> {};

template<typename CharT, typename Traits, typename Alloc>
struct type_id_t<std::basic_string<CharT, Traits, Alloc>, void>
    : std::integral_constant<arg_type_id, arg_type_id::kString> {};

struct arg_store_value {
    template<typename... Ts>
    using aligned_storage_t = typename std::aligned_storage<size_of<Ts...>::value, alignment_of<Ts...>::value>::type;
    aligned_storage_t<double, long long, void*> data;
    arg_store_value() = default;
    template<typename Ty>
    explicit arg_store_value(const Ty& v) {
        ::new (&data) Ty(v);
    }
    template<typename Ty>
    Ty as() const {
        return Ty(*reinterpret_cast<const Ty*>(&data));
    }
};

template<typename StrTy, typename Ty, typename = void>
struct arg_fmt_func_t;

template<typename StrTy, typename Ty>
struct arg_fmt_func_t<
    StrTy, Ty, std::enable_if_t<has_formatter<Ty, StrTy>::value && (type_id_t<Ty>::value < arg_type_id::kPointer)>> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        uxs::to_basic_string(s, val.as<Ty>(), fmt);  // stored by value
    }
};

template<typename StrTy, typename Ty>
struct arg_fmt_func_t<
    StrTy, Ty, std::enable_if_t<has_formatter<Ty, StrTy>::value && (type_id_t<Ty>::value >= arg_type_id::kPointer)>> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        uxs::to_basic_string(s, *val.as<const Ty*>(), fmt);  // stored by pointer
    }
};

template<typename StrTy, typename Ty>
struct arg_fmt_func_t<StrTy, Ty*, std::enable_if_t<!is_character<Ty>::value>> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        fmt.flags |= fmt_flags::kHex | fmt_flags::kAlternate;
        uxs::to_basic_string(s, val.as<uintptr_t>(), fmt);
    }
};

template<typename CharT, typename StrTy>
UXS_EXPORT void adjust_string(StrTy& s, span<const CharT> val, fmt_opts& fmt);

template<typename StrTy>
struct arg_fmt_func_t<StrTy, typename StrTy::value_type*, void> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        using CharT = typename StrTy::value_type;
        adjust_string<CharT>(s, std::basic_string_view<CharT>(val.as<const CharT*>()), fmt);
    }
};

template<typename StrTy, typename Traits>
struct arg_fmt_func_t<StrTy, std::basic_string_view<typename StrTy::value_type, Traits>, void> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        using CharT = typename StrTy::value_type;
        adjust_string<CharT>(s, *val.as<const std::basic_string_view<CharT, Traits>*>(), fmt);
    }
};

template<typename StrTy, typename Traits, typename Alloc>
struct arg_fmt_func_t<StrTy, std::basic_string<typename StrTy::value_type, Traits, Alloc>, void> {
    static void func(StrTy& s, arg_store_value val, fmt_opts& fmt) {
        using CharT = typename StrTy::value_type;
        adjust_string<CharT>(s, *val.as<const std::basic_string<CharT, Traits, Alloc>*>(), fmt);
    }
};

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

template<>
struct arg_type<std::nullptr_t, void> {
    using type = void*;
};

template<typename Ty>
std::enable_if_t<(type_id_t<typename arg_type<Ty>::type>::value < arg_type_id::kPointer), arg_store_value>
make_arg_store_value(const Ty& v) {
    return arg_store_value{v};  // store by value
}

template<typename Ty>
std::enable_if_t<(type_id_t<typename arg_type<Ty>::type>::value >= arg_type_id::kPointer), arg_store_value>
make_arg_store_value(const Ty& v) {
    return arg_store_value{&v};  // store by pointer
}

template<typename Ty>
arg_store_value make_arg_store_value(Ty* v) {
    return arg_store_value{v};  // store the pointer
}

inline arg_store_value make_arg_store_value(std::nullptr_t) { return arg_store_value{static_cast<void*>(nullptr)}; }

template<typename StrTy>
struct arg_store_item {
    arg_store_item() = default;
    template<typename Ty>
    explicit arg_store_item(const Ty& arg)
        : value(make_arg_store_value(arg)), fmt_func(arg_fmt_func_t<StrTy, typename arg_type<Ty>::type>::func),
          type_id(type_id_t<typename arg_type<Ty>::type>::value) {}
    arg_store_value value;
    void (*fmt_func)(StrTy&, arg_store_value, fmt_opts&) = nullptr;
    arg_type_id type_id = arg_type_id::kUnsignedChar;
};

template<typename StrTy, typename... Args>
using arg_store = std::array<const arg_store_item<StrTy>, sizeof...(Args)>;

template<typename StrTy>
using arg_list = span<const arg_store_item<StrTy>>;

enum class parse_flags : unsigned {
    kDefault = 0,
    kSpecIntegral = 1,
    kSpecFloat = 2,
    kSpecChar = 3,
    kSpecPointer = 4,
    kSpecString = 5,
    kSpecMask = 0xff,
    kDynamicWidth = 0x100,
    kDynamicPrec = 0x200,
    kUseLocale = 0x400,
    kArgNumSpecified = 0x1000,
    kWidthArgNumSpecified = 0x2000,
    kPrecArgNumSpecified = 0x4000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(parse_flags, unsigned);

struct arg_specs {
    fmt_opts fmt;
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
        kLocale,
        kDec,
        kBin,
        kBinCap,
        kOct,
        kHex,
        kHexCap,
        kChar,
        kHexFloat,
        kHexFloatCap,
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
        tbl['#'] = code_t::kSharp, tbl['0'] = code_t::kZero, tbl['L'] = code_t::kLocale;
        tbl['d'] = code_t::kDec, tbl['b'] = code_t::kBin, tbl['o'] = code_t::kOct, tbl['x'] = code_t::kHex;
        tbl['B'] = code_t::kBinCap, tbl['X'] = code_t::kHexCap, tbl['c'] = code_t::kChar;
        tbl['a'] = code_t::kHexFloat, tbl['A'] = code_t::kHexFloatCap;
        tbl['f'] = code_t::kFixed, tbl['e'] = code_t::kScientific, tbl['g'] = code_t::kGeneral;
        tbl['F'] = code_t::kFixedCap, tbl['E'] = code_t::kScientificCap, tbl['G'] = code_t::kGeneralCap;
        tbl['s'] = code_t::kString, tbl['p'] = code_t::kPtr, tbl['P'] = code_t::kPtrCap;
    }
};

#if __cplusplus < 201703L
extern UXS_EXPORT const meta_tbl_t g_meta_tbl;
#else   // __cplusplus < 201703L
static constexpr meta_tbl_t g_meta_tbl{};
#endif  // __cplusplus < 201703L

template<typename CharT, typename Ty>
CONSTEXPR const CharT* accum_num(const CharT* p, const CharT* last, Ty& num) {
    for (unsigned dig = 0; p != last && (dig = dig_v(*p)) < 10; ++p) { num = 10 * num + dig; }
    return p;
}

template<typename CharT>
CONSTEXPR const CharT* parse_arg_spec(const CharT* p, const CharT* last, arg_specs& specs) NOEXCEPT {
    assert(p != last && *p != '}');

    unsigned dig = 0;
    if ((dig = dig_v(*p)) < 10) {
        specs.flags |= parse_flags::kArgNumSpecified;
        specs.n_arg = dig;
        p = accum_num(++p, last, specs.n_arg);
        if (p == last) { return nullptr; }
        if (*p == '}') { return p; }
    }
    if (*p != ':' || ++p == last) { return nullptr; }

    if (p + 1 != last) {
        switch (*(p + 1)) {  // adjustment with fill character
            case '<': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::kLeft, p += 2; break;
            case '^': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::kInternal, p += 2; break;
            case '>': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::kRight, p += 2; break;
            default: break;
        }
    }

    enum class state_t { kStart = 0, kSign, kAlternate, kLeadingZeroes, kWidth, kPrecision, kLocale, kType, kFinish };

    state_t state = state_t::kStart;
    while (p != last) {
        auto ch = static_cast<typename std::make_unsigned<CharT>::type>(*p++);
        if (ch >= g_meta_tbl.tbl.size()) { return nullptr; }
        switch (g_meta_tbl.tbl[ch]) {
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < state_t::next_state) { \
        state = state_t::next_state; \
        action; \
        break; \
    } \
    return nullptr;

            // adjustment
            case meta_tbl_t::kLeft: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kLeft);
            case meta_tbl_t::kMid: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kInternal);
            case meta_tbl_t::kRight: UXS_FMT_SPECIFIER_CASE(kSign, specs.fmt.flags |= fmt_flags::kRight);

            // sign specifiers
            case meta_tbl_t::kPlus: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignPos);
            case meta_tbl_t::kSpace: UXS_FMT_SPECIFIER_CASE(kAlternate, specs.fmt.flags |= fmt_flags::kSignAlign);
            case meta_tbl_t::kMinus: UXS_FMT_SPECIFIER_CASE(kAlternate, {});

            // alternate
            case meta_tbl_t::kSharp: UXS_FMT_SPECIFIER_CASE(kLeadingZeroes, specs.fmt.flags |= fmt_flags::kAlternate);

            // leading zeroes
            case meta_tbl_t::kZero:
                UXS_FMT_SPECIFIER_CASE(kWidth, {
                    if (!(specs.fmt.flags & fmt_flags::kAdjustField)) { specs.fmt.flags |= fmt_flags::kLeadingZeroes; }
                });

            // width
            case meta_tbl_t::kOpenBr:
                UXS_FMT_SPECIFIER_CASE(kPrecision, {
                    if (p == last) { return nullptr; }
                    specs.flags |= parse_flags::kDynamicWidth;
                    if (*p == '}') {
                        ++p;
                        break;
                    } else if ((dig = dig_v(*p)) < 10) {
                        specs.flags |= parse_flags::kWidthArgNumSpecified;
                        specs.n_width_arg = dig;
                        p = accum_num(++p, last, specs.n_width_arg);
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
                UXS_FMT_SPECIFIER_CASE(kLocale, {
                    if (p == last) { return nullptr; }
                    if ((dig = dig_v(*p)) < 10) {
                        specs.fmt.prec = dig;
                        p = accum_num(++p, last, specs.fmt.prec);
                        break;
                    } else if (*p == '{' && ++p != last) {
                        specs.flags |= parse_flags::kDynamicPrec;
                        if (*p == '}') {
                            ++p;
                            break;
                        } else if ((dig = dig_v(*p)) < 10) {
                            specs.flags |= parse_flags::kPrecArgNumSpecified;
                            specs.n_prec_arg = dig;
                            p = accum_num(++p, last, specs.n_prec_arg);
                            if (p != last && *p++ == '}') { break; }
                        }
                    }
                    return nullptr;
                });

            // locale
            case meta_tbl_t::kLocale: UXS_FMT_SPECIFIER_CASE(kType, { specs.flags |= parse_flags::kUseLocale; });

            // types
            case meta_tbl_t::kBin:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecIntegral;
                    specs.fmt.flags |= fmt_flags::kBin;
                });
            case meta_tbl_t::kOct:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecIntegral;
                    specs.fmt.flags |= fmt_flags::kOct;
                });
            case meta_tbl_t::kHex:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecIntegral;
                    specs.fmt.flags |= fmt_flags::kHex;
                });

            case meta_tbl_t::kBinCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecIntegral;
                    specs.fmt.flags |= fmt_flags::kBin | fmt_flags::kUpperCase;
                });
            case meta_tbl_t::kHexCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecIntegral;
                    specs.fmt.flags |= fmt_flags::kHex | fmt_flags::kUpperCase;
                });

            case meta_tbl_t::kHexFloat:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kHex;
                });
            case meta_tbl_t::kHexFloatCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kHex | fmt_flags::kUpperCase;
                });

            case meta_tbl_t::kFixed:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kFixed;
                });
            case meta_tbl_t::kFixedCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kFixed | fmt_flags::kUpperCase;
                });

            case meta_tbl_t::kScientific:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kScientific;
                });
            case meta_tbl_t::kScientificCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kScientific | fmt_flags::kUpperCase;
                });

            case meta_tbl_t::kGeneral:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kGeneral;
                });
            case meta_tbl_t::kGeneralCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecFloat;
                    specs.fmt.flags |= fmt_flags::kGeneral | fmt_flags::kUpperCase;
                });

            case meta_tbl_t::kDec: UXS_FMT_SPECIFIER_CASE(kFinish, specs.flags |= parse_flags::kSpecIntegral);
            case meta_tbl_t::kString: UXS_FMT_SPECIFIER_CASE(kFinish, specs.flags |= parse_flags::kSpecString);
            case meta_tbl_t::kChar: UXS_FMT_SPECIFIER_CASE(kFinish, specs.flags |= parse_flags::kSpecChar);
            case meta_tbl_t::kPtr: UXS_FMT_SPECIFIER_CASE(kFinish, specs.flags |= parse_flags::kSpecPointer);
            case meta_tbl_t::kPtrCap:
                UXS_FMT_SPECIFIER_CASE(kFinish, {
                    specs.flags |= parse_flags::kSpecPointer;
                    specs.fmt.flags |= fmt_flags::kUpperCase;
                });
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
CONSTEXPR parse_format_error_code parse_format(span<const CharT> fmt, const size_t arg_count, const AppendFn& append_fn,
                                               const AppendArgFn& append_arg_fn, const GetUIntArgFn& get_uint_arg_fn) {
    unsigned n_arg_auto = 0;
    const CharT *first0 = fmt.data(), *first = first0, *last = first0 + fmt.size();
    while (first != last) {
        if (*first == '{' || *first == '}') {
            append_fn(first0, first);
            first0 = ++first;
            if (first == last) { return parse_format_error_code::kInvalidFormat; }
            if (*(first - 1) == '{' && *first != '{') {
                arg_specs specs;
                if (*first == '}') {  // most usual `{}` specifier
                    specs.n_arg = n_arg_auto++;
                    if (specs.n_arg >= arg_count) { return parse_format_error_code::kOutOfArgList; }
                } else if ((first = parse_arg_spec(first, last, specs))) {
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
                } else {
                    return parse_format_error_code::kInvalidFormat;
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

template<typename CharT>
UXS_EXPORT basic_membuffer<CharT>& vformat(basic_membuffer<CharT>& s, span<const CharT> fmt,
                                           arg_list<basic_membuffer<CharT>> args, const std::locale* p_loc = nullptr);

}  // namespace sfmt

template<typename StrTy>
using basic_format_arg = sfmt::arg_store_item<StrTy>;
using format_arg = basic_format_arg<membuffer>;
using wformat_arg = basic_format_arg<wmembuffer>;

template<typename StrTy>
using basic_format_args = sfmt::arg_list<StrTy>;
using format_args = basic_format_args<membuffer>;
using wformat_args = basic_format_args<wmembuffer>;

template<typename StrTy, typename... Args>
sfmt::arg_store<StrTy, Args...> make_basic_format_args(const Args&... args) NOEXCEPT {
    return {{sfmt::arg_store_item<StrTy>{args}...}};
}

template<typename StrTy = membuffer, typename... Args>
sfmt::arg_store<StrTy, Args...> make_format_args(const Args&... args) NOEXCEPT {
    return make_basic_format_args<StrTy>(args...);
}

template<typename StrTy = wmembuffer, typename... Args>
sfmt::arg_store<StrTy, Args...> make_wformat_args(const Args&... args) NOEXCEPT {
    return make_basic_format_args<StrTy>(args...);
}

template<typename CharT, typename Traits = std::char_traits<CharT>>
struct basic_runtime_string {
    std::basic_string_view<CharT, Traits> s;
};

template<typename Str, typename = std::enable_if_t<
                           std::is_convertible<const Str&, std::basic_string_view<typename Str::value_type>>::value>>
basic_runtime_string<typename Str::value_type> make_runtime_string(const Str& s) {
    return basic_runtime_string<typename Str::value_type>{s};
}

template<typename CharT>
basic_runtime_string<CharT> make_runtime_string(const CharT* s) {
    return basic_runtime_string<CharT>{s};
}

inline void format_string_error(const char*) {}

template<typename CharT, typename Traits = std::char_traits<CharT>, typename... Args>
class basic_format_string {
 public:
    template<typename Ty,
             typename = std::enable_if_t<std::is_convertible<const Ty&, std::basic_string_view<CharT, Traits>>::value>>
    CONSTEVAL basic_format_string(const Ty& fmt) : checked(fmt) {
#if defined(HAS_CONSTEVAL)
        const std::array<sfmt::arg_type_id, sizeof...(Args)> arg_type_ids{
            sfmt::type_id_t<typename sfmt::arg_type<Args>::type>::value...};
        const auto error_code = sfmt::parse_format<CharT>(
            checked, sizeof...(Args), [](const CharT*, const CharT*) constexpr {},
            [&arg_type_ids](unsigned n_arg, sfmt::arg_specs& specs) constexpr {
                const sfmt::arg_type_id id = arg_type_ids[n_arg];
                const sfmt::parse_flags flag = specs.flags & sfmt::parse_flags::kSpecMask;
                auto signed_needed = []() { format_string_error("argument format requires signed argument"); };
                const auto numeric_needed = []() { throw format_error("argument format requires numeric argument"); };
                if (flag == sfmt::parse_flags::kDefault) {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField) &&
                        (id < sfmt::arg_type_id::kSignedChar || id > sfmt::arg_type_id::kSignedLongLong) &&
                        (id < sfmt::arg_type_id::kFloat || id > sfmt::arg_type_id::kLongDouble)) {
                        signed_needed();
                    }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes) && id > sfmt::arg_type_id::kSignedLongLong &&
                        (id < sfmt::arg_type_id::kFloat || id > sfmt::arg_type_id::kPointer)) {
                        numeric_needed();
                    }
                    return;
                } else if (flag == sfmt::parse_flags::kSpecIntegral) {
                    if (id <= sfmt::arg_type_id::kBool) {
                        if (!!(specs.fmt.flags & fmt_flags::kSignField) &&
                            (id < sfmt::arg_type_id::kSignedChar || id > sfmt::arg_type_id::kWChar) &&
                            (id < sfmt::arg_type_id::kFloat || id > sfmt::arg_type_id::kLongDouble)) {
                            signed_needed();
                        }
                        return;
                    }
                } else if (flag == sfmt::parse_flags::kSpecFloat) {
                    if (id >= sfmt::arg_type_id::kFloat && id <= sfmt::arg_type_id::kLongDouble) { return; }
                } else if (flag == sfmt::parse_flags::kSpecChar) {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes)) { numeric_needed(); }
                    if (id >= sfmt::arg_type_id::kUnsignedChar && id <= sfmt::arg_type_id::kWChar) { return; }
                } else if (flag == sfmt::parse_flags::kSpecPointer) {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (id == sfmt::arg_type_id::kPointer) { return; }
                } else if (flag == sfmt::parse_flags::kSpecString) {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes)) { numeric_needed(); }
                    if (id == sfmt::arg_type_id::kBool || id == sfmt::arg_type_id::kString) { return; }
                }
                format_string_error("invalid argument format specifier");
            },
            [&arg_type_ids](unsigned n_arg) constexpr->unsigned {
                if (arg_type_ids[n_arg] > sfmt::arg_type_id::kSignedLongLong) {
                    format_string_error("argument is not an integer");
                }
                return 0;
            });
        if (error_code == sfmt::parse_format_error_code::kSuccess) {
        } else if (error_code == sfmt::parse_format_error_code::kOutOfArgList) {
            format_string_error("out of argument list");
        } else {
            format_string_error("invalid specifier syntax");
        }
#endif  // defined(HAS_CONSTEVAL)
    }
    basic_format_string(basic_runtime_string<CharT, Traits> fmt) : checked(fmt.s) {}
    std::basic_string_view<CharT, Traits> get() const { return checked; }

 private:
    std::basic_string_view<CharT, Traits> checked;
};

#if defined(_MSC_VER) && _MSC_VER <= 1800
template<typename... Args>
using format_string = basic_format_string<char>;
template<typename... Args>
using wformat_string = basic_format_string<wchar_t>;
#else   // defined(_MSC_VER) && _MSC_VER <= 1800
template<typename... Args>
using format_string = basic_format_string<char, std::char_traits<char>, type_identity_t<Args>...>;
template<typename... Args>
using wformat_string = basic_format_string<wchar_t, std::char_traits<wchar_t>, type_identity_t<Args>...>;
#endif  // defined(_MSC_VER) && _MSC_VER <= 1800

// ---- vformat

template<typename... Args>
NODISCARD std::string vformat(std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args);
    return std::string(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::wstring vformat(std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::string vformat(const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args, &loc);
    return std::string(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::wstring vformat(const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args, &loc);
    return std::wstring(buf.data(), buf.size());
}

// ---- format

template<typename... Args>
NODISCARD std::string format(format_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_format_args(args...));
}

template<typename... Args>
NODISCARD std::wstring format(wformat_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
NODISCARD std::string format(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_format_args(args...));
}

template<typename... Args>
NODISCARD std::wstring format(const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to

template<typename... Args>
char* vformat_to(char* p, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return sfmt::vformat<char>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to(wchar_t* p, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return sfmt::vformat<wchar_t>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
char* vformat_to(char* p, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return sfmt::vformat<char>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to(wchar_t* p, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return sfmt::vformat<wchar_t>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

// ---- format_to

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to_n

template<typename... Args>
char* vformat_to_n(char* p, size_t n, std::string_view fmt, format_args args) {
    membuffer buf(p, p + n);
    return sfmt::vformat<char>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to_n(wchar_t* p, size_t n, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p, p + n);
    return sfmt::vformat<wchar_t>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
char* vformat_to_n(char* p, size_t n, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p, p + n);
    return sfmt::vformat<char>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<char>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to_n(wchar_t* p, size_t n, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p, p + n);
    return sfmt::vformat<wchar_t>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wchar_t>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

// ---- format_to_n

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_wformat_args(args...));
}

// ---- vprint

template<typename CharT>
basic_iobuf<CharT>& print_quoted_text(basic_iobuf<CharT>& out, std::basic_string_view<CharT> text) {
    const CharT *p1 = text.data(), *pend = text.data() + text.size();
    out.put('\"');
    for (const CharT* p2 = text.data(); p2 != pend; ++p2) {
        std::string_view esc;
        switch (*p2) {
            case '\"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\a': esc = "\\a"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\v': esc = "\\v"; break;
            default: continue;
        }
        out.write(as_span(p1, p2 - p1));
        for (char ch : esc) { out.put(ch); }
        p1 = p2 + 1;
    }
    out.write(as_span(p1, pend - p1));
    out.put('\"');
    return out;
}

template<typename Ty>
class basic_membuffer_for_iobuf final : public basic_membuffer<Ty> {
 public:
    explicit basic_membuffer_for_iobuf(basic_iobuf<Ty>& out) NOEXCEPT
        : basic_membuffer<Ty>(out.first_avail(), out.last_avail()),
          out_(out) {}
    ~basic_membuffer_for_iobuf() override { out_.advance(this->curr() - out_.first_avail()); }

    bool try_grow(size_t extra) override {
        out_.advance(this->curr() - out_.first_avail());
        if (!out_.reserve().good()) { return false; }
        this->set(out_.first_avail(), out_.last_avail());
        return this->avail() >= extra;
    }

 private:
    basic_iobuf<Ty>& out_;
};

using membuffer_for_iobuf = basic_membuffer_for_iobuf<char>;
using wmembuffer_for_iobuf = basic_membuffer_for_iobuf<wchar_t>;

template<typename... Args>
iobuf& vprint(iobuf& out, std::string_view fmt, format_args args) {
    membuffer_for_iobuf buf(out);
    sfmt::vformat<char>(buf, fmt, args);
    return out;
}

template<typename... Args>
wiobuf& vprint(wiobuf& out, std::wstring_view fmt, wformat_args args) {
    wmembuffer_for_iobuf buf(out);
    sfmt::vformat<wchar_t>(buf, fmt, args);
    return out;
}

template<typename... Args>
iobuf& vprint(iobuf& out, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer_for_iobuf buf(out);
    sfmt::vformat<char>(buf, fmt, args, &loc);
    return out;
}

template<typename... Args>
wiobuf& vprint(wiobuf& out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer_for_iobuf buf(out);
    sfmt::vformat<wchar_t>(buf, fmt, args, &loc);
    return out;
}

// ---- print

template<typename... Args>
iobuf& print(iobuf& out, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_format_args(args...));
}

template<typename... Args>
wiobuf& print(wiobuf& out, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
iobuf& print(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out, fmt.get(), make_format_args(args...));
}

template<typename... Args>
iobuf& print(iobuf& out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_format_args(args...));
}

template<typename... Args>
wiobuf& print(wiobuf& out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
iobuf& print(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out, loc, fmt.get(), make_format_args(args...));
}

// ---- println

template<typename... Args>
iobuf& println(iobuf& out, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
wiobuf& println(wiobuf& out, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_wformat_args(args...)).endl();
}

template<typename... Args>
iobuf& println(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out, fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
iobuf& println(iobuf& out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
wiobuf& println(wiobuf& out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_wformat_args(args...)).endl();
}

template<typename... Args>
iobuf& println(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out, loc, fmt.get(), make_format_args(args...)).endl();
}

}  // namespace uxs
