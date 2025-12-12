#pragma once

#include "format_base.h"

#include <tuple>

namespace uxs {

template<typename Tuple, typename CharT = char>
struct tuple_formattable : std::false_type {};

template<typename Ty1, typename Ty2, typename CharT>
struct tuple_formattable<std::pair<Ty1, Ty2>, CharT>
    : std::conjunction<formattable<Ty1, CharT>, formattable<Ty2, CharT>> {
    using underlying_type = std::pair<formatter_t<Ty1, CharT>, formatter_t<Ty2, CharT>>;
};

template<typename... Ts, typename CharT>
struct tuple_formattable<std::tuple<Ts...>, CharT> : std::conjunction<formattable<Ts, CharT>...> {
    using underlying_type = std::tuple<formatter_t<Ts, CharT>...>;
};

namespace detail {
template<typename Ty, typename = void>
struct is_pair_like : std::false_type {};
template<typename Ty>
struct is_pair_like<Ty, std::enable_if_t<std::tuple_size<Ty>::value == 2>> : std::true_type {};
template<typename Range, typename = void>
struct is_range_of_pairs : std::false_type {};
template<typename Range>
struct is_range_of_pairs<Range, std::enable_if_t<is_pair_like<range_element_t<Range>>::value>> : std::true_type {};
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
                                             formattable<range_element_t<Range>>::value>> : std::true_type {};
}  // namespace detail

template<typename Range, typename CharT>
struct range_formattable
    : std::integral_constant<range_format, detail::is_range_formattable<Range, CharT>::value ?
                                               (detail::is_key_type_defined<Range>::value ?
                                                    ((detail::is_range_of_pairs<Range>::value &&
                                                      detail::is_mapped_type_defined<Range>::value) ?
                                                         range_format::map :
                                                         range_format::set) :
                                                    range_format::sequence) :
                                               range_format::disabled> {};

template<typename CharT, typename Traits>
struct range_formattable<std::basic_string_view<CharT, Traits>, CharT>
    : std::integral_constant<range_format, range_format::string> {};

template<typename CharT, typename Traits, typename Alloc>
struct range_formattable<std::basic_string<CharT, Traits, Alloc>, CharT>
    : std::integral_constant<range_format, range_format::string> {};

template<typename Tuple, typename CharT>
struct formatter<Tuple, CharT, std::enable_if_t<tuple_formattable<Tuple, CharT>::value>> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;
    typename tuple_formattable<Tuple, CharT>::underlying_type underlying_;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

    template<typename Formatter, typename = std::void_t<decltype(std::declval<Formatter&>().set_debug_format())>>
    UXS_CONSTEXPR static void call_set_debug_format(Formatter& f) {
        f.set_debug_format();
    }

    template<typename Formatter, typename... Dummy>
    UXS_CONSTEXPR static void call_set_debug_format(Formatter&, Dummy&&...) {}

    UXS_CONSTEXPR void switch_to_map_style(std::true_type) noexcept {
        set_separator(string_literal<CharT, ':', ' '>{});
    }

