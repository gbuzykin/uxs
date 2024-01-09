#pragma once

#include "alignment.h"
#include "io/iobuf.h"
#include "stringcvt.h"

namespace uxs {

class UXS_EXPORT_ALL_STUFF_FOR_GNUC format_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit format_error(const char* message);
    UXS_EXPORT explicit format_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};

namespace sfmt {

enum class type_id : uint8_t {
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

template<typename StrTy>
struct arg_custom_value {
    arg_custom_value(const void* v, void (*fn)(StrTy&, const void*, fmt_opts&)) noexcept : val(v), print_fn(fn) {}
    const void* val;                                   // value pointer
    void (*print_fn)(StrTy&, const void*, fmt_opts&);  // printing function pointer
};

namespace detail {
template<typename Ty>
struct arg_type_id : std::integral_constant<type_id, type_id::custom> {};

template<typename StrTy, typename Ty>
struct arg_store_type {
    using type = arg_custom_value<StrTy>;
};

#define UXS_FMT_DECLARE_ARG_TYPE_ID(ty, store_ty, id) \
    template<> \
    struct arg_type_id<ty> : std::integral_constant<type_id, id> {}; \
    template<typename StrTy> \
    struct arg_store_type<StrTy, ty> { \
        using type = store_ty; \
    };
UXS_FMT_DECLARE_ARG_TYPE_ID(bool, bool, type_id::boolean)
UXS_FMT_DECLARE_ARG_TYPE_ID(char, typename StrTy::value_type, type_id::character)
UXS_FMT_DECLARE_ARG_TYPE_ID(wchar_t, typename StrTy::value_type, type_id::character)
UXS_FMT_DECLARE_ARG_TYPE_ID(int32_t, int32_t, type_id::integer)
UXS_FMT_DECLARE_ARG_TYPE_ID(int64_t, int64_t, type_id::long_integer)
UXS_FMT_DECLARE_ARG_TYPE_ID(uint32_t, uint32_t, type_id::unsigned_integer)
UXS_FMT_DECLARE_ARG_TYPE_ID(uint64_t, uint64_t, type_id::unsigned_long_integer)
UXS_FMT_DECLARE_ARG_TYPE_ID(float, float, type_id::single_precision)
UXS_FMT_DECLARE_ARG_TYPE_ID(double, double, type_id::double_precision)
UXS_FMT_DECLARE_ARG_TYPE_ID(long double, long double, type_id::long_double_precision)
#undef UXS_FMT_DECLARE_ARG_TYPE_ID

template<typename Ty>
struct arg_type_id<Ty*>
    : std::integral_constant<type_id, is_character<Ty>::value ? type_id::z_string : type_id::pointer> {};
template<typename CharT, typename Traits>
struct arg_type_id<std::basic_string_view<CharT, Traits>> : std::integral_constant<type_id, type_id::string> {};
template<typename CharT, typename Traits, typename Alloc>
struct arg_type_id<std::basic_string<CharT, Traits, Alloc>> : std::integral_constant<type_id, type_id::string> {};

template<typename StrTy, typename Ty>
struct arg_store_type<StrTy, Ty*> {
    using type = void*;
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
using arg_type_id = detail::arg_type_id<scvt::reduce_type_t<Ty>>;

template<typename StrTy, typename Ty>
using arg_store_type_t = typename detail::arg_store_type<StrTy, scvt::reduce_type_t<Ty>>::type;

template<typename StrTy, typename Ty>
using arg_size = uxs::size_of<arg_store_type_t<StrTy, Ty>>;

template<typename StrTy, typename Ty>
using arg_alignment = std::alignment_of<arg_store_type_t<StrTy, Ty>>;

template<typename StrTy, size_t, typename...>
struct arg_store_size_evaluator;
template<typename StrTy, size_t Size>
struct arg_store_size_evaluator<StrTy, Size> : std::integral_constant<size_t, Size> {};
template<typename StrTy, size_t Size, typename Ty, typename... Rest>
struct arg_store_size_evaluator<StrTy, Size, Ty, Rest...>
    : std::integral_constant<
          size_t, arg_store_size_evaluator<StrTy,
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
        uxs::to_basic_string(s, *static_cast<const Ty*>(val), fmt);
    }
};

template<typename StrTy, typename... Args>
class arg_store {
 public:
    using char_type = typename StrTy::value_type;
    static const size_t arg_count = sizeof...(Args);
    explicit arg_store(const Args&... args) noexcept { store_values(0, arg_count * sizeof(unsigned), args...); }
    const void* data() const noexcept { return reinterpret_cast<const void*>(&storage_); }

