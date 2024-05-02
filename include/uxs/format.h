#pragma once

#include "alignment.h"
#include "io/iobuf.h"
#include "stringcvt.h"

namespace uxs {
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
template<typename Ty, typename = void>
struct reduce_type {
    using type = std::remove_cv_t<Ty>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_integral<Ty>::value && std::is_unsigned<Ty>::value &&
                                        !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_integral<Ty>::value && std::is_signed<Ty>::value &&
                                        !is_boolean<Ty>::value && !is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::int32_t)), std::int32_t, std::int64_t>;
};
template<typename Ty>
struct reduce_type<Ty, std::enable_if_t<std::is_array<Ty>::value>> {
    using type = typename std::add_pointer<std::remove_cv_t<typename std::remove_extent<Ty>::type>>::type;
};
template<typename Ty>
struct reduce_type<Ty*> {
    using type = typename std::add_pointer<std::remove_cv_t<Ty>>::type;
};
template<>
struct reduce_type<std::nullptr_t> {
    using type = void*;
};
}  // namespace detail

template<typename Ty>
using reduce_type_t = typename detail::reduce_type<Ty>::type;

// --------------------------

template<typename StrTy>
struct custom_arg_handle {
    CONSTEXPR custom_arg_handle(const void* v, void (*fn)(StrTy&, const void*, fmt_opts&)) noexcept
        : val(v), print_fn(fn) {}
    const void* val;                                   // value pointer
    void (*print_fn)(StrTy&, const void*, fmt_opts&);  // printing function pointer
};

// --------------------------

namespace detail {
template<typename Ty>
struct arg_type_index : std::integral_constant<type_index, type_index::custom> {};

template<typename StrTy, typename Ty>
struct arg_store_type {
    using type = custom_arg_handle<StrTy>;
};

#define UXS_FMT_DECLARE_ARG_TYPE_INDEX(ty, store_ty, index) \
    template<> \
    struct arg_type_index<ty> : std::integral_constant<type_index, index> {}; \
    template<typename StrTy> \
    struct arg_store_type<StrTy, ty> { \
        using type = store_ty; \
    };
UXS_FMT_DECLARE_ARG_TYPE_INDEX(bool, bool, type_index::boolean)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(char, typename StrTy::value_type, type_index::character)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(wchar_t, typename StrTy::value_type, type_index::character)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int32_t, std::int32_t, type_index::integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::int64_t, std::int64_t, type_index::long_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint32_t, std::uint32_t, type_index::unsigned_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(std::uint64_t, std::uint64_t, type_index::unsigned_long_integer)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(float, float, type_index::single_precision)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(double, double, type_index::double_precision)
UXS_FMT_DECLARE_ARG_TYPE_INDEX(long double, long double, type_index::long_double_precision)
#undef UXS_FMT_DECLARE_ARG_TYPE_INDEX

template<typename Ty>
struct arg_type_index<Ty*>
    : std::integral_constant<type_index, is_character<Ty>::value ? type_index::z_string : type_index::pointer> {};
template<typename CharT, typename Traits>
struct arg_type_index<std::basic_string_view<CharT, Traits>> : std::integral_constant<type_index, type_index::string> {
};
template<typename CharT, typename Traits, typename Alloc>
struct arg_type_index<std::basic_string<CharT, Traits, Alloc>>
    : std::integral_constant<type_index, type_index::string> {};

template<typename StrTy, typename Ty>
struct arg_store_type<StrTy, Ty*> {
    using type = Ty*;
};
template<typename StrTy, typename CharT, typename Traits>
struct arg_store_type<StrTy, std::basic_string_view<CharT, Traits>> {
    using type = std::basic_string_view<CharT>;
};
template<typename StrTy, typename CharT, typename Traits, typename Alloc>
struct arg_store_type<StrTy, std::basic_string<CharT, Traits, Alloc>> {
    using type = std::basic_string_view<CharT>;
};
}  // namespace detail

template<typename Ty>
using arg_type_index = detail::arg_type_index<reduce_type_t<Ty>>;

template<typename StrTy, typename Ty>
using arg_store_type_t = typename detail::arg_store_type<StrTy, reduce_type_t<Ty>>::type;

template<typename StrTy, typename Ty>
using arg_size = uxs::size_of<arg_store_type_t<StrTy, Ty>>;