    UXS_CONSTEXPR void switch_to_map_style(std::false_type) {
        throw format_error("`m` specifier requires a pair or a tuple with two elements");
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR void parse_element(ParseCtx& /*ctx*/, typename std::tuple_size<Tuple>::type) {}

    template<typename ParseCtx, std::size_t I>
    UXS_CONSTEXPR void parse_element(ParseCtx& ctx, std::integral_constant<std::size_t, I>) {
        if (ctx.begin() == ctx.end() || *ctx.begin() != ':') { call_set_debug_format(std::get<I>(underlying_)); }
        ctx.advance_to(std::get<I>(underlying_).parse(ctx));
        parse_element(ctx, std::integral_constant<std::size_t, I + 1>{});
    }

    template<typename FmtCtx>
    void format_element(FmtCtx& /*ctx*/, const Tuple& /*val*/, typename std::tuple_size<Tuple>::type) const {}

    template<typename FmtCtx, std::size_t I>
    void format_element(FmtCtx& ctx, const Tuple& val, std::integral_constant<std::size_t, I>) const {
        if UXS_CONSTEXPR (I != 0) { ctx.out() += separator_; }
        std::get<I>(underlying_).format(ctx, std::get<I>(val));
        format_element(ctx, val, std::integral_constant<std::size_t, I + 1>{});
    }

    template<typename FmtCtx>
    void format_impl(FmtCtx& ctx, const Tuple& val) const {
        ctx.out() += opening_bracket_;
        format_element(ctx, val, std::integral_constant<std::size_t, 0>{});
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
        if (it != ctx.end() && *it == ':') {
            std::size_t dummy_id = unspecified_size;
            it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
            if (opts_.prec >= 0 || !!(opts_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
            if (it != ctx.end() && (*it == 'n' || *it == 'm')) {
                if (*it == 'm') { switch_to_map_style(detail::is_pair_like<Tuple>{}); }
                set_brackets({}, {});
                ++it;
            }
            ctx.advance_to(it);
        }
        parse_element(ctx, std::integral_constant<std::size_t, 0>{});
        return ctx.begin();
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const Tuple& val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        if (opts.width == 0) { return format_impl(ctx, val); }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        format_impl(buf_ctx, val);
        const std::size_t len = estimate_string_width<CharT>(buf.begin(), buf.end());
        const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.data(), buf.size()); };
        return opts.width > len ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(len), opts) : fn(ctx.out());
    }
};

template<typename Ty, typename CharT = char>
struct range_formatter {
 private:
    static_assert(formattable<Ty, CharT>::value, "range_formatter<> template parameter must be formattable");

    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;
    std::size_t prec_arg_id_ = unspecified_size;
    formatter<Ty, CharT> underlying_;
    bool format_as_string_ = false;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

    template<typename Formatter, typename = std::void_t<decltype(std::declval<Formatter&>().set_debug_format())>>
    UXS_CONSTEXPR static void call_set_debug_format(Formatter& f) {
        f.set_debug_format();
    }

    template<typename Formatter, typename... Dummy>
    UXS_CONSTEXPR static void call_set_debug_format(Formatter&, Dummy&&...) {}

    UXS_CONSTEXPR void switch_to_map_style(std::true_type) noexcept {
        underlying_.set_separator(string_literal<CharT, ':', ' '>{});
        underlying_.set_brackets({}, {});
    }

    UXS_CONSTEXPR void switch_to_map_style(std::false_type) {
        throw format_error("`m` specifier requires a range of pairs or tuples with two elements");
    }

    UXS_CONSTEXPR void switch_to_string_style(std::true_type) noexcept { format_as_string_ = true; }

    UXS_CONSTEXPR void switch_to_string_style(std::false_type) {
        throw format_error("`s` specifier requires a range of native characters");
    }

    template<typename StrTy, typename Range>
    static std::size_t format_as_string(StrTy& s, const Range& val, fmt_opts opts, std::true_type) {
        if (!(opts.flags & fmt_flags::debug_format)) {
            std::size_t width = 0;
            std::uint32_t code = 0;
            auto first = std::begin(val);
            auto last = std::end(val);
            if (opts.prec >= 0 || opts.width > 0) {
                const std::size_t max_width = opts.prec >= 0 ? opts.prec : std::numeric_limits<std::size_t>::max();
                last = first;
                for (auto next = first; utf_decoder<CharT>{}.decode(last, std::end(val), next, code) != 0; last = next) {
                    const unsigned w = get_utf_code_width(code);
                    if (max_width - width < w) { break; }
                    width += w;
                }
            }
            while (first != last) { s += *first++; }
            return width;
        }
        return append_escaped_text(s, std::begin(val), std::end(val), false,
                                   opts.prec >= 0 ? opts.prec : std::numeric_limits<std::size_t>::max());
    }