 private:
    static const size_t storage_size = arg_store_size_evaluator<StrTy, arg_count * sizeof(unsigned), Args...>::value;
    static const size_t storage_alignment = arg_store_alignment_evaluator<StrTy, unsigned, Args...>::value;
    typename std::aligned_storage<storage_size, storage_alignment>::type storage_;

    template<typename Ty>
    static void store_value(const Ty& v,
                            std::enable_if_t<(arg_type_id<Ty>::value < type_id::pointer), void*> data) noexcept {
        static_assert(arg_type_id<Ty>::value != type_id::character || sizeof(Ty) <= sizeof(char_type),
                      "inconsistent character argument type");
        ::new (data) arg_store_type_t<StrTy, Ty>(v);
    }

    template<typename Ty>
    static void store_value(const Ty& v,
                            std::enable_if_t<(arg_type_id<Ty>::value == type_id::custom), void*> data) noexcept {
        static_assert(has_formatter<scvt::reduce_type_t<Ty>, StrTy>::value, "value of this type cannot be formatted");
        ::new (data) arg_custom_value<StrTy>(&v, &arg_fmt_func_t<StrTy, scvt::reduce_type_t<Ty>>::func);
    }

    template<typename Ty>
    static void store_value(Ty* v, void* data) noexcept {
        static_assert(!is_character<Ty>::value || std::is_same<std::remove_cv_t<Ty>, char_type>::value,
                      "inconsistent string argument type");
        ::new (data) const void*(v);
    }

    static void store_value(std::nullptr_t, void* data) noexcept { ::new (data) const void*(nullptr); }

    template<typename CharT, typename Traits>
    static void store_value(const std::basic_string_view<CharT, Traits>& s, void* data) noexcept {
        using Ty = std::basic_string_view<CharT, Traits>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    template<typename CharT, typename Traits, typename Alloc>
    static void store_value(const std::basic_string<CharT, Traits, Alloc>& s, void* data) noexcept {
        using Ty = std::basic_string_view<CharT, Traits>;
        static_assert(std::is_same<CharT, char_type>::value, "inconsistent string argument type");
        static_assert(std::is_trivially_copyable<Ty>::value && std::is_trivially_destructible<Ty>::value,
                      "std::basic_string_view<> is assumed to be trivially copyable and destructible");
        ::new (data) Ty(s.data(), s.size());
    }

    void store_values(size_t i, size_t offset) noexcept {}

    template<typename Ty, typename... Ts>
    void store_values(size_t i, size_t offset, const Ty& v, const Ts&... other) noexcept {
        offset = uxs::align_up<arg_alignment<StrTy, Ty>::value>::value(offset);
        ::new (reinterpret_cast<unsigned*>(&storage_) + i) unsigned(static_cast<unsigned>(offset << 8) |
                                                                    static_cast<unsigned>(arg_type_id<Ty>::value));
        store_value(v, reinterpret_cast<uint8_t*>(&storage_) + offset);
        store_values(i + 1, offset + arg_size<StrTy, Ty>::value, other...);
    }
};

template<typename StrTy>
class arg_store<StrTy> {
 public:
    using char_type = typename StrTy::value_type;
    static const size_t arg_count = 0;
    const void* data() const noexcept { return &storage_; }

