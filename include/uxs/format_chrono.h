#pragma once

#include "format.h"

#include <chrono>
#include <ostream>

namespace uxs {

namespace detail {

template<typename FmtCtx, typename StreamBuf>
class formatbuf : public StreamBuf {
 public:
    using char_type = typename StreamBuf::char_type;
    using int_type = typename StreamBuf::int_type;
    using traits_type = typename StreamBuf::traits_type;

    explicit formatbuf(FmtCtx& ctx) : ctx_(ctx) {}

 private:
    FmtCtx& ctx_;

    int_type overflow(int_type ch) override {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) { ctx_.out().push_back(static_cast<char_type>(ch)); }
        return ch;
    }

    std::streamsize xsputn(const char_type* s, std::streamsize count) override {
        ctx_.out().append(s, s + count);
        return count;
    }
};

template<typename Ty, typename CharT>
struct chrono_formatter {
 private:
    fmt_opts specs_;
    std::size_t width_arg_id_ = dynamic_extent;
    std::string_view fmt_;

 protected:
    enum class specs {
        end_of_format = 0,
        ordinary_char,
        default_spec,
        percent,
        new_line,
        tab,
        century,
        year_yy,
        year_full,
        month_brief,
        month_full,
        month_mm,
    };

    template<typename ParseCtx, typename SpecCheckerFn>
    UXS_CONSTEXPR typename ParseCtx::iterator parse_impl(ParseCtx& ctx, const SpecCheckerFn& spec_checker) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = dynamic_extent;
        it = ParseCtx::parse_standard(ctx, it + 1, specs_, width_arg_id_, dummy_id);
        if (specs_.prec >= 0 || !!(specs_.flags & ~(fmt_flags::adjust_field | fmt_flags::localize))) {
            ParseCtx::syntax_error();
        }
        if (it == ctx.end() || *it != '%') { ParseCtx::syntax_error(); }
        auto it0 = it;
        while (true) {
            auto prev_it = it;
            auto chrono_spec = parse_format_spec(it, ctx.end());
            if (chrono_spec == specs::end_of_format) { break; }
            switch (chrono_spec) {
                case specs::percent:
                case specs::new_line:
                case specs::tab: break;
                default: {
                    if (!spec_checker(chrono_spec)) { return prev_it; }
                } break;
            }
        }
        fmt_ = std::string_view(it0, it);
        return it;
    }

    template<typename FmtCtx, typename ValueFormatterFn>
    void format_impl(FmtCtx& ctx, const Ty& val, const ValueFormatterFn& value_formatter) const {
        fmt_opts specs = specs_;
        if (width_arg_id_ != dynamic_extent) {
            specs.width = ctx.arg(width_arg_id_).get_unsigned(std::numeric_limits<decltype(specs.width)>::max());
        }

        const bool localize = !!(specs.flags & fmt_flags::localize);

        const auto fn = [this, &value_formatter, &val, localize](FmtCtx& ctx) {
            if (fmt_.empty()) {
                value_formatter(ctx, specs::default_spec, val, localize);
                return;
            }
            auto it0 = fmt_.begin(), it = it0;
            while (true) {
                auto prev_it = it;
                auto chrono_spec = parse_format_spec(it, fmt_.end());
                if (chrono_spec == specs::end_of_format) { break; }
                if (chrono_spec != specs::ordinary_char) {
                    ctx.out().append(it0, prev_it);
                    it0 = it;
                    switch (chrono_spec) {
                        case specs::percent: ctx.out().push_back('%'); break;
                        case specs::new_line: ctx.out().push_back('\n'); break;
                        case specs::tab: ctx.out().push_back('\t'); break;
                        default: value_formatter(ctx, chrono_spec, val, localize); break;
                    }
                }
            }
            ctx.out().append(it0, it);
        };

        if (specs.width == 0) {
            fn(ctx);
            return;
        }

        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        fn(buf_ctx);
        append_adjusted(
            ctx.out(), [&buf](basic_membuffer<CharT>& s) { s.append(buf.data(), buf.data() + buf.size()); },
            static_cast<unsigned>(buf.size()), specs);
    }

    template<typename Iter>
    static UXS_CONSTEXPR specs parse_format_spec(Iter& first, Iter last) {
        if (first == last || *first == '{' || *first == '}') { return specs::end_of_format; }
        if (*first++ != '%') { return specs::ordinary_char; }
        if (first == last) { return specs::end_of_format; }
        switch (*first++) {
            case '%': return specs::percent;
            case 'n': return specs::new_line;
            case 't': return specs::tab;
            case 'C': return specs::century;
            case 'y': return specs::year_yy;
            case 'Y': return specs::year_full;
            case 'b': return specs::month_brief;
            case 'h': return specs::month_brief;
            case 'B': return specs::month_full;
            case 'm': return specs::month_mm;
            default: {
                first = last;
                return specs::end_of_format;
            } break;
        }
    }