    template<typename StrTy, typename Range>
    static std::size_t format_as_string(StrTy&, const Range&, fmt_opts, std::false_type) {
        return 0;
    }

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
    UXS_CONSTEXPR formatter<Ty, CharT>& underlying() { return underlying_; }
    UXS_CONSTEXPR const formatter<Ty, CharT>& underlying() const { return underlying_; }
    UXS_CONSTEXPR void set_separator(std::basic_string_view<CharT> sep) noexcept { separator_ = sep; }
    UXS_CONSTEXPR void set_brackets(std::basic_string_view<CharT> opening,
                                    std::basic_string_view<CharT> closing) noexcept {
        opening_bracket_ = opening;
        closing_bracket_ = closing;
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it == ':') {
            std::size_t dummy_id = unspecified_size;
            it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
            if (!!(opts_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
            if (it != ctx.end()) {
                switch (*it) {
                    case 'n': {
                        set_brackets({}, {});
                        ++it;
                    } break;
                    case 's': {
                        switch_to_string_style(std::is_same<Ty, CharT>{});
                        return it + 1;
                    } break;
                    case '?': {
                        if (it + 1 == ctx.end() || *(it + 1) != 's') { return it; }
                        opts_.flags |= fmt_flags::debug_format;
                        switch_to_string_style(std::is_same<Ty, CharT>{});
                        return it + 2;
                    } break;
                    default: break;
                }
            }
            if (opts_.prec >= 0) { ParseCtx::unexpected_prec_error(); }
            if (it != ctx.end() && *it == 'm') {
                switch_to_map_style(detail::is_pair_like<Ty>{});
                if (*(it - 1) != 'n') { set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{}); }
                ++it;
            }
            ctx.advance_to(it);
        }
        if (ctx.begin() == ctx.end() || *ctx.begin() != ':') { call_set_debug_format(underlying_); }
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx, typename Range>
    void format(FmtCtx& ctx, const Range& val) const {
        static_assert(std::is_same<sfmt::reduce_type_t<range_element_t<Range>, CharT>, Ty>::value,
                      "inconsistent template parameter and range types");
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        if (prec_arg_id_ != unspecified_size) {
            opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(opts.prec)>();
        }
        if (opts.width == 0) {
            return format_as_string_ ?
                       static_cast<void>(format_as_string(ctx.out(), val, opts, std::is_same<Ty, CharT>{})) :
                       format_impl(ctx, val);
        }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        std::size_t len = 0;
        if (format_as_string_) {
            len = format_as_string(buf, val, opts, std::is_same<Ty, CharT>{});
        } else {
            format_impl(buf_ctx, val);
            len = estimate_string_width<CharT>(buf.begin(), buf.end());
        }
        const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.data(), buf.size()); };
        return opts.width > len ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(len), opts) : fn(ctx.out());
    }
};

template<typename Ty, typename CharT>
using range_formatter_t = range_formatter<sfmt::reduce_type_t<Ty, CharT>, CharT>;

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<range_formattable<Range, CharT>::value == range_format::map>>
    : range_formatter_t<range_element_t<Range>, CharT> {
    UXS_CONSTEXPR formatter() noexcept {
        this->set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{});
        this->underlying().set_separator(string_literal<CharT, ':', ' '>{});
        this->underlying().set_brackets({}, {});
    }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<range_formattable<Range, CharT>::value == range_format::set>>
    : range_formatter_t<range_element_t<Range>, CharT> {
    UXS_CONSTEXPR formatter() noexcept {
        this->set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{});
    }
};

template<typename Range, typename CharT>
struct formatter<Range, CharT, std::enable_if_t<range_formattable<Range, CharT>::value == range_format::sequence>>
    : range_formatter_t<range_element_t<Range>, CharT> {};

}  // namespace uxs
