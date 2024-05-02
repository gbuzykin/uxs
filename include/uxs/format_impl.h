#pragma once

#include "format.h"

namespace uxs {
namespace sfmt {

template<typename Ty>
Ty get_arg_value(const void* data) {
    return *static_cast<const Ty*>(data);
}

inline unsigned get_arg_integer_value(type_index index, const void* data) {
    switch (index) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty, index) \
    case index: { \
        ty val = get_arg_value<ty>(data); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        return static_cast<unsigned>(val); \
    } break;
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty, index) \
    case index: { \
        return static_cast<unsigned>(get_arg_value<ty>(data)); \
    } break;
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int32_t, type_index::integer)
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int64_t, type_index::long_integer)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint32_t, type_index::unsigned_integer)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint64_t, type_index::unsigned_long_integer)
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
        default: UNREACHABLE_CODE;
    }
}

template<typename StrTy>
StrTy& vformat(StrTy& s, std::basic_string_view<typename StrTy::value_type> fmt, basic_format_args<StrTy> args,
               const std::locale* p_loc) {
    using char_type = typename StrTy::value_type;
    parse_format<char_type>(
        fmt, args.metadata(), [&s](const char_type* p0, const char_type* p) { s.append(p0, p); },
        [&s, &args, p_loc](arg_specs& specs) {
            if (!!(specs.flags & parse_flags::dynamic_width)) {
                specs.fmt.width = get_arg_integer_value(args.index(specs.n_width_arg), args.data(specs.n_width_arg));
            }
            if (!!(specs.flags & parse_flags::dynamic_prec)) {
                specs.fmt.prec = get_arg_integer_value(args.index(specs.n_prec_arg), args.data(specs.n_prec_arg));
            }
            if (!!(specs.fmt.flags & fmt_flags::localize)) {
                static const std::locale default_locale{};
                specs.fmt.loc = p_loc ? p_loc : &default_locale;
            }
            switch (args.index(specs.n_arg)) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty, index) \
    case index: { \
        formatter<ty, char_type>{}.format(s, get_arg_value<ty>(args.data(specs.n_arg)), specs.fmt); \
    } break;
                UXS_FMT_FORMAT_ARG_VALUE(bool, type_index::boolean)
                UXS_FMT_FORMAT_ARG_VALUE(char_type, type_index::character)
                UXS_FMT_FORMAT_ARG_VALUE(std::int32_t, type_index::integer)
                UXS_FMT_FORMAT_ARG_VALUE(std::int64_t, type_index::long_integer)
                UXS_FMT_FORMAT_ARG_VALUE(std::uint32_t, type_index::unsigned_integer)
                UXS_FMT_FORMAT_ARG_VALUE(std::uint64_t, type_index::unsigned_long_integer)
                UXS_FMT_FORMAT_ARG_VALUE(float, type_index::single_precision)
                UXS_FMT_FORMAT_ARG_VALUE(double, type_index::double_precision)
                UXS_FMT_FORMAT_ARG_VALUE(long double, type_index::long_double_precision)
                UXS_FMT_FORMAT_ARG_VALUE(const void*, type_index::pointer)
                UXS_FMT_FORMAT_ARG_VALUE(const char_type*, type_index::z_string)
                UXS_FMT_FORMAT_ARG_VALUE(std::basic_string_view<char_type>, type_index::string)
#undef UXS_FMT_FORMAT_ARG_VALUE
                case type_index::custom: {
                    const auto* custom = static_cast<const custom_arg_handle<StrTy>*>(args.data(specs.n_arg));
                    custom->print_fn(s, custom->val, specs.fmt);
                } break;
            }
        });
    return s;
}

}  // namespace sfmt
}  // namespace uxs