 private:
    uint8_t storage_ = 0;
};

template<typename StrTy>
class arg_list {
 public:
    template<typename... Args>
    arg_list(const arg_store<StrTy, Args...>& store) noexcept
        : data_(store.data()), size_(arg_store<StrTy, Args...>::arg_count) {}

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return !size_; }
    type_id type(size_t i) const noexcept {
        return static_cast<type_id>(static_cast<const unsigned*>(data_)[i] & 0xff);
    }
    const void* data(size_t i) const noexcept {
        return static_cast<const uint8_t*>(data_) + (static_cast<const unsigned*>(data_)[i] >> 8);
    }

 private:
    const void* data_;
    size_t size_;
};

enum class parse_flags : unsigned {
    none = 0,
    spec_integer = 1,
    spec_real = 2,
    spec_character = 3,
    spec_pointer = 4,
    spec_string = 5,
    spec_mask = 0xff,
    prec_specified = 0x100,
    dynamic_width = 0x200,
    dynamic_prec = 0x400,
    use_locale = 0x800,
    arg_num_specified = 0x1000,
    width_arg_num_specified = 0x2000,
    prec_arg_num_specified = 0x4000,
};
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(parse_flags, unsigned);

struct arg_specs {
    fmt_opts fmt;
    parse_flags flags = parse_flags::none;
    unsigned n_arg = 0;
    unsigned n_width_arg = 0;
    unsigned n_prec_arg = 0;
};

struct meta_tbl_t {
    enum code_t {
        left = 0,
        internal,
        right,
        open_brace,
        dot,
        digit19,
        plus,
        space,
        minus,
        sharp,
        zero,
        locale,
        decimal,
        binary,
        binary_cap,
        octal,
        hex,
        hex_cap,
        fixed,
        fixed_cap,
        scientific,
        scientific_cap,
        general,
        general_cap,
        hex_real,
        hex_real_cap,
        pointer,
        pointer_cap,
        character,
        string,
        close_brace,
        other
    };
    std::array<uint8_t, 128> tbl;
    CONSTEXPR meta_tbl_t() : tbl() {
        for (unsigned ch = 0; ch < tbl.size(); ++ch) { tbl[ch] = code_t::other; }
        tbl['<'] = code_t::left, tbl['^'] = code_t::internal, tbl['>'] = code_t::right;
        tbl['{'] = code_t::open_brace, tbl['}'] = code_t::close_brace, tbl['.'] = code_t::dot;
        for (unsigned ch = '1'; ch <= '9'; ++ch) { tbl[ch] = code_t::digit19; }
        tbl['+'] = code_t::plus, tbl[' '] = code_t::space, tbl['-'] = code_t::minus;
        tbl['#'] = code_t::sharp, tbl['0'] = code_t::zero, tbl['L'] = code_t::locale;
        tbl['d'] = code_t::decimal, tbl['b'] = code_t::binary, tbl['o'] = code_t::octal, tbl['x'] = code_t::hex;
        tbl['B'] = code_t::binary_cap, tbl['X'] = code_t::hex_cap;
        tbl['f'] = code_t::fixed, tbl['e'] = code_t::scientific, tbl['g'] = code_t::general;
        tbl['F'] = code_t::fixed_cap, tbl['E'] = code_t::scientific_cap, tbl['G'] = code_t::general_cap;
        tbl['a'] = code_t::hex_real, tbl['A'] = code_t::hex_real_cap;
        tbl['p'] = code_t::pointer, tbl['P'] = code_t::pointer_cap;
        tbl['c'] = code_t::character, tbl['s'] = code_t::string;
    }
};

#if __cplusplus < 201703L
extern UXS_EXPORT const meta_tbl_t g_meta_tbl;
#else   // __cplusplus < 201703L
static constexpr meta_tbl_t g_meta_tbl{};
#endif  // __cplusplus < 201703L

template<typename CharT, typename Ty>
CONSTEXPR const CharT* accum_num(const CharT* p, const CharT* last, Ty& num) noexcept {
    for (unsigned dig = 0; p != last && (dig = dig_v(*p)) < 10; ++p) { num = 10 * num + dig; }
    return p;
}

template<typename CharT>
CONSTEXPR const CharT* parse_arg_spec(const CharT* p, const CharT* last, arg_specs& specs) noexcept {
    assert(p != last && *p != '}');

    unsigned dig = 0;
    if ((dig = dig_v(*p)) < 10) {
        specs.flags |= parse_flags::arg_num_specified;
        specs.n_arg = dig;
        p = accum_num(++p, last, specs.n_arg);
        if (p == last) { return nullptr; }
        if (*p == '}') { return p; }
    }
    if (*p != ':' || ++p == last) { return nullptr; }

    if (p + 1 != last) {
        switch (*(p + 1)) {  // adjustment with fill character
            case '<': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::left, p += 2; break;
            case '^': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::internal, p += 2; break;
            case '>': specs.fmt.fill = *p, specs.fmt.flags |= fmt_flags::right, p += 2; break;
            default: break;
        }
    }

    enum class state_t { start = 0, sign, alternate, leading_zeroes, width, precision, locale, type, finish };

    state_t state = state_t::start;
    while (p != last) {
        auto ch = static_cast<typename std::make_unsigned<CharT>::type>(*p++);
        if (ch >= g_meta_tbl.tbl.size()) { return nullptr; }
        switch (g_meta_tbl.tbl[ch]) {
#define UXS_FMT_SPECIFIER_CASE(next_state, action) \
    if (state < next_state) { \
        state = next_state; \
        action; \
        break; \
    } \
    return nullptr;

            // adjustment
            case meta_tbl_t::left: UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::left; });
            case meta_tbl_t::internal:
                UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::internal; });
            case meta_tbl_t::right: UXS_FMT_SPECIFIER_CASE(state_t::sign, { specs.fmt.flags |= fmt_flags::right; });

