#pragma once

#include "chars.h"
#include "format_common.h"
#include "span.h"
#include "string_conv_common.h"
#include "type_traits.h"

namespace uxs {

namespace fmt {

enum class index_t : std::uint8_t {
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
    string,
    custom,
};

template<typename CharT>
class custom_arg_handle {
 public:
    using format_context = basic_format_context<CharT>;
    using parse_context = typename format_context::parse_context;
    using format_func_type = typename parse_context::iterator (*)(format_context&, parse_context&,
                                                                  const custom_arg_handle&);

    explicit custom_arg_handle(format_func_type print_fn) noexcept : print_fn_(print_fn) {}
    custom_arg_handle(const custom_arg_handle&) = delete;
    custom_arg_handle& operator=(const custom_arg_handle&) = delete;

    typename parse_context::iterator format(format_context& ctx, parse_context& parse_ctx) const {
        return print_fn_(ctx, parse_ctx, *this);
    }

 private:
    format_func_type print_fn_;
};

template<typename Ty, typename CharT>
struct type_index {};
#define UXS_FMT_DECLARE_ARG_TYPE_INDEX(ty, index) \
    template<typename CharT> \
    struct type_index<ty, CharT> : std::integral_constant<index_t, index> {}
UXS_FMT_DECLARE_ARG_TYPE_INDEX(bool, index_t::boolean);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(char32_t, index_t::character);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int32_t, index_t::integer);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int64_t, index_t::long_integer);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint32_t, index_t::unsigned_integer);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint64_t, index_t::unsigned_long_integer);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(float, index_t::single_precision);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(double, index_t::double_precision);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(long double, index_t::long_double_precision);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(const void*, index_t::pointer);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::basic_string_view<CharT>, index_t::string);
UXS_FMT_DECLARE_ARG_TYPE_INDEX(custom_arg_handle<CharT>, index_t::custom);
#undef UXS_FMT_DECLARE_ARG_TYPE_INDEX

// --------------------------

template<typename Ty, typename CharT>
class custom_arg_handle_impl : public custom_arg_handle<CharT> {
 public:
    using format_context = typename custom_arg_handle<CharT>::format_context;
    using parse_context = typename custom_arg_handle<CharT>::parse_context;

    explicit custom_arg_handle_impl(const Ty& val) noexcept : custom_arg_handle<CharT>(func), val_(&val) {}

 private:
    const Ty* val_;

    static typename parse_context::iterator func(format_context& ctx, parse_context& parse_ctx,
                                                 const custom_arg_handle<CharT>& h) {
        return ctx.format_arg(parse_ctx, *static_cast<const custom_arg_handle_impl&>(h).val_);
    }
};

template<typename OtherCharT, typename CharT>
class custom_arg_handle_impl<std::basic_string_view<OtherCharT>, CharT> : public custom_arg_handle<CharT> {
 public:
    using format_context = typename custom_arg_handle<CharT>::format_context;
    using parse_context = typename custom_arg_handle<CharT>::parse_context;

    explicit custom_arg_handle_impl(std::basic_string_view<OtherCharT> val) noexcept
        : custom_arg_handle<CharT>(func), val_(val) {}

 private:
    std::basic_string_view<OtherCharT> val_;

    static typename parse_context::iterator func(format_context& ctx, parse_context& parse_ctx,
                                                 const custom_arg_handle<CharT>& h) {
        return ctx.format_arg(parse_ctx, static_cast<const custom_arg_handle_impl&>(h).val_);
    }
};

// --------------------------

template<typename Ty, typename CharT, typename = void>
struct arg_type_index : std::integral_constant<index_t, index_t::custom> {};
template<typename Ty, typename CharT>
struct arg_type_index<Ty, CharT, std::void_t<typename type_index<reduce_type_t<Ty, CharT>, CharT>::type>>
    : std::integral_constant<index_t, type_index<reduce_type_t<Ty, CharT>, CharT>::value> {};

template<typename Ty, typename CharT>
using arg_store_type = std::conditional<(arg_type_index<Ty, CharT>::value) < index_t::custom, reduce_type_t<Ty, CharT>,
                                        custom_arg_handle_impl<reduce_type_t<Ty, CharT>, CharT>>;

template<typename Ty, typename CharT>
using arg_size = est::size_of<typename arg_store_type<Ty, CharT>::type>;

template<typename Ty, typename CharT>
using arg_alignment = std::alignment_of<typename arg_store_type<Ty, CharT>::type>;

