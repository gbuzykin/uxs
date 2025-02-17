#pragma once

#include "format.h"

#include <chrono>
#include <cmath>
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
        if (!traits_type::eq_int_type(ch, traits_type::eof())) { ctx_.out() += static_cast<char_type>(ch); }
        return ch;
    }

    std::streamsize xsputn(const char_type* s, std::streamsize count) override {
        ctx_.out().append(s, count);
        return count;
    }
};

enum class chrono_specifier {
    end_of_format = 0,
    ordinary_char,
    default_spec,
    percent,
    new_line,
    tab,
    // --- year ---
    century,
    year_yy,
    year_yyyy,
    // --- month ---
    month_brief,
    month_full,
    month_mm,
    // --- day ---
    day_zero,
    day_space,
    // --- day of the week ---
    day_of_the_week_brief,
    day_of_the_week_full,
    day_of_the_week_1_7,
    day_of_the_week_0_6,
    // -- ISO 8601 week-based year
    year_ISO_8601_yy,
    year_ISO_8601_yyyy,
    week_of_the_year_ISO_8601,
    // --- day/week of the year ---
    day_of_the_year,
    week_of_the_year_monday_first,
    week_of_the_year_sunday_first,
    // --- date ---
    month_day_year,
    year_month_day,
    locale_date,
    // --- time of day ---
    hours_24,
    hours_12,
    minutes,
    seconds,
    hours_am_pm,
    hours_minutes,
    hours_minutes_seconds,
    locale_time_12,
    locale_time,
    // --- ticks ---
    ticks,
    unit_suffix,
    // --- time zone ---
    time_zone,
    time_zone_abbreviation,
    // --- miscellaneous ---
    locale_date_time,
};

struct chrono_specs {
    chrono_specifier spec;
    char spec_char;
    char modifier;
    fmt_opts opts;
};

template<typename... CharTs>
UXS_CONSTEXPR bool check_chrono_modifier(char modifier) {
    return !modifier;
}

template<typename... CharTs>
UXS_CONSTEXPR bool check_chrono_modifier(char modifier, char m, CharTs... others) {
    return modifier == m || check_chrono_modifier(modifier, others...);
}

template<typename Iter>
UXS_CONSTEXPR chrono_specifier parse_chrono_format_spec(Iter& first, Iter last, char& modifier) {
    modifier = '\0';
    if (first == last || *first == '{' || *first == '}') { return chrono_specifier::end_of_format; }
    if (*first++ != '%') { return chrono_specifier::ordinary_char; }
    if (first == last) { return chrono_specifier::end_of_format; }
    switch (*first) {
        case '%': {
            ++first;
            return chrono_specifier::percent;
        } break;
        case 'n': {
            ++first;
            return chrono_specifier::new_line;
        } break;
        case 't': {
            ++first;
            return chrono_specifier::tab;
        } break;
        case 'O':
        case 'E': {
            modifier = *first++;
            if (first == last) { return chrono_specifier::end_of_format; }
        } break;
        default: break;
    }
    switch (*first++) {
        // --- year ---
        case 'C': {
            if (check_chrono_modifier(modifier, 'E')) { return chrono_specifier::century; }
        } break;
        case 'y': {
            if (check_chrono_modifier(modifier, 'O', 'E')) { return chrono_specifier::year_yy; }
        } break;
        case 'Y': {
            if (check_chrono_modifier(modifier, 'E')) { return chrono_specifier::year_yyyy; }
        } break;
        // --- month ---
        case 'b':
        case 'h': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::month_brief; }
        } break;
        case 'B': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::month_full; }
        } break;
        case 'm': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::month_mm; }
        } break;
        // --- day ---
        case 'd': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::day_zero; }
        } break;
        case 'e': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::day_space; }
        } break;
        // --- day of the week ---
        case 'a': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::day_of_the_week_brief; }
        } break;
        case 'A': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::day_of_the_week_full; }
        } break;
        case 'u': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::day_of_the_week_1_7; }
        } break;
        case 'w': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::day_of_the_week_0_6; }
        } break;
        // -- ISO 8601 week-based year
        case 'g': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::year_ISO_8601_yy; }
        } break;
        case 'G': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::year_ISO_8601_yyyy; }
        } break;
        case 'V': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::week_of_the_year_ISO_8601; }
        } break;
        // --- day/week of the year ---
        case 'j': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::day_of_the_year; }
        } break;
        case 'U': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::week_of_the_year_monday_first; }
        } break;
        case 'W': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::week_of_the_year_sunday_first; }
        } break;
        // --- date ---
        case 'D': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::month_day_year; }
        } break;
        case 'F': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::year_month_day; }
        } break;
        case 'x': {
            if (check_chrono_modifier(modifier, 'E')) { return chrono_specifier::locale_date; }
        } break;
        // --- time of day ---
        case 'H': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::hours_24; }
        } break;
        case 'I': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::hours_12; }
        } break;
        case 'M': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::minutes; }
        } break;
        case 'S': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::seconds; }
        } break;
        case 'p': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::hours_am_pm; }
        } break;
        case 'R': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::hours_minutes; }
        } break;
        case 'T': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::hours_minutes_seconds; }
        } break;
        case 'r': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::locale_time_12; }
        } break;
        case 'X': {
            if (check_chrono_modifier(modifier, 'E')) { return chrono_specifier::locale_time; }
        } break;
        // --- ticks ---
        case 'Q': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::ticks; }
        } break;
        case 'q': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::unit_suffix; }
        } break;
        // --- time zone ---
        case 'z': {
            if (check_chrono_modifier(modifier, 'O', 'E')) { return chrono_specifier::time_zone; }
        } break;
        case 'Z': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::time_zone_abbreviation; }
        } break;
        // --- miscellaneous ---
        case 'c': {
            if (check_chrono_modifier(modifier, 'E')) { return chrono_specifier::locale_date_time; }
        } break;
        default: break;
    }
    first = last;
    return chrono_specifier::end_of_format;
}