            // sign specifiers
            case meta_tbl_t::plus:
                UXS_FMT_SPECIFIER_CASE(state_t::alternate, { specs.fmt.flags |= fmt_flags::sign_pos; });
            case meta_tbl_t::space:
                UXS_FMT_SPECIFIER_CASE(state_t::alternate, { specs.fmt.flags |= fmt_flags::sign_align; });
            case meta_tbl_t::minus: UXS_FMT_SPECIFIER_CASE(state_t::alternate, {});

            // alternate
            case meta_tbl_t::sharp:
                UXS_FMT_SPECIFIER_CASE(state_t::leading_zeroes, { specs.fmt.flags |= fmt_flags::alternate; });

            // leading zeroes
            case meta_tbl_t::zero:
                UXS_FMT_SPECIFIER_CASE(state_t::width, {
                    if (!(specs.fmt.flags & fmt_flags::adjust_field)) { specs.fmt.flags |= fmt_flags::leading_zeroes; }
                });

            // width
            case meta_tbl_t::open_brace:
                UXS_FMT_SPECIFIER_CASE(state_t::precision, {
                    if (p == last) { return nullptr; }
                    specs.flags |= parse_flags::dynamic_width;
                    if (*p == '}') {
                        ++p;
                        break;
                    } else if ((dig = dig_v(*p)) < 10) {
                        specs.flags |= parse_flags::width_arg_num_specified;
                        specs.n_width_arg = dig;
                        p = accum_num(++p, last, specs.n_width_arg);
                        if (p != last && *p++ == '}') { break; }
                    }
                    return nullptr;
                });
            case meta_tbl_t::digit19:
                UXS_FMT_SPECIFIER_CASE(state_t::precision, {
                    specs.fmt.width = static_cast<unsigned>(*(p - 1) - '0');
                    p = accum_num(p, last, specs.fmt.width);
                });

            // precision
            case meta_tbl_t::dot:
                UXS_FMT_SPECIFIER_CASE(state_t::locale, {
                    if (p == last) { return nullptr; }
                    specs.flags |= parse_flags::prec_specified;
                    if ((dig = dig_v(*p)) < 10) {
                        specs.fmt.prec = dig;
                        p = accum_num(++p, last, specs.fmt.prec);
                        break;
                    } else if (*p == '{' && ++p != last) {
                        specs.flags |= parse_flags::dynamic_prec;
                        if (*p == '}') {
                            ++p;
                            break;
                        } else if ((dig = dig_v(*p)) < 10) {
                            specs.flags |= parse_flags::prec_arg_num_specified;
                            specs.n_prec_arg = dig;
                            p = accum_num(++p, last, specs.n_prec_arg);
                            if (p != last && *p++ == '}') { break; }
                        }
                    }
                    return nullptr;
                });