    template<typename FmtCtx>
    static void format_locale(FmtCtx& ctx, const std::tm& tm, const std::locale& loc, char format, char modifier) {
        using char_type = typename FmtCtx::char_type;
        formatbuf<FmtCtx, std::basic_streambuf<char_type>> format_buf(ctx);
        std::basic_ostream<char_type> os(&format_buf);
        os.imbue(loc);
        const auto& facet = std::use_facet<std::time_put<char_type>>(loc);
        auto end = facet.put(os, os, char_type(' '), &tm, format, modifier);
        if (end.failed()) { throw format_error("failed to format time"); }
    }

    template<typename FmtCtx>
    static void format_century(FmtCtx& ctx, std::chrono::year y) {
        const int y_int = static_cast<int>(y);
        unsigned sign = y_int >= 0 ? 0 : 1;
        fmt_opts fmt{fmt_flags::leading_zeroes, -1, 2 + sign};
        scvt::fmt_integer(ctx.out(), (sign ? (y_int - 99) : y_int) / 100, fmt, locale_ref{});
    }

    template<typename FmtCtx>
    static void format_year_yy(FmtCtx& ctx, std::chrono::year y) {
        int yy = static_cast<int>(y) % 100;
        if (yy < 0) { yy += 100; }
        ctx.out().push_back('0' + yy / 10);
        ctx.out().push_back('0' + yy % 10);
    }

    template<typename FmtCtx>
    static void format_year_full(FmtCtx& ctx, std::chrono::year y) {
        const int y_int = static_cast<int>(y);
        unsigned sign = y_int >= 0 ? 0 : 1;
        fmt_opts fmt{fmt_flags::leading_zeroes, -1, 4 + sign};
        scvt::fmt_integer(ctx.out(), y_int, fmt, locale_ref{});
    }

    template<typename FmtCtx>
    static void format_month_brief(FmtCtx& ctx, std::chrono::month m, bool localize) {
        unsigned mon = static_cast<unsigned>(m);
        if (mon >= 12) { return; }
        if (!localize) {
            using char_type = typename FmtCtx::char_type;
            static UXS_CONSTEXPR std::basic_string_view<char_type> brief_name_list[] = {
                string_literal<char_type, 'J', 'a', 'n'>{}(), string_literal<char_type, 'F', 'e', 'b'>{}(),
                string_literal<char_type, 'M', 'a', 'r'>{}(), string_literal<char_type, 'A', 'p', 'r'>{}(),
                string_literal<char_type, 'M', 'a', 'y'>{}(), string_literal<char_type, 'J', 'u', 'n'>{}(),
                string_literal<char_type, 'J', 'u', 'l'>{}(), string_literal<char_type, 'A', 'u', 'g'>{}(),
                string_literal<char_type, 'S', 'e', 'p'>{}(), string_literal<char_type, 'O', 'c', 't'>{}(),
                string_literal<char_type, 'N', 'o', 'v'>{}(), string_literal<char_type, 'D', 'e', 'c'>{}(),
            };
            ctx.out().append(brief_name_list[mon].begin(), brief_name_list[mon].end());
        } else {
            std::tm tm{};
            tm.tm_mon = mon;
            format_locale(ctx, tm, *ctx.locale(), 'b', '\0');
        }
    }

    template<typename FmtCtx>
    static void format_month_full(FmtCtx& ctx, std::chrono::month m, bool localize) {
        unsigned mon = static_cast<unsigned>(m);
        if (mon >= 12) { return; }
        if (!localize) {
            using char_type = typename FmtCtx::char_type;
            //static UXS_CONSTEXPR std::basic_string_view<char_type> full_name_list[] = {
            //    string_literal<char_type, 'J', 'a', 'n', 'u', 'a', 'r', 'y'>{}(),
            //    string_literal<char_type, 'F', 'e', 'b', 'r', 'u', 'a', 'r', 'y'>{}(),
            //    string_literal<char_type, 'M', 'a', 'r'>{}(),
            //    string_literal<char_type, 'A', 'p', 'r'>{}(),
            //    string_literal<char_type, 'M', 'a', 'y'>{}(),
            //    string_literal<char_type, 'J', 'u', 'n'>{}(),
            //    string_literal<char_type, 'J', 'u', 'l'>{}(),
            //    string_literal<char_type, 'A', 'u', 'g'>{}(),
            //    string_literal<char_type, 'S', 'e', 'p'>{}(),
            //    string_literal<char_type, 'O', 'c', 't'>{}(),
            //    string_literal<char_type, 'N', 'o', 'v'>{}(),
            //    string_literal<char_type, 'D', 'e', 'c'>{}(),
            //};


            // static UXS_CONSTEXPR std::pair<const char*, unsigned> full_name_list[] = {
            //     {"January", 7}, {"February", 8}, {"March", 5},     {"April", 5},   {"May", 3},      {"June", 4},
            //     {"July", 4},    {"August", 6},   {"September", 9}, {"October", 7}, {"November", 8}, {"December", 8}};
            // ctx.out().append(brief_name_list[mon], brief_name_list[mon] + 3);
        } else {
            std::tm tm{};
            tm.tm_mon = mon;
            format_locale(ctx, tm, *ctx.locale(), 'B', '\0');
        }
    }