template<typename FmtCtx>
void format_chrono_locale(FmtCtx& ctx, const std::tm& tm, char spec, char modifier, bool localize) {
    using char_type = typename FmtCtx::char_type;
    const auto locale = localize ? *ctx.locale() : std::locale::classic();
    formatbuf<FmtCtx, std::basic_streambuf<char_type>> format_buf(ctx);
    std::basic_ostream<char_type> os(&format_buf);
    os.imbue(locale);
    const auto& facet = std::use_facet<std::time_put<char_type>>(locale);
    auto end = facet.put(os, os, ' ', &tm, spec, modifier);
    if (end.failed()) { throw format_error("failed to format time"); }
}

template<typename FmtCtx>
void format_chrono_locale(FmtCtx& ctx, const chrono_specs& specs, const std::tm& tm) {
    format_chrono_locale(ctx, tm, specs.spec_char, specs.modifier, !!(specs.opts.flags & fmt_flags::localize));
}

inline void format_chrono_out_of_bounds() { throw format_error("time point is out-of-bounds"); }

// --- year ---

template<typename FmtCtx>
void format_chrono_century(FmtCtx& ctx, std::chrono::year y) {
    const int year = static_cast<int>(y);
    const unsigned sign = year >= 0 ? 0 : 1;
    const int century = (sign ? (year - 99) : year) / 100;
    scvt::fmt_integer(ctx.out(), century, fmt_opts{fmt_flags::leading_zeroes, -1, 2 + sign});
}

template<typename FmtCtx>
void format_chrono_year_yy(FmtCtx& ctx, std::chrono::year y) {
    const int year = static_cast<int>(y) % 100;
    scvt::fmt_integer(ctx.out(), year >= 0 ? year : year + 100, fmt_opts{fmt_flags::leading_zeroes, -1, 2});
}

template<typename FmtCtx>
void format_chrono_year_yyyy(FmtCtx& ctx, std::chrono::year y) {
    const int year = static_cast<int>(y);
    const unsigned sign = year >= 0 ? 0 : 1;
    scvt::fmt_integer(ctx.out(), year, fmt_opts{fmt_flags::leading_zeroes, -1, 4 + sign});
}

template<typename FmtCtx>
void format_chrono_year(FmtCtx& ctx, const chrono_specs& specs, std::chrono::year y) {
    if (!specs.modifier) {
        switch (specs.spec) {
            case detail::chrono_specifier::century: format_chrono_century(ctx, y); break;
            case detail::chrono_specifier::year_yy: format_chrono_year_yy(ctx, y); break;
            case detail::chrono_specifier::year_yyyy: format_chrono_year_yyyy(ctx, y); break;
            default: {
                format_chrono_year_yyyy(ctx, y);
                if (!y.ok()) {
                    using char_type = typename FmtCtx::char_type;
                    ctx.out() += string_literal<char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v', 'a',
                                                'l', 'i', 'd', ' ', 'y', 'e', 'a', 'r'>{}();
                }
            } break;
        }
    } else {
        if (!y.ok()) { format_chrono_out_of_bounds(); }
        std::tm tm{};
        tm.tm_year = static_cast<int>(y) - 1900;
        format_chrono_locale(ctx, specs, tm);
    }
}

