#pragma once

#include "utf.h"
#include "util/format.h"

namespace util {
namespace detail {

enum class fmt_parse_flags : unsigned {
    kDefault = 0,
    kDynamicWidth = 1,
    kDynamicPrec = 2,
    kArgNumSpecified = 0x10,
    kWidthArgNumSpecified = 0x20,
    kPrecArgNumSpecified = 0x40,
};
UTIL_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_parse_flags, unsigned);

struct fmt_arg_specs {
    fmt_state fmt;
    fmt_parse_flags flags = fmt_parse_flags::kDefault;
    size_t n_arg = 0;
    size_t n_width_arg = 0;
    size_t n_prec_arg = 0;
};

template<typename CharT>
const CharT* fmt_parse_adjustment(const CharT* p, fmt_state& fmt) {
    switch (*p) {
        case '<': fmt.flags |= fmt_flags::kLeft; return p + 1;
        case '^': fmt.flags |= fmt_flags::kInternal; return p + 1;
        case '>': return p + 1;
        default: {
            switch (*(p + 1)) {
                case '<': fmt.fill = *p, fmt.flags |= fmt_flags::kLeft; return p + 2;
                case '^': fmt.fill = *p, fmt.flags |= fmt_flags::kInternal; return p + 2;
                case '>': fmt.fill = *p; return p + 2;
                default: break;
            }
        } break;
    }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_sign(const CharT* p, fmt_state& fmt) {
    switch (*p) {
        case '+': fmt.flags |= fmt_flags::kSignPos; return p + 1;
        case ' ': fmt.flags |= fmt_flags::kSignAlign; return p + 1;
        case '-': return p + 1;
        default: break;
    }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_alternate(const CharT* p, fmt_state& fmt) {
    if (*p == '#') {
        fmt.flags |= fmt_flags::kAlternate;
        return p + 1;
    }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_leading_zeroes(const CharT* p, fmt_state& fmt) {
    if (*p == '0') {
        fmt.flags |= fmt_flags::kLeadingZeroes;
        return p + 1;
    }
    return p;
}

template<typename CharT, typename Ty>
const CharT* fmt_parse_num(const CharT* p, Ty& num) {
    while (is_digit(*p)) { num = 10 * num + static_cast<Ty>(*p++ - '0'); }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_width(const CharT* p, detail::fmt_arg_specs& specs) {
    if (is_digit(*p)) {
        specs.fmt.width = static_cast<unsigned>(*p++ - '0');
        p = fmt_parse_num(p, specs.fmt.width);
    } else if (*p == '{') {
        specs.flags |= detail::fmt_parse_flags::kDynamicWidth;
        if (*++p == '}') { return p + 1; }
        if (is_digit(*p)) {
            specs.flags |= detail::fmt_parse_flags::kWidthArgNumSpecified;
            specs.n_width_arg = static_cast<size_t>(*p++ - '0');
            p = fmt_parse_num(p, specs.n_width_arg);
        }
        while (*p++ != '}') {}
    }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_precision(const CharT* p, detail::fmt_arg_specs& specs) {
    if (*p != '.') { return p; }
    if (is_digit(*++p)) {
        specs.fmt.prec = static_cast<int>(*p++ - '0');
        p = fmt_parse_num(p, specs.fmt.prec);
    } else if (*p == '{') {
        specs.flags |= detail::fmt_parse_flags::kDynamicPrec;
        if (*++p == '}') { return p + 1; }
        if (is_digit(*p)) {
            specs.flags |= detail::fmt_parse_flags::kPrecArgNumSpecified;
            specs.n_prec_arg = static_cast<size_t>(*p++ - '0');
            p = fmt_parse_num(p, specs.n_prec_arg);
        }
        while (*p++ != '}') {}
    }
    return p;
}

template<typename CharT>
const CharT* fmt_parse_type(const CharT* p, fmt_state& fmt) {
    switch (*p++) {
        case 's': return p;
        case 'c': return p;
        case 'b': fmt.flags |= fmt_flags::kBin; return p;
        case 'B': fmt.flags |= fmt_flags::kBin | fmt_flags::kUpperCase; return p;
        case 'o': fmt.flags |= fmt_flags::kOct; return p;
        case 'd': return p;
        case 'x': fmt.flags |= fmt_flags::kHex; return p;
        case 'X': fmt.flags |= fmt_flags::kHex | fmt_flags::kUpperCase; return p;
        case 'p': return p;
        case 'P': fmt.flags |= fmt_flags::kUpperCase; return p;
        case 'f': {
            fmt.flags |= fmt_flags::kFixed;
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        case 'F': {
            fmt.flags |= fmt_flags::kFixed | fmt_flags::kUpperCase;
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        case 'e': {
            fmt.flags |= fmt_flags::kScientific;
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        case 'E': {
            fmt.flags |= fmt_flags::kScientific | fmt_flags::kUpperCase;
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        case 'g': {
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        case 'G': {
            fmt.flags |= fmt_flags::kUpperCase;
            if (fmt.prec < 0) { fmt.prec = 6; }
            return p;
        } break;
        default: break;
    }
    return p - 1;
}

template<typename CharT>
void fmt_parse_arg_spec(const CharT* p, fmt_arg_specs& specs) {
    specs.fmt = fmt_state();
    specs.flags = fmt_parse_flags::kDefault;
    if (*p == '}') { return; }
    if (is_digit(*p)) {
        specs.flags |= fmt_parse_flags::kArgNumSpecified;
        specs.n_arg = static_cast<size_t>(*p++ - '0');
        p = fmt_parse_num(p, specs.n_arg);
    }
    if (*p++ == ':') {
        p = fmt_parse_adjustment(p, specs.fmt);
        p = fmt_parse_sign(p, specs.fmt);
        p = fmt_parse_alternate(p, specs.fmt);
        p = fmt_parse_leading_zeroes(p, specs.fmt);
        p = fmt_parse_width(p, specs);
        p = fmt_parse_precision(p, specs);
        fmt_parse_type(p, specs.fmt);
    }
}

template<typename StrTy, typename CharT>
std::pair<const CharT*, bool> fmt_parse_next(StrTy& s, const CharT* p, const CharT* last, fmt_arg_specs& specs) {
    const CharT* p0 = p;
    do {
        if (*p == '{' || *p == '}') {
            s.append(p0, p);
            p0 = ++p;
            if (p == last) { break; }
            int balance = 1;
            if (*(p - 1) == '{' && *p != '{') {
                do {
                    if (*p == '}' && --balance == 0) {
                        fmt_parse_arg_spec(p0, specs);
                        return {++p, true};
                    } else if (*p == '{') {
                        ++balance;
                    }
                } while (++p != last);
                return {p, false};
            }
        }
    } while (++p != last);
    s.append(p0, p);
    return {p, false};
}

template<typename StrTy>
unsigned get_fmt_arg_integer_value(const detail::fmt_arg_list_item<StrTy>& arg, const char* msg_not_integer,
                                   const char* msg_negative) {
#define FMT_ARG_INTEGER_VALUE_CASE(ty) \
    if (arg.second == detail::fmt_arg_appender<StrTy, ty>::append) { \
        ty val = *reinterpret_cast<const ty*>(arg.first); \
        if (val < 0) { throw format_error(msg_negative); } \
        return static_cast<unsigned>(val); \
    }
#define FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    if (arg.second == detail::fmt_arg_appender<StrTy, ty>::append) { \
        return static_cast<unsigned>(*reinterpret_cast<const ty*>(arg.first)); \
    }
    FMT_ARG_INTEGER_VALUE_CASE(int32_t)
    FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint32_t)
    FMT_ARG_INTEGER_VALUE_CASE(int8_t)
    FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint8_t)
    FMT_ARG_INTEGER_VALUE_CASE(int16_t)
    FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint16_t)
    FMT_ARG_INTEGER_VALUE_CASE(int64_t)
    FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(uint64_t)
#undef FMT_ARG_INTEGER_VALUE_CASE
#undef FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
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
        while (prec > 0 && last - p > count) {
            p += count;
            count = utf_count_chars<CharT>()(*p), --prec;
        }
        if (prec > 0) {
            len -= prec;
        } else if (last - p > count) {
            last = p + count;
        }
    } else if (fmt.width > 0) {
        while (last - p > count) {
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
    size_t n = 0;
    auto check_arg_idx = [&args](size_t idx) {
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

}  // namespace util
