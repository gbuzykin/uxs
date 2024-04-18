#pragma once

#include "alignment.h"
#include "io/iobuf.h"
#include "stringcvt.h"

namespace uxs {

template<typename StrTy>
class basic_format_context;

using format_context = basic_format_context<membuffer>;
using wformat_context = basic_format_context<wmembuffer>;

namespace detail {
template<typename Ty, typename FmtCtx>
struct has_formatter {
    template<typename U>
    static auto test(U& ctx, const Ty& v)
        -> always_true<decltype(formatter<Ty, typename U::char_type>().format(ctx, v, std::declval<fmt_opts&>()))>;
    template<typename U>
    static auto test(...) -> std::false_type;
    using type = decltype(test<FmtCtx>(std::declval<FmtCtx&>(), std::declval<const Ty&>()));
};
}  // namespace detail

template<typename Ty, typename FmtCtx = format_context>
struct formattable : detail::has_formatter<Ty, FmtCtx>::type {};

#define UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(ty, func) \
    template<typename CharT> \
    struct formatter<ty, CharT> { \
        template<typename FmtCtx> \
        void format(FmtCtx& ctx, ty val, fmt_opts& fmt) const { \
            func(ctx.out(), val, fmt, ctx.locale()); \
        } \
    };
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(bool, scvt::fmt_boolean)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(CharT, scvt::fmt_character)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(const CharT*, scvt::fmt_string<CharT>)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::basic_string_view<CharT>, scvt::fmt_string)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::int32_t, scvt::fmt_integer)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::int64_t, scvt::fmt_integer)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::uint32_t, scvt::fmt_integer)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(std::uint64_t, scvt::fmt_integer)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(float, scvt::fmt_float)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(double, scvt::fmt_float)
UXS_FMT_IMPLEMENT_STANDARD_FORMATTER(long double, scvt::fmt_float)
#undef UXS_FMT_IMPLEMENT_STANDARD_FORMATTER

template<typename CharT>
struct formatter<const void*, CharT> {
    template<typename FmtCtx>
    void format(FmtCtx& ctx, const void* val, fmt_opts& fmt) const {
        fmt.flags |= fmt_flags::hex | fmt_flags::alternate;
        scvt::fmt_integer(ctx.out(), reinterpret_cast<std::uintptr_t>(val), fmt, ctx.locale());
    }
};

namespace sfmt {

enum class type_index : std::uint8_t {
    boolean = 0,
    character,
    integer,
    long_integer,
    unsigned_integer,
    unsigned_long_integer,
    single_precision,
    double_precision,
    long_double_precision,
    pointer,
    z_string,
    string,
    custom
};

// --------------------------

namespace detail {
template<typename Ty, typename CharT, typename = void>
struct reduce_type {
    using type = std::remove_cv_t<Ty>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT,
                   std::enable_if_t<std::is_integral<Ty>::value && std::is_unsigned<Ty>::value &&
                                    !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT,
                   std::enable_if_t<std::is_integral<Ty>::value && std::is_signed<Ty>::value &&
                                    !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::int32_t)), std::int32_t, std::int64_t>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT, std::enable_if_t<is_character<Ty>::value>> {
    using type = CharT;
};
template<typename CharT, typename Traits>
struct reduce_type<std::basic_string_view<CharT, Traits>, CharT> {
    using type = std::basic_string_view<CharT>;
};
template<typename CharT, typename Traits, typename Alloc>
struct reduce_type<std::basic_string<CharT, Traits, Alloc>, CharT> {
    using type = std::basic_string_view<CharT>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT, std::enable_if_t<std::is_array<Ty>::value>> {
    using type =
        std::conditional_t<is_character<typename std::remove_extent<Ty>::type>::value, const CharT*, const void*>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty*, CharT> {
    using type = std::conditional_t<is_character<Ty>::value, const CharT*, const void*>;
};
template<typename CharT>
struct reduce_type<std::nullptr_t, CharT> {
    using type = const void*;
};
}  // namespace detail

template<typename Ty, typename CharT>
using reduce_type_t = typename detail::reduce_type<Ty, CharT>::type;

// --------------------------

namespace detail {
template<typename Ty, typename CharT>
struct arg_type_index;
#define UXS_FMT_DECLARE_ARG_TYPE_INDEX(ty, index) \
    template<typename CharT> \
    struct arg_type_index<ty, CharT> : std::integral_constant<type_index, index> {};
UXS_FMT_DECLARE_ARG_TYPE_INDEX(bool, type_index::boolean)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(CharT, type_index::character)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int32_t, type_index::integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int64_t, type_index::long_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint32_t, type_index::unsigned_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint64_t, type_index::unsigned_long_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(float, type_index::single_precision)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(double, type_index::double_precision)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(long double, type_index::long_double_precision)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(const void*, type_index::pointer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(const CharT*, type_index::z_string)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::basic_string_view<CharT>, type_index::string)
#undef UXS_FMT_DECLARE_ARG_TYPE_INDEX
}  // namespace detail

// --------------------------

template<typename FmtCtx>
class custom_arg_handle {
 public:
    using format_func_type = void (*)(FmtCtx&, const void*, fmt_opts&);