template<typename CharT, std::size_t Size, typename... Ts>
struct arg_store_size_evaluator {};
template<typename CharT, std::size_t Size>
struct arg_store_size_evaluator<CharT, Size> : std::integral_constant<std::size_t, Size> {};
template<typename CharT, std::size_t Size, typename Ty, typename... Rest>
struct arg_store_size_evaluator<CharT, Size, Ty, Rest...>
    : std::integral_constant<
          std::size_t,
          arg_store_size_evaluator<CharT,
                                   est::align_up<arg_alignment<Ty, CharT>::value>::template type<Size>::value +
                                       arg_size<Ty, CharT>::value,
                                   Rest...>::value> {};

template<typename CharT, typename... Ts>
using arg_store_alignment_evaluator = est::maximum<arg_alignment<Ts, CharT>...>;

template<typename CharT, typename... Args>
class arg_store {
 public:
    enum : std::size_t { arg_count = sizeof...(Args) };

    static_assert(std::is_trivially_copyable<std::basic_string_view<CharT>>::value &&
                      std::is_trivially_destructible<std::basic_string_view<CharT>>::value,
                  "std::basic_string_view<> is assumed to be trivially copyable and destructible");

#if __cplusplus >= 201703L
    arg_store(const arg_store&) = delete;
#else   // __cplusplus >= 201703L
    arg_store(const arg_store&) noexcept = default;
#endif  // __cplusplus >= 201703L
    arg_store& operator=(const arg_store&) = delete;

    explicit arg_store(const Args&... args) noexcept { store_values(0, arg_count * sizeof(unsigned), args...); }
    const void* data() const noexcept { return data_; }

 private:
    enum : std::size_t {
        storage_size = arg_store_size_evaluator<CharT, arg_count * sizeof(unsigned), Args...>::value,
        storage_alignment = arg_store_alignment_evaluator<CharT, unsigned, Args...>::value,
    };

    alignas(storage_alignment) std::uint8_t data_[storage_size];

    template<typename OtherCharT, typename = std::enable_if_t<est::is_character<OtherCharT>::value>>
    static void store_value(OtherCharT ch, void* data) noexcept {
        ::new (data) char32_t(static_cast<typename std::make_unsigned<OtherCharT>::type>(ch));
    }

    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
    static void store_value(const StrLikeTy& s, void* data) noexcept {
        ::new (data) typename arg_store_type<StrLikeTy, CharT>::type(to_normal_string_view(s));
    }

    template<typename Ty, typename = std::enable_if_t<is_formattable<Ty, CharT>::value>, typename... Dummy>
    static void store_value(const Ty& val, void* data, Dummy&&...) noexcept {
        static_assert(sizeof...(Dummy) == 0, "invalid function argument count");
        ::new (data) typename arg_store_type<Ty, CharT>::type(val);
    }

    void store_values(std::size_t /*i*/, std::size_t /*offset*/) noexcept {}

    template<typename Ty, typename... Ts>
    void store_values(std::size_t i, std::size_t offset, const Ty& val, const Ts&... other) noexcept {
        static_assert(is_formattable<Ty, CharT>::value, "value of this type cannot be formatted");
        offset = est::align_up<arg_alignment<Ty, CharT>::value>{}(offset);
        ::new (static_cast<void*>(reinterpret_cast<unsigned*>(&data_) + i)) unsigned(
            static_cast<unsigned>(offset << 8) | static_cast<unsigned>(arg_type_index<Ty, CharT>::value));
        store_value(val, &data_[offset]);
        store_values(i + 1, offset + arg_size<Ty, CharT>::value, other...);
    }
};

template<typename CharT>
class arg_store<CharT> {
 public:
    enum : std::size_t { arg_count = 0 };
#if __cplusplus >= 201703L
    arg_store(const arg_store&) = delete;
#else   // __cplusplus >= 201703L
    arg_store(const arg_store&) noexcept = default;
#endif  // __cplusplus >= 201703L
    arg_store& operator=(const arg_store&) = delete;
    arg_store() noexcept = default;
    const void* data() const noexcept { return nullptr; }
};

// --------------------------

