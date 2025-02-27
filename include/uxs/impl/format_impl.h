#pragma once

#include "uxs/format_base.h"

namespace uxs {
namespace sfmt {

template<typename FmtCtx>
struct arg_visitor {
    FmtCtx& ctx;
    typename FmtCtx::parse_context& parse_ctx;
    arg_visitor(FmtCtx& ctx, typename FmtCtx::parse_context& parse_ctx) noexcept : ctx(ctx), parse_ctx(parse_ctx) {}
    template<typename Ty>
    void operator()(Ty v) const {
        ctx.format_arg(parse_ctx, v);
    }
};

template<typename FmtCtx>
void vformat(FmtCtx ctx, typename FmtCtx::parse_context parse_ctx) {
    using iterator = typename FmtCtx::parse_context::iterator;
    sfmt::parse_format(
        parse_ctx, [&ctx](iterator first, iterator last) { ctx.out() += to_string_view(first, last); },
        [&ctx](typename FmtCtx::parse_context& parse_ctx, std::size_t id) {
            ctx.arg(id).visit(arg_visitor<FmtCtx>{ctx, parse_ctx});
        });
}

}  // namespace sfmt
}  // namespace uxs