// --- month ---

template<typename FmtCtx>
void format_chrono_month_brief(FmtCtx& ctx, std::chrono::month m) {
    using char_type = typename FmtCtx::char_type;
    const unsigned month = static_cast<unsigned>(m);
    if (month < 1 || month > 12) { format_chrono_out_of_bounds(); }
    static UXS_CONSTEXPR std::basic_string_view<char_type> brief_name_list[] = {
        string_literal<char_type, 'J', 'a', 'n'>{}(), string_literal<char_type, 'F', 'e', 'b'>{}(),
        string_literal<char_type, 'M', 'a', 'r'>{}(), string_literal<char_type, 'A', 'p', 'r'>{}(),
        string_literal<char_type, 'M', 'a', 'y'>{}(), string_literal<char_type, 'J', 'u', 'n'>{}(),
        string_literal<char_type, 'J', 'u', 'l'>{}(), string_literal<char_type, 'A', 'u', 'g'>{}(),
        string_literal<char_type, 'S', 'e', 'p'>{}(), string_literal<char_type, 'O', 'c', 't'>{}(),
        string_literal<char_type, 'N', 'o', 'v'>{}(), string_literal<char_type, 'D', 'e', 'c'>{}(),
    };
    ctx.out() += brief_name_list[month - 1];
}

template<typename FmtCtx>
void format_chrono_month_full(FmtCtx& ctx, std::chrono::month m) {
    using char_type = typename FmtCtx::char_type;
    const unsigned month = static_cast<unsigned>(m);
    if (month < 1 || month > 12) { format_chrono_out_of_bounds(); }
    static UXS_CONSTEXPR std::basic_string_view<char_type> full_name_list[] = {
        string_literal<char_type, 'J', 'a', 'n', 'u', 'a', 'r', 'y'>{}(),
        string_literal<char_type, 'F', 'e', 'b', 'r', 'u', 'a', 'r', 'y'>{}(),
        string_literal<char_type, 'M', 'a', 'r', 'c', 'h'>{}(),
        string_literal<char_type, 'A', 'p', 'r', 'i', 'l'>{}(),
        string_literal<char_type, 'M', 'a', 'y'>{}(),
        string_literal<char_type, 'J', 'u', 'n', 'e'>{}(),
        string_literal<char_type, 'J', 'u', 'l', 'y'>{}(),
        string_literal<char_type, 'A', 'u', 'g', 'u', 's', 't'>{}(),
        string_literal<char_type, 'S', 'e', 'p', 't', 'e', 'm', 'b', 'e', 'r'>{}(),
        string_literal<char_type, 'O', 'c', 't', 'o', 'b', 'e', 'r'>{}(),
        string_literal<char_type, 'N', 'o', 'v', 'e', 'm', 'b', 'e', 'r'>{}(),
        string_literal<char_type, 'D', 'e', 'c', 'e', 'm', 'b', 'e', 'r'>{}(),
    };
    ctx.out() += full_name_list[month - 1];
}

template<typename FmtCtx>
void format_chrono_month_mm(FmtCtx& ctx, std::chrono::month m) {
    const unsigned month = static_cast<unsigned>(m);
    scvt::fmt_integer(ctx.out(), month, fmt_opts{fmt_flags::leading_zeroes, -1, 2});
}