    template<typename Ty>
    CONSTEXPR custom_arg_handle(const Ty& v) noexcept : val_(&v), print_fn_(func<Ty>) {}

    void format(FmtCtx& ctx, fmt_opts& fmt) const { print_fn_(ctx, val_, fmt); }

 private:
    const void* val_;            // value pointer
    format_func_type print_fn_;  // printing function pointer

    template<typename Ty>
    static void func(FmtCtx& ctx, const void* val, fmt_opts& fmt) {
        ctx.format_arg(*static_cast<const Ty*>(val), fmt);
    }
};

// --------------------------

template<typename Ty, typename CharT, typename = void>
struct arg_type_index : std::integral_constant<type_index, type_index::custom> {};
template<typename Ty, typename CharT>
struct arg_type_index<Ty, CharT, std::void_t<typename detail::arg_type_index<reduce_type_t<Ty, CharT>, CharT>::type>>
    : std::integral_constant<type_index, detail::arg_type_index<reduce_type_t<Ty, CharT>, CharT>::value> {};

template<typename FmtCtx, typename Ty>
using arg_store_type = std::conditional<(arg_type_index<Ty, typename FmtCtx::char_type>::value) < type_index::custom,
                                        reduce_type_t<Ty, typename FmtCtx::char_type>, custom_arg_handle<FmtCtx>>;

template<typename FmtCtx, typename Ty>
using arg_size = uxs::size_of<typename arg_store_type<FmtCtx, Ty>::type>;

template<typename FmtCtx, typename Ty>
using arg_alignment = std::alignment_of<typename arg_store_type<FmtCtx, Ty>::type>;

template<typename FmtCtx, std::size_t, typename...>
struct arg_store_size_evaluator;
template<typename FmtCtx, std::size_t Size>
struct arg_store_size_evaluator<FmtCtx, Size> : std::integral_constant<std::size_t, Size> {};
template<typename FmtCtx, std::size_t Size, typename Ty, typename... Rest>
struct arg_store_size_evaluator<FmtCtx, Size, Ty, Rest...>
    : std::integral_constant<
          std::size_t,
          arg_store_size_evaluator<FmtCtx,
                                   uxs::align_up<arg_alignment<FmtCtx, Ty>::value>::template type<Size>::value +
                                       arg_size<FmtCtx, Ty>::value,
                                   Rest...>::value> {};

template<typename FmtCtx, typename...>
struct arg_store_alignment_evaluator;
template<typename FmtCtx, typename Ty>
struct arg_store_alignment_evaluator<FmtCtx, Ty> : arg_alignment<FmtCtx, Ty> {};
template<typename FmtCtx, typename Ty1, typename Ty2, typename... Rest>
struct arg_store_alignment_evaluator<FmtCtx, Ty1, Ty2, Rest...>
    : std::conditional<(arg_alignment<FmtCtx, Ty1>::value > arg_alignment<FmtCtx, Ty2>::value),
                       arg_store_alignment_evaluator<FmtCtx, Ty1, Rest...>,
                       arg_store_alignment_evaluator<FmtCtx, Ty2, Rest...>>::type {};

template<typename FmtCtx, typename... Args>
class arg_store {
 public:
    using char_type = typename FmtCtx::char_type;
    static const std::size_t arg_count = sizeof...(Args);

    arg_store& operator=(const arg_store&) = delete;

    CONSTEXPR explicit arg_store(const Args&... args) noexcept {
        store_values(0, arg_count * sizeof(unsigned), args...);
    }
    CONSTEXPR const void* data() const noexcept { return data_; }

 private:
    static const std::size_t storage_size =
        arg_store_size_evaluator<FmtCtx, arg_count * sizeof(unsigned), Args...>::value;
    static const std::size_t storage_alignment = arg_store_alignment_evaluator<FmtCtx, unsigned, Args...>::value;
    alignas(storage_alignment) std::uint8_t data_[storage_size];

