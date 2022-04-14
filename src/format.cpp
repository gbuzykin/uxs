#include "util/format_impl.h"

using namespace util;

inline const char* fmt_parse_adjustment(const char* p, fmt_state& fmt) {
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

inline const char* fmt_parse_sign(const char* p, fmt_state& fmt) {
    switch (*p) {
        case '+': fmt.flags |= fmt_flags::kSignPos; return p + 1;
        case ' ': fmt.flags |= fmt_flags::kSignAlign; return p + 1;
        case '-': return p + 1;
        default: break;
    }
    return p;
}

inline const char* fmt_parse_alternate(const char* p, fmt_state& fmt) {
    if (*p == '#') {
        fmt.flags |= fmt_flags::kShowPoint | fmt_flags::kShowBase;
        return p + 1;
    }
    return p;
}

inline const char* fmt_parse_leading_zeroes(const char* p, fmt_state& fmt) {
    if (*p == '0') {
        fmt.flags |= fmt_flags::kLeadingZeroes;
        return p + 1;
    }
    return p;
}

template<typename Ty>
const char* fmt_parse_num(const char* p, Ty& num) {
    while (is_digit(*p)) { num = 10 * num + static_cast<Ty>(*p++ - '0'); }
    return p;
}

inline const char* fmt_parse_width(const char* p, detail::fmt_arg_specs& specs) {
    if (is_digit(*p)) {
        specs.fmt.width = static_cast<unsigned>(*p++ - '0');
        p = fmt_parse_num<unsigned>(p, specs.fmt.width);
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

inline const char* fmt_parse_precision(const char* p, detail::fmt_arg_specs& specs) {
    if (*p != '.') { return p; }
    if (is_digit(*++p)) {
        specs.fmt.prec = static_cast<int>(*p++ - '0');
        p = fmt_parse_num<int>(p, specs.fmt.prec);
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

inline const char* fmt_parse_type(const char* p, fmt_state& fmt) {
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

namespace util {
namespace detail {

void fmt_parse_arg_spec(const char* p, fmt_arg_specs& specs) {
    specs.fmt = fmt_state();
    specs.flags = fmt_parse_flags::kDefault;
    if (*p == '}') { return; }
    if (is_digit(*p)) {
        specs.flags |= fmt_parse_flags::kArgNumSpecified;
        specs.n_arg = static_cast<size_t>(*p++ - '0');
        p = fmt_parse_num<size_t>(p, specs.n_arg);
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

template std::string& fmt_append_string(std::string_view, std::string&, fmt_state&);
template char_buf_appender& fmt_append_string(std::string_view, char_buf_appender&, fmt_state&);
template char_n_buf_appender& fmt_append_string(std::string_view, char_n_buf_appender&, fmt_state&);

}  // namespace detail

template std::string& format_append_v(std::string_view, std::string&,
                                      span<const detail::fmt_arg_list_item<std::string>>);
template char_buf_appender& format_append_v(std::string_view, char_buf_appender&,
                                            span<const detail::fmt_arg_list_item<char_buf_appender>>);
template char_n_buf_appender& format_append_v(std::string_view, char_n_buf_appender&,
                                              span<const detail::fmt_arg_list_item<char_n_buf_appender>>);

}  // namespace util
