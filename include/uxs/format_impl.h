#pragma once

#include "format.h"

namespace uxs {
namespace sfmt {

template<typename FmtCtx>
unsigned get_arg_integer_value(basic_format_arg<FmtCtx> arg, unsigned limit) {
    switch (arg.index()) {
#define UXS_FMT_ARG_SIGNED_INTEGER_VALUE_CASE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        ty val = *arg.template get<ty>(); \
        if (val < 0) { throw format_error("negative argument specified"); } \
        if (static_cast<std::make_unsigned<ty>::type>(val) > limit) { throw format_error("too large integer"); } \
        return static_cast<unsigned>(val); \
    } break;
#define UXS_FMT_ARG_UNSIGNED_INTEGER_VALUE_CASE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        ty val = *arg.template get<ty>(); \
        if (val > limit) { throw format_error("too large integer"); } \
        return static_cast<unsigned>(val); \
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
void vformat(FmtCtx ctx, typename FmtCtx::parse_context parse_ctx) {
    using parse_context = typename FmtCtx::parse_context;
    using char_type = typename FmtCtx::char_type;
    using iterator = typename parse_context::const_iterator;
    sfmt::parse_format(
        parse_ctx, [&ctx](iterator first, iterator last) { ctx.out().append(first, last); },
        [&ctx](parse_context& parse_ctx, std::size_t id, fmt_opts& specs, std::size_t width_arg_id,
               std::size_t prec_arg_id) {
            if (width_arg_id != dynamic_extent) {
                specs.width = get_arg_integer_value(ctx.arg(width_arg_id),
                                                    std::numeric_limits<decltype(specs.width)>::max());
            }
            if (prec_arg_id != dynamic_extent) {
                specs.prec = get_arg_integer_value(ctx.arg(prec_arg_id),
                                                   std::numeric_limits<decltype(specs.prec)>::max());
            }
            auto arg = ctx.arg(id);
            switch (arg.index()) {
#define UXS_FMT_FORMAT_ARG_VALUE(ty) \
    case format_arg_type_index<FmtCtx, ty>::value: { \
        ctx.format_arg(parse_ctx, *arg.template get<ty>(), specs); \
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
                UXS_FMT_FORMAT_ARG_VALUE(typename basic_format_arg<FmtCtx>::handle)
#undef UXS_FMT_FORMAT_ARG_VALUE
            }
        });
}

}  // namespace sfmt
}  // namespace uxs