    template<typename Ty>
    CONSTEXPR static void store_value(
        const Ty& v,
        std::enable_if_t<(arg_type_index<Ty, char_type>::value < type_index::pointer), void*> data) noexcept {
        static_assert(!is_character<Ty>::value || sizeof(Ty) <= sizeof(char_type),
                      "inconsistent character argument type");
        ::new (data) reduce_type_t<Ty, char_type>(v);
    }

    template<typename Ty>
    CONSTEXPR static void store_value(
        const Ty& v,
        std::enable_if_t<(arg_type_index<Ty, char_type>::value == type_index::custom), void*> data) noexcept {
        using ReducedTy = reduce_type_t<Ty, char_type>;
        static_assert(formattable<ReducedTy, FmtCtx>::value, "value of this type cannot be formatted");
        ::new (data) custom_arg_handle<FmtCtx>(static_cast<const ReducedTy&>(v));
    }

    template<typename Ty>
    CONSTEXPR static void store_value(Ty* v, void* data) noexcept {
        static_assert(!is_character<Ty>::value || std::is_same<std::remove_cv_t<Ty>, char_type>::value,
                      "inconsistent string argument type");
        ::new (data) const void*(v);
    }

    CONSTEXPR static void store_value(std::nullptr_t, void* data) noexcept { ::new (data) const void*(nullptr); }

    template<typename CharT, typename Traits>
    CONSTEXPR static void store_value(const std::basic_string_view<CharT, Traits>& s, void* data) noexcept {
        using Ty = std::basic_string_view<char_type>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    template<typename CharT, typename Traits, typename Alloc>
    CONSTEXPR static void store_value(const std::basic_string<CharT, Traits, Alloc>& s, void* data) noexcept {
        using Ty = std::basic_string_view<char_type>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    CONSTEXPR void store_values(std::size_t i, std::size_t offset) noexcept {}

    template<typename Ty, typename... Ts>
    CONSTEXPR void store_values(std::size_t i, std::size_t offset, const Ty& v, const Ts&... other) noexcept {
        offset = uxs::align_up<arg_alignment<FmtCtx, Ty>::value>::value(offset);
        ::new (reinterpret_cast<unsigned*>(&data_) + i) unsigned(
            static_cast<unsigned>(offset << 8) | static_cast<unsigned>(arg_type_index<Ty, char_type>::value));
        store_value(v, &data_[offset]);
        store_values(i + 1, offset + arg_size<FmtCtx, Ty>::value, other...);
    }
};

template<typename FmtCtx>
class arg_store<FmtCtx> {
 public:
    using char_type = typename FmtCtx::char_type;
    static const std::size_t arg_count = 0;
    arg_store& operator=(const arg_store&) = delete;
    CONSTEXPR arg_store() noexcept = default;
    CONSTEXPR const void* data() const noexcept { return nullptr; }
};

// --------------------------

enum class parse_flags : unsigned {
    none = 0,
    prec_specified = 0x100,
    dynamic_width = 0x200,
    dynamic_prec = 0x400,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(parse_flags, unsigned);

struct arg_specs {
    fmt_opts fmt;
    parse_flags flags = parse_flags::none;
    unsigned n_arg = 0;
    unsigned n_width_arg = 0;
    unsigned n_prec_arg = 0;
};

template<typename CharT, typename Ty>
CONSTEXPR const CharT* parse_num(const CharT* p, const CharT* last, Ty& num) noexcept {
    for (unsigned dig = 0; p != last && (dig = dig_v(*p)) < 10; ++p) { num = 10 * num + dig; }
    return p;
}

template<typename CharT>
CONSTEXPR const CharT* parse_arg_spec(const CharT* p, const CharT* last, uxs::span<const unsigned> args_metadata,
                                      arg_specs& specs, unsigned& n_arg_auto) {
    auto syntax_error = []() { throw format_error("invalid specifier syntax"); };
    auto out_of_arg_list_error = []() { throw format_error("out of argument list"); };
    auto arg_is_not_an_integer_error = []() { throw format_error("argument is not an integer"); };

    // obtain argument number
    unsigned dig = 0;
    if ((dig = dig_v(*p)) < 10) {
        specs.n_arg = dig;
        p = parse_num(++p, last, specs.n_arg);
    } else {
        specs.n_arg = n_arg_auto++;
    }
    if (specs.n_arg >= args_metadata.size()) { out_of_arg_list_error(); }

    if (p == last || *p != ':' || ++p == last) { return p; }

    enum class state_t { adjustment = 0, sign, alternate, leading_zeroes, width, precision, locale, type, finish };

    state_t state = state_t::adjustment;

    if (p + 1 != last) {
        switch (*(p + 1)) {  // adjustment with fill character
            case '<': {
                specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::left;
                p += 2, state = state_t::sign;
            } break;
            case '^': {
                specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::internal;
                p += 2, state = state_t::sign;
            } break;
            case '>': {
                specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::right;
                p += 2, state = state_t::sign;
            } break;
            default: break;
        }
    }

    for (; p != last; ++p) {
        switch (*p) {
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < next_state) { \
        state = next_state; \
        action; \
        break; \
    } \
    return p;

            // adjustment
            case '<': UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::left; });
            case '^': UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::internal; });
            case '>': UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::right; });