            // locale
            case meta_tbl_t::locale: UXS_FMT_SPECIFIER_CASE(state_t::type, { specs.flags |= parse_flags::use_locale; });

            // types
            case meta_tbl_t::decimal:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.flags |= parse_flags::spec_integer; });

            case meta_tbl_t::binary:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_integer;
                    specs.fmt.flags |= fmt_flags::bin;
                });
            case meta_tbl_t::binary_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_integer;
                    specs.fmt.flags |= fmt_flags::bin | fmt_flags::uppercase;
                });

            case meta_tbl_t::octal:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_integer;
                    specs.fmt.flags |= fmt_flags::oct;
                });

            case meta_tbl_t::hex:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_integer;
                    specs.fmt.flags |= fmt_flags::hex;
                });
            case meta_tbl_t::hex_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_integer;
                    specs.fmt.flags |= fmt_flags::hex | fmt_flags::uppercase;
                });

            case meta_tbl_t::fixed:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::fixed;
                });
            case meta_tbl_t::fixed_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::fixed | fmt_flags::uppercase;
                });

            case meta_tbl_t::scientific:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::scientific;
                });
            case meta_tbl_t::scientific_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::scientific | fmt_flags::uppercase;
                });

            case meta_tbl_t::general:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::general;
                });
            case meta_tbl_t::general_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::general | fmt_flags::uppercase;
                });

            case meta_tbl_t::hex_real:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::hex;
                });
            case meta_tbl_t::hex_real_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_real;
                    specs.fmt.flags |= fmt_flags::hex | fmt_flags::uppercase;
                });

            case meta_tbl_t::pointer:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.flags |= parse_flags::spec_pointer; });
            case meta_tbl_t::pointer_cap:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, {
                    specs.flags |= parse_flags::spec_pointer;
                    specs.fmt.flags |= fmt_flags::uppercase;
                });
            case meta_tbl_t::character:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.flags |= parse_flags::spec_character; });
            case meta_tbl_t::string:
                UXS_FMT_SPECIFIER_CASE(state_t::finish, { specs.flags |= parse_flags::spec_string; });
#undef UXS_FMT_SPECIFIER_CASE

            // end of format specifier
            case meta_tbl_t::close_brace: return p - 1;
            case meta_tbl_t::other: return nullptr;

            default: UNREACHABLE_CODE;
        }
    }

    return nullptr;
}

enum class parse_format_error_code { success = 0, invalid_format, out_of_arg_list };

template<typename CharT, typename AppendFn, typename AppendArgFn, typename GetUIntArgFn>
CONSTEXPR parse_format_error_code parse_format(std::basic_string_view<CharT> fmt, const size_t arg_count,
                                               const AppendFn& append_fn, const AppendArgFn& append_arg_fn,
                                               const GetUIntArgFn& get_uint_arg_fn) {
    unsigned n_arg_auto = 0;
    const CharT *first0 = fmt.data(), *first = first0, *last = first0 + fmt.size();
    while (first != last) {
        if (*first == '{' || *first == '}') {
            append_fn(first0, first);
            first0 = ++first;
            if (first == last) { return parse_format_error_code::invalid_format; }
            if (*(first - 1) == '{' && *first != '{') {
                arg_specs specs;
                if (*first == '}') {  // most usual `{}` specifier
                    specs.n_arg = n_arg_auto++;
                    if (specs.n_arg >= arg_count) { return parse_format_error_code::out_of_arg_list; }
                } else if ((first = parse_arg_spec(first, last, specs))) {
                    // obtain argument number
                    if (!(specs.flags & parse_flags::arg_num_specified)) { specs.n_arg = n_arg_auto++; }
                    if (specs.n_arg >= arg_count) { return parse_format_error_code::out_of_arg_list; }
                    if (!!(specs.flags & parse_flags::dynamic_width)) {
                        // obtain argument number for width
                        if (!(specs.flags & parse_flags::width_arg_num_specified)) { specs.n_width_arg = n_arg_auto++; }
                        if (specs.n_width_arg >= arg_count) { return parse_format_error_code::out_of_arg_list; }
                        specs.fmt.width = get_uint_arg_fn(specs.n_width_arg);
                    }
                    if (!!(specs.flags & parse_flags::dynamic_prec)) {
                        // obtain argument number for precision
                        if (!(specs.flags & parse_flags::prec_arg_num_specified)) { specs.n_prec_arg = n_arg_auto++; }
                        if (specs.n_prec_arg >= arg_count) { return parse_format_error_code::out_of_arg_list; }
                        specs.fmt.prec = get_uint_arg_fn(specs.n_prec_arg);
                    }
                } else {
                    return parse_format_error_code::invalid_format;
                }
                append_arg_fn(specs.n_arg, specs);
                first0 = first + 1;
            } else if (*(first - 1) != *first) {
                return parse_format_error_code::invalid_format;
            }
        }
        ++first;
    }
    append_fn(first0, last);
    return parse_format_error_code::success;
}