struct parse_context_utils {
    [[noreturn]] static void report_syntax_error() { throw format_error("invalid specifier syntax"); }
    [[noreturn]] static void report_unexpected_prec_error() { throw format_error("unexpected precision specifier"); }
    [[noreturn]] static void report_unexpected_sign_error() { throw format_error("unexpected sign specifier"); }
    [[noreturn]] static void report_unexpected_alternate_error() {
        throw format_error("unexpected alternate specifier");
    }
    [[noreturn]] static void report_unexpected_leading_zeroes_error() {
        throw format_error("unexpected leading zeroes specifier");
    }
    [[noreturn]] static void report_unexpected_locale_specific_error() {
        throw format_error("unexpected locale-specific specifier");
    }
    [[noreturn]] static void report_type_error() { throw format_error("unacceptable type specifier"); }
    [[noreturn]] static void report_out_of_argument_list_error() { throw format_error("out of argument list"); }
    [[noreturn]] static void report_integer_out_of_range_error() { throw format_error("integer out of range"); }
    [[noreturn]] static void report_invalid_argument_type_error() {
        throw format_error("argument is not of valid type");
    }
    [[noreturn]] static void report_negative_argument_specified_error() {
        throw format_error("negative argument specified");
    }
    [[noreturn]] static void report_automatic_argument_indexing_error() {
        throw format_error("automatic argument indexing error");
    }
    [[noreturn]] static void report_manual_argument_indexing_error() {
        throw format_error("manual argument indexing error");
    }

    template<typename InputIt, typename Ty>
    static UXS_CONSTEXPR InputIt parse_number(InputIt first, InputIt last, Ty& num) {
        for (unsigned dig = 0; first != last && (dig = dig_v{}(*first)) < 10; ++first) {
            Ty num0 = num;
            num = 10 * num + dig;
            if (num < num0) { report_integer_out_of_range_error(); }
        }
        return first;
    }

    template<typename ParseCtx, typename Ty>
    static UXS_CONSTEXPR std::pair<typename ParseCtx::iterator, bool> parse_dynamic_integral_parameter(
        ParseCtx& ctx, typename ParseCtx::iterator it, Ty& param) {
        std::size_t id = 0;
        if (it == ctx.end()) { return {it, false}; }
        if (*it == '}') {
            id = ctx.next_arg_id();
        } else if ((id = dig_v{}(*it)) < 10) {
            it = parse_number(it + 1, ctx.end(), id);
            ctx.check_arg_id(id);
            if (it == ctx.end() || *it != '}') { return {it, false}; }
        } else {
            return {it, false};
        }
        ctx.get_dynamic_integral(id, param);
        return {it + 1, true};
    }

    template<typename ParseCtx, typename Ty>
    static UXS_CONSTEXPR std::pair<typename ParseCtx::iterator, bool> parse_integral_parameter(
        ParseCtx& ctx, typename ParseCtx::iterator it, Ty& param) {
        unsigned dig = 0;
        if (it == ctx.end()) { return {it, false}; }
        if ((dig = dig_v{}(*it)) < 10) {
            param = static_cast<Ty>(dig);
            it = parse_number(it + 1, ctx.end(), param);
        } else if (*it == '{') {
            const auto result = parse_dynamic_integral_parameter(ctx, it + 1, param);
            if (!result.second) { return {it, false}; }
            it = result.first;
        } else {
            return {it, false};
        }
        return {it, true};
    }

