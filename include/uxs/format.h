#pragma once

#include "format_ranges.h"  // NOLINT
#include "string_conv.h"

#include "io/iomembuffer.h"

namespace uxs {

template<typename CharT>
struct formatter<bool, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = est::unspecified_size;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = est::unspecified_size;
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
        if (width_arg_id_ != est::unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        sconv::fmt_boolean(ctx.out(), val, opts, ctx.locale());
    }
};

template<typename CharT>
struct formatter<char32_t, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = est::unspecified_size;

 public:
    UXS_CONSTEXPR void set_debug_format() { opts_.flags |= fmt_flags::debug_format; }
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = est::unspecified_size;
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
    void format(FmtCtx& ctx, char32_t val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != est::unspecified_size) {
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
        std::size_t width_arg_id_ = est::unspecified_size; \
\
     public: \
        template<typename ParseCtx> \
        UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) { \
            auto it = ctx.begin(); \
            if (it == ctx.end() || *it != ':') { return it; } \
            std::size_t dummy_id = est::unspecified_size; \
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
            if (width_arg_id_ != est::unspecified_size) { \
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
        std::size_t width_arg_id_ = est::unspecified_size; \
        std::size_t prec_arg_id_ = est::unspecified_size; \
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
            if (width_arg_id_ != est::unspecified_size) { \
                opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>(); \
            } \
            if (prec_arg_id_ != est::unspecified_size) { \
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
    std::size_t width_arg_id_ = est::unspecified_size;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = est::unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none;
        if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); }
        if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); }
        if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); }
        if (!!(opts_.flags & fmt_flags::localize)) { ParseCtx::report_unexpected_locale_specific_error(); }
        if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::pointer) {
            ParseCtx::report_type_error();
        }
        return type == ParseCtx::type_spec::none ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, const void* val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != est::unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        opts.flags |= fmt_flags::hex | fmt_flags::alternate;
        sconv::fmt_integer(ctx.out(), reinterpret_cast<std::uintptr_t>(val), opts, ctx.locale());
    }
};

template<typename CharT>
struct formatter<std::basic_string_view<CharT>, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = est::unspecified_size;
    std::size_t prec_arg_id_ = est::unspecified_size;

 public:
    UXS_CONSTEXPR void set_debug_format() { opts_.flags |= fmt_flags::debug_format; }
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, prec_arg_id_);
        auto type = it != ctx.end() ? ParseCtx::classify_standard_type(*it, opts_) : ParseCtx::type_spec::none;
        if (!!(opts_.flags & fmt_flags::sign_field)) { ParseCtx::report_unexpected_sign_error(); }
        if (!!(opts_.flags & fmt_flags::leading_zeroes)) { ParseCtx::report_unexpected_leading_zeroes_error(); }
        if (!!(opts_.flags & fmt_flags::alternate)) { ParseCtx::report_unexpected_alternate_error(); }
        if (!!(opts_.flags & fmt_flags::localize)) { ParseCtx::report_unexpected_locale_specific_error(); }
        if (type == ParseCtx::type_spec::debug_string) {
            set_debug_format();
        } else if (type != ParseCtx::type_spec::none && type != ParseCtx::type_spec::string) {
            ParseCtx::report_type_error();
        }
        return type == ParseCtx::type_spec::none ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::basic_string_view<CharT> val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != est::unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        if (prec_arg_id_ != est::unspecified_size) {
            opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(opts.prec)>();
        }
        sconv::fmt_string<CharT>(ctx.out(), val, opts, ctx.locale());
    }
};

// ---- vformat

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
std::basic_string<string_char_type<StrLikeTy>> vformat(const StrLikeTy& fmt,
                                                       format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, fmt, args);
    return std::basic_string<string_char_type<StrLikeTy>>(buf.data(), buf.size());
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
std::basic_string<string_char_type<StrLikeTy>> vformat(const std::locale& loc, const StrLikeTy& fmt,
                                                       format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, loc, fmt, args);
    return std::basic_string<string_char_type<StrLikeTy>>(buf.data(), buf.size());
}

