#pragma once

#include "span.h"
#include "stringcvt.h"

#include <array>

namespace util {

namespace detail {

template<typename StrTy>
UTIL_EXPORT StrTy& fmt_append_string(std::basic_string_view<typename StrTy::value_type> val, StrTy& s, fmt_state& fmt);

template<typename Ty, typename StrTy, typename = void>
struct fmt_arg_appender;

template<typename Ty, typename StrTy>
struct fmt_arg_appender<
    Ty, StrTy, std::void_t<typename basic_string_converter<typename StrTy::value_type, Ty>::is_string_converter>> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return basic_string_converter<typename StrTy::value_type, Ty>::to_string(*reinterpret_cast<const Ty*>(val), s,
                                                                                 fmt);
    }
};

template<typename CharT>
struct is_character : std::false_type {};
template<>
struct is_character<char> : std::true_type {};
template<>
struct is_character<wchar_t> : std::true_type {};

template<typename Ty, typename StrTy>
struct fmt_arg_appender<Ty*, StrTy, std::enable_if_t<!is_character<Ty>::value>> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        fmt.flags &= ~fmt_flags::kBaseField;
        fmt.flags |= fmt_flags::kHex | fmt_flags::kShowBase;
        return basic_string_converter<typename StrTy::value_type, uintptr_t>::to_string(
            reinterpret_cast<uintptr_t>(val), s, fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<typename StrTy::value_type*, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(
            std::basic_string_view<typename StrTy::value_type>(static_cast<const typename StrTy::value_type*>(val)), s,
            fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<std::basic_string_view<typename StrTy::value_type>, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(*reinterpret_cast<const std::basic_string_view<typename StrTy::value_type>*>(val), s,
                                 fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<std::basic_string<typename StrTy::value_type>, StrTy, void> {
    static StrTy& append(const void* val, StrTy& s, fmt_state& fmt) {
        return fmt_append_string(*reinterpret_cast<const std::basic_string<typename StrTy::value_type>*>(val), s, fmt);
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
UTIL_EXPORT StrTy& format_append_v(std::basic_string_view<typename StrTy::value_type> fmt, StrTy& s,
                                   span<const detail::fmt_arg_list_item<StrTy>> args);

template<typename StrTy, typename... Ts>
StrTy& format_append(std::basic_string_view<typename StrTy::value_type> fmt, StrTy& s, const Ts&... args) {
    return format_append_v<StrTy>(fmt, s, detail::make_fmt_args<StrTy>(args...));
}

template<typename... Ts>
std::string format(std::string_view fmt, const Ts&... args) {
    std::string result;
    format_append(fmt, result, args...);
    return result;
}

template<typename... Ts>
std::wstring format(std::wstring_view fmt, const Ts&... args) {
    std::wstring result;
    format_append(fmt, result, args...);
    return result;
}

template<typename... Ts>
char* format_to(char* dst, std::string_view fmt, const Ts&... args) {
    char_buf_appender appender(dst);
    return format_append(fmt, appender, args...).get_ptr();
}

template<typename... Ts>
wchar_t* format_to(wchar_t* dst, std::wstring_view fmt, const Ts&... args) {
    wchar_buf_appender appender(dst);
    return format_append(fmt, appender, args...).get_ptr();
}

template<typename... Ts>
char* format_to_n(char* dst, size_t n, std::string_view fmt, const Ts&... args) {
    char_n_buf_appender appender(dst, n);
    return format_append(fmt, appender, args...).get_ptr();
}

template<typename... Ts>
wchar_t* format_to_n(wchar_t* dst, size_t n, std::wstring_view fmt, const Ts&... args) {
    wchar_n_buf_appender appender(dst, n);
    return format_append(fmt, appender, args...).get_ptr();
}

}  // namespace util
