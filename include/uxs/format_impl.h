#pragma once

#include "format.h"
#include "utf.h"

namespace uxs {
namespace sfmt {

template<typename CharT>
struct count_utf_chars;

template<>
struct count_utf_chars<char> {
    unsigned operator()(std::uint8_t ch) const { return get_utf8_byte_count(ch); }
};

#if defined(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct count_utf_chars<wchar_t> {
    unsigned operator()(std::uint32_t ch) const { return 1; }
};
#else   // define(WCHAR_MAX) && WCHAR_MAX > 0xffff
template<>
struct count_utf_chars<wchar_t> {
    unsigned operator()(std::uint16_t ch) const { return get_utf16_word_count(ch); }
};
#endif  // define(WCHAR_MAX) && WCHAR_MAX > 0xffff

template<typename CharT, typename StrTy>
void adjust_string(StrTy& s, std::basic_string_view<CharT> val, const fmt_opts& fmt) {
    const CharT *first = val.data(), *last = first + val.size();
    std::size_t len = 0;
    unsigned count = 0;
    const CharT* p = first;
    if (fmt.prec >= 0) {
        unsigned prec = fmt.prec;
        len = prec;
        while (prec > 0 && static_cast<std::size_t>(last - p) > count) {
            p += count;
            count = count_utf_chars<CharT>{}(*p), --prec;
        }
        if (prec > 0) {
            len -= prec;
        } else if (static_cast<std::size_t>(last - p) > count) {
            last = p + count;
        }
    } else if (fmt.width > 0) {
        while (static_cast<std::size_t>(last - p) > count) {
            p += count;
            count = count_utf_chars<CharT>{}(*p), ++len;
        }
    }
    if (fmt.width > len) {
        unsigned left = fmt.width - static_cast<unsigned>(len), right = left;
        switch (fmt.flags & fmt_flags::adjust_field) {
            case fmt_flags::right: right = 0; break;
            case fmt_flags::internal: left >>= 1, right -= left; break;
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
void format_arg_value(StrTy& s, type_index index, const void* data, fmt_opts& fmt) {
    using char_type = typename StrTy::value_type;
    switch (index) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty, index) \
    case index: { \
        to_basic_string(s, get_arg_value<ty>(data), fmt); \
    } break;
        UXS_FMT_FORMAT_ARG_VALUE(bool, type_index::boolean)
        UXS_FMT_FORMAT_ARG_VALUE(char_type, type_index::character)
        UXS_FMT_FORMAT_ARG_VALUE(signed, type_index::integer)
        UXS_FMT_FORMAT_ARG_VALUE(signed long long, type_index::long_integer)
        UXS_FMT_FORMAT_ARG_VALUE(unsigned, type_index::unsigned_integer)
        UXS_FMT_FORMAT_ARG_VALUE(unsigned long long, type_index::unsigned_long_integer)
        UXS_FMT_FORMAT_ARG_VALUE(float, type_index::single_precision)
        UXS_FMT_FORMAT_ARG_VALUE(double, type_index::double_precision)
#undef UXS_FMT_FORMAT_ARG_VALUE
        case type_index::pointer: {
            fmt.flags |= fmt_flags::hex | fmt_flags::alternate;
            to_basic_string(s, get_arg_value<uintptr_t>(data), fmt);
        } break;
        case type_index::z_string: {
            adjust_string<char_type>(s, get_arg_value<const char_type*>(data), fmt);
        } break;
        case type_index::string: {
            adjust_string<char_type>(s, get_arg_value<std::basic_string_view<char_type>>(data), fmt);
        } break;
        case type_index::custom: {
            const auto* custom = static_cast<const custom_arg_handle<StrTy>*>(data);
            custom->print_fn(s, custom->val, fmt);
        } break;
        default: UNREACHABLE_CODE;
    }
}

inline unsigned get_arg_integer_value(type_index index, const void* data) {
    switch (index) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty, index) \
    case index: { \
        ty val = get_arg_value<ty>(data); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        return static_cast<unsigned>(val); \
    } break;
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty, index) \
    case index: { \
        return static_cast<unsigned>(get_arg_value<ty>(data)); \
    } break;
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(signed, type_index::integer)
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(signed long long, type_index::long_integer)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(unsigned, type_index::unsigned_integer)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(unsigned long long, type_index::unsigned_long_integer)
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
        default: UNREACHABLE_CODE;
    }
}

template<typename StrTy>
StrTy& vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, basic_format_args<StrTy> args,
               const std::locale* p_loc) {
    using char_type = typename StrTy::value_type;
    if (fmt.size() == 2 && compare2(fmt.data(), "{}")) {
        if (args.empty()) { throw format_error("out of argument list"); }
        fmt_opts arg_fmt;
        format_arg_value(s, args.index(0), args.data(0), arg_fmt);
        return s;
    }
    parse_format<char_type>(
        fmt, args.metadata(), [&s](const char_type* p0, const char_type* p) { s.append(p0, p); },
        [&s, &args, p_loc](arg_specs& specs) {
            if (!!(specs.flags & parse_flags::dynamic_width)) {
                specs.fmt.width = get_arg_integer_value(args.index(specs.n_width_arg), args.data(specs.n_width_arg));
            }
            if (!!(specs.flags & parse_flags::dynamic_prec)) {
                specs.fmt.prec = get_arg_integer_value(args.index(specs.n_prec_arg), args.data(specs.n_prec_arg));
            }
            if (!!(specs.fmt.flags & fmt_flags::localize)) {
                static const std::locale default_locale{};
                specs.fmt.loc = p_loc ? p_loc : &default_locale;
            }
            format_arg_value(s, args.index(specs.n_arg), args.data(specs.n_arg), specs.fmt);
        });
    return s;
}

}  // namespace sfmt
}  // namespace uxs
