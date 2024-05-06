#pragma once

#include "format.h"

namespace uxs {
namespace sfmt {

template<typename FmtCtx>
struct arg_visitor {
    FmtCtx& ctx;
    typename FmtCtx::parse_context& parse_ctx;
    arg_visitor(FmtCtx& ctx, typename FmtCtx::parse_context& parse_ctx) : ctx(ctx), parse_ctx(parse_ctx) {}
    template<typename Ty>
    void operator()(const Ty& v) const {
        ctx.format_arg(parse_ctx, v);
    }
};

template<typename FmtCtx>
void vformat(FmtCtx ctx, typename FmtCtx::parse_context parse_ctx) {
    using parse_context = typename FmtCtx::parse_context;
    using iterator = typename parse_context::const_iterator;
    sfmt::parse_format(
        parse_ctx, [&ctx](iterator first, iterator last) { ctx.out().append(first, last); },
        [&ctx](parse_context& parse_ctx, std::size_t id) {
            ctx.arg(id).visit(arg_visitor<FmtCtx>{ctx, parse_ctx});
        });
}

}  // namespace sfmt
}  // namespace uxs