    template<typename ParseCtx>
    static UXS_CONSTEXPR typename ParseCtx::iterator parse_adjustment(ParseCtx& ctx, typename ParseCtx::iterator it,
                                                                      fmt_opts& opts) {
        if (it == ctx.end()) { return it; }

        enum class state_t { adjustment = 0, width };

        state_t state = state_t::adjustment;

        if (it + 1 != ctx.end()) {
            switch (*(it + 1)) {  // adjustment with fill character
                case '<': {
                    opts.fill = *it, opts.flags |= fmt_flags::left;
                    it += 2, state = state_t::width;
                } break;
                case '^': {
                    opts.fill = *it, opts.flags |= fmt_flags::internal;
                    it += 2, state = state_t::width;
                } break;
                case '>': {
                    opts.fill = *it, opts.flags |= fmt_flags::right;
                    it += 2, state = state_t::width;
                } break;
                default: break;
            }
        }

        while (it != ctx.end()) {
            switch (*it) {
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < next_state) { \
        action; \
        state = next_state; \
        break; \
    } \
    return it
                // adjustment
                case '<': UXS_FMT_SPECIFIER_CASE(state_t::width, (opts.flags |= fmt_flags::left, ++it));
                case '^': UXS_FMT_SPECIFIER_CASE(state_t::width, (opts.flags |= fmt_flags::internal, ++it));
                case '>': UXS_FMT_SPECIFIER_CASE(state_t::width, (opts.flags |= fmt_flags::right, ++it));
#undef UXS_FMT_SPECIFIER_CASE

                // width
                case '{': {
                    const auto result = parse_dynamic_integral_parameter(ctx, it + 1, opts.width);
                    if (!result.second) { return it; }
                    return result.first;
                } break;
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    opts.width = static_cast<unsigned>(*it - '0');
                    return parse_number(it + 1, ctx.end(), opts.width);
                } break;

                default: return it;
            }
        }

        return it;
    }

    template<typename ParseCtx>
    static UXS_CONSTEXPR typename ParseCtx::iterator parse_standard(ParseCtx& ctx, typename ParseCtx::iterator it,
                                                                    fmt_opts& opts) {
        if (it == ctx.end()) { return it; }

        enum class state_t { adjustment = 0, sign, alternate, leading_zeroes, width, precision, locale };

        state_t state = state_t::adjustment;

        if (it + 1 != ctx.end()) {
            switch (*(it + 1)) {  // adjustment with fill character
                case '<': {
                    opts.fill = *it, opts.flags |= fmt_flags::left;
                    it += 2, state = state_t::sign;
                } break;
                case '^': {
                    opts.fill = *it, opts.flags |= fmt_flags::internal;
                    it += 2, state = state_t::sign;
                } break;
                case '>': {
                    opts.fill = *it, opts.flags |= fmt_flags::right;
                    it += 2, state = state_t::sign;
                } break;
                default: break;
            }
        }

        while (it != ctx.end()) {
            switch (*it) {
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < next_state) { \
        action; \
        state = next_state; \
        break; \
    } \
    return it
                // adjustment
                case '<': UXS_FMT_SPECIFIER_CASE(state_t::sign, (opts.flags |= fmt_flags::left, ++it));
                case '^': UXS_FMT_SPECIFIER_CASE(state_t::sign, (opts.flags |= fmt_flags::internal, ++it));
                case '>': UXS_FMT_SPECIFIER_CASE(state_t::sign, (opts.flags |= fmt_flags::right, ++it));

                // sign specifiers
                case '-': UXS_FMT_SPECIFIER_CASE(state_t::alternate, (opts.flags |= fmt_flags::sign_neg, ++it));
                case '+': UXS_FMT_SPECIFIER_CASE(state_t::alternate, (opts.flags |= fmt_flags::sign_pos, ++it));
                case ' ': UXS_FMT_SPECIFIER_CASE(state_t::alternate, (opts.flags |= fmt_flags::sign_align, ++it));

                // alternate
                case '#': UXS_FMT_SPECIFIER_CASE(state_t::leading_zeroes, (opts.flags |= fmt_flags::alternate, ++it));

                // leading zeroes
                case '0': UXS_FMT_SPECIFIER_CASE(state_t::width, (opts.flags |= fmt_flags::leading_zeroes, ++it));

                // width
                case '{':
                    UXS_FMT_SPECIFIER_CASE(state_t::precision, {
                        const auto result = parse_dynamic_integral_parameter(ctx, it + 1, opts.width);
                        if (!result.second) { return it; }
                        it = result.first;
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
                        opts.width = static_cast<unsigned>(*it - '0');
                        it = parse_number(it + 1, ctx.end(), opts.width);
                    });

                // precision
                case '.':
                    UXS_FMT_SPECIFIER_CASE(state_t::locale, {
                        opts.prec = 0;  // specified
                        const auto result = parse_integral_parameter(ctx, it + 1, opts.prec);
                        if (!result.second) { return it; }
                        it = result.first;
                    });
#undef UXS_FMT_SPECIFIER_CASE

                // locale
                case 'L': opts.flags |= fmt_flags::localize; return it + 1;

                default: return it;
            }
        }

        return it;
    }

    enum class type_spec {
        none = 0,
        integer,
        floating_point,
        pointer,
        character,
        string,
        debug_string,
    };

    template<typename CharT>
    static UXS_CONSTEXPR type_spec classify_standard_type(CharT ch, fmt_opts& opts) {
        switch (ch) {
            case 'd': opts.flags |= fmt_flags::dec; return type_spec::integer;
            case 'B': opts.flags |= fmt_flags::bin | fmt_flags::uppercase; return type_spec::integer;
            case 'b': opts.flags |= fmt_flags::bin; return type_spec::integer;
            case 'o': opts.flags |= fmt_flags::oct; return type_spec::integer;
            case 'X': opts.flags |= fmt_flags::hex | fmt_flags::uppercase; return type_spec::integer;
            case 'x': opts.flags |= fmt_flags::hex; return type_spec::integer;
            case 'F': opts.flags |= fmt_flags::fixed | fmt_flags::uppercase; return type_spec::floating_point;
            case 'f': opts.flags |= fmt_flags::fixed; return type_spec::floating_point;
            case 'E': opts.flags |= fmt_flags::scientific | fmt_flags::uppercase; return type_spec::floating_point;
            case 'e': opts.flags |= fmt_flags::scientific; return type_spec::floating_point;
            case 'G': opts.flags |= fmt_flags::general | fmt_flags::uppercase; return type_spec::floating_point;
            case 'g': opts.flags |= fmt_flags::general; return type_spec::floating_point;
            case 'A': opts.flags |= fmt_flags::hex | fmt_flags::uppercase; return type_spec::floating_point;
            case 'a': opts.flags |= fmt_flags::hex; return type_spec::floating_point;
            case 'P': opts.flags |= fmt_flags::uppercase; return type_spec::pointer;
            case 'p': return type_spec::pointer;
            case 'c': opts.flags |= fmt_flags::character; return type_spec::character;
            case 's': return type_spec::string;
            case '?': return type_spec::debug_string;
            default: return type_spec::none;
        }
    }

    template<typename Formatter, typename = std::void_t<decltype(std::declval<Formatter&>().set_debug_format())>>
    static UXS_CONSTEXPR void set_debug_format(Formatter& f) {
        f.set_debug_format();
    }

    template<typename Formatter, typename... Dummy>
    static UXS_CONSTEXPR void set_debug_format(Formatter&, Dummy&&...) {
        static_assert(sizeof...(Dummy) == 0, "invalid function argument count");
    }
};