inline void report_format_error(const char*) {}

template<typename StrTy>
UXS_EXPORT StrTy& vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, arg_list<StrTy> args,
                          const std::locale* p_loc = nullptr);

}  // namespace sfmt

template<typename StrTy>
using basic_format_args = sfmt::arg_list<StrTy>;
using format_args = basic_format_args<membuffer>;
using wformat_args = basic_format_args<wmembuffer>;

template<typename StrTy = membuffer, typename... Args>
NODISCARD sfmt::arg_store<StrTy, Args...> make_format_args(const Args&... args) noexcept {
    return sfmt::arg_store<StrTy, Args...>{args...};
}

template<typename StrTy = wmembuffer, typename... Args>
NODISCARD sfmt::arg_store<StrTy, Args...> make_wformat_args(const Args&... args) noexcept {
    return sfmt::arg_store<StrTy, Args...>{args...};
}

template<typename CharT>
struct basic_runtime_string {
    std::basic_string_view<CharT> s;
};

template<typename Str, typename = std::enable_if_t<
                           std::is_convertible<const Str&, std::basic_string_view<typename Str::value_type>>::value>>
NODISCARD basic_runtime_string<typename Str::value_type> make_runtime_string(const Str& s) noexcept {
    return basic_runtime_string<typename Str::value_type>{s};
}

template<typename CharT>
NODISCARD basic_runtime_string<CharT> make_runtime_string(const CharT* s) noexcept {
    return basic_runtime_string<CharT>{s};
}