template<typename FmtCtx>
void format_chrono_month(FmtCtx& ctx, const chrono_specs& specs, std::chrono::month m) {
    const bool localize = !!(specs.opts.flags & fmt_flags::localize);
    if (!specs.modifier) {
        switch (specs.spec) {
            case detail::chrono_specifier::month_brief: {
                if (!localize) { return format_chrono_month_brief(ctx, m); }
            } break;
            case detail::chrono_specifier::month_full: {
                if (!localize) { return format_chrono_month_full(ctx, m); }
            } break;
            case detail::chrono_specifier::month_mm: return format_chrono_month_mm(ctx, m);
            default: {
                if (m.ok()) {
                    if (*ctx.locale() == std::locale::classic()) { return format_chrono_month_brief(ctx, m); }
                    std::tm tm{};
                    tm.tm_mon = static_cast<unsigned>(m) - 1;
                    return format_chrono_locale(ctx, tm, 'b', '\0', true);
                }
                using char_type = typename FmtCtx::char_type;
                scvt::fmt_integer(ctx.out(), static_cast<unsigned>(m));
                ctx.out() += string_literal<char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v', 'a', 'l',
                                            'i', 'd', ' ', 'm', 'o', 'n', 't', 'h'>{}();
                return;
            } break;
        }
    }
    if (!m.ok()) { format_chrono_out_of_bounds(); }
    std::tm tm{};
    tm.tm_mon = static_cast<unsigned>(m) - 1;
    format_chrono_locale(ctx, specs, tm);
}

// --- day ---

template<typename FmtCtx>
void format_chrono_day_dd(FmtCtx& ctx, std::chrono::day d, char pad = '0') {
    const unsigned day = static_cast<unsigned>(d);
    scvt::fmt_integer(ctx.out(), day, fmt_opts{fmt_flags::right, -1, 2, pad});
}

template<typename FmtCtx>
void format_chrono_day(FmtCtx& ctx, const chrono_specs& specs, std::chrono::day d) {
    if (!specs.modifier) {
        switch (specs.spec) {
            case detail::chrono_specifier::day_zero: format_chrono_day_dd(ctx, d, '0'); break;
            case detail::chrono_specifier::day_space: format_chrono_day_dd(ctx, d, ' '); break;
            default: {
                format_chrono_day_dd(ctx, d, '0');
                if (!d.ok()) {
                    using char_type = typename FmtCtx::char_type;
                    ctx.out() += string_literal<char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v', 'a',
                                                'l', 'i', 'd', ' ', 'd', 'a', 'y'>{}();
                }
            } break;
        }
    } else {
        if (!d.ok()) { format_chrono_out_of_bounds(); }
        std::tm tm{};
        tm.tm_mday = static_cast<unsigned>(d);
        format_chrono_locale(ctx, specs, tm);
    }
}

// --- date ---

inline void make_tm_for_wday(std::tm& tm, std::chrono::year_month_day ymd) {
    tm.tm_wday = std::chrono::weekday{std::chrono::sys_days{ymd}}.c_encoding();
}

inline void make_tm_for_date(std::tm& tm, std::chrono::year_month_day ymd) {
    std::chrono::sys_days days{ymd};
    tm.tm_year = static_cast<int>(ymd.year()) - 1900;
    tm.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
    tm.tm_mday = static_cast<unsigned>(ymd.day());
    tm.tm_yday = (days - std::chrono::sys_days{ymd.year() / std::chrono::January / 0}).count();
    tm.tm_wday = std::chrono::weekday{days}.c_encoding();
}

template<typename FmtCtx>
void format_chrono_date(FmtCtx& ctx, const chrono_specs& specs, std::chrono::year_month_day ymd) {
    switch (specs.spec) {
        // --- year ---
        case detail::chrono_specifier::century:
        case detail::chrono_specifier::year_yy:
        case detail::chrono_specifier::year_yyyy: return format_chrono_year(ctx, specs, ymd.year());
        // --- month ---
        case detail::chrono_specifier::month_brief:
        case detail::chrono_specifier::month_full:
        case detail::chrono_specifier::month_mm: return format_chrono_month(ctx, specs, ymd.month());
        // --- day ---
        case detail::chrono_specifier::day_zero:
        case detail::chrono_specifier::day_space: return format_chrono_day(ctx, specs, ymd.day());
        // --- day of the week ---
        // case detail::day_of_the_week_brief:
        // case detail::day_of_the_week_full:
        // case detail::day_of_the_week_1_7:
        // case detail::day_of_the_week_0_6:
        // -- ISO 8601 week-based year
        // case detail::year_ISO_8601_yy:
        // case detail::year_ISO_8601_yyyy:
        // case detail::week_of_the_year_ISO_8601:
        // --- day/week of the year ---
        // case detail::day_of_the_year:
        // case detail::week_of_the_year_monday_first:
        // case detail::week_of_the_year_sunday_first:
        // --- date ---
        // case detail::chrono_specifier::locale_date: {
        //    std::tm tm{};
        //    make_tm_for_date(tm, ymd);
        //    format_chrono_locale(ctx, specs, tm);
        //} break;
        case detail::chrono_specifier::month_day_year: {
            detail::format_chrono_month_mm(ctx, ymd.month());
            ctx.out() += '/';
            detail::format_chrono_day_dd(ctx, ymd.day());
            ctx.out() += '/';
            return detail::format_chrono_year_yy(ctx, ymd.year());
        } break;
        case detail::chrono_specifier::default_spec: {
            detail::format_chrono_year_yyyy(ctx, ymd.year());
            ctx.out() += '-';
            detail::format_chrono_month_mm(ctx, ymd.month());
            ctx.out() += '-';
            detail::format_chrono_day_dd(ctx, ymd.day());
            if (specs.spec == detail::chrono_specifier::default_spec && !ymd.ok()) {
                using char_type = typename FmtCtx::char_type;
                ctx.out() += string_literal<char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v', 'a', 'l',
                                            'i', 'd', ' ', 'd', 'a', 't', 'e'>{}();
            }
            return;
        } break;
        default: break;
    }
    std::tm tm{};
    make_tm_for_date(tm, ymd);
    format_chrono_locale(ctx, specs, tm);
}

