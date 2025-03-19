#pragma once

#include "format_base.h"
#include "optional.h"

namespace uxs {

template<typename Ty, typename CharT>
struct formatter<est::optional<Ty>, CharT, std::enable_if_t<formattable<Ty, CharT>::value>> {
 private:
    formatter<Ty, CharT> underlying_;

 public:
    UXS_CONSTEXPR void set_debug_format() { underlying_.set_debug_format(); }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const est::optional<Ty>& val) const {
        if (val) { return underlying_.format(ctx, *val); }
        ctx.out() += string_literal<CharT, 'n', 'u', 'l', 'l'>{}();
    }
};

}  // namespace uxs