template<typename ParseCtx, typename OnTextFn, typename OnArgFn>
UXS_CONSTEXPR void parse_format(ParseCtx& ctx, OnTextFn&& on_text_fn, OnArgFn&& on_arg_fn) {
    auto it0 = ctx.begin();
    for (auto it = it0; it != ctx.end(); ++it) {
        if (*it != '{' && *it != '}') { continue; }
        on_text_fn(it0, it);
        it0 = ++it;
        if (it != ctx.end() && *(it - 1) == '{' && *it != '{') {
            std::size_t arg_id = 0;
            if ((arg_id = dig_v{}(*it)) < 10) {
                it = ParseCtx::parse_number(it + 1, ctx.end(), arg_id);
                ctx.check_arg_id(arg_id);
                if (it == ctx.end()) { ParseCtx::report_syntax_error(); }
            } else {
                arg_id = ctx.next_arg_id();
            }
            ctx.advance_to(it);
            it = on_arg_fn(ctx, arg_id);
            if (it == ctx.end() || *it != '}') { ParseCtx::report_syntax_error(); }
            it0 = it + 1;
        } else if (it == ctx.end() || *(it - 1) != *it) {
            ParseCtx::report_syntax_error();
        }
    }
    on_text_fn(it0, ctx.end());
}

template<typename FmtCtx>
UXS_EXPORT void format_impl(FmtCtx, typename FmtCtx::parse_context);

}  // namespace fmt

template<typename CharT>
class basic_format_arg {
 public:
    using char_type = CharT;

    basic_format_arg(fmt::index_t index, const void* data) noexcept : index_(index), data_(data) {}

    fmt::index_t index() const noexcept { return index_; }

    template<typename Ty, typename = std::enable_if_t<std::is_integral<Ty>::value>>
    Ty get_integral() const {
        return get_integral_dispatch(std::numeric_limits<Ty>::max(), std::is_unsigned<Ty>());
    }