template<typename FmtCtx>
void format_chrono_hours_24(FmtCtx& ctx, std::chrono::hours h) {
    const int hours = h.count();
    const unsigned sign = hours >= 0 ? 0 : 1;
    scvt::fmt_integer(ctx.out(), hours, fmt_opts{fmt_flags::leading_zeroes, -1, 2 + sign});
}

template<typename FmtCtx, typename Rep, typename Period>
void format_chrono_hours_24(FmtCtx& ctx, std::chrono::duration<Rep, Period> d) {
    format_chrono_hours_24(ctx, std::chrono::duration_cast<std::chrono::hours>(d));
}

template<typename FmtCtx, typename Duration>
void format_chrono_hours_24(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms) {
    format_chrono_hours_24(ctx, hms.hours());
}

template<typename FmtCtx>
void format_chrono_hours_12(FmtCtx& ctx, std::chrono::hours h) {
    int hours = h.count();
    if (hours >= 0 && hours <= 24) {
        hours = hours > 0 ? (hours <= 12 ? hours : hours - 12) : 12;
        scvt::fmt_integer(ctx.out(), hours, fmt_opts{fmt_flags::leading_zeroes, -1, 2});
    } else {
        ctx.out() += '?';
    }
}

template<typename FmtCtx, typename Rep, typename Period>
void format_chrono_hours_12(FmtCtx& ctx, std::chrono::duration<Rep, Period> d) {
    format_chrono_hours_12(ctx, std::chrono::duration_cast<std::chrono::hours>(d));
}

template<typename FmtCtx, typename Duration>
void format_chrono_hours_12(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms) {
    format_chrono_hours_12(ctx, hms.hours());
}

template<typename FmtCtx>
void format_chrono_minutes(FmtCtx& ctx, std::chrono::minutes m) {
    const int minutes = m.count() % 60;
    const unsigned sign = minutes >= 0 ? 0 : 1;
    scvt::fmt_integer(ctx.out(), minutes, fmt_opts{fmt_flags::leading_zeroes, -1, 2 + sign});
}

template<typename FmtCtx, typename Rep, typename Period>
void format_chrono_minutes(FmtCtx& ctx, std::chrono::duration<Rep, Period> d) {
    format_chrono_minutes(ctx, std::chrono::duration_cast<std::chrono::minutes>(d));
}

template<typename FmtCtx, typename Duration>
void format_chrono_minutes(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms) {
    format_chrono_minutes(ctx, hms.minutes());
}

template<typename FmtCtx>
void format_chrono_seconds(FmtCtx& ctx, std::chrono::seconds s) {
    const int seconds = s.count() % 60;
    const unsigned sign = seconds >= 0 ? 0 : 1;
    scvt::fmt_integer(ctx.out(), seconds, fmt_opts{fmt_flags::leading_zeroes, -1, 2 + sign});
}

