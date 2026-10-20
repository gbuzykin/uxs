#pragma once

#include "format_common.h"
#include "string_conv_common.h"

#include <tuple>

namespace uxs {

namespace detail {

template<typename Ty, typename CharT, typename IndexSequence, typename = void>
struct tuple_formatter_impl {};
template<typename Ty, typename CharT, std::size_t... Indices>
struct tuple_formatter_impl<
    Ty, CharT, std::index_sequence<Indices...>,
    std::enable_if_t<format_kind<Ty>::value == range_format::disabled &&
                     std::conjunction<is_formattable<typename std::tuple_element<Indices, Ty>::type, CharT>...>::value>> {
    using type = std::tuple<formatter_t<typename std::tuple_element<Indices, Ty>::type, CharT>...>;
};

template<typename Ty, typename CharT, typename = void>
struct tuple_formatter {};
template<typename Ty, typename CharT>
struct tuple_formatter<Ty, CharT, std::void_t<typename std::tuple_size<Ty>::type>>
    : tuple_formatter_impl<Ty, CharT, std::make_index_sequence<std::tuple_size<Ty>::value>> {};

template<typename Ty, typename = void>
struct is_pair_like : std::false_type {};
template<typename Ty>
struct is_pair_like<Ty, std::enable_if_t<std::tuple_size<Ty>::value == 2>> : std::true_type {};

template<typename Range, typename = void>
struct is_range_of_pairs : std::false_type {};
template<typename Range>
struct is_range_of_pairs<Range, std::enable_if_t<is_pair_like<est::range_element_t<Range>>::value>> : std::true_type {};

template<typename Range, typename = void>
struct is_key_type_defined : std::false_type {};
template<typename Range>
struct is_key_type_defined<Range, std::void_t<typename Range::key_type>> : std::true_type {};

template<typename Range, typename = void>
struct is_mapped_type_defined : std::false_type {};
template<typename Range>
struct is_mapped_type_defined<Range, std::void_t<typename Range::mapped_type>> : std::true_type {};

template<typename Range, typename CharT, typename = void>
struct is_range_formattable : std::false_type {};
template<typename Range, typename CharT>
struct is_range_formattable<Range, CharT,
                            std::enable_if_t<std::is_same<decltype(std::begin(std::declval<const Range&>())),
                                                          decltype(std::end(std::declval<const Range&>()))>::value &&
                                             is_formattable<est::range_element_t<Range>>::value>> : std::true_type {};

}  // namespace detail

template<typename Range, typename CharT>
struct format_kind : std::integral_constant<range_format, detail::is_range_formattable<Range, CharT>::value ?
                                                              (detail::is_key_type_defined<Range>::value ?
                                                                   ((detail::is_range_of_pairs<Range>::value &&
                                                                     detail::is_mapped_type_defined<Range>::value) ?
                                                                        range_format::map :
                                                                        range_format::set) :
                                                                   range_format::sequence) :
                                                              range_format::disabled> {};

template<typename StrCharT, typename CharT>
struct format_kind<std::basic_string_view<StrCharT>, CharT>
    : std::integral_constant<range_format, range_format::string> {};

template<typename Ty, typename CharT>
struct formatter<Ty, CharT, std::void_t<typename detail::tuple_formatter<Ty, CharT>::type>> {
 private:
    fmt_opts opts_;
    typename detail::tuple_formatter<Ty, CharT>::type underlying_;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

    template<typename Ty_ = Ty>
    UXS_CONSTEXPR void switch_to_map_style(std::true_type /* is pair-like */) noexcept {
        set_separator(string_literal<CharT, ':', ' '>{});
    }