template<typename CharT, typename... Args>
class basic_format_string {
 public:
    template<typename Ty,
             typename = std::enable_if_t<std::is_convertible<const Ty&, std::basic_string_view<CharT>>::value>>
    CONSTEVAL basic_format_string(const Ty& fmt) noexcept : checked(fmt) {
#if defined(HAS_CONSTEVAL)
        const std::array<sfmt::type_id, sizeof...(Args)> arg_type_ids = {sfmt::arg_type_id<Args>::value...};
        const auto error_code = sfmt::parse_format<CharT>(
            checked, sizeof...(Args), [](const CharT*, const CharT*) constexpr {},
            [&arg_type_ids](unsigned n_arg, sfmt::arg_specs& specs) constexpr {
                const sfmt::type_id id = arg_type_ids[n_arg];
                const sfmt::parse_flags flag = specs.flags & sfmt::parse_flags::spec_mask;
                auto unexpected_prec = []() { sfmt::report_format_error("unexpected precision specified"); };
                auto signed_needed = []() { sfmt::report_format_error("argument format requires signed argument"); };
                auto numeric_needed = []() { sfmt::report_format_error("argument format requires numeric argument"); };
                if (flag == sfmt::parse_flags::none) {
                    if (!!(specs.flags & sfmt::parse_flags::prec_specified) &&
                        (id < sfmt::type_id::single_precision || id > sfmt::type_id::long_double_precision) &&
                        id != sfmt::type_id::z_string && id != sfmt::type_id::string) {
                        unexpected_prec();
                    }
                    if (!!(specs.fmt.flags & fmt_flags::sign_field) &&
                        (id < sfmt::type_id::integer || id > sfmt::type_id::long_integer) &&
                        (id < sfmt::type_id::single_precision || id > sfmt::type_id::long_double_precision)) {
                        signed_needed();
                    }
                    if (!!(specs.fmt.flags & fmt_flags::leading_zeroes) &&
                        (id < sfmt::type_id::integer || id > sfmt::type_id::pointer)) {
                        numeric_needed();
                    }
                    return;
                } else if (flag == sfmt::parse_flags::spec_integer) {
                    if (!!(specs.flags & sfmt::parse_flags::prec_specified)) { unexpected_prec(); }
                    if (id <= sfmt::type_id::unsigned_long_integer) {
                        if (!!(specs.fmt.flags & fmt_flags::sign_field) &&
                            (id < sfmt::type_id::character || id > sfmt::type_id::long_integer)) {
                            signed_needed();
                        }
                        return;
                    }
                } else if (flag == sfmt::parse_flags::spec_real) {
                    if (id >= sfmt::type_id::single_precision && id <= sfmt::type_id::long_double_precision) { return; }
                } else if (flag == sfmt::parse_flags::spec_character) {
                    if (!!(specs.flags & sfmt::parse_flags::prec_specified)) { unexpected_prec(); }
                    if (!!(specs.fmt.flags & fmt_flags::sign_field)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::leading_zeroes)) { numeric_needed(); }
                    if (id >= sfmt::type_id::character && id <= sfmt::type_id::unsigned_long_integer) { return; }
                } else if (flag == sfmt::parse_flags::spec_pointer) {
                    if (!!(specs.flags & sfmt::parse_flags::prec_specified)) { unexpected_prec(); }
                    if (!!(specs.fmt.flags & fmt_flags::sign_field)) { signed_needed(); }
                    if (id == sfmt::type_id::pointer) { return; }
                } else if (flag == sfmt::parse_flags::spec_string) {
                    if (!!(specs.fmt.flags & fmt_flags::sign_field)) { signed_needed(); }
                    if (!!(specs.fmt.flags & fmt_flags::leading_zeroes)) { numeric_needed(); }
                    if (id == sfmt::type_id::boolean || id == sfmt::type_id::z_string || id == sfmt::type_id::string) {
                        return;
                    }
                }
                sfmt::report_format_error("invalid argument format specifier");
            },
            [&arg_type_ids](unsigned n_arg) constexpr->unsigned {
                if (arg_type_ids[n_arg] < sfmt::type_id::integer ||
                    arg_type_ids[n_arg] > sfmt::type_id::unsigned_long_integer) {
                    sfmt::report_format_error("argument is not an integer");
                }
                return 0;
            });
        if (error_code == sfmt::parse_format_error_code::success) {
        } else if (error_code == sfmt::parse_format_error_code::out_of_arg_list) {
            sfmt::report_format_error("out of argument list");
        } else {
            sfmt::report_format_error("invalid specifier syntax");
        }
#endif  // defined(HAS_CONSTEVAL)
    }
    basic_format_string(basic_runtime_string<CharT> fmt) noexcept : checked(fmt.s) {}
    std::basic_string_view<CharT> get() const noexcept { return checked; }

 private:
    std::basic_string_view<CharT> checked;
};

template<typename... Args>
using format_string = basic_format_string<char, type_identity_t<Args>...>;
template<typename... Args>
using wformat_string = basic_format_string<wchar_t, type_identity_t<Args>...>;

// ---- vformat