// ---- vformat_to

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
string_char_type<StrLikeTy>* vformat_to(string_char_type<StrLikeTy>* p, const StrLikeTy& fmt,
                                        format_args_t<string_char_type<StrLikeTy>> args) {
    basic_membuffer<string_char_type<StrLikeTy>> buf(p);
    vformat_append(buf, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const string_char_type<StrLikeTy>&>::value &&
                                     !std::is_same<OutputIt, string_char_type<StrLikeTy>*>::value>>
OutputIt vformat_to(OutputIt out, const StrLikeTy& fmt, format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
string_char_type<StrLikeTy>* vformat_to(string_char_type<StrLikeTy>* p, const std::locale& loc, const StrLikeTy& fmt,
                                        format_args_t<string_char_type<StrLikeTy>> args) {
    basic_membuffer<string_char_type<StrLikeTy>> buf(p);
    vformat_append(buf, loc, fmt, args);
    return buf.endp();
}

template<typename OutputIt, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const string_char_type<StrLikeTy>&>::value &&
                                     !std::is_same<OutputIt, string_char_type<StrLikeTy>*>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, const StrLikeTy& fmt,
                    format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, loc, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
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

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
format_to_n_result<string_char_type<StrLikeTy>*> vformat_to_n(string_char_type<StrLikeTy>* p, std::size_t n,
                                                              const StrLikeTy& fmt,
                                                              format_args_t<string_char_type<StrLikeTy>> args) {
    basic_membuffer_with_size_tracker<string_char_type<StrLikeTy>> buf(p, n);
    vformat_append(buf, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const string_char_type<StrLikeTy>&>::value &&
                                     !std::is_same<OutputIt, string_char_type<StrLikeTy>*>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const StrLikeTy& fmt,
                                          format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
format_to_n_result<string_char_type<StrLikeTy>*> vformat_to_n(string_char_type<StrLikeTy>* p, std::size_t n,
                                                              const std::locale& loc, const StrLikeTy& fmt,
                                                              format_args_t<string_char_type<StrLikeTy>> args) {
    basic_membuffer_with_size_tracker<string_char_type<StrLikeTy>> buf(p, n);
    vformat_append(buf, loc, fmt, args);
    return {buf.endp(), buf.tracked_size()};
}

template<typename OutputIt, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::enable_if_t<est::is_output_iterator<OutputIt, const string_char_type<StrLikeTy>&>::value &&
                                     !std::is_same<OutputIt, string_char_type<StrLikeTy>*>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const std::locale& loc, const StrLikeTy& fmt,
                                          format_args_t<string_char_type<StrLikeTy>> args) {
    basic_inline_dynbuffer<string_char_type<StrLikeTy>> buf;
    vformat_append(buf, loc, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

// ---- vprint

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
basic_iobuf<string_char_type<StrLikeTy>>& vprint(basic_iobuf<string_char_type<StrLikeTy>>& out, const StrLikeTy& fmt,
                                                 format_args_t<string_char_type<StrLikeTy>> args) {
    basic_iomembuffer<string_char_type<StrLikeTy>> buf(out);
    vformat_append(buf, fmt, args);
    return out;
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
basic_iobuf<string_char_type<StrLikeTy>>& vprint(basic_iobuf<string_char_type<StrLikeTy>>& out, const std::locale& loc,
                                                 const StrLikeTy& fmt,
                                                 format_args_t<string_char_type<StrLikeTy>> args) {
    basic_iomembuffer<string_char_type<StrLikeTy>> buf(out);
    vformat_append(buf, loc, fmt, args);
    return out;
}

// --------------------------

#define UXS_DECLARE_TYPE_ALIASES(type, prefix) \
    using prefix##format_context = basic_format_context<type>; \
    using prefix##format_parse_context = basic_format_parse_context<type>; \
    using prefix##format_args = format_args_t<type>; \
    template<typename... Args> \
    using prefix##format_string = format_string_t<type, Args...>; \
    using prefix##runtime_format = basic_runtime_format<type>
UXS_DECLARE_TYPE_ALIASES(char, );
#if UXS_USE_WCHAR_T != 0
UXS_DECLARE_TYPE_ALIASES(wchar_t, w);
#endif  // UXS_USE_WCHAR_T != 0
#if UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char8_t, u8);
#endif  // UXS_USE_CHAR8_T != 0
UXS_DECLARE_TYPE_ALIASES(char16_t, u16);
UXS_DECLARE_TYPE_ALIASES(char32_t, u32);
#undef UXS_DECLARE_TYPE_ALIASES

#define UXS_FMT_IMPLEMENT_WRAPPER_FUNCTIONS(char_type) \
    template<typename... Args> \
    std::basic_string<char_type> format(format_string_t<char_type, Args...> fmt, const Args&... args) { \
        return vformat(fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename... Args> \
    std::basic_string<char_type> format(const std::locale& loc, format_string_t<char_type, Args...> fmt, \
                                        const Args&... args) { \
        return vformat(loc, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename OutputIt, typename... Args, \
             typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char_type&>::value>> \
    OutputIt format_to(OutputIt out, format_string_t<char_type, Args...> fmt, const Args&... args) { \
        return vformat_to(std::move(out), fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename OutputIt, typename... Args, \
             typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char_type&>::value>> \
    OutputIt format_to(OutputIt out, const std::locale& loc, format_string_t<char_type, Args...> fmt, \
                       const Args&... args) { \
        return vformat_to(std::move(out), loc, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename OutputIt, typename... Args, \
             typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char_type&>::value>> \
    format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, format_string_t<char_type, Args...> fmt, \
                                             const Args&... args) { \
        return vformat_to_n(std::move(out), n, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename OutputIt, typename... Args, \
             typename = std::enable_if_t<est::is_output_iterator<OutputIt, const char_type&>::value>> \
    format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, const std::locale& loc, \
                                             format_string_t<char_type, Args...> fmt, const Args&... args) { \
        return vformat_to_n(std::move(out), n, loc, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename... Args> \
    basic_iobuf<char_type>& print(basic_iobuf<char_type>& out, format_string_t<char_type, Args...> fmt, \
                                  const Args&... args) { \
        return vprint(out, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename... Args> \
    basic_iobuf<char_type>& print(basic_iobuf<char_type>& out, const std::locale& loc, \
                                  format_string_t<char_type, Args...> fmt, const Args&... args) { \
        return vprint(out, loc, fmt.get(), format_args_t<char_type>::make(args...)); \
    } \
    template<typename... Args> \
    basic_iobuf<char_type>& println(basic_iobuf<char_type>& out, format_string_t<char_type, Args...> fmt, \
                                    const Args&... args) { \
        return vprint(out, fmt.get(), format_args_t<char_type>::make(args...)).endl(); \
    } \
    template<typename... Args> \
    basic_iobuf<char_type>& println(basic_iobuf<char_type>& out, const std::locale& loc, \
                                    format_string_t<char_type, Args...> fmt, const Args&... args) { \
        return vprint(out, loc, fmt.get(), format_args_t<char_type>::make(args...)).endl(); \
    } \
    static_assert(true, "")
UXS_FMT_IMPLEMENT_WRAPPER_FUNCTIONS(char);
UXS_FMT_IMPLEMENT_WRAPPER_FUNCTIONS(wchar_t);
#undef UXS_FMT_IMPLEMENT_WRAPPER_FUNCTIONS

template<typename... Args>
iobuf& print(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), fmt.get(), format_args::make(args...));
}

template<typename... Args>
iobuf& print(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), loc, fmt.get(), format_args::make(args...));
}

template<typename... Args>
iobuf& println(format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), fmt.get(), format_args::make(args...)).endl();
}

template<typename... Args>
iobuf& println(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vprint(stdbuf::out(), loc, fmt.get(), format_args::make(args...)).endl();
}

}  // namespace uxs
