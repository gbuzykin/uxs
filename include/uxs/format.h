#pragma once

#include "format_ranges.h"  // NOLINT
#include "string_conv.h"

#include "io/iomembuffer.h"

namespace uxs {

template<typename CharT>
struct formatter<bool, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none;
        if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); }
        if (type == ParseCtx::type_spec::none || type == ParseCtx::type_spec::string) {
            if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); }
            if (!!(opts_.flags & fmt_flags::leading_zeroes)) { ParseCtx::report_unexpected_leading_zeroes_error(); }
            if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); }
        } else if (type != ParseCtx::type_spec::integer) {
            ParseCtx::report_type_error();
        }
        return type == ParseCtx::type_spec::none ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, bool val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        sconv::fmt_boolean(ctx.out(), val, opts, ctx.locale());
    }
};

template<typename CharT>
struct formatter<CharT, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;

 public:
    UXS_CONSTEXPR void set_debug_format() { opts_.flags |= fmt_flags::debug_format; }
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none;
        if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); }
        if (type == ParseCtx::type_spec::none || type == ParseCtx::type_spec::character ||
            type == ParseCtx::type_spec::debug_string) {
            if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); }
            if (!!(opts_.flags & fmt_flags::leading_zeroes)) { ParseCtx::report_unexpected_leading_zeroes_error(); }
            if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); }
            if (type == ParseCtx::type_spec::debug_string) { set_debug_format(); }
        } else if (type != ParseCtx::type_spec::integer) {
            ParseCtx::report_type_error();
        }
        return type == ParseCtx::type_spec::none ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, CharT val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        sconv::fmt_character(ctx.out(), val, opts, ctx.locale());
    }
};

#define UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(ty) \
    template<typename CharT> \
    struct formatter<ty, CharT> { \
     private: \
        fmt_opts opts_; \
        std::size_t width_arg_id_ = unspecified_size; \
\
     public: \
        template<typename ParseCtx> \
        UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) { \
            auto it = ctx.begin(); \
            if (it == ctx.end() || *it != ':') { return it; } \
            std::size_t dummy_id = unspecified_size; \
            it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id); \
            auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none; \
            if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); } \
            if (type == ParseCtx::type_spec::character) { \
                if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); } \
                if (!!(opts_.flags & fmt_flags::leading_zeroes)) { \
                    ParseCtx::report_unexpected_leading_zeroes_error(); \
                } \
                if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); } \
            } else if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::integer) { \
                ParseCtx::report_type_error(); \
            } \
            return type == ParseCtx::type_spec::none ? it : it + 1; \
        } \
        template<typename FmtCtx> \
        void format(FmtCtx& ctx, ty val) const { \
            fmt_opts opts = opts_; \
            if (width_arg_id_ != unspecified_size) { \
                opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>(); \
            } \
            sconv::fmt_integer(ctx.out(), val, opts, ctx.locale()); \
        } \
    }
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::int32_t);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::int64_t);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::uint32_t);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::uint64_t);
#undef UXS_FMT_IMPLEMENT_STANDARD_FORMATTER

#define UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(ty) \
    template<typename CharT> \
    struct formatter<ty, CharT> { \
     private: \
        fmt_opts opts_; \
        std::size_t width_arg_id_ = unspecified_size; \
        std::size_t prec_arg_id_ = unspecified_size; \
\
     public: \
        template<typename ParseCtx> \
        UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) { \
            auto it = ctx.begin(); \
            if (it == ctx.end() || *it != ':') { return it; } \
            it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, prec_arg_id_); \
            auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none; \
            if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::floating_point) { \
                ParseCtx::report_type_error(); \
            } \
            return type == ParseCtx::type_spec::none ? it : it + 1; \
        } \
        template<typename FmtCtx> \
        void format(FmtCtx& ctx, ty val) const { \
            fmt_opts opts = opts_; \
            if (width_arg_id_ != unspecified_size) { \
                opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>(); \
            } \
            if (prec_arg_id_ != unspecified_size) { \
                opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(opts.prec)>(); \
            } \
            sconv::fmt_float(ctx.out(), val, opts, ctx.locale()); \
        } \
    }
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(float);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(double);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(long double);
#undef UXS_FMT_IMPLEMENT_STANDARD_FORMATTER