    template<typename Func>
    auto visit(Func&& fn) -> decltype(fn(std::int32_t{})) {
        switch (index_) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty) \
    case fmt::type_index<ty, CharT>::value: { \
        return fn(*static_cast<ty const*>(data_)); \
    } break
            UXS_FMT_FORMAT_ARG_VALUE(bool);
            UXS_FMT_FORMAT_ARG_VALUE(char32_t);
            UXS_FMT_FORMAT_ARG_VALUE(std::int32_t);
            UXS_FMT_FORMAT_ARG_VALUE(std::int64_t);
            UXS_FMT_FORMAT_ARG_VALUE(std::uint32_t);
            UXS_FMT_FORMAT_ARG_VALUE(std::uint64_t);
            UXS_FMT_FORMAT_ARG_VALUE(float);
            UXS_FMT_FORMAT_ARG_VALUE(double);
            UXS_FMT_FORMAT_ARG_VALUE(long double);
            UXS_FMT_FORMAT_ARG_VALUE(const void*);
            UXS_FMT_FORMAT_ARG_VALUE(std::basic_string_view<char_type>);
            UXS_FMT_FORMAT_ARG_VALUE(fmt::custom_arg_handle<char_type>);
#undef UXS_FMT_FORMAT_ARG_VALUE
        }
        UXS_UNREACHABLE_CODE;
    }

 private:
    fmt::index_t index_;
    const void* data_;

    template<typename Ty>
    Ty get_integral_dispatch(Ty pos_limit, std::true_type /* unsigned */) const {
        switch (index_) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty) \
    case fmt::type_index<ty, CharT>::value: { \
        ty val = *static_cast<const ty*>(data_); \
        if (val < 0) { fmt::parse_context_utils::report_negative_argument_specified_error(); } \
        if (static_cast<std::make_unsigned<ty>::type>(val) > pos_limit) { \
            fmt::parse_context_utils::report_integer_out_of_range_error(); \
        } \
        return static_cast<Ty>(val); \
    } break
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int32_t);
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int64_t);
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    case fmt::type_index<ty, CharT>::value: { \
        ty val = *static_cast<const ty*>(data_); \
        if (val > pos_limit) { fmt::parse_context_utils::report_integer_out_of_range_error(); } \
        return static_cast<Ty>(val); \
    } break
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint32_t);
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint64_t);
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
            default: fmt::parse_context_utils::report_invalid_argument_type_error();
        }
    }

    template<typename Ty>
    Ty get_integral_dispatch(Ty pos_limit, std::false_type /* unsigned */) const {
        switch (index_) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty) \
    case fmt::type_index<ty, CharT>::value: { \
        ty val = *static_cast<const ty*>(data_); \
        if (val < -pos_limit - 1 || val > pos_limit) { \
            fmt::parse_context_utils::report_integer_out_of_range_error(); \
        } \
        return static_cast<Ty>(val); \
    } break
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int32_t);
            UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int64_t);
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    case fmt::type_index<ty, CharT>::value: { \
        ty val = *static_cast<const ty*>(data_); \
        if (val > static_cast<typename std::make_unsigned<Ty>::type>(pos_limit)) { \
            fmt::parse_context_utils::report_integer_out_of_range_error(); \
        } \
        return static_cast<Ty>(val); \
    } break
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint32_t);
            UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint64_t);
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
            default: fmt::parse_context_utils::report_invalid_argument_type_error();
        }
    }
};

template<typename CharT>
class basic_format_args {
 public:
    using char_type = CharT;

    template<typename... Args>
    basic_format_args(const fmt::arg_store<CharT, Args...>& store) noexcept
        : data_(store.data()), size_(fmt::arg_store<CharT, Args...>::arg_count) {}

    std::size_t size() const noexcept { return size_; }

    basic_format_arg<char_type> get(std::size_t id) const noexcept {
        const unsigned meta = static_cast<const unsigned*>(data_)[id];
        return basic_format_arg<char_type>(static_cast<fmt::index_t>(meta & 0xff),
                                           static_cast<const std::uint8_t*>(data_) + (meta >> 8));
    }

    template<typename... Args>
    static fmt::arg_store<CharT, Args...> make(const Args&... args) noexcept {
        return fmt::arg_store<CharT, Args...>(args...);
    }

 private:
    const void* data_;
    std::size_t size_;
};

template<typename CharT>
class basic_format_parse_context : public fmt::parse_context_utils {
 public:
    using char_type = CharT;
    using iterator = typename std::basic_string_view<char_type>::const_iterator;
    using const_iterator = typename std::basic_string_view<char_type>::const_iterator;
    using format_args_type = basic_format_args<char_type>;
    using format_arg_type = basic_format_arg<char_type>;

    basic_format_parse_context(iterator first, iterator last, format_args_type args) noexcept
        : first_(first), last_(last), args_(args) {}
#if __cplusplus >= 201703L
    basic_format_parse_context(const basic_format_parse_context&) = delete;
#else   // __cplusplus >= 201703L
    basic_format_parse_context(const basic_format_parse_context&) noexcept = default;
#endif  // __cplusplus >= 201703L
    basic_format_parse_context& operator=(const basic_format_parse_context&) = delete;

