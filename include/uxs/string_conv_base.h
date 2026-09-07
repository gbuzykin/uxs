#pragma once

#include "membuffer.h"
#include "string_util.h"

#include <locale>

namespace uxs {

enum class sconv_errc { ok = 0, out_of_range, invalid, empty };

template<typename CharT>
struct from_chars_result {
#if __cplusplus < 201703L
    from_chars_result(const CharT* ptr, sconv_errc ec) noexcept : ptr(ptr), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit UXS_CONSTEXPR operator bool() const noexcept { return ec == sconv_errc::ok; }
    friend UXS_CONSTEXPR bool operator==(const from_chars_result& lhs, const from_chars_result& rhs) noexcept {
        return lhs.ptr == rhs.ptr && lhs.ec == rhs.ec;
    }
    friend UXS_CONSTEXPR bool operator!=(const from_chars_result& lhs, const from_chars_result& rhs) noexcept {
        return !(lhs == rhs);
    }
    const CharT* ptr;
    sconv_errc ec;
};

struct from_string_result {
#if __cplusplus < 201703L
    from_string_result(std::size_t count, sconv_errc ec) noexcept : count(count), ec(ec) {}
#endif  // __cplusplus < 201703L
    explicit UXS_CONSTEXPR operator bool() const noexcept { return ec == sconv_errc::ok; }
    friend UXS_CONSTEXPR bool operator==(const from_string_result& lhs, const from_string_result& rhs) noexcept {
        return lhs.count == rhs.count && lhs.ec == rhs.ec;
    }
    friend UXS_CONSTEXPR bool operator!=(const from_string_result& lhs, const from_string_result& rhs) noexcept {
        return !(lhs == rhs);
    }
    std::size_t count;
    sconv_errc ec;
};

enum class fmt_flags : unsigned {
    none = 0,
    dec = 1,
    bin = 2,
    oct = 3,
    hex = 4,
    character = 5,
    base_field = 7,
    uppercase = 8,
    fixed = 0x10,
    scientific = 0x20,
    general = 0x30,
    float_field = 0x30,
    sign_neg = 0x40,
    sign_pos = 0x80,
    sign_align = 0xc0,
    sign_field = 0xc0,
    left = 0x100,
    right = 0x200,
    internal = 0x300,
    adjust_field = 0x300,
    leading_zeroes = 0x400,
    alternate = 0x800,
    localize = 0x1000,
    debug_format = 0x2000,
    mandatory_frac = 0x4000,
    throw_on_inf_nan = 0x8000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(fmt_flags);

struct fmt_opts {
    UXS_CONSTEXPR fmt_opts() noexcept = default;
    explicit UXS_CONSTEXPR fmt_opts(fmt_flags fl, int p = -1, unsigned w = 0, int f = ' ') noexcept
        : flags(fl), prec(p), width(w), fill(f) {}
    fmt_flags flags = fmt_flags::none;
    int prec = -1;
    unsigned width = 0;
    int fill = ' ';
};

class UXS_EXPORT_ALL_STUFF_FOR_GNUC format_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit format_error(const char* message);
    UXS_EXPORT explicit format_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};

class locale_ref {
 public:
    locale_ref() noexcept = default;
    explicit locale_ref(const std::locale& loc) noexcept : ref_(&loc) {}
    explicit operator bool() const noexcept { return ref_ != nullptr; }
    std::locale operator*() const noexcept { return ref_ ? *ref_ : std::locale(); }