template<typename StrTy, typename Ty>
using arg_alignment = std::alignment_of<arg_store_type_t<StrTy, Ty>>;

template<typename StrTy, std::size_t, typename...>
struct arg_store_size_evaluator;
template<typename StrTy, std::size_t Size>
struct arg_store_size_evaluator<StrTy, Size> : std::integral_constant<std::size_t, Size> {};
template<typename StrTy, std::size_t Size, typename Ty, typename... Rest>
struct arg_store_size_evaluator<StrTy, Size, Ty, Rest...>
    : std::integral_constant<
          std::size_t,
          arg_store_size_evaluator<StrTy,
                                   uxs::align_up<arg_alignment<StrTy, Ty>::value>::template type<Size>::value +
                                       arg_size<StrTy, Ty>::value,
                                   Rest...>::value> {};

template<typename StrTy, typename...>
struct arg_store_alignment_evaluator;
template<typename StrTy, typename Ty>
struct arg_store_alignment_evaluator<StrTy, Ty> : arg_alignment<StrTy, Ty> {};
template<typename StrTy, typename Ty1, typename Ty2, typename... Rest>
struct arg_store_alignment_evaluator<StrTy, Ty1, Ty2, Rest...>
    : std::conditional<(arg_alignment<StrTy, Ty1>::value > arg_alignment<StrTy, Ty2>::value),
                       arg_store_alignment_evaluator<StrTy, Ty1, Rest...>,
                       arg_store_alignment_evaluator<StrTy, Ty2, Rest...>>::type {};

template<typename StrTy, typename Ty>
struct arg_fmt_func_t {
    static void func(StrTy& s, const void* val, fmt_opts& fmt) {
        to_basic_string(s, *static_cast<const Ty*>(val), fmt);
    }
};

template<typename StrTy, typename... Args>
class arg_store {
 public:
    using char_type = typename StrTy::value_type;
    static const std::size_t arg_count = sizeof...(Args);

    arg_store& operator=(const arg_store&) = delete;

    CONSTEXPR explicit arg_store(const Args&... args) noexcept {
        store_values(0, arg_count * sizeof(unsigned), args...);
    }
    CONSTEXPR const void* data() const noexcept { return data_; }

 private:
    static const std::size_t storage_size =
        arg_store_size_evaluator<StrTy, arg_count * sizeof(unsigned), Args...>::value;
    static const std::size_t storage_alignment = arg_store_alignment_evaluator<StrTy, unsigned, Args...>::value;
    alignas(storage_alignment) std::uint8_t data_[storage_size];

    template<typename Ty>
    CONSTEXPR static void store_value(
        const Ty& v, std::enable_if_t<(arg_type_index<Ty>::value < type_index::pointer), void*> data) noexcept {
        static_assert(arg_type_index<Ty>::value != type_index::character || sizeof(Ty) <= sizeof(char_type),
                      "inconsistent character argument type");
        ::new (data) arg_store_type_t<StrTy, Ty>(v);
    }

