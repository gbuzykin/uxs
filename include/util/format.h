#pragma once

#include "span.h"
#include "stringcvt.h"

#include <array>

namespace util {

namespace detail {

template<typename StrTy>
UTIL_EXPORT StrTy& fmt_append_string(std::string_view val, StrTy& s, fmt_state& fmt);

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

template<typename StrTy>
using fmt_arg_list_item = std::pair<const void*, StrTy& (*)(const void*, StrTy&, fmt_state&)>;

template<typename Ty>
const void* get_fmt_arg_ptr(const Ty& arg) {
    return reinterpret_cast<const void*>(&arg);
}

template<typename Ty>
const void* get_fmt_arg_ptr(Ty* arg) {
    return arg;
}

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

}  // namespace detail

template<typename StrTy>
UTIL_EXPORT StrTy& format_append_v(std::string_view fmt, StrTy& s, span<const detail::fmt_arg_list_item<StrTy>> args);

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