            // sign specifiers
            case '-': UXS_FMT_SPECIFIER_CASE(state_t::alternate, { specs.fmt.flags |= fmt_flags::sign_neg; });
            case '+': UXS_FMT_SPECIFIER_CASE(state_t::alternate, { specs.fmt.flags |= fmt_flags::sign_pos; });
            case ' ': UXS_FMT_SPECIFIER_CASE(state_t::alternate, { specs.fmt.flags |= fmt_flags::sign_align; });

            // alternate
            case '#': UXS_FMT_SPECIFIER_CASE(state_t::leading_zeroes, { specs.fmt.flags |= fmt_flags::alternate; });

            // leading zeroes
            case '0': UXS_FMT_SPECIFIER_CASE(state_t::width, { specs.fmt.flags |= fmt_flags::leading_zeroes; });

            // locale
            case 'L': UXS_FMT_SPECIFIER_CASE(state_t::type, { specs.fmt.flags |= fmt_flags::localize; });

            // width
            case '{':
                UXS_FMT_SPECIFIER_CASE(state_t::precision, {
                    if (++p == last) { syntax_error(); }
                    specs.flags |= parse_flags::dynamic_width;
                    // obtain argument number for width
                    if (*p == '}') {
                        specs.n_width_arg = n_arg_auto++;
                    } else if ((dig = dig_v(*p)) < 10) {
                        specs.n_width_arg = dig;
                        p = parse_num(++p, last, specs.n_width_arg);
                        if (p == last || *p != '}') { syntax_error(); }
                    } else {
                        syntax_error();
                    }
                    if (specs.n_width_arg >= args_metadata.size()) { out_of_arg_list_error(); }
                    const type_index index = static_cast<type_index>(args_metadata[specs.n_width_arg] & 0xff);
                    if (index < type_index::integer || index > type_index::unsigned_long_integer) {
                        arg_is_not_an_integer_error();
                    }
                    break;
                });
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                UXS_FMT_SPECIFIER_CASE(state_t::precision, {
                    specs.fmt.width = static_cast<unsigned>(*p - '0');
                    p = parse_num(++p, last, specs.fmt.width) - 1;
                });

            // precision
            case '.':
                UXS_FMT_SPECIFIER_CASE(state_t::locale, {
                    if (++p == last) { syntax_error(); }
                    specs.flags |= parse_flags::prec_specified;
                    if ((dig = dig_v(*p)) < 10) {
                        specs.fmt.prec = dig;
                        p = parse_num(++p, last, specs.fmt.prec) - 1;
                    } else if (*p == '{' && ++p != last) {
                        specs.flags |= parse_flags::dynamic_prec;
                        // obtain argument number for precision
                        if (*p == '}') {
                            specs.n_prec_arg = n_arg_auto++;
                        } else if ((dig = dig_v(*p)) < 10) {
                            specs.n_prec_arg = dig;
                            p = parse_num(++p, last, specs.n_prec_arg);
                            if (p == last || *p != '}') { syntax_error(); }
                        } else {
                            syntax_error();
                        }
                        if (specs.n_prec_arg >= args_metadata.size()) { out_of_arg_list_error(); }
                        const type_index index = static_cast<type_index>(args_metadata[specs.n_prec_arg] & 0xff);
                        if (index < type_index::integer || index > type_index::unsigned_long_integer) {
                            arg_is_not_an_integer_error();
                        }
                    } else {
                        syntax_error();
                    }
                    break;
                });

            // types
            case 'd': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::dec; });

            case 'B': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'b': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::bin; });

            case 'o': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::oct; });

            case 'X': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'x': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::hex; });

            case 'F': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'f': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::fixed; });

            case 'E': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'e': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::scientific; });

            case 'G': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'g': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::general; });

            case 'A': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'a': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::scientific_hex; });

            case 'P': specs.fmt.flags |= fmt_flags::uppercase; /*fallthrough*/
            case 'p': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::pointer; });

            case 'c': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::character; });

            case 's': UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.fmt.flags |= fmt_flags::string; });