template<typename FmtCtx, typename Rep, typename Period>
void format_chrono_seconds(FmtCtx& ctx, const chrono_specs& specs, std::chrono::duration<Rep, Period> d) {
    double integral_part;
    double duration_secs = std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
    if (duration_secs < 0) {
        ctx.out() += '-';
        duration_secs = -duration_secs;
    }
    const double fractional_part = std::modf(duration_secs, &integral_part);
    const int seconds = static_cast<int>(static_cast<long long>(integral_part) % 60);
    if (fractional_part != 0.) {
        if (seconds < 10) { ctx.out() += '0'; }
        to_basic_string(ctx.out(), *ctx.locale(), seconds + fractional_part,
                        fmt_opts{specs.opts.flags | fmt_flags::fixed, specs.opts.prec});
    } else {
        scvt::fmt_integer(ctx.out(), seconds, fmt_opts{fmt_flags::leading_zeroes, -1, 2});
    }
}

template<typename FmtCtx, typename Duration>
void format_chrono_seconds(FmtCtx& ctx, const chrono_specs& specs, std::chrono::hh_mm_ss<Duration> hms) {
    format_chrono_seconds(ctx, specs, hms.seconds());
}

template<typename Rep, typename Period>
void make_tm_for_time(std::tm& tm, std::chrono::duration<Rep, Period> d) {
    tm.tm_hour = std::chrono::duration_cast<std::chrono::hours>(d).count();
    tm.tm_min = std::chrono::duration_cast<std::chrono::minutes>(d).count() % 60;
    tm.tm_sec = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(d).count() % 60);
}

template<typename Duration>
void make_tm_for_time(std::tm& tm, std::chrono::hh_mm_ss<Duration> hms) {
    tm.tm_hour = hms.hours().count();
    tm.tm_min = hms.minutes().count();
    tm.tm_sec = static_cast<int>(hms.seconds().count());
}

template<typename FmtCtx, typename Time>
void format_chrono_time(FmtCtx& ctx, const chrono_specs& specs, Time t) {
    if (!specs.modifier) {
        switch (specs.spec) {
            case detail::chrono_specifier::hours_24: return detail::format_chrono_hours_24(ctx, t);
            case detail::chrono_specifier::hours_12: return detail::format_chrono_hours_12(ctx, t); break;
            case detail::chrono_specifier::minutes: return detail::format_chrono_minutes(ctx, t); break;
            case detail::chrono_specifier::seconds: return detail::format_chrono_seconds(ctx, specs, t); break;
            case detail::chrono_specifier::hours_minutes: {
                detail::format_chrono_hours_24(ctx, t);
                ctx.out() += ':';
                return detail::format_chrono_minutes(ctx, t);
            } break;
            case detail::chrono_specifier::hours_am_pm:
            case detail::chrono_specifier::locale_time_12:
            case detail::chrono_specifier::locale_time: break;
            default: {
                detail::format_chrono_hours_24(ctx, t);
                ctx.out() += ':';
                detail::format_chrono_minutes(ctx, t);
                ctx.out() += ':';
                return detail::format_chrono_seconds(ctx, specs, t);
            } break;
        }
    }
    std::tm tm{};
    make_tm_for_time(tm, t);
    format_chrono_locale(ctx, specs, tm);
}

template<typename Ty, typename = void>
struct is_floating_point_duration : std::false_type {};

template<typename Rep, typename Period>
struct is_floating_point_duration<std::chrono::duration<Rep, Period>,
                                  std::enable_if_t<std::is_floating_point<Rep>::value>> : std::true_type {};

template<typename Period, typename = void>
struct duration_suffix_writer {
    template<typename FmtCtx>
    void write(FmtCtx& ctx) {
        ctx.out() += '[';
        scvt::fmt_integer(ctx.out(), Period::type::num);
        ctx.out() += '/';
        scvt::fmt_integer(ctx.out(), Period::type::den);
        ctx.out() += string_literal<typename FmtCtx::char_type, ']', 's'>{}();
    }
};

template<typename Period>
struct duration_suffix_writer<Period, std::enable_if_t<Period::type::den == 1>> {
    template<typename FmtCtx>
    void write(FmtCtx& ctx) {
        ctx.out() += '[';
        scvt::fmt_integer(ctx.out(), Period::type::num);
        ctx.out() += string_literal<typename FmtCtx::char_type, ']', 's'>{}();
    }
};

