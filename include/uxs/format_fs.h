#pragma once

#include "format_base.h"

#include <filesystem>

namespace uxs {

using utf_native_path_adapter = utf_string_adapter<std::filesystem::path::value_type>;

template<typename CharT>
struct range_formattable<std::filesystem::path, CharT>  //
    : std::integral_constant<range_format, range_format::disabled> {};

template<typename CharT>
struct formatter<std::filesystem::path, CharT> {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = unspecified_size;
    bool use_generic_ = false;

 public:
    UXS_CONSTEXPR void set_debug_format() { opts_.flags |= fmt_flags::debug_format; }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = unspecified_size;
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, dummy_id);
        if (opts_.prec >= 0 || !!(opts_.flags & ~fmt_flags::adjust_field)) { ParseCtx::syntax_error(); }
        if (it != ctx.end() && *it == '?') {
            set_debug_format();
            ++it;
        }
        if (it == ctx.end() || *it != 'g') { return it; }
        use_generic_ = true;
        return it + 1;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const std::filesystem::path& val) const {
        fmt_opts opts = opts_;
        if (width_arg_id_ != unspecified_size) {
            opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(opts.width)>();
        }
        scvt::fmt_string<CharT>(
            ctx.out(),
            use_generic_ ? utf_string_adapter<CharT>{}(val.generic_string<std::filesystem::path::value_type>()) :
                           utf_string_adapter<CharT>{}(val.native()),
            opts, ctx.locale());
    }
};

}  // namespace uxs