#undef UXS_FMT_SPECIFIER_CASE

            default: return p;
        }
    }

    return p;
}

template<typename CharT, typename AppendTextFn, typename AppendArgFn>
CONSTEXPR void parse_format(std::basic_string_view<CharT> fmt, uxs::span<const unsigned> args_metadata,
                            const AppendTextFn& append_text_fn, const AppendArgFn& append_arg_fn) {
    auto syntax_error = []() { throw format_error("invalid specifier syntax"); };
    auto unexpected_prec_error = []() { throw format_error("unexpected precision specifier"); };
    auto unexpected_sign_error = []() { throw format_error("unexpected sign specifier"); };
    auto unexpected_leading_zeroes_error = []() { throw format_error("unexpected leading zeroes specifier"); };
    auto type_error = []() { throw format_error("invalid argument type specifier"); };

    unsigned n_arg_auto = 0;
    const CharT *first0 = fmt.data(), *last = first0 + fmt.size();
    for (const CharT* first = first0; first != last; ++first) {
        if (*first == '{' || *first == '}') {
            append_text_fn(first0, first);
            first0 = ++first;
            if (first == last) { syntax_error(); }
            if (*(first - 1) == '{' && *first != '{') {
                arg_specs specs;
                first = parse_arg_spec(first, last, args_metadata, specs, n_arg_auto);
                if (first == last || *first != '}') { syntax_error(); }

                const type_index index = static_cast<type_index>(args_metadata[specs.n_arg] & 0xff);
                switch (specs.fmt.flags & (fmt_flags::base_field | fmt_flags::float_field | fmt_flags::type_field)) {
                    case fmt_flags::none: {
                        if (!!(specs.flags & parse_flags::prec_specified) &&
                            (index < type_index::single_precision || index > type_index::long_double_precision) &&
                            index != type_index::z_string && index != type_index::string) {
                            unexpected_prec_error();
                        }
                        if (!!(specs.fmt.flags & fmt_flags::sign_field) &&
                            (index < type_index::integer || index > type_index::long_double_precision)) {
                            unexpected_sign_error();
                        }
                        if (!!(specs.fmt.flags & fmt_flags::leading_zeroes) &&
                            (index < type_index::integer || index > type_index::pointer)) {
                            unexpected_leading_zeroes_error();
                        }
                    } break;

                    case fmt_flags::dec:
                    case fmt_flags::bin:
                    case fmt_flags::oct:
                    case fmt_flags::hex: {
                        if (!!(specs.flags & parse_flags::prec_specified)) { unexpected_prec_error(); }
                        if (index > type_index::unsigned_long_integer) { type_error(); }
                    } break;

                    case fmt_flags::fixed:
                    case fmt_flags::scientific:
                    case fmt_flags::general:
                    case fmt_flags::scientific_hex: {
                        if (index < type_index::single_precision || index > type_index::long_double_precision) {
                            type_error();
                        }
                    } break;

                    case fmt_flags::character: {
                        if (!!(specs.flags & parse_flags::prec_specified)) { unexpected_prec_error(); }
                        if (!!(specs.fmt.flags & fmt_flags::sign_field)) { unexpected_sign_error(); }
                        if (!!(specs.fmt.flags & fmt_flags::leading_zeroes)) { unexpected_leading_zeroes_error(); }
                        if (index < type_index::character || index > type_index::unsigned_long_integer) {
                            type_error();
                        }
                    } break;

                    case fmt_flags::pointer: {
                        if (!!(specs.flags & parse_flags::prec_specified)) { unexpected_prec_error(); }
                        if (!!(specs.fmt.flags & fmt_flags::sign_field)) { unexpected_sign_error(); }
                        if (index != type_index::pointer) { type_error(); }
                    } break;

                    case fmt_flags::string: {
                        if (!!(specs.flags & parse_flags::prec_specified) && index == type_index::boolean) {
                            unexpected_prec_error();
                        }
                        if (!!(specs.fmt.flags & fmt_flags::sign_field)) { unexpected_sign_error(); }
                        if (!!(specs.fmt.flags & fmt_flags::leading_zeroes)) { unexpected_leading_zeroes_error(); }
                        if (index != type_index::boolean && index != type_index::z_string &&
                            index != type_index::string) {
                            type_error();
                        }
                    } break;

                    default: break;
                }

                append_arg_fn(specs);
                first0 = first + 1;
            } else if (*(first - 1) != *first) {
                syntax_error();
            }
        }
    }
    append_text_fn(first0, last);
}

template<typename FmtCtx>
UXS_EXPORT void vformat(FmtCtx ctx, std::basic_string_view<typename FmtCtx::char_type> fmt);

}  // namespace sfmt