    template<typename FmtCtx>
    static void format_month_mm(FmtCtx& ctx, std::chrono::month m) {
        unsigned mon = static_cast<unsigned>(m);
        if (mon >= 12) { return; }
        ctx.out().push_back('0' + mon / 10);
        ctx.out().push_back('0' + mon % 10);
    }
};

}  // namespace detail

#if 0
template<typename Duration, typename CharT>
struct formatter<std::chrono::sys_time<Duration>, CharT> {
 private:
    fmt_opts specs_;
    std::size_t width_arg_id_ = dynamic_extent;
    std::string_view fmt_;

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        std::size_t dummy_id = dynamic_extent;
        it = ParseCtx::parse_standard(ctx, it + 1, specs_, width_arg_id_, dummy_id);
        if (specs_.prec >= 0 || !!(specs_.flags & ~(fmt_flags::adjust_field | fmt_flags::localize))) {
            ParseCtx::syntax_error();
        }
        auto it0 = it;
        while (detail::parse_format_spec(it, ctx.end()) != detail::specs::end_of_format) {}
        fmt_ = std::string_view(it0, it);
        return it;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::sys_time<Duration> val) const {
        fmt_opts specs = specs_;
        if (width_arg_id_ != dynamic_extent) {
            specs.width = ctx.arg(width_arg_id_).get_unsigned(std::numeric_limits<decltype(specs.width)>::max());
        }
        auto it0 = fmt_.begin(), it = it0;
        while (true) {
            auto prev_it = it;
            auto chrono_spec = detail::parse_format_spec(it, fmt_.end());
            if (chrono_spec == detail::specs::end_of_format) { break; }
            if (chrono_spec != detail::specs::ordinary_char) {
                const bool localize = !!(specs_.flags & fmt_flags::localize);
                ctx.out().append(it0, prev_it);
                it0 = it;
                switch (chrono_spec) {
                    case detail::specs::percent: ctx.out().push_back('%'); break;
                    case detail::specs::new_line: ctx.out().push_back('\n'); break;
                    case detail::specs::tab: ctx.out().push_back('\t'); break;

                    case detail::specs::century: {
                        const std::chrono::year_month_day ymd(std::chrono::floor<std::chrono::days>(val));
                        detail::format_chrono_century(ctx, ymd.year());
                    } break;
                    case detail::specs::year_yy: {
                        // format_chrono_year_yy(ctx, std::chrono::year{val});
                    } break;
                    case detail::specs::year_full: {
                        // format_chrono_year_full(ctx, std::chrono::year{val});
                    } break;
                    case detail::specs::month_brief: {
                        const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(val)};
                        detail::format_chrono_month_brief(ctx, ymd.month(), localize);
                    } break;
                    case detail::specs::month_full: {
                    } break;
                    case detail::specs::month_mm: {
                    } break;

                    default: break;
                }
            }
        }
        ctx.out().append(it0, it);
    }
};
#endif

template<typename CharT>
struct formatter<std::chrono::year, CharT> : detail::chrono_formatter<std::chrono::year, CharT> {
 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        return this->parse_impl(ctx, spec_checker);
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::year val) const {
        this->format_impl(ctx, val, value_formatter<FmtCtx>);
    }

 private:
    using super = detail::chrono_formatter<std::chrono::year, CharT>;
    using specs = typename super::specs;

    static UXS_CONSTEXPR bool spec_checker(specs spec) {
        switch (spec) {
            case specs::century:
            case specs::year_yy:
            case specs::year_full: return true;
            default: return false;
        }
    }

    template<typename FmtCtx>
    static void value_formatter(FmtCtx& ctx, specs spec, std::chrono::year val, bool localize) {
        switch (spec) {
            case specs::century: super::format_century(ctx, val); break;
            case specs::year_yy: super::format_year_yy(ctx, val); break;
            default: super::format_year_full(ctx, val); break;
        }
    }
};

template<typename CharT>
struct formatter<std::chrono::month, CharT> : detail::chrono_formatter<std::chrono::month, CharT> {
 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        return this->parse_impl(ctx, spec_checker);
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::month val) const {
        this->format_impl(ctx, val, value_formatter<FmtCtx>);
    }

 private:
    using super = detail::chrono_formatter<std::chrono::month, CharT>;
    using specs = typename super::specs;

    static UXS_CONSTEXPR bool spec_checker(specs spec) {
        switch (spec) {
            case specs::month_brief:
            case specs::month_full:
            case specs::month_mm: return true;
            default: return false;
        }
    }

    template<typename FmtCtx>
    static void value_formatter(FmtCtx& ctx, specs spec, std::chrono::month val, bool localize) {
        switch (spec) {
            case specs::month_mm: super::format_month_mm(ctx, val); break;
            case specs::month_full: super::format_month_full(ctx, val, localize); break;
            default: super::format_month_brief(ctx, val, localize); break;
        }
    }
};

}  // namespace uxs
