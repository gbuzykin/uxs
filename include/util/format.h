#pragma once

#include "span.h"
#include "stringcvt.h"
#include "utf.h"

#include <array>
#include <stdexcept>

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

UTIL_EXPORT void fmt_parse_arg_spec(const char* p, fmt_arg_specs& specs);

template<typename StrTy>
std::pair<const char*, bool> fmt_parse(const char* p, const char* last, StrTy& s, fmt_arg_specs& specs) {
    const char* p0 = p;
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
StrTy& fmt_append_string(std::string_view val, StrTy& s, fmt_state& fmt) {
    const char *first = val.data(), *last = first + val.size();
    size_t len = 0;
    unsigned count = 0;
    if (fmt.prec >= 0) {
        unsigned prec = fmt.prec;
        const char* p = first;
        while (prec > 0 && last - p > count) {
            p += count;
            count = get_utf8_byte_count(*p), --prec;
        }
        if (prec == 0 && last - p > count) { last = p + count; }
        if (fmt.width == 0) { return s.append(first, last); }
        len = static_cast<unsigned>(fmt.prec) - prec;
    } else if (fmt.width > 0) {
        const char* p = first;
        while (last - p > count) {
            p += count;
            count = get_utf8_byte_count(*p), ++len;
        }
    }

    if (len >= fmt.width) { return s.append(first, last); }

    switch (fmt.flags & fmt_flags::kAdjustField) {
        case fmt_flags::kLeft: {
            s.append(first, last).append(fmt.width - static_cast<unsigned>(len), fmt.fill);
        } break;
        case fmt_flags::kInternal: {
            unsigned right = fmt.width - static_cast<unsigned>(len), left = right >> 1;
            right -= left;
            s.append(left, fmt.fill).append(first, last).append(right, fmt.fill);
        } break;
        default: {
            s.append(fmt.width - static_cast<unsigned>(len), fmt.fill).append(first, last);
        } break;
    }
    return s;
}

template<typename Ty, typename StrTy, typename = void>
struct fmt_arg_appender;

template<typename Ty, typename StrTy>
struct fmt_arg_appender<Ty, StrTy, std::void_t<typename string_converter<Ty>::is_string_converter>> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return string_converter<Ty>::to_string(*reinterpret_cast<const Ty*>(val), s, fmt);
    }
};

template<typename Ty, typename StrTy>
struct fmt_arg_appender<Ty*, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        fmt.flags &= ~fmt_flags::kBaseField;
        fmt.flags |= fmt_flags::kHex | fmt_flags::kShowBase;
        return string_converter<uintptr_t>::to_string(reinterpret_cast<uintptr_t>(val), s, fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<char*, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(std::string_view(static_cast<const char*>(val)), s, fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<std::string_view, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(*reinterpret_cast<const std::string_view*>(val), s, fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<StrTy, char*, void> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        return fmt_append_string(s, std::string_view(static_cast<const char*>(val)), fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<std::string, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(*reinterpret_cast<const std::string*>(val), s, fmt);
    }
};

template<typename Ty>
const void* get_fmt_arg_ptr(const Ty& arg) {
    return reinterpret_cast<const void*>(&arg);
}

template<typename Ty>
const void* get_fmt_arg_ptr(Ty* arg) {
    return arg;
}

template<typename StrTy>
using fmt_arg_list_item = std::pair<const void*, StrTy& (*)(const void*, StrTy&, fmt_state&)>;

template<typename Ty, typename = void>
struct fmt_arg_type {
    using type = Ty;
};

template<typename Ty>
struct fmt_arg_type<Ty, std::enable_if_t<std::is_array<Ty>::value>> {
    using type = typename std::add_pointer<std::remove_cv_t<typename std::remove_extent<Ty>::type>>::type;
};

template<typename Ty>
struct fmt_arg_type<Ty*, void> {
    using type = typename std::add_pointer<std::remove_cv_t<Ty>>::type;
};

template<typename StrTy, typename... Ts>
auto make_fmt_args(const Ts&... args) NOEXCEPT -> std::array<fmt_arg_list_item<StrTy>, sizeof...(Ts)> {
    return {{{get_fmt_arg_ptr(args), fmt_arg_appender<typename fmt_arg_type<Ts>::type, StrTy>::append}...}};
}

template<typename StrTy>
unsigned get_fmt_arg_integer_value(const fmt_arg_list_item<StrTy>& arg, const char* msg_not_integer,
                                   const char* msg_negative) {
#define FMT_ARG_INTEGER_VALUE_CASE(ty) \
    if (arg.second == fmt_arg_appender<ty, StrTy>::append) { \
        ty val = *reinterpret_cast<const ty*>(arg.first); \
        if (val < 0) { throw std::runtime_error(msg_negative); } \
        return static_cast<unsigned>(val); \
    }
#define FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    if (arg.second == fmt_arg_appender<ty, StrTy>::append) { \
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
    throw std::runtime_error(msg_not_integer);
}

}  // namespace detail

template<typename StrTy>
StrTy& format_append_v(std::string_view fmt, StrTy& s, span<const detail::fmt_arg_list_item<StrTy>> args) {
    size_t n = 0;
    auto check_arg_idx = [&args](size_t idx) {
        if (idx >= args.size()) { throw std::out_of_range("out of format argument list"); }
    };
    detail::fmt_arg_specs specs;
    const char *first = fmt.data(), *last = fmt.data() + fmt.size();
    while (first != last) {
        bool put_arg = false;
        std::tie(first, put_arg) = detail::fmt_parse(first, last, s, specs);
        if (put_arg) {
            // obtain argument number
            if (!(specs.flags & detail::fmt_parse_flags::kArgNumSpecified)) { specs.n_arg = n++; }
            check_arg_idx(specs.n_arg);

            if (!!(specs.flags & detail::fmt_parse_flags::kDynamicWidth)) {
                // obtain argument number for width
                if (!(specs.flags & detail::fmt_parse_flags::kWidthArgNumSpecified)) { specs.n_width_arg = n++; }
                check_arg_idx(specs.n_width_arg);
                specs.fmt.width = detail::get_fmt_arg_integer_value(
                    args[specs.n_width_arg], "agrument width is not integer", "agrument width is negative");
            }
            if (!!(specs.flags & detail::fmt_parse_flags::kDynamicPrec)) {
                // obtain argument number for precision
                if (!(specs.flags & detail::fmt_parse_flags::kPrecArgNumSpecified)) { specs.n_prec_arg = n++; }
                check_arg_idx(specs.n_prec_arg);
                specs.fmt.prec = detail::get_fmt_arg_integer_value(
                    args[specs.n_prec_arg], "agrument precision is not integer", "agrument precision is negative");
            }
            args[specs.n_arg].second(args[specs.n_arg].first, s, specs.fmt);
        }
    }
    return s;
}

template<typename StrTy, typename... Ts>
StrTy& format_append(std::string_view fmt, StrTy& s, const Ts&... args) {
    return format_append_v<StrTy>(fmt, s, detail::make_fmt_args<StrTy>(args...));
}

template<typename... Ts>
std::string format(std::string_view fmt, const Ts&... args) {
    std::string result;
    format_append(fmt, result, args...);
    return result;
}

template<typename... Ts>
char* format_to(char* dst, std::string_view fmt, const Ts&... args) {
    char_buf_appender appender(dst);
    return format_append(fmt, appender, args...).get_ptr();
}

template<typename... Ts>
char* format_to_n(char* dst, size_t n, std::string_view fmt, const Ts&... args) {
    char_n_buf_appender appender(dst, n);
    return format_append(fmt, appender, args...).get_ptr();
}

}  // namespace util