    iterator begin() const noexcept { return first_; }
    iterator end() const noexcept { return last_; }
    void advance_to(iterator it) noexcept { first_ = it; }

    UXS_NODISCARD std::size_t next_arg_id() {
        if (next_id_ == est::unspecified_size) { report_automatic_argument_indexing_error(); }
        if (next_id_ >= args_.size()) { report_out_of_argument_list_error(); }
        return next_id_++;
    }

    void check_arg_id(std::size_t id) {
        if (next_id_ != est::unspecified_size && next_id_ > 0) { report_manual_argument_indexing_error(); }
        if (id >= args_.size()) { report_out_of_argument_list_error(); }
        next_id_ = est::unspecified_size;
    }

    template<typename Ty, typename = std::enable_if_t<std::is_integral<Ty>::value>>
    void get_dynamic_integral(std::size_t id, Ty& param) const {
        param = arg(id).template get_integral<Ty>();
    }

    format_arg_type arg(std::size_t id) const { return args_.get(id); }

 private:
    iterator first_;
    iterator last_;
    format_args_type args_;
    std::size_t next_id_ = 0;
};

#if defined(UXS_HAS_CONSTEVAL)
template<typename CharT>
class compile_parse_context : public fmt::parse_context_utils {
 public:
    using char_type = CharT;
    using iterator = typename std::basic_string_view<char_type>::const_iterator;
    using const_iterator = typename std::basic_string_view<char_type>::const_iterator;

    constexpr compile_parse_context(iterator first, iterator last, est::span<const fmt::index_t> arg_types) noexcept
        : first_(first), last_(last), arg_types_(arg_types) {}
    compile_parse_context(const compile_parse_context&) = delete;
    compile_parse_context& operator=(const compile_parse_context&) = delete;

    constexpr iterator begin() const noexcept { return first_; }
    constexpr iterator end() const noexcept { return last_; }
    constexpr void advance_to(iterator it) noexcept { first_ = it; }

    [[nodiscard]] constexpr std::size_t next_arg_id() {
        if (next_id_ == est::unspecified_size) { report_automatic_argument_indexing_error(); }
        if (next_id_ >= arg_types_.size()) { report_out_of_argument_list_error(); }
        return next_id_++;
    }

    constexpr void check_arg_id(std::size_t id) {
        if (next_id_ != est::unspecified_size && next_id_ > 0) { report_manual_argument_indexing_error(); }
        if (id >= arg_types_.size()) { report_out_of_argument_list_error(); }
        next_id_ = est::unspecified_size;
    }

    template<typename Ty, typename = std::enable_if_t<std::is_integral<Ty>::value>>
    constexpr void get_dynamic_integral(std::size_t id, Ty& /*param*/) const {
        check_dynamic_spec<std::int32_t, std::uint32_t, std::int64_t, std::uint64_t>(id);
    }

    template<typename Ty>
    constexpr iterator parse_arg() {
        formatter_t<Ty, char_type> f;
        return f.parse(*this);
    }

 private:
    iterator first_;
    iterator last_;
    est::span<const fmt::index_t> arg_types_;
    std::size_t next_id_ = 0;

    template<typename... Ts>
    constexpr void check_dynamic_spec(std::size_t id) const {
        if (((arg_types_[id] != fmt::type_index<Ts, char_type>::value) && ...)) {
            report_invalid_argument_type_error();
        }
    }
};
#endif  // defined(UXS_HAS_CONSTEVAL)

template<typename CharT>
class basic_format_context {
 public:
    using char_type = CharT;
    using output_type = basic_membuffer<char_type>;
    using parse_context = basic_format_parse_context<char_type>;
    using custom_arg_handle = fmt::custom_arg_handle<char_type>;

    basic_format_context(output_type& out, locale_ref loc) noexcept : out_(out), loc_(loc) {}
#if __cplusplus >= 201703L
    basic_format_context(const basic_format_context&) = delete;
#else   // __cplusplus >= 201703L
    basic_format_context(const basic_format_context&) noexcept = default;
#endif  // __cplusplus >= 201703L
    basic_format_context& operator=(const basic_format_context&) = delete;
    output_type& out() noexcept { return out_; }
    locale_ref locale() const noexcept { return loc_; }

    basic_format_context rebind(output_type& out) noexcept { return basic_format_context(out, loc_); }

