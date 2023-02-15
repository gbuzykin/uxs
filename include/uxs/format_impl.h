#pragma once

#include "format.h"
#include "utf.h"

#include <cstring>
#include <locale>

namespace uxs {
namespace sfmt {

template<typename CharT>
struct count_utf_chars;

template<>
struct count_utf_chars<char> {
    unsigned operator()(uint8_t ch) const { return get_utf8_byte_count(ch); }
};

#if defined(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct count_utf_chars<wchar_t> {
    unsigned operator()(uint32_t ch) const { return 1; }
};
#else   // define(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct count_utf_chars<wchar_t> {
    unsigned operator()(uint16_t ch) const { return get_utf16_word_count(ch); }
};
#endif  // define(WCHAR_MAX) && WCHAR_MAX > 0xffff

template<typename CharT, typename StrTy>
void adjust_string(StrTy& s, std::basic_string_view<CharT> val, fmt_opts& fmt) {
    const CharT *first = val.data(), *last = first + val.size();
    size_t len = 0;
    unsigned count = 0;
    const CharT* p = first;
    if (fmt.prec >= 0) {
        unsigned prec = fmt.prec;
        len = prec;
        while (prec > 0 && static_cast<size_t>(last - p) > count) {
            p += count;
            count = count_utf_chars<CharT>()(*p), --prec;
        }
        if (prec > 0) {
            len -= prec;
        } else if (static_cast<size_t>(last - p) > count) {
            last = p + count;
        }
    } else if (fmt.width > 0) {
        while (static_cast<size_t>(last - p) > count) {
            p += count;
            count = count_utf_chars<CharT>()(*p), ++len;
        }
    }
    if (fmt.width > len) {
        unsigned left = fmt.width - static_cast<unsigned>(len), right = left;
        switch (fmt.flags & fmt_flags::kAdjustField) {
            case fmt_flags::kRight: right = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            case fmt_flags::kLeft:
            default: left = 0; break;
        }
        s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
    } else {
        s.append(first, last);
    }
}

template<typename CharT>
bool compare2(const CharT* lhs, const char* rhs) {
    return lhs[0] == rhs[0] && lhs[1] == rhs[1];
}

inline bool compare2(const char* lhs, const char* rhs) { return std::memcmp(lhs, rhs, 2) == 0; }

template<typename Ty>
Ty get_arg_value(const void* data) {
    return *static_cast<const Ty*>(data);
}

template<typename StrTy>
void format_arg_value(StrTy& s, type_id type, const void* data, fmt_opts& fmt) {
    using CharT = typename StrTy::value_type;
    switch (type) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty, id) \
    case type_id::id: { \
        uxs::to_basic_string(s, get_arg_value<ty>(data), fmt); \
    } break;
        UXS_FMT_FORMAT_ARG_VALUE(unsigned, kUnsigned)
        UXS_FMT_FORMAT_ARG_VALUE(unsigned long long, kUnsignedLongLong)
        UXS_FMT_FORMAT_ARG_VALUE(signed, kSigned)
        UXS_FMT_FORMAT_ARG_VALUE(signed long long, kSignedLongLong)
        UXS_FMT_FORMAT_ARG_VALUE(CharT, kChar)
        UXS_FMT_FORMAT_ARG_VALUE(bool, kBool)
        UXS_FMT_FORMAT_ARG_VALUE(float, kFloat)
        UXS_FMT_FORMAT_ARG_VALUE(double, kDouble)
#undef UXS_FMT_FORMAT_ARG_VALUE
        case type_id::kPointer: {
            fmt.flags |= fmt_flags::kHex | fmt_flags::kAlternate;
            uxs::to_basic_string(s, get_arg_value<uintptr_t>(data), fmt);
        } break;
        case type_id::kCString: {
            adjust_string<CharT>(s, get_arg_value<const CharT*>(data), fmt);
        } break;
        case type_id::kString: {
            adjust_string<CharT>(s, get_arg_value<std::basic_string_view<CharT>>(data), fmt);
        } break;
        case type_id::kCustom: {
            const auto* custom = static_cast<const arg_custom_value<StrTy>*>(data);
            custom->print_fn(s, custom->val, fmt);
        } break;
        default: UNREACHABLE_CODE;
    }
}

