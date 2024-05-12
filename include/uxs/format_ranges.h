#pragma once

#include "format.h"

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

enum class range_format { disabled = 0, sequence, set, map, string };

template<typename Range, typename CharT = char>
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
    fmt_opts specs_;
    std::size_t width_arg_id_ = dynamic_extent;
    typename tuple_formattable<Tuple, CharT>::underlying_type underlying_;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

    UXS_CONSTEXPR void switch_to_map_style(std::true_type) noexcept {
        set_separator(string_literal<CharT, ':', ' '>{});
    }

    UXS_CONSTEXPR void switch_to_map_style(std::false_type) {
        throw format_error("`m` specifier requires a pair or a tuple with two elements");
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR void parse(ParseCtx& ctx, typename std::tuple_size<Tuple>::type) {}

    template<typename ParseCtx, std::size_t I>
    UXS_CONSTEXPR void parse(ParseCtx& ctx, std::integral_constant<std::size_t, I>) {
        ctx.advance_to(std::get<I>(underlying_).parse(ctx));
        parse(ctx, std::integral_constant<std::size_t, I + 1>{});
    }

    template<typename FmtCtx>
    void format_element(FmtCtx& ctx, const Tuple& val, typename std::tuple_size<Tuple>::type) const {}

    template<typename FmtCtx, std::size_t I>
    void format_element(FmtCtx& ctx, const Tuple& val, std::integral_constant<std::size_t, I>) const {
        if UXS_CONSTEXPR (I != 0) { ctx.out().append(separator_.begin(), separator_.end()); }
        std::get<I>(underlying_).format(ctx, std::get<I>(val));
        format_element(ctx, val, std::integral_constant<std::size_t, I + 1>{});
    }

    template<typename FmtCtx>
    void format_impl(FmtCtx& ctx, const Tuple& val) const {
        ctx.out().append(opening_bracket_.begin(), opening_bracket_.end());
        format_element(ctx, val, std::integral_constant<std::size_t, 0>{});
        ctx.out().append(closing_bracket_.begin(), closing_bracket_.end());
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
            std::size_t dummy_id = dynamic_extent;
            it = ParseCtx::parse_standard(ctx, it + 1, specs_, width_arg_id_, dummy_id);
            if (specs_.prec >= 0 || !!(specs_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
            if (it != ctx.end() && (*it == 'n' || *it == 'm')) {
                if (*it == 'm') { switch_to_map_style(detail::is_pair_like<Tuple>{}); }
                set_brackets({}, {});
                ++it;
            }
            ctx.advance_to(it);
        }
        parse(ctx, std::integral_constant<std::size_t, 0>{});
        return ctx.begin();
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const Tuple& val) const {
        unsigned width = specs_.width;
        if (width_arg_id_ != dynamic_extent) {
            width = ctx.arg(width_arg_id_).get_unsigned(std::numeric_limits<decltype(width)>::max());
        }
        if (width == 0) { return format_impl(ctx, val); }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        format_impl(buf_ctx, val);
        const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.begin(), buf.end()); };
        return width > buf.size() ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(buf.size()), specs_) :
                                    fn(ctx.out());
    }
};

template<typename Ty, typename CharT = char>
struct range_formatter {
 private:
    static_assert(formattable<Ty, CharT>::value, "range_formatter<> template parameter must be formattable");

    fmt_opts specs_;
    std::size_t width_arg_id_ = dynamic_extent;
    formatter<Ty, CharT> underlying_;
    bool format_as_string_ = false;
    std::basic_string_view<CharT> separator_;
    std::basic_string_view<CharT> opening_bracket_;
    std::basic_string_view<CharT> closing_bracket_;

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

    template<typename FmtCtx, typename Range>
    static void format_as_string(FmtCtx& ctx, const Range& val, std::true_type) {
        for (auto it = std::begin(val); it != std::end(val); ++it) { ctx.out().push_back(*it); }
    }

    template<typename FmtCtx, typename Range>
    static void format_as_string(FmtCtx& ctx, const Range& val, std::false_type) {}

    template<typename FmtCtx, typename Range>
    void format_impl(FmtCtx& ctx, const Range& val) const {
        if (format_as_string_) { return format_as_string(ctx, val, std::is_same<Ty, CharT>{}); }
        ctx.out().append(opening_bracket_.begin(), opening_bracket_.end());
        for (auto it = std::begin(val); it != std::end(val); ++it) {
            if (it != std::begin(val)) { ctx.out().append(separator_.begin(), separator_.end()); }
            underlying_.format(ctx, *it);
        }
        ctx.out().append(closing_bracket_.begin(), closing_bracket_.end());
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
            std::size_t dummy_id = dynamic_extent;
            it = ParseCtx::parse_standard(ctx, it + 1, specs_, width_arg_id_, dummy_id);
            if (specs_.prec >= 0 || !!(specs_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
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
                    default: break;
                }
            }
            if (it != ctx.end() && *it == 'm') {
                switch_to_map_style(detail::is_pair_like<Ty>{});
                if (*(it - 1) != 'n') { set_brackets(string_literal<CharT, '{'>{}, string_literal<CharT, '}'>{}); }
                ++it;
            }
            ctx.advance_to(it);
        }
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx, typename Range>
    void format(FmtCtx& ctx, const Range& val) const {
        static_assert(std::is_same<sfmt::reduce_type_t<range_element_t<Range>, CharT>, Ty>::value,
                      "inconsistent template parameter and range types");
        unsigned width = specs_.width;
        if (width_arg_id_ != dynamic_extent) {
            width = ctx.arg(width_arg_id_).get_unsigned(std::numeric_limits<decltype(width)>::max());
        }
        if (width == 0) { return format_impl(ctx, val); }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        format_impl(buf_ctx, val);
        const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.begin(), buf.end()); };
        return width > buf.size() ? append_adjusted(ctx.out(), fn, static_cast<unsigned>(buf.size()), specs_) :
                                    fn(ctx.out());
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