    template<typename Ty>
    CONSTEXPR static void store_value(
        const Ty& v, std::enable_if_t<(arg_type_index<Ty>::value == type_index::custom), void*> data) noexcept {
        static_assert(has_formatter<reduce_type_t<Ty>, StrTy>::value, "value of this type cannot be formatted");
        ::new (data) custom_arg_handle<StrTy>(&v, &arg_fmt_func_t<StrTy, reduce_type_t<Ty>>::func);
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
        using Ty = std::basic_string_view<CharT, Traits>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    template<typename CharT, typename Traits, typename Alloc>
    CONSTEXPR static void store_value(const std::basic_string<CharT, Traits, Alloc>& s, void* data) noexcept {
        using Ty = std::basic_string_view<CharT, Traits>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    CONSTEXPR void store_values(std::size_t i, std::size_t offset) noexcept {}

    template<typename Ty, typename... Ts>
    CONSTEXPR void store_values(std::size_t i, std::size_t offset, const Ty& v, const Ts&... other) noexcept {
        offset = uxs::align_up<arg_alignment<StrTy, Ty>::value>::value(offset);
        ::new (reinterpret_cast<unsigned*>(&data_) + i) unsigned(static_cast<unsigned>(offset << 8) |
                                                                 static_cast<unsigned>(arg_type_index<Ty>::value));
        store_value(v, &data_[offset]);
        store_values(i + 1, offset + arg_size<StrTy, Ty>::value, other...);
    }
};

template<typename StrTy>
class arg_store<StrTy> {
 public:
    using char_type = typename StrTy::value_type;
    static const std::size_t arg_count = 0;
    arg_store& operator=(const arg_store&) = delete;
    CONSTEXPR arg_store() noexcept = default;
    CONSTEXPR const void* data() const noexcept { return nullptr; }
};

template<typename StrTy>
class arg_list {
 public:
    template<typename... Args>
    CONSTEXPR arg_list(const arg_store<StrTy, Args...>& store) noexcept
        : data_(store.data()), size_(arg_store<StrTy, Args...>::arg_count) {}

    CONSTEXPR std::size_t size() const noexcept { return size_; }
    CONSTEXPR bool empty() const noexcept { return !size_; }
    CONSTEXPR type_index index(std::size_t i) const noexcept {
        return static_cast<type_index>(static_cast<const unsigned*>(data_)[i] & 0xff);
    }
    CONSTEXPR uxs::span<const unsigned> metadata() const noexcept {
        return uxs::as_span(static_cast<const unsigned*>(data_), size_);
    }
    CONSTEXPR const void* data(std::size_t i) const noexcept {
        return static_cast<const std::uint8_t*>(data_) + (static_cast<const unsigned*>(data_)[i] >> 8);
    }

 private:
    const void* data_;
    std::size_t size_;
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

template<typename StrTy>
UXS_EXPORT StrTy& vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, arg_list<StrTy> args,
                          const std::locale* p_loc = nullptr);

}  // namespace sfmt

template<typename StrTy>
using basic_format_args = sfmt::arg_list<StrTy>;
using format_args = basic_format_args<membuffer>;
using wformat_args = basic_format_args<wmembuffer>;

template<typename StrTy = membuffer, typename... Args>
NODISCARD CONSTEXPR sfmt::arg_store<StrTy, Args...> make_format_args(const Args&... args) noexcept {
    return sfmt::arg_store<StrTy, Args...>{args...};
}

template<typename... Args>
NODISCARD CONSTEXPR sfmt::arg_store<wmembuffer, Args...> make_wformat_args(const Args&... args) noexcept {
    return sfmt::arg_store<wmembuffer, Args...>{args...};
}

template<typename CharT, typename... Args>
class basic_format_string {
 public:
    template<typename Ty,
             typename = std::enable_if_t<std::is_convertible<const Ty&, std::basic_string_view<CharT>>::value>>
    CONSTEVAL basic_format_string(const Ty& fmt) noexcept : fmt_(fmt) {
#if defined(HAS_CONSTEVAL)
        constexpr std::array<unsigned, sizeof...(Args)> args_metadata = {
            static_cast<unsigned>(sfmt::arg_type_index<Args>::value)...};
        sfmt::parse_format<CharT>(fmt_, args_metadata, [](auto&&...) constexpr {}, [](auto&&...) constexpr {});
#endif  // defined(HAS_CONSTEVAL)
    }
    CONSTEXPR std::basic_string_view<CharT> get() const noexcept { return fmt_; }

 private:
    std::basic_string_view<CharT> fmt_;
};

template<typename... Args>
using format_string = basic_format_string<char, type_identity_t<Args>...>;
template<typename... Args>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Args>...>;

// ---- basic_vformat

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, basic_format_args<StrTy> args) {
    return sfmt::vformat(s, fmt, args);
}

template<typename StrTy>
StrTy& basic_vformat(StrTy& s, const std::locale& loc, std::basic_string_view<typename StrTy::value_type> fmt,
                     basic_format_args<StrTy> args) {
    return sfmt::vformat(s, fmt, args, &loc);
}

// ---- basic_format

template<typename StrTy, typename... Args>
StrTy& basic_format(StrTy& s, basic_format_string<typename StrTy::value_type, Args...> fmt, const Args&... args) {
    return basic_vformat(s, fmt.get(), make_format_args<StrTy>(args...));
}

template<typename StrTy, typename... Args>
StrTy& basic_format(StrTy& s, const std::locale& loc, basic_format_string<typename StrTy::value_type, Args...> fmt,
                    const Args&... args) {
    return basic_vformat(s, loc, fmt.get(), make_format_args<StrTy>(args...));
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