template<typename CharT>
struct formatter<const void*, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none;
        if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); }
        if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); }
        if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); }
        if (!!(opts_.flags & fmt_flags::localize)) { ParseCtx::report_unexpected_localize_error(); }
        if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::pointer) {
            ParseCtx::report_type_error();
        }
        return type == ParseCtx::type_spec::none ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, const void* val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        opts.flags |= fmt_flags::hex | fmt_flags::alternate;
        sconv::fmt_integer(ctx.out(), reinterpret_cast<std::uintptr_t>(val), opts, ctx.locale());
    }
};

#define UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(ty) \
    template<typename CharT> \
    struct formatter<ty, CharT> { \
     private: \
        fmt_opts opts_; \
        std::size_t width_arg_id_ = unspecified_size; \
        std::size_t prec_arg_id_ = unspecified_size; \
\
     public: \
        UXS_CONSTEXPR void set_debug_format() { opts_.flags |= fmt_flags::debug_format; } \
        template<typename ParseCtx> \
        UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) { \
            auto it = ctx.begin(); \
            if (it == ctx.end() || *it != ':') { return it; } \
            it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, prec_arg_id_); \
            auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none; \
            if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); } \
            if (!!(opts_.flags & fmt_flags::leading_zeroes)) { ParseCtx::report_unexpected_leading_zeroes_error(); } \
            if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); } \
            if (!!(opts_.flags & fmt_flags::localize)) { ParseCtx::report_unexpected_localize_error(); } \
            if (type == ParseCtx::type_spec::debug_string) { \
                set_debug_format(); \
            } else if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::string) { \
                ParseCtx::report_type_error(); \
            } \
            return type == ParseCtx::type_spec::none ? it : it + 1; \
        } \
        template<typename FmtCtx> \
        void format(FmtCtx& ctx, ty val) const { \
            fmt_opts opts = opts_; \
            if (width_arg_id_ != unspecified_size) { \
                opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>(); \
            } \
            if (prec_arg_id_ != unspecified_size) { \
                opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(opts.prec)>(); \
            } \
            sconv::fmt_string<CharT>(ctx.out(), val, opts, ctx.locale()); \
        } \
    }
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(const CharT*);
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::basic_string_view<CharT>);
#undef UXS_FMT_IMPLEMENT_STANDARD_FORMATTER

// ---- vformat_append

namespace detail {
template<typename CharT>
void vformat_append_impl(basic_membuffer<CharT>& out, locale_ref loc, std::basic_string_view<CharT> fmt,
                         basic_format_args<basic_format_context<CharT>> args) {
    fmt::format_impl(basic_format_context<CharT>(out, loc, args),
                     basic_format_parse_context<CharT>(fmt.begin(), fmt.end()));
}
template<typename StrTy,
         typename = std::enable_if_t<!std::is_convertible<StrTy&, basic_membuffer<typename StrTy::value_type>&>::value>>
void vformat_append_impl(StrTy& out, locale_ref loc, std::basic_string_view<typename StrTy::value_type> fmt,
                         basic_format_args<basic_format_context<typename StrTy::value_type>> args) {
    using char_type = typename StrTy::value_type;
    basic_inline_dynbuffer<char_type> buf;
    fmt::format_impl(basic_format_context<char_type>(buf, loc, args),
                     basic_format_parse_context<char_type>(fmt.begin(), fmt.end()));
    out.append(buf.data(), buf.size());
}
}  // namespace detail

template<typename StrTy>
void vformat_append(StrTy& out, std::basic_string_view<typename StrTy::value_type> fmt,
                    basic_format_args<basic_format_context<typename StrTy::value_type>> args) {
    detail::vformat_append_impl(out, locale_ref(), fmt, args);
}

template<typename StrTy>
void vformat_append(StrTy& out, const std::locale& loc, std::basic_string_view<typename StrTy::value_type> fmt,
                    basic_format_args<basic_format_context<typename StrTy::value_type>> args) {
    detail::vformat_append_impl(out, locale_ref(loc), fmt, args);
}

// ---- format_append

template<typename StrTy, typename... Args>
void format_append(StrTy& out, basic_format_string<typename StrTy::value_type, est::type_identity_t<Args>...> fmt,
                   const Args&... args) {
    vformat_append(out, fmt.get(), make_format_args<basic_format_context<typename StrTy::value_type>>(args...));
}

template<typename StrTy, typename... Args>
void format_append(StrTy& out, const std::locale& loc,
                   basic_format_string<typename StrTy::value_type, est::type_identity_t<Args>...> fmt,
                   const Args&... args) {
    vformat_append(out, loc, fmt.get(), make_format_args<basic_format_context<typename StrTy::value_type>>(args...));
}

