#pragma once

#include "format_common.h"
#include "string_conv_common.h"

#if __cplusplus < 201703L || !UXS_HAS_INCLUDE(<filesystem>)
#    error Header file `format_fs.h` requires C++17 and <filesystem> header
#endif  // __cplusplus < 201703L

#include <filesystem>

namespace uxs {

using utf_native_path_adapter = utf_string_adapter<std::filesystem::path::value_type>;

template<typename CharT>
struct format_kind<std::filesystem::path, CharT> : std::integral_constant<range_format, range_format::disabled> {};

template<typename CharT>
struct formatter<std::filesystem::path, CharT> {
 private:
    fmt_opts opts_;
    bool use_generic_ = false;

 public:
    constexpr void set_debug_format() { opts_.flags |= fmt_flags::debug_format; }

    template<typename ParseCtx>
    constexpr typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (*it != ':') { return it; }
        it = ParseCtx::parse_adjustment(ctx, it + 1, opts_);
        if (it == ctx.end()) { return it; }
        if (*it == '?') {
            set_debug_format();
            if (++it == ctx.end()) { return it; }
        }
        if (*it != 'g') { return it; }
        use_generic_ = true;
        return it + 1;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const std::filesystem::path& val) const {
        append_formatted_string(
            ctx.out(), use_generic_ ? val.generic_string<std::filesystem::path::value_type>() : val.native(), opts_);
    }
};

}  // namespace uxs