#define UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(ratio, ...) \
    template<> \
    struct duration_suffix_writer<ratio> { \
        template<typename FmtCtx> \
        void write(FmtCtx& ctx) { \
            ctx.out() += string_literal<typename FmtCtx::char_type, __VA_ARGS__>{}(); \
        } \
    };
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::atto, 'a', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::femto, 'f', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::pico, 'p', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::nano, 'n', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::micro, 'u', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::milli, 'm', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::centi, 'c', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::deci, 'd', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::ratio<1>, 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::deca, 'd', 'a', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::hecto, 'h', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::kilo, 'k', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::mega, 'M', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::giga, 'G', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::tera, 'T', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::peta, 'P', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::exa, 'E', 's')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::ratio<60>, 'm', 'i', 'n')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::ratio<3600>, 'h')
UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX(std::ratio<86400>, 'd')
#undef UXS_FMT_IMPLEMENT_CHRONO_DURATION_SUFFIX

template<typename DeriverFormatterTy, typename Ty, typename CharT>
struct chrono_formatter {
 private:
    fmt_opts opts_;
    std::size_t width_arg_id_ = dynamic_extent;
    std::size_t prec_arg_id_ = dynamic_extent;
    std::string_view fmt_;

    template<typename FmtCtx>
    void format_impl(FmtCtx& ctx, chrono_specs& specs, const Ty& val) const {
        if (fmt_.empty()) {
            specs.spec = chrono_specifier::default_spec;
            specs.spec_char = specs.modifier = '\0';
            DeriverFormatterTy::template value_writer<FmtCtx>(ctx, specs, val);
            return;
        }
        auto it0 = fmt_.begin(), it = it0;
        while (true) {
            auto first = it;
            specs.spec = parse_chrono_format_spec(it, fmt_.end(), specs.modifier);
            if (specs.spec == chrono_specifier::end_of_format) { break; }
            if (specs.spec != chrono_specifier::ordinary_char) {
                ctx.out() += to_string_view(it0, first);
                it0 = it;
                switch (specs.spec) {
                    case chrono_specifier::percent: ctx.out() += '%'; break;
                    case chrono_specifier::new_line: ctx.out() += '\n'; break;
                    case chrono_specifier::tab: ctx.out() += '\t'; break;
                    default: {
                        specs.spec_char = *(it - 1);
                        DeriverFormatterTy::template value_writer<FmtCtx>(ctx, specs, val);
                    } break;
                }
            }
        }
        ctx.out() += to_string_view(it0, it);
    }

 public:
    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        if (it == ctx.end() || *it != ':') { return it; }
        it = ParseCtx::parse_standard(ctx, it + 1, opts_, width_arg_id_, prec_arg_id_);
        if ((!is_floating_point_duration<Ty>::value && opts_.prec >= 0) ||
            !!(opts_.flags & ~(fmt_flags::adjust_field | fmt_flags::localize))) {
            ParseCtx::syntax_error();
        }
        if (it == ctx.end() || *it != '%') { return it; }
        auto it0 = it;
        while (true) {
            char modifier = '\0';
            auto spec = parse_chrono_format_spec(it, ctx.end(), modifier);
            if (spec == chrono_specifier::end_of_format) { break; }
            switch (spec) {
                case chrono_specifier::percent:
                case chrono_specifier::new_line:
                case chrono_specifier::tab: break;
                default: {
                    if (!DeriverFormatterTy::spec_checker(spec)) {
                        throw format_error("unacceptable chrono specifier");
                    }
                } break;
            }
        }
        fmt_ = to_string_view(it0, it);
        return it;
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const Ty& val) const {
        chrono_specs specs;
        specs.opts = opts_;
        if (width_arg_id_ != dynamic_extent) {
            specs.opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(specs.opts.width)>();
        }
        if (prec_arg_id_ != dynamic_extent) {
            specs.opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(specs.opts.prec)>();
        }
        if (specs.opts.width == 0) { return format_impl(ctx, specs, val); }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        format_impl(buf_ctx, specs, val);
        const unsigned len = static_cast<unsigned>(buf.size());
        const auto fn = [&buf](basic_membuffer<CharT>& s) { s.append(buf.data(), buf.size()); };
        return specs.opts.width > len ? append_adjusted(ctx.out(), fn, len, specs.opts) : fn(ctx.out());
    }
};

}  // namespace detail