// ---- vformat

inline std::string vformat(std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, fmt, args);
    return std::string(buf.data(), buf.size());
}

inline std::wstring vformat(std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

inline std::string vformat(const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return std::string(buf.data(), buf.size());
}

inline std::wstring vformat(const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

// ---- format

template<typename... Args>
std::string format(format_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_format_args(args...));
}

template<typename... Args>
std::wstring format(wformat_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
std::string format(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_format_args(args...));
}

template<typename... Args>
std::wstring format(const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to

inline char* vformat_to(char* p, std::string_view fmt, format_args args) {
    membuffer buf(p);
    vformat_append(buf, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline wchar_t* vformat_to(wchar_t* p, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    vformat_append(buf, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline char* vformat_to(char* p, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p);
    vformat_append(buf, loc, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline wchar_t* vformat_to(wchar_t* p, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    vformat_append(buf, loc, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

// ---- format_to

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to_n

template<typename OutputIt>
struct format_to_n_result {
#if __cplusplus < 201703L
    format_to_n_result(OutputIt out, std::size_t size) : out(out), size(size) {}
#endif  // __cplusplus < 201703L
    OutputIt out;
    std::size_t size;
};

inline format_to_n_result<char*> vformat_to_n(char* p, std::size_t n, std::string_view fmt, format_args args) {
    membuffer_with_size_tracker buf(p, n);
    vformat_append(buf, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<wchar_t*> vformat_to_n(wchar_t* p, std::size_t n, std::wstring_view fmt, wformat_args args) {
    wmembuffer_with_size_tracker buf(p, n);
    vformat_append(buf, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<char*> vformat_to_n(char* p, std::size_t n, const std::locale& loc, std::string_view fmt,
                                              format_args args) {
    membuffer_with_size_tracker buf(p, n);
    vformat_append(buf, loc, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const std::locale& loc, std::string_view fmt,
                                          format_args args) {
    inline_dynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<wchar_t*> vformat_to_n(wchar_t* p, std::size_t n, const std::locale& loc,
                                                 std::wstring_view fmt, wformat_args args) {
    wmembuffer_with_size_tracker buf(p, n);
    vformat_append(buf, loc, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const std::locale& loc, std::wstring_view fmt,
                                          wformat_args args) {
    inline_wdynbuffer buf;
    vformat_append(buf, loc, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

// ---- format_to_n

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, const std::locale& loc,
                                         format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, const std::locale& loc,
                                         wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_wformat_args(args...));
}

// ---- vprint

inline iobuf& vprint(iobuf& out, std::string_view fmt, format_args args) {
    iomembuffer buf(out);
    vformat_append(buf, fmt, args);
    return out;
}

inline wiobuf& vprint(wiobuf& out, std::wstring_view fmt, wformat_args args) {
    wiomembuffer buf(out);
    vformat_append(buf, fmt, args);
    return out;
}

inline iobuf& vprint(iobuf& out, const std::locale& loc, std::string_view fmt, format_args args) {
    iomembuffer buf(out);
    vformat_append(buf, loc, fmt, args);
    return out;
}

inline wiobuf& vprint(wiobuf& out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wiomembuffer buf(out);
    vformat_append(buf, loc, fmt, args);
    return out;
}

// ---- print

template<typename... Args>
iobuf& print(iobuf& out, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_format_args(args...));
}

template<typename... Args>
wiobuf& print(wiobuf& out, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
iobuf& print(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), fmt.get(), make_format_args(args...));
}

template<typename... Args>
iobuf& print(iobuf& out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_format_args(args...));
}

template<typename... Args>
wiobuf& print(wiobuf& out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
iobuf& print(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), loc, fmt.get(), make_format_args(args...));
}

// ---- println

template<typename... Args>
iobuf& println(iobuf& out, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
wiobuf& println(wiobuf& out, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, fmt.get(), make_wformat_args(args...)).endl();
}

template<typename... Args>
iobuf& println(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
iobuf& println(iobuf& out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_format_args(args...)).endl();
}

template<typename... Args>
wiobuf& println(wiobuf& out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vprint(out, loc, fmt.get(), make_wformat_args(args...)).endl();
}

template<typename... Args>
iobuf& println(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), loc, fmt.get(), make_format_args(args...)).endl();
}

}  // namespace uxs