 private:
    const std::locale* ref_ = nullptr;
};

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct from_string_impl;

template<typename Ty, typename CharT = char, typename = void>
struct is_from_string_convertible : std::false_type {};
template<typename Ty, typename CharT>
struct is_from_string_convertible<
    Ty, CharT,
    std::enable_if_t<
        std::is_same<decltype(std::declval<const from_string_impl<Ty, CharT>&>()(nullptr, nullptr, std::declval<Ty&>())),
                     from_chars_result<CharT>>::value>> : std::true_type {};

#if __cplusplus >= 201402L
template<typename Ty, typename CharT = char>
constexpr bool is_from_string_convertible_v = is_from_string_convertible<Ty, CharT>::value;
#endif  // __cplusplus >= 201402L
#if __cplusplus >= 202002L && defined(__cpp_concepts)
template<typename Ty, typename CharT = char>
concept from_string_convertible = is_from_string_convertible_v<Ty, CharT>;
#endif  // cpp_concepts

template<typename CharT, typename Ty, typename = std::enable_if_t<is_from_string_convertible<Ty, CharT>::value>>
UXS_CONSTEXPR from_chars_result<CharT> from_chars(const CharT* first, const CharT* last, Ty& val) {
    return from_string_impl<Ty, CharT>{}(first, last, val);
}

template<typename StrLikeTy, typename Ty, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR from_string_result from_string_v(const StrLikeTy& s, Ty& val) {
    const auto sv = to_string_view(s);
    const auto result = from_chars(sv.data(), sv.data() + sv.size(), val);
    return {static_cast<std::size_t>(result.ptr - sv.data()), result.ec};
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
UXS_CONSTEXPR Ty from_string(const StrLikeTy& s, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    from_string_v(s, val);
    return val;
}

template<typename Ty, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>, typename... Args>
UXS_CONSTEXPR Ty from_string_errc(const StrLikeTy& s, sconv_errc& ec, Args&&... args) {
    Ty val(std::forward<Args>(args)...);
    ec = from_string_v(s, val).ec;
    return val;
}

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct to_string_impl;

namespace detail {
template<typename Ty, typename StrTy, typename... Args>
struct to_string_convertible_impl {
    template<typename U>
    static auto test(U* out)
        -> est::always_true<decltype(std::declval<const to_string_impl<Ty, typename StrTy::value_type>&>()(
            *out, std::declval<const Ty&>(), std::declval<const Args&>()...))>;
    template<typename U>
    static std::false_type test(...);
    using type = decltype(test<StrTy>(nullptr));
};
}  // namespace detail

template<typename Ty, typename CharT = char, typename... Args>
struct is_to_string_convertible : detail::to_string_convertible_impl<Ty, basic_membuffer<CharT>, Args...>::type {};
#if __cplusplus >= 201402L
template<typename Ty, typename CharT = char>
constexpr bool is_to_string_convertible_v = is_to_string_convertible<Ty, CharT>::value;
#endif  // __cplusplus >= 201402L
#if __cplusplus >= 202002L && defined(__cpp_concepts)
template<typename Ty, typename CharT = char>
concept to_string_convertible = is_to_string_convertible_v<Ty, CharT>;
#endif  // cpp_concepts

// ---- to_string

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type>::value>>
void to_string_append(StrTy& out, const Ty& val) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val);
}

template<typename StrTy, typename Ty,
         typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type, fmt_opts>::value>>
void to_string_append(StrTy& out, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt);
}

template<
    typename StrTy, typename Ty,
    typename = std::enable_if_t<is_to_string_convertible<Ty, typename StrTy::value_type, fmt_opts, locale_ref>::value>>
void to_string_append(StrTy& out, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    to_string_impl<Ty, typename StrTy::value_type>{}(out, val, fmt, locale_ref(loc));
}

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const Ty& val) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, val);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const Ty& val, fmt_opts fmt) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, val, fmt);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

template<typename CharT = char, typename Ty>
std::basic_string<CharT> to_string(const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_inline_dynbuffer<CharT> buf;
    to_string_append(buf, loc, val, fmt);
    return std::basic_string<CharT>(buf.data(), buf.size());
}

// ---- to_chars

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const Ty& val) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, val);
    return buf.endp();
}

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const Ty& val, fmt_opts fmt) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, val, fmt);
    return buf.endp();
}

template<typename CharT, typename Ty>
CharT* to_chars(CharT* p, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_membuffer<CharT> buf(p);
    to_string_append(buf, loc, val, fmt);
    return buf.endp();
}

// ---- to_chars_n

template<typename CharT>
struct chars_to_n_result {
#if __cplusplus < 201703L
    chars_to_n_result(CharT* out, std::size_t size) noexcept : out(out), size(size) {}
#endif  // __cplusplus < 201703L
    CharT* out;
    std::size_t size;
};

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const Ty& val) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, val);
    return {buf.endp(), buf.tracked_size()};
}

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const Ty& val, fmt_opts fmt) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, val, fmt);
    return {buf.endp(), buf.tracked_size()};
}

template<typename CharT, typename Ty>
chars_to_n_result<CharT> to_chars_n(CharT* p, std::size_t n, const std::locale& loc, const Ty& val, fmt_opts fmt) {
    basic_membuffer_with_size_tracker<CharT> buf(p, n);
    to_string_append(buf, loc, val, fmt);
    return {buf.endp(), buf.tracked_size()};
}

// --------------------------

template<typename Ty, typename CharT = char, typename = void>
struct formatter;

template<typename StrTy, typename Func>
void append_adjusted(StrTy& out, Func&& fn, unsigned len, fmt_opts fmt, bool prefer_right = false) {
    unsigned left = fmt.width - len;
    unsigned right = left;
    if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::left) {
        left = 0;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::internal) {
        left >>= 1, right -= left;
    } else if ((fmt.flags & fmt_flags::adjust_field) == fmt_flags::right || prefer_right) {
        right = 0;
    } else {
        left = 0;
    }
    out.append(left, fmt.fill);
    fn(out);
    out.append(right, fmt.fill);
}

}  // namespace uxs
