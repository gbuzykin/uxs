#pragma once

#include "utf.h"

#include "uxs/format.h"

namespace uxs {
namespace fmt {

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

template<typename StrTy>
StrTy& format_string(StrTy& s, std::basic_string_view<typename StrTy::value_type> val, fmt_state& fmt) {
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
            case fmt_flags::kLeft: left = 0; break;
            case fmt_flags::kInternal: left >>= 1, right -= left; break;
            default: right = 0; break;
        }
        return s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
    }
    return s.append(first, last);
}

}  // namespace fmt

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt,
                     span<const fmt::arg_list_item<StrTy>> args) {
    auto get_fmt_arg_integer_value = [&args](unsigned n_arg) -> unsigned {
        switch (args[n_arg].type_id) {
#define UXS_FMT_ARG_INTEGER_VALUE_CASE(ty, type_id) \
    case fmt::arg_type_id::type_id: { \
        ty val = *reinterpret_cast<const ty*>(args[n_arg].p_arg); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        return static_cast<unsigned>(val); \
    } break;
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty, type_id) \
    case fmt::arg_type_id::type_id: { \
        return static_cast<unsigned>(*reinterpret_cast<const ty*>(args[n_arg].p_arg)); \
    } break;
            UXS_FMT_ARG_INTEGER_VALUE_CASE(int8_t, kInt8)
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint8_t, kUInt8)
            UXS_FMT_ARG_INTEGER_VALUE_CASE(int16_t, kInt16)
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint16_t, kUInt16)
            UXS_FMT_ARG_INTEGER_VALUE_CASE(int32_t, kInt32)
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint32_t, kUInt32)
            UXS_FMT_ARG_INTEGER_VALUE_CASE(int64_t, kInt64)
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint64_t, kUInt64)
#undef UXS_FMT_ARG_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
            default: throw format_error("argument is not an integer");
        }
    };
    using CharT = typename StrTy::value_type;
    const auto error_code = fmt::parse_format(
        fmt, args.size(), [&s](const CharT* p0, const CharT* p) { s.append(p0, p); },
        [&s, &args](unsigned n_arg, fmt::arg_specs& specs) {
            if (!fmt::check_type(specs, args[n_arg].type_id)) { throw format_error("invalid argument type specifier"); }
            args[n_arg].fmt_func(s, args[n_arg].p_arg, specs.fmt);
        },
        get_fmt_arg_integer_value);
    if (error_code == fmt::parse_format_error_code::kSuccess) {
    } else if (error_code == fmt::parse_format_error_code::kOutOfArgList) {
        throw format_error("out of argument list");
    } else {
        throw format_error("invalid specifier syntax");
    }
    return s;
}

}  // namespace uxs