    template<typename Ty_ = Ty>
    [[noreturn]] void switch_to_map_style(std::false_type /* is pair-like */) {
        throw format_error("`m` specifier requires a pair-like type");
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse_element(ParseCtx& /*ctx*/, typename ParseCtx::iterator it,
                                                            typename std::tuple_size<Ty>::type) {
        return it;
    }

    template<typename ParseCtx, std::size_t I>
    UXS_CONSTEXPR typename ParseCtx::iterator parse_element(ParseCtx& ctx, typename ParseCtx::iterator it,
                                                            std::integral_constant<std::size_t, I>) {
        if (*it != ':') { ParseCtx::set_debug_format(std::get<I>(underlying_)); }
        ctx.advance_to(it);
        it = std::get<I>(underlying_).parse(ctx);
        if (it == ctx.end()) { return it; }
        return parse_element(ctx, it, std::integral_constant<std::size_t, I + 1>());
    }

    template<typename FmtCtx>
    void format_element(FmtCtx& /*ctx*/, const Ty& /*val*/, typename std::tuple_size<Ty>::type) const {}

    template<typename FmtCtx, std::size_t I>
    void format_element(FmtCtx& ctx, const Ty& val, std::integral_constant<std::size_t, I>) const {
        put_separator(ctx.out(), std::bool_constant<I != 0>());
        std::get<I>(underlying_).format(ctx, std::get<I>(val));
        format_element(ctx, val, std::integral_constant<std::size_t, I + 1>());
    }

    template<typename StrTy>
    void put_separator(StrTy& out, std::true_type /* I != 0 */) const {
        out += separator_;
    }

    template<typename StrTy>
    void put_separator(StrTy& /*out*/, std::false_type /* I != 0 */) const {}

    template<typename FmtCtx>
    void format_impl(FmtCtx& ctx, const Ty& val) const {
        ctx.out() += opening_bracket_;
        format_element(ctx, val, std::integral_constant<std::size_t, 0>());
        ctx.out() += closing_bracket_;
    }

 public:
    UXS_CONSTEXPR formatter() noexcept
        : separator_(string_literal<CharT, ',', ' '>{}), opening_bracket_(string_literal<CharT, '('>{}),
          closing_bracket_(string_literal<CharT, ')'>{}) {}
    UXS_CONSTEXPR void set_separator(std::basic_string_view<CharT> sep) noexcept { separator_ = sep; }
    UXS_CONSTEXPR void set_brackets(std::basic_string_view<CharT> opening,
                                    std::basic_string_view<CharT> closing) noexcept {
        opening_bracket_ = opening;
        closing_bracket_ = closing;
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (*it == ':') {
            it = ParseCtx::parse_adjustment(ctx, it + 1, opts_);
            if (it == ctx.end()) { return it; }
            if (*it == 'n' || *it == 'm') {
                set_brackets({}, {});
                if (*it == 'm') { switch_to_map_style(detail::is_pair_like<Ty>()); }
                if (++it == ctx.end()) { return it; }
            }
        }
        return parse_element(ctx, it, std::integral_constant<std::size_t, 0>());
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const Ty& val) const {
        if (opts_.width == 0) { return format_impl(ctx, val); }
        basic_inline_dynbuffer<CharT> buf;
        auto buf_ctx = ctx.rebind(buf);
        format_impl(buf_ctx, val);
        const std::size_t width = eval_string_printable_width<CharT>(buf.begin(), buf.end());
        const auto fn = [&buf](typename FmtCtx::output_type& out) { out.append(buf.data(), buf.size()); };
        return opts_.width > width ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(width), opts_) :
                                     fn(ctx.out());
    }
};

template<typename Ty, typename CharT = char>
struct range_formatter {
 private:
    static_assert(is_formattable<Ty, CharT>::value, "range_formatter<> template parameter must be formattable");

    fmt_opts opts_;
    formatter_t<Ty, CharT> underlying_;
    bool format_as_string_ = false;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

    template<typename Ty_ = Ty>
    UXS_CONSTEXPR void switch_to_map_style(std::true_type /* range of pair-like elements */) noexcept {
        underlying_.set_separator(string_literal<CharT, ':', ' '>{});
        underlying_.set_brackets({}, {});
    }

    template<typename Ty_ = Ty>
    [[noreturn]] void switch_to_map_style(std::false_type /* range of pair-like elements */) {
        throw format_error("`m` specifier requires a range of pairs-like items");
    }

    template<typename Ty_ = Ty>
    UXS_CONSTEXPR void switch_to_string_style(std::true_type /* range of chars */) noexcept {
        set_format_as_string();
    }

    template<typename Ty_ = Ty>
    [[noreturn]] void switch_to_string_style(std::false_type /* range of chars */) {
        throw format_error("`s` specifier requires a range of characters");
    }

    template<typename StrTy, typename Range>
    void format_as_string(StrTy& out, const Range& val, std::true_type /* range of chars */) const {
        append_formatted_string(out, val, opts_);
    }

    template<typename StrTy, typename Range>
    void format_as_string(StrTy& /*out*/, const Range& /*val*/, std::false_type /* range of chars */) const {}

