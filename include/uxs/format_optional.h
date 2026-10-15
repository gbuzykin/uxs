#pragma once

#include "format_common.h"

#if __cplusplus < 201703L || !UXS_HAS_INCLUDE(<optional>)
#    error Header file `format_optional.h` requires C++17 and <optional> header
#endif  // __cplusplus < 201703L

#include <optional>

namespace uxs {

template<typename Ty, typename CharT>
struct formatter<std::optional<Ty>, CharT, std::enable_if_t<is_formattable<Ty, CharT>::value>> {
 private:
    formatter_t<Ty, CharT> underlying_;

 public:
    constexpr void set_debug_format() { underlying_.set_debug_format(); }

    template<typename ParseCtx>
    constexpr typename ParseCtx::iterator parse(ParseCtx& ctx) {
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const std::optional<Ty>& val) const {
        if (val) { return underlying_.format(ctx, *val); }
        ctx.out() += string_literal<CharT, 'n', 'u', 'l', 'l'>{}();
    }
};

}  // namespace uxs
