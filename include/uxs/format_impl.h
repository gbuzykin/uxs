#pragma once

#include "format.h"

namespace uxs {
namespace sfmt {

template<typename FmtCtx>
unsigned get_arg_integer_value(basic_format_arg<FmtCtx> arg) {
    switch (arg.index()) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        ty val = *arg.template get<ty>(); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        return static_cast<unsigned>(val); \
    } break;
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        return static_cast<unsigned>(*arg.template get<ty>()); \
    } break;
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int32_t)
        UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(std::int64_t)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint32_t)
        UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(std::uint64_t)
#undef UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE
#undef UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE
        default: UNREACHABLE_CODE;
    }
}

template<typename FmtCtx>
void vformat(FmtCtx ctx, std::basic_string_view<typename FmtCtx::char_type> fmt) {
    using char_type = typename FmtCtx::char_type;
    parse_format<char_type>(
        fmt, ctx.args().metadata(), [&ctx](const char_type* p0, const char_type* p) { ctx.out().append(p0, p); },
        [&ctx](arg_specs& specs) {
            if (!!(specs.flags & parse_flags::dynamic_width)) {
                specs.fmt.width = get_arg_integer_value(ctx.arg(specs.n_width_arg));
            }
            if (!!(specs.flags & parse_flags::dynamic_prec)) {
                specs.fmt.prec = get_arg_integer_value(ctx.arg(specs.n_prec_arg));
            }
            auto arg = ctx.arg(specs.n_arg);
            switch (arg.index()) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        ctx.format_arg(*arg.template get<ty>(), specs.fmt); \
    } break;
                UXS_FMT_FORMAT_ARG_VALUE(bool)
                UXS_FMT_FORMAT_ARG_VALUE(char_type)
                UXS_FMT_FORMAT_ARG_VALUE(std::int32_t)
                UXS_FMT_FORMAT_ARG_VALUE(std::int64_t)
                UXS_FMT_FORMAT_ARG_VALUE(std::uint32_t)
                UXS_FMT_FORMAT_ARG_VALUE(std::uint64_t)
                UXS_FMT_FORMAT_ARG_VALUE(float)
                UXS_FMT_FORMAT_ARG_VALUE(double)
                UXS_FMT_FORMAT_ARG_VALUE(long double)
                UXS_FMT_FORMAT_ARG_VALUE(const void*)
                UXS_FMT_FORMAT_ARG_VALUE(const char_type*)
                UXS_FMT_FORMAT_ARG_VALUE(std::basic_string_view<char_type>)
                UXS_FMT_FORMAT_ARG_VALUE(custom_arg_handle<FmtCtx>)
#undef UXS_FMT_FORMAT_ARG_VALUE
            }
        });
}

}  // namespace sfmt
}  // namespace uxs