    template<typename FmtCtx, typename Range>
    void format_impl(FmtCtx& ctx, const Range& val) const {
        ctx.out() += opening_bracket_;
        for (auto it = std::begin(val); it != std::end(val); ++it) {
            if (it != std::begin(val)) { ctx.out() += separator_; }
            underlying_.format(ctx, *it);
        }
        ctx.out() += closing_bracket_;
    }

 public:
    UXS_CONSTEXPR range_formatter() noexcept
        : separator_(string_literal<CharT, ',', ' '>{}), opening_bracket_(string_literal<CharT, '['>{}),
          closing_bracket_(string_literal<CharT, ']'>{}) {}
    UXS_CONSTEXPR formatter_t<Ty, CharT>& underlying() { return underlying_; }
    UXS_CONSTEXPR const formatter_t<Ty, CharT>& underlying() const { return underlying_; }
    UXS_CONSTEXPR void set_format_as_string() noexcept { format_as_string_ = true; }
    UXS_CONSTEXPR void set_separator(std::basic_string_view<CharT> sep) noexcept { separator_ = sep; }
    UXS_CONSTEXPR void set_brackets(std::basic_string_view<CharT> opening,
                                    std::basic_string_view<CharT> closing) noexcept {
        opening_bracket_ = opening;
        closing_bracket_ = closing;
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (*it == ':') {
            it = ParseCtx::parse_standard(ctx, it + 1, opts_);
            if (!!(opts_.flags & ~fmt_flags::adjust_field)) { ParseCtx::report_syntax_error(); }
            if (it == ctx.end()) { return it; }
            switch (*it) {
                case 'n': {
                    set_brackets({}, {});
                    if (++it == ctx.end()) { return it; }
                    if (*it == 'm') {
                        switch_to_map_style(detail::is_pair_like<Ty>());
                        if (++it == ctx.end()) { return it; }
                    }
                } break;
                case 'm': {
                    set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{});
                    switch_to_map_style(detail::is_pair_like<Ty>());
                    if (++it == ctx.end()) { return it; }
                } break;
                case 's': {
                    switch_to_string_style(est::is_character<Ty>());
                    return it + 1;
                } break;
                case '?': {
                    if (it + 1 == ctx.end() || *(it + 1) != 's') { return it; }
                    opts_.flags |= fmt_flags::debug_format;
                    switch_to_string_style(est::is_character<Ty>());
                    return it + 2;
                } break;
                default: break;
            }
            if (opts_.prec >= 0) { ParseCtx::report_unexpected_prec_error(); }
        }
        if (*it != ':') { ParseCtx::set_debug_format(underlying_); }
        ctx.advance_to(it);
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx, typename Range>
    void format(FmtCtx& ctx, const Range& val) const {
        static_assert(std::is_same<est::range_element_t<Range>, Ty>::value,
                      "inconsistent template parameter and range types");
        if (format_as_string_) { return format_as_string(ctx.out(), val, est::is_character<Ty>()); }
        if (opts_.width == 0) { return format_impl(ctx, val); }
        basic_inline_dynbuffer<CharT> buf;
        auto buf_ctx = ctx.rebind(buf);
        format_impl(buf_ctx, val);
        const std::size_t width = eval_string_printable_width<CharT>(buf.begin(), buf.end());
        const auto fn = [&buf](typename FmtCtx::output_type& out) { out.append(buf.data(), buf.size()); };
        return opts_.width > width ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(width), opts_) :
                                     fn(ctx.out());
    }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT,
                 std::enable_if_t<format_kind<Range, CharT>::value == range_format::string &&
                                  !std::is_same<est::range_element_t<Range>, CharT>::value>>
    : range_formatter<est::range_element_t<Range>, CharT> {
    UXS_CONSTEXPR formatter() noexcept { this->set_format_as_string(); }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<format_kind<Range, CharT>::value == range_format::map>>
    : range_formatter<est::range_element_t<Range>, CharT> {
    UXS_CONSTEXPR formatter() noexcept {
        this->set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{});
        this->underlying().set_separator(string_literal<CharT, ':', ' '>{});
        this->underlying().set_brackets({}, {});
    }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<format_kind<Range, CharT>::value == range_format::set>>
    : range_formatter<est::range_element_t<Range>, CharT> {
    UXS_CONSTEXPR formatter() noexcept {
        this->set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{});
    }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<format_kind<Range, CharT>::value == range_format::sequence>>
    : range_formatter<est::range_element_t<Range>, CharT> {};

}  // namespace uxs