    template<typename Ty>
    typename parse_context::iterator format_arg(parse_context& parse_ctx, const Ty& val) {
        formatter_t<Ty, char_type> f;
        const auto it = f.parse(parse_ctx);
        f.format(*this, val);
        return it;
    }

    typename parse_context::iterator format_arg(parse_context& parse_ctx, const custom_arg_handle& h) {
        return h.format(*this, parse_ctx);
    }

 private:
    output_type& out_;
    locale_ref loc_;
};

template<typename CharT>
struct basic_runtime_format {
    std::basic_string_view<CharT> str;
    explicit basic_runtime_format(std::basic_string_view<CharT> s) noexcept : str(s) {}
};

template<typename CharT, typename... Args>
class basic_format_string {
 public:
    using char_type = CharT;
    template<typename StrLikeTy, typename = std::enable_if_t<is_normal_string_like<StrLikeTy, char_type>::value>>
    UXS_CONSTEVAL basic_format_string(const StrLikeTy& fmt) noexcept : fmt_(fmt) {
#if defined(UXS_HAS_CONSTEVAL)
        using parse_context = compile_parse_context<char_type>;
        constexpr std::array<fmt::index_t, sizeof...(Args)> arg_types{fmt::arg_type_index<Args, char_type>::value...};
        constexpr std::array<typename parse_context::iterator (parse_context::*)(), sizeof...(Args)> parsers{
            &parse_context::template parse_arg<Args>...};
        parse_context ctx(fmt_.begin(), fmt_.end(), arg_types);
        fmt::parse_format(ctx, [](auto&&...) {}, [&parsers](auto& ctx, std::size_t id) { return (ctx.*parsers[id])(); });
#endif  // defined(UXS_HAS_CONSTEVAL)
    }
    basic_format_string(basic_runtime_format<char_type> fmt) noexcept : fmt_(fmt.str) {}
    std::basic_string_view<char_type> get() const noexcept { return fmt_; }

 private:
    std::basic_string_view<char_type> fmt_;
};

template<typename CharT, typename... Args>
using format_string_t = basic_format_string<CharT, est::type_identity_t<Args>...>;

template<typename CharT>
using format_args_t = basic_format_args<CharT>;

namespace detail {
template<typename CharT>
void vformat_append_impl(typename basic_format_context<CharT>::output_type& out, locale_ref loc,
                         std::basic_string_view<CharT> fmt, format_args_t<CharT> args) {
    fmt::format_impl(basic_format_context<CharT>(out, loc),
                     basic_format_parse_context<CharT>(fmt.begin(), fmt.end(), args));
}
template<typename StrTy, typename = std::enable_if_t<!std::is_convertible<
                             StrTy&, typename basic_format_context<typename StrTy::value_type>::output_type&>::value>>
void vformat_append_impl(StrTy& out, locale_ref loc, std::basic_string_view<typename StrTy::value_type> fmt,
                         format_args_t<typename StrTy::value_type> args) {
    using char_type = typename StrTy::value_type;
    basic_inline_dynbuffer<char_type> buf;
    fmt::format_impl(basic_format_context<char_type>(buf, loc),
                     basic_format_parse_context<char_type>(fmt.begin(), fmt.end(), args));
    out.append(buf.data(), buf.size());
}
}  // namespace detail

template<typename StrTy>
void vformat_append(StrTy& out, std::basic_string_view<typename StrTy::value_type> fmt,
                    format_args_t<typename StrTy::value_type> args) {
    detail::vformat_append_impl(out, locale_ref(), fmt, args);
}

template<typename StrTy>
void vformat_append(StrTy& out, const std::locale& loc, std::basic_string_view<typename StrTy::value_type> fmt,
                    format_args_t<typename StrTy::value_type> args) {
    detail::vformat_append_impl(out, locale_ref(loc), fmt, args);
}

template<typename StrTy, typename... Args>
void format_append(StrTy& out, format_string_t<typename StrTy::value_type, Args...> fmt, const Args&... args) {
    vformat_append(out, fmt.get(), format_args_t<typename StrTy::value_type>::make(args...));
}

template<typename StrTy, typename... Args>
void format_append(StrTy& out, const std::locale& loc, format_string_t<typename StrTy::value_type, Args...> fmt,
                   const Args&... args) {
    vformat_append(out, loc, fmt.get(), format_args_t<typename StrTy::value_type>::make(args...));
}

}  // namespace uxs
