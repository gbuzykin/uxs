#pragma once

#include "io/iobuf.h"
#include "stringcvt.h"

#include <array>
#include <stdexcept>

namespace util {

class format_error : public std::runtime_error {
 public:
    explicit format_error(const char* message) : std::runtime_error(message) {}
    explicit format_error(const std::string& message) : std::runtime_error(message) {}
};

namespace detail {

template<typename StrTy>
UTIL_EXPORT StrTy& fmt_append_string(StrTy& s, std::basic_string_view<typename StrTy::value_type> val, fmt_state& fmt);

template<typename StrTy, typename Ty, typename = void>
struct fmt_arg_appender;

template<typename StrTy, typename Ty>
struct fmt_arg_appender<StrTy, Ty, std::void_t<typename string_converter<Ty>::is_string_converter>> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        return basic_to_string(s, *reinterpret_cast<const Ty*>(val), fmt);
    }
};

template<typename StrTy, typename Ty>
struct fmt_arg_appender<StrTy, Ty*, std::enable_if_t<!is_character<Ty>::value>> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        fmt.flags &= ~fmt_flags::kBaseField;
        fmt.flags |= fmt_flags::kHex | fmt_flags::kAlternate;
        return basic_to_string(s, reinterpret_cast<uintptr_t>(val), fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<StrTy, typename StrTy::value_type*, void> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return fmt_append_string(s, std::basic_string_view<CharT>(static_cast<const CharT*>(val)), fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<StrTy, std::basic_string_view<typename StrTy::value_type>, void> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return fmt_append_string(s, *reinterpret_cast<const std::basic_string_view<CharT>*>(val), fmt);
    }
};

template<typename StrTy>
struct fmt_arg_appender<StrTy, std::basic_string<typename StrTy::value_type>, void> {
    static StrTy& append(StrTy& s, const void* val, fmt_state& fmt) {
        using CharT = typename StrTy::value_type;
        return fmt_append_string(s, *reinterpret_cast<const std::basic_string<CharT>*>(val), fmt);
    }
};

template<typename StrTy>
using fmt_arg_list_item = std::pair<const void*, StrTy& (*)(StrTy&, const void*, fmt_state&)>;

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
    return {{{get_fmt_arg_ptr(args), fmt_arg_appender<StrTy, typename fmt_arg_type<Ts>::type>::append}...}};
}

}  // namespace detail

template<typename StrTy>
UTIL_EXPORT StrTy& basic_format(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt,
                                span<const detail::fmt_arg_list_item<StrTy>> args);

template<typename StrTy, typename... Ts>
StrTy& format_append(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, const Ts&... args) {
    return basic_format<StrTy>(s, fmt, detail::make_fmt_args<StrTy>(args...));
}

template<typename... Ts>
std::string format(std::string_view fmt, const Ts&... args) {
    util::dynbuf_appender appender;
    format_append(appender, fmt, args...);
    return std::string(appender.data(), appender.size());
}

template<typename... Ts>
std::wstring format(std::wstring_view fmt, const Ts&... args) {
    util::wdynbuf_appender appender;
    format_append(appender, fmt, args...);
    return std::wstring(appender.data(), appender.size());
}

template<typename... Ts>
char* format_to(char* dst, std::string_view fmt, const Ts&... args) {
    unlimbuf_appender appender(dst);
    return format_append(appender, fmt, args...).curr();
}

template<typename... Ts>
wchar_t* format_to(wchar_t* dst, std::wstring_view fmt, const Ts&... args) {
    wunlimbuf_appender appender(dst);
    return format_append(appender, fmt, args...).curr();
}

template<typename... Ts>
char* format_to_n(char* dst, size_t n, std::string_view fmt, const Ts&... args) {
    limbuf_appender appender(dst, n);
    return format_append(appender, fmt, args...).curr();
}

template<typename... Ts>
wchar_t* format_to_n(wchar_t* dst, size_t n, std::wstring_view fmt, const Ts&... args) {
    wlimbuf_appender appender(dst, n);
    return format_append(appender, fmt, args...).curr();
}

template<typename... Ts>
iobuf& fprint(iobuf& buf, std::string_view fmt, const Ts&... args) {
    util::dynbuf_appender appender;
    format_append(appender, fmt, args...);
    return buf.write(as_span(appender.data(), appender.size()));
}

template<typename... Ts>
wiobuf& fprint(wiobuf& buf, std::wstring_view fmt, const Ts&... args) {
    util::wdynbuf_appender appender;
    format_append(appender, fmt, args...);
    return buf.write(as_span(appender.data(), appender.size()));
}

template<typename... Ts>
iobuf& fprintln(iobuf& buf, std::string_view fmt, const Ts&... args) {
    util::dynbuf_appender appender;
    format_append(appender, fmt, args...);
    appender.push_back('\n');
    return buf.write(as_span(appender.data(), appender.size())).flush();
}

template<typename... Ts>
wiobuf& fprintln(wiobuf& buf, std::wstring_view fmt, const Ts&... args) {
    util::wdynbuf_appender appender;
    format_append(appender, fmt, args...);
    appender.push_back('\n');
    return buf.write(as_span(appender.data(), appender.size())).flush();
}

template<typename... Ts>
iobuf& print(std::string_view fmt, const Ts&... args) {
    return fprint(stdbuf::out, fmt, args...);
}

template<typename... Ts>
iobuf& println(std::string_view fmt, const Ts&... args) {
    return fprintln(stdbuf::out, fmt, args...);
}

}  // namespace util