template<typename FmtCtx, typename Ty>
struct format_arg_type_index : sfmt::detail::arg_type_index<Ty, typename FmtCtx::char_type> {};
template<typename FmtCtx>
struct format_arg_type_index<FmtCtx, sfmt::custom_arg_handle<FmtCtx>>
    : std::integral_constant<sfmt::type_index, sfmt::type_index::custom> {};

template<typename FmtCtx>
class basic_format_arg {
 public:
    using char_type = typename FmtCtx::char_type;
    using handle = sfmt::custom_arg_handle<FmtCtx>;

    basic_format_arg(sfmt::type_index index, const void* data) noexcept : index_(index), data_(data) {}

    NODISCARD sfmt::type_index index() const noexcept { return index_; }

    template<typename Ty>
    NODISCARD const Ty* get() const noexcept {
        if (index_ != format_arg_type_index<FmtCtx, Ty>::value) { return nullptr; }
        return static_cast<const Ty*>(data_);
    }

    template<typename Ty>
    NODISCARD const Ty& as() const {
        if (index_ != format_arg_type_index<FmtCtx, Ty>::value) { throw format_error("invalid value type"); }
        return *static_cast<const Ty*>(data_);
    }

 private:
    sfmt::type_index index_;
    const void* data_;
};

template<typename FmtCtx>
class basic_format_args {
 public:
    template<typename... Args>
    basic_format_args(const sfmt::arg_store<FmtCtx, Args...>& store) noexcept
        : data_(store.data()), size_(sfmt::arg_store<FmtCtx, Args...>::arg_count) {}

    NODISCARD std::size_t size() const noexcept { return size_; }
    NODISCARD bool empty() const noexcept { return !size_; }
    NODISCARD uxs::span<const unsigned> metadata() const noexcept {
        return uxs::as_span(static_cast<const unsigned*>(data_), size_);
    }

    NODISCARD basic_format_arg<FmtCtx> at(std::size_t id) const noexcept {
        assert(id < size_);
        const unsigned meta = static_cast<const unsigned*>(data_)[id];
        return basic_format_arg<FmtCtx>(static_cast<sfmt::type_index>(meta & 0xff),
                                        static_cast<const std::uint8_t*>(data_) + (meta >> 8));
    }

 private:
    const void* data_;
    std::size_t size_;
};

template<typename StrTy>
class basic_format_context {
 public:
    using output_type = StrTy;
    using char_type = typename StrTy::value_type;
    template<typename Ty>
    using formatter_type = formatter<Ty, char_type>;
    using format_args_type = basic_format_args<basic_format_context>;
    using format_arg_type = basic_format_arg<basic_format_context>;

    basic_format_context(StrTy& s, const format_args_type& args) : s_(s), args_(args) {}
    basic_format_context(StrTy& s, const std::locale& loc, const format_args_type& args)
        : s_(s), loc_(loc), args_(args) {}
    basic_format_context& operator=(const basic_format_context&) = delete;
    NODISCARD StrTy& out() { return s_; }
    NODISCARD locale_ref locale() const { return loc_; }
    NODISCARD const format_args_type& args() const { return args_; }
    NODISCARD format_arg_type arg(std::size_t id) const { return args_.at(id); }

    template<typename Ty>
    void format_arg(const Ty& val, fmt_opts& fmt) {
        formatter_type<Ty>().format(*this, val, fmt);
    }

    void format_arg(const typename format_arg_type::handle& h, fmt_opts& fmt) { h.format(*this, fmt); }

 private:
    StrTy& s_;
    locale_ref loc_;
    const basic_format_args<basic_format_context>& args_;
};

using format_args = basic_format_args<format_context>;
using wformat_args = basic_format_args<wformat_context>;

template<typename FmtCtx = format_context, typename... Args>
NODISCARD CONSTEXPR sfmt::arg_store<FmtCtx, Args...> make_format_args(const Args&... args) noexcept {
    return sfmt::arg_store<FmtCtx, Args...>{args...};
}

template<typename... Args>
NODISCARD CONSTEXPR sfmt::arg_store<wformat_context, Args...> make_wformat_args(const Args&... args) noexcept {
    return sfmt::arg_store<wformat_context, Args...>{args...};
}