template<typename StrTy>
StrTy& vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, basic_format_args<StrTy> args,
               const std::locale* p_loc) {
    using CharT = typename StrTy::value_type;
    if (fmt.size() == 2 && compare2(fmt.data(), "{}")) {
        if (args.empty()) { throw format_error("out of argument list"); }
        fmt_opts arg_fmt;
        format_arg_value(s, args.type(0), args.data(0), arg_fmt);
        return s;
    }
    auto get_fmt_arg_integer_value = [&args](unsigned n_arg) -> unsigned {
        switch (args.type(n_arg)) {
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty, id) \
    case type_id::id: { \
        return static_cast<unsigned>(get_arg_value<ty>(args.data(n_arg))); \
    } break;
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty, id) \
    case type_id::id: { \
        ty val = get_arg_value<ty>(args.data(n_arg)); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        return static_cast<unsigned>(val); \
    } break;
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(unsigned, kUnsigned)
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(unsigned long long, kUnsignedLongLong)
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(signed, kSigned)
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(signed long long, kSignedLongLong)
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
            default: throw format_error("argument is not an integer");
        }
    };
    const auto error_code = parse_format<CharT>(
        fmt, args.size(), [&s](const CharT* p0, const CharT* p) { s.append(p0, p); },
        [&s, &args, p_loc](unsigned n_arg, arg_specs& specs) {
            const type_id id = args.type(n_arg);
            const auto type_error = []() { throw format_error("invalid argument type specifier"); };
            const auto signed_needed = []() { throw format_error("argument format requires signed argument"); };
            const auto numeric_needed = []() { throw format_error("argument format requires numeric argument"); };
            switch (specs.flags & parse_flags::kSpecMask) {
                case parse_flags::kSpecIntegral: {
                    if (id <= type_id::kSignedLongLong) {
                        if (!!(specs.fmt.flags & fmt_flags::kSignField) && id < type_id::kSigned) { signed_needed(); }
                    } else if (id == type_id::kChar) {
                        uxs::to_basic_string(s, static_cast<int>(get_arg_value<CharT>(args.data(n_arg))), specs.fmt);
                        return;
                    } else if (id == type_id::kBool) {
                        if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                        uxs::to_basic_string(s, static_cast<int>(get_arg_value<bool>(args.data(n_arg))), specs.fmt);
                        return;
                    } else {
                        type_error();
                    }
                } break;
                case parse_flags::kSpecFloat: {
                    if (id < type_id::kFloat || id > type_id::kLongDouble) { type_error(); }
                } break;
                case parse_flags::kSpecChar: {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes)) { numeric_needed(); }
                    if (id <= type_id::kSignedLongLong) {
                        int64_t code = 0;
                        switch (id) {
#define UXS_FMT_ARG_INTEGER_VALUE_CASE(ty, id) \
    case type_id::id: { \
        code = get_arg_value<ty>(args.data(n_arg)); \
    } break;
                            UXS_FMT_ARG_INTEGER_VALUE_CASE(unsigned, kUnsigned)
                            UXS_FMT_ARG_INTEGER_VALUE_CASE(unsigned long long, kUnsignedLongLong)
                            UXS_FMT_ARG_INTEGER_VALUE_CASE(signed, kSigned)
                            UXS_FMT_ARG_INTEGER_VALUE_CASE(signed long long, kSignedLongLong)
#undef UXS_FMT_ARG_INTEGER_VALUE_CASE
                            default: UNREACHABLE_CODE;
                        }
                        const int64_t char_mask = (1ull << (8 * sizeof(CharT))) - 1;
                        if ((code & char_mask) != code && (~code & char_mask) != code) {
                            throw format_error("integral cannot be represented as character");
                        }
                        uxs::to_basic_string(s, static_cast<CharT>(code), specs.fmt);
                        return;
                    } else if (id > type_id::kChar) {
                        type_error();
                    }
                } break;
                case parse_flags::kSpecPointer: {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (id != type_id::kPointer) { type_error(); }
                } break;
                case parse_flags::kSpecString: {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes)) { numeric_needed(); }
                    if (id != type_id::kBool && id != type_id::kCString && id != type_id::kString) { type_error(); }
                } break;
                default: {
                    if (!!(specs.fmt.flags & fmt_flags::kSignField) &&
                        (id < type_id::kSigned || id > type_id::kSignedLongLong) &&
                        (id < type_id::kFloat || id > type_id::kLongDouble)) {
                        signed_needed();
                    }
                    if (!!(specs.fmt.flags & fmt_flags::kLeadingZeroes) && id > type_id::kSignedLongLong &&
                        (id < type_id::kFloat || id > type_id::kPointer)) {
                        numeric_needed();
                    }
                } break;
            }
            if (!(specs.flags & parse_flags::kUseLocale)) {
                format_arg_value(s, args.type(n_arg), args.data(n_arg), specs.fmt);
            } else {
                std::locale loc(p_loc ? *p_loc : std::locale());
                specs.fmt.loc = &loc;
                format_arg_value(s, args.type(n_arg), args.data(n_arg), specs.fmt);
            }
        },
        get_fmt_arg_integer_value);
    if (error_code == parse_format_error_code::kSuccess) {
    } else if (error_code == parse_format_error_code::kOutOfArgList) {
        throw format_error("out of argument list");
    } else {
        throw format_error("invalid specifier syntax");
    }
    return s;
}

}  // namespace sfmt
}  // namespace uxs