template<typename CharT, typename Rep, typename Period>
struct formatter<std::chrono::duration<Rep, Period>, CharT>
    : detail::chrono_formatter<formatter<std::chrono::duration<Rep, Period>, CharT>, std::chrono::duration<Rep, Period>,
                               CharT> {
 private:
    using value_type = std::chrono::duration<Rep, Period>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::hours_24 && spec <= detail::chrono_specifier::unit_suffix;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        if (specs.spec >= detail::chrono_specifier::hours_24 && specs.spec <= detail::chrono_specifier::locale_time) {
            detail::format_chrono_time(ctx, specs, val);
        } else if (specs.spec == detail::chrono_specifier::ticks) {
            to_basic_string(ctx.out(), *ctx.locale(), val.count(), fmt_opts{specs.opts.flags, specs.opts.prec});
        } else if (specs.spec == detail::chrono_specifier::unit_suffix) {
            detail::duration_suffix_writer<typename Period::type>{}.write(ctx);
        } else {
            to_basic_string(ctx.out(), *ctx.locale(), val.count(), fmt_opts{specs.opts.flags, specs.opts.prec});
            detail::duration_suffix_writer<typename Period::type>{}.write(ctx);
        }
    }
};

template<typename CharT>
struct formatter<std::chrono::year, CharT>
    : detail::chrono_formatter<formatter<std::chrono::year, CharT>, std::chrono::year, CharT> {
 private:
    using value_type = std::chrono::year;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::century && spec <= detail::chrono_specifier::year_yyyy;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        detail::format_chrono_year(ctx, specs, val);
    }
};

template<typename CharT>
struct formatter<std::chrono::month, CharT>
    : detail::chrono_formatter<formatter<std::chrono::month, CharT>, std::chrono::month, CharT> {
 private:
    using value_type = std::chrono::month;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::month_brief && spec <= detail::chrono_specifier::month_mm;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        detail::format_chrono_month(ctx, specs, val);
    }
};

template<typename CharT>
struct formatter<std::chrono::day, CharT>
    : detail::chrono_formatter<formatter<std::chrono::day, CharT>, std::chrono::day, CharT> {
 private:
    using value_type = std::chrono::day;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::day_zero && spec <= detail::chrono_specifier::day_space;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        detail::format_chrono_day(ctx, specs, val);
    }
};

template<typename CharT>
struct formatter<std::chrono::year_month_day, CharT>
    : detail::chrono_formatter<formatter<std::chrono::year_month_day, CharT>, std::chrono::year_month_day, CharT> {
 private:
    using value_type = std::chrono::year_month_day;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::century && spec <= detail::chrono_specifier::locale_date;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        if (!val.ok()) { detail::format_chrono_out_of_bounds(); }
        detail::format_chrono_date(ctx, specs, val);
    }
};

template<typename CharT, typename Duration>
struct formatter<std::chrono::hh_mm_ss<Duration>, CharT>
    : detail::chrono_formatter<formatter<std::chrono::hh_mm_ss<Duration>, CharT>, std::chrono::hh_mm_ss<Duration>, CharT> {
 private:
    using value_type = std::chrono::hh_mm_ss<Duration>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::hours_24 && spec <= detail::chrono_specifier::locale_time;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        detail::format_chrono_time(ctx, specs, val);
    }
};

template<typename CharT, typename Duration>
struct formatter<std::chrono::sys_time<Duration>, CharT>
    : detail::chrono_formatter<formatter<std::chrono::sys_time<Duration>, CharT>, std::chrono::sys_time<Duration>, CharT> {
 private:
    using value_type = std::chrono::sys_time<Duration>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec != detail::chrono_specifier::ticks && spec != detail::chrono_specifier::unit_suffix;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, const detail::chrono_specs& specs, value_type val) {
        const auto days = std::chrono::floor<std::chrono::days>(val);
        if (specs.spec >= detail::chrono_specifier::century && specs.spec <= detail::chrono_specifier::locale_date) {
            return detail::format_chrono_date(ctx, specs, std::chrono::year_month_day{days});
        } else if (specs.spec >= detail::chrono_specifier::hours_24 &&
                   specs.spec <= detail::chrono_specifier::locale_time) {
            return detail::format_chrono_time(ctx, specs, std::chrono::hh_mm_ss{val - days});
        } else if (specs.spec == detail::chrono_specifier::default_spec) {
            detail::format_chrono_date(ctx, specs, std::chrono::year_month_day{days});
            ctx.out() += ' ';
            return detail::format_chrono_time(ctx, specs, std::chrono::hh_mm_ss{val - days});
        }
        std::tm tm{};
        detail::make_tm_for_date(tm, std::chrono::year_month_day{days});
        detail::make_tm_for_time(tm, std::chrono::hh_mm_ss{val - days});
        detail::format_chrono_locale(ctx, specs, tm);
    }
};

}  // namespace uxs