template<typename CharT, typename... Args>
class basic_format_string {
 public:
    using char_type = CharT;
    template<typename Ty,
             typename = std::enable_if_t<std::is_convertible<const Ty&, std::basic_string_view<char_type>>::value>>
    CONSTEVAL basic_format_string(const Ty& fmt) noexcept : fmt_(fmt) {
#if defined(HAS_CONSTEVAL)
        constexpr std::array<unsigned, sizeof...(Args)> args_metadata = {
            static_cast<unsigned>(sfmt::arg_type_index<Args, char_type>::value)...};
        sfmt::parse_format<char_type>(fmt_, args_metadata, [](auto&&...) constexpr {}, [](auto&&...) constexpr {});
#endif  // defined(HAS_CONSTEVAL)
    }
    NODISCARD CONSTEXPR std::basic_string_view<char_type> get() const noexcept { return fmt_; }

 private:
    std::basic_string_view<char_type> fmt_;
};

template<typename... Args>
using format_string = basic_format_string<char, type_identity_t<Args>...>;
template<typename... Args>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Args>...>;

// ---- basic_vformat

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt,
                     basic_format_args<basic_format_context<StrTy>> args) {
    sfmt::vformat(basic_format_context<StrTy>{s, args}, fmt);
    return s;
}

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, const std::locale& loc, std::basic_string_view<typename StrTy::value_type> fmt,
                     basic_format_args<basic_format_context<StrTy>> args) {
    sfmt::vformat(basic_format_context<StrTy>{s, loc, args}, fmt);
    return s;
}

// ---- basic_format

template<typename StrTy, typename... Args>
StrTy& basic_format(StrTy& s, basic_format_string<typename StrTy::value_type, Args...> fmt, const Args&... args) {
    return basic_vformat(s, fmt.get(), make_format_args<basic_format_context<StrTy>>(args...));
}

template<typename StrTy, typename... Args>
StrTy& basic_format(StrTy& s, const std::locale& loc, basic_format_string<typename StrTy::value_type, Args...> fmt,
                    const Args&... args) {
    return basic_vformat(s, loc, fmt.get(), make_format_args<basic_format_context<StrTy>>(args...));
}

// ---- vformat

NODISCARD inline std::string vformat(std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, fmt, args);
    return std::string(buf.data(), buf.size());
}

NODISCARD inline std::wstring vformat(std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

NODISCARD inline std::string vformat(const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, loc, fmt, args);
    return std::string(buf.data(), buf.size());
}

NODISCARD inline std::wstring vformat(const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, loc, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

// ---- format

template<typename... Args>
NODISCARD std::string format(format_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_format_args(args...));
}

template<typename... Args>
NODISCARD std::wstring format(wformat_string<Args...> fmt, const Args&... args) {
    return vformat(fmt.get(), make_wformat_args(args...));
}

template<typename... Args>
NODISCARD std::string format(const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_format_args(args...));
}