template<typename... Args>
NODISCARD std::string vformat(std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args);
    return std::string(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::wstring vformat(std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args);
    return std::wstring(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::string vformat(const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args, &loc);
    return std::string(buf.data(), buf.size());
}

template<typename... Args>
NODISCARD std::wstring vformat(const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args, &loc);
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

template<typename... Args>
char* vformat_to(char* p, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return sfmt::vformat<membuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to(wchar_t* p, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return sfmt::vformat<wmembuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
char* vformat_to(char* p, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p);
    return sfmt::vformat<membuffer>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), buf.size(), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to(wchar_t* p, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p);
    return sfmt::vformat<wmembuffer>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to(OutputIt out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args, &loc);
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

template<typename... Args>
char* vformat_to_n(char* p, size_t n, std::string_view fmt, format_args args) {
    membuffer buf(p, p + n);
    return sfmt::vformat<membuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to_n(wchar_t* p, size_t n, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p, p + n);
    return sfmt::vformat<wmembuffer>(buf, fmt, args).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
char* vformat_to_n(char* p, size_t n, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer buf(p, p + n);
    return sfmt::vformat<membuffer>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, const std::locale& loc, std::string_view fmt, format_args args) {
    inline_dynbuffer buf;
    sfmt::vformat<membuffer>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

template<typename... Args>
wchar_t* vformat_to_n(wchar_t* p, size_t n, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer buf(p, p + n);
    return sfmt::vformat<wmembuffer>(buf, fmt, args, &loc).curr();
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt vformat_to_n(OutputIt out, size_t n, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    inline_wdynbuffer buf;
    sfmt::vformat<wmembuffer>(buf, fmt, args, &loc);
    return std::copy_n(buf.data(), std::min(buf.size(), n), std::move(out));
}

// ---- format_to_n

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, fmt.get(), make_wformat_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const char&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, const std::locale& loc, format_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_format_args(args...));
}

template<typename OutputIt, typename... Args,
         typename = std::enable_if_t<is_output_iterator<OutputIt, const wchar_t&>::value>>
OutputIt format_to_n(OutputIt out, size_t n, const std::locale& loc, wformat_string<Args...> fmt, const Args&... args) {
    return vformat_to_n(std::move(out), n, loc, fmt.get(), make_wformat_args(args...));
}

// ---- vprint

template<typename CharT, typename Traits = std::char_traits<CharT>>
basic_iobuf<CharT>& print_quoted_text(basic_iobuf<CharT>& out, std::basic_string_view<CharT, Traits> text) {
    const CharT *p1 = text.data(), *pend = text.data() + text.size();
    out.put('\"');
    for (const CharT* p2 = text.data(); p2 != pend; ++p2) {
        std::string_view esc;
        switch (*p2) {
            case '\"': esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\a': esc = "\\a"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\v': esc = "\\v"; break;
            default: continue;
        }
        out.write(uxs::as_span(p1, p2 - p1));
        for (char ch : esc) { out.put(ch); }
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

    bool try_grow(size_t extra) override {
        out_.advance(this->curr() - out_.first_avail());
        if (!out_.reserve().good()) { return false; }
        this->set(out_.first_avail(), out_.last_avail());
        return this->avail() >= extra;
    }

 private:
    basic_iobuf<Ty>& out_;
};

using membuffer_for_iobuf = basic_membuffer_for_iobuf<char>;
using wmembuffer_for_iobuf = basic_membuffer_for_iobuf<wchar_t>;

template<typename... Args>
iobuf& vprint(iobuf& out, std::string_view fmt, format_args args) {
    membuffer_for_iobuf buf(out);
    sfmt::vformat<membuffer>(buf, fmt, args);
    return out;
}

template<typename... Args>
wiobuf& vprint(wiobuf& out, std::wstring_view fmt, wformat_args args) {
    wmembuffer_for_iobuf buf(out);
    sfmt::vformat<wmembuffer>(buf, fmt, args);
    return out;
}

template<typename... Args>
iobuf& vprint(iobuf& out, const std::locale& loc, std::string_view fmt, format_args args) {
    membuffer_for_iobuf buf(out);
    sfmt::vformat<membuffer>(buf, fmt, args, &loc);
    return out;
}

template<typename... Args>
wiobuf& vprint(wiobuf& out, const std::locale& loc, std::wstring_view fmt, wformat_args args) {
    wmembuffer_for_iobuf buf(out);
    sfmt::vformat<wmembuffer>(buf, fmt, args, &loc);
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