template<typename... Args>
NODISCARD std::wstring format(const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat(loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to

inline char* vformat_to(char* p, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return basic_vformat<membuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline wchar_t* vformat_to(wchar_t* p, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return basic_vformat<wmembuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline char* vformat_to(char* p, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return basic_vformat<membuffer>(buf, loc, fmt, args).curr();
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, loc, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

inline wchar_t* vformat_to(wchar_t* p, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return basic_vformat<wmembuffer>(buf, loc, fmt, args).curr();
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, loc, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

// ---- format_to

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to(OutputIt out, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to(std::move(out), loc, fmt.get(), make_wformat_args(args...));
}

// ---- vformat_to_n

template<typename OutputIt>
struct format_to_n_result {
#if __cplusplus < 201703L
    format_to_n_result(OutputIt o, std::size_t s) : out(o), size(s) {}
#endif  // __cplusplus < 201703L
    OutputIt out;
    std::size_t size;
};

template<typename Ty>
class basic_membuffer_with_size_counter final : public basic_membuffer<Ty> {
 public:
    basic_membuffer_with_size_counter(Ty* first, Ty* last) noexcept
        : basic_membuffer<Ty>(first, last), size_(static_cast<std::size_t>(last - first)) {}
    std::size_t size() const { return this->avail() ? size_ - this->avail() : size_; }

 private:
    std::size_t size_ = 0;

    std::size_t try_grow(std::size_t extra) override {
        size_ += extra;
        return 0;
    }
};

inline format_to_n_result<char*> vformat_to_n(char* p, std::size_t n, std::string_view fmt, format_args args) {
    basic_membuffer_with_size_counter<char> buf(p, p + n);
    return {basic_vformat<membuffer>(buf, fmt, args).curr(), buf.size()};
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<wchar_t*> vformat_to_n(wchar_t* p, std::size_t n, std::wstring_view fmt, wformat_args args) {
    basic_membuffer_with_size_counter<wchar_t> buf(p, p + n);
    return {basic_vformat<wmembuffer>(buf, fmt, args).curr(), buf.size()};
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<char*> vformat_to_n(char* p, std::size_t n, const std::locale& loc, std::string_view fmt,
                                              format_args args) {
    basic_membuffer_with_size_counter<char> buf(p, p + n);
    return {basic_vformat<membuffer>(buf, loc, fmt, args).curr(), buf.size()};
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const std::locale& loc, std::string_view fmt,
                                          format_args args) {
    inline_dynbuffer buf;
    basic_vformat<membuffer>(buf, loc, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

inline format_to_n_result<wchar_t*> vformat_to_n(wchar_t* p, std::size_t n, const std::locale& loc,
                                                 std::wstring_view fmt, wformat_args args) {
    basic_membuffer_with_size_counter<wchar_t> buf(p, p + n);
    return {basic_vformat<wmembuffer>(buf, loc, fmt, args).curr(), buf.size()};
}

template<typename OutputIt, typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> vformat_to_n(OutputIt out, std::size_t n, const std::locale& loc, std::wstring_view fmt,
                                          wformat_args args) {
    inline_wdynbuffer buf;
    basic_vformat<wmembuffer>(buf, loc, fmt, args);
    return {std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out)), buf.size()};
}

// ---- format_to_n

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, const std::locale& loc,
                                         format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
format_to_n_result<OutputIt> format_to_n(OutputIt out, std::size_t n, const std::locale& loc,
                                         wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_wformat_args(args...));
}

// ---- vprint

template<typename CharT, typename Traits = std::char_traits<CharT>>
basic_iobuf<CharT>& print_quoted_text(basic_iobuf<CharT>& out, std::basic_string_view<CharT, Traits> text) {
    const CharT *p1 = text.data(), *pend = text.data() + text.size();
    out.put('\"');
    for (const CharT* p2 = text.data(); p2 != pend; ++p2) {
        char esc = '\0';
        switch (*p2) {
            case '\"': esc = '\"'; break;
            case '\\': esc = '\\'; break;
            case '\a': esc = 'a'; break;
            case '\b': esc = 'b'; break;
            case '\f': esc = 'f'; break;
            case '\n': esc = 'n'; break;
            case '\r': esc = 'r'; break;
            case '\t': esc = 't'; break;
            case '\v': esc = 'v'; break;
            default: continue;
        }
        out.write(uxs::as_span(p1, p2 - p1));
        out.put('\\'), out.put(esc);
        p1 = p2 + 1;
    }
    out.write(uxs::as_span(p1, pend - p1));
    out.put('\"');
    return out;
}

template<typename Ty>
class basic_membuffer_for_iobuf final : public basic_membuffer<Ty> {
 public:
    explicit basic_membuffer_for_iobuf(basic_iobuf<Ty>& out) noexcept
        : basic_membuffer<Ty>(out.first_avail(), out.last_avail()), out_(out) {}
    ~basic_membuffer_for_iobuf() override { out_.advance(this->curr() - out_.first_avail()); }

 private:
    basic_iobuf<Ty>& out_;

    std::size_t try_grow(std::size_t extra) override {
        out_.advance(this->curr() - out_.first_avail());
        if (!out_.reserve().good()) { return false; }
        this->set(out_.first_avail(), out_.last_avail());
        return this->avail();
    }
};

inline iobuf& vprint(iobuf& out, std::string_view fmt, format_args args) {
    basic_membuffer_for_iobuf<char> buf(out);
    basic_vformat<membuffer>(buf, fmt, args);
    return out;
}

inline wiobuf& vprint(wiobuf& out, std::wstring_view fmt, wformat_args args) {
    basic_membuffer_for_iobuf<wchar_t> buf(out);
    basic_vformat<wmembuffer>(buf, fmt, args);
    return out;
}

inline iobuf& vprint(iobuf& out, const std::locale& loc, std::string_view fmt, format_args args) {
    basic_membuffer_for_iobuf<char> buf(out);
    basic_vformat<membuffer>(buf, loc, fmt, args);
    return out;
}

inline wiobuf& vprint(wiobuf& out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    basic_membuffer_for_iobuf<wchar_t> buf(out);
    basic_vformat<wmembuffer>(buf, loc, fmt, args);
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
    return vprint(stdbuf::out, fmt.get(), make_format_args(args...));
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
    return vprint(stdbuf::out, loc, fmt.get(), make_format_args(args...));
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
    return vprint(stdbuf::out, fmt.get(), make_format_args(args...)).endl();
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
    return vprint(stdbuf::out, loc, fmt.get(), make_format_args(args...)).endl();
}

}  // namespace uxs
