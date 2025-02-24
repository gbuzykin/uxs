#pragma once

#include "format_base.h"

#include <chrono>
#include <ctime>
#include <ios>
#include <ostream>
#include <ratio>
#include <streambuf>

namespace uxs {

namespace detail {

enum class chrono_specifier {
    end_of_format = 0,
    ordinary_char,
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
    weekday_brief,
    weekday_full,
    weekday_1_7,
    weekday_0_6,
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
    hours,
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

template<typename Duration>
struct local_time_format_t {
    std::chrono::sys_time<Duration> time;
    const std::string& abbrev;
    std::chrono::seconds offset;
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
            if (check_chrono_modifier(modifier)) { return chrono_specifier::weekday_brief; }
        } break;
        case 'A': {
            if (check_chrono_modifier(modifier)) { return chrono_specifier::weekday_full; }
        } break;
        case 'u': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::weekday_1_7; }
        } break;
        case 'w': {
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::weekday_0_6; }
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
            if (check_chrono_modifier(modifier, 'O')) { return chrono_specifier::hours; }
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

template<typename FmtCtx>
void format_chrono_locale(FmtCtx& ctx, const std::tm& tm, char spec, char modifier, fmt_opts opts) {
    using char_type = typename FmtCtx::char_type;
    formatbuf<FmtCtx, std::basic_streambuf<char_type>> format_buf(ctx);
    std::basic_ostream<char_type> os(&format_buf);
    auto loc = !!(opts.flags & fmt_flags::localize) ? *ctx.locale() : std::locale::classic();
    os.imbue(loc);
    const auto& facet = std::use_facet<std::time_put<char_type>>(loc);
    auto end = facet.put(os, os, ' ', &tm, spec, modifier);
    if (end.failed()) { throw format_error("failed to format time"); }
}

template<typename FmtCtx>
void format_chrono_locale(FmtCtx& ctx, const std::tm& tm, const chrono_specs& specs) {
    format_chrono_locale(ctx, tm, specs.spec_char, specs.modifier, specs.opts);
}

inline bool is_locale_classic(locale_ref loc, fmt_opts opts) {
    return !(opts.flags & fmt_flags::localize) || *loc == std::locale::classic();
}

template<typename FmtCtx>
void format_append_2digs(FmtCtx& ctx, int v) {
    assert(v >= 0 && v < 100);
    const char* digs = scvt::get_digits(v);
    if constexpr (std::is_same_v<typename FmtCtx::char_type, char>) {
        ctx.out().append(digs, 2);
    } else {
        ctx.out() += digs[0];
        ctx.out() += digs[1];
    }
}

inline void format_chrono_out_of_bounds() { throw format_error("time point is out-of-bounds"); }

// --- year ---

template<typename FmtCtx>
void format_chrono_century(FmtCtx& ctx, std::chrono::year y) {
    const int year = std::min(std::max(static_cast<int>(y), -9900), 9999);
    int century = (year >= 0 ? year : year - 99) / 100;
    if (century < 0) { ctx.out() += '-', century = -century; }
    format_append_2digs(ctx, century);
}

template<typename FmtCtx>
void format_chrono_year_yy(FmtCtx& ctx, std::chrono::year y) {
    const int year = static_cast<int>(y) % 100;
    format_append_2digs(ctx, year >= 0 ? year : year + 100);
}

template<typename FmtCtx>
void format_chrono_year_yyyy(FmtCtx& ctx, std::chrono::year y) {
    int year = std::min(std::max(static_cast<int>(y), -9999), 9999);
    if (year < 0) { ctx.out() += '-', year = -year; }
    format_append_2digs(ctx, year / 100);
    format_append_2digs(ctx, year % 100);
}

template<typename FmtCtx>
void format_chrono_year(FmtCtx& ctx, std::chrono::year y, const chrono_specs& specs) {
    if (!specs.modifier) {
        switch (specs.spec) {
            case chrono_specifier::century: return format_chrono_century(ctx, y);
            case chrono_specifier::year_yy: return format_chrono_year_yy(ctx, y);
            case chrono_specifier::year_yyyy: return format_chrono_year_yyyy(ctx, y);
            default: break;
        }
    }
    if (!y.ok()) { format_chrono_out_of_bounds(); }
    std::tm tm{};
    tm.tm_year = static_cast<int>(y) - 1900;
    format_chrono_locale(ctx, tm, specs);
}

// --- month ---

template<typename FmtCtx>
void format_chrono_month_brief(FmtCtx& ctx, std::chrono::month m) {
    using char_type = typename FmtCtx::char_type;
    const unsigned month = static_cast<unsigned>(m);
    assert(month >= 1 && month <= 12);
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
    assert(month >= 1 && month <= 12);
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
    const unsigned month = std::min(static_cast<unsigned>(m), 99U);
    format_append_2digs(ctx, month);
}

template<typename FmtCtx>
void format_chrono_month(FmtCtx& ctx, std::chrono::month m, const chrono_specs& specs) {
    if (!specs.modifier) {
        const bool is_classic = is_locale_classic(ctx.locale(), specs.opts);
        switch (specs.spec) {
            case chrono_specifier::month_brief: {
                if (is_classic) {
                    if (!m.ok()) { format_chrono_out_of_bounds(); }
                    return format_chrono_month_brief(ctx, m);
                }
            } break;
            case chrono_specifier::month_full: {
                if (is_classic) {
                    if (!m.ok()) { format_chrono_out_of_bounds(); }
                    return format_chrono_month_full(ctx, m);
                }
            } break;
            case chrono_specifier::month_mm: return format_chrono_month_mm(ctx, m);
            default: break;
        }
    }
    if (!m.ok()) { format_chrono_out_of_bounds(); }
    std::tm tm{};
    tm.tm_mon = static_cast<unsigned>(m) - 1;
    format_chrono_locale(ctx, tm, specs);
}

// --- day ---

template<typename FmtCtx>
void format_chrono_day_dd(FmtCtx& ctx, std::chrono::day d) {
    format_append_2digs(ctx, std::min(static_cast<unsigned>(d), 99U));
}

template<typename FmtCtx>
void format_chrono_day_dd_space(FmtCtx& ctx, std::chrono::day d) {
    const unsigned day = std::min(static_cast<unsigned>(d), 99U);
    if (day >= 10) { return format_append_2digs(ctx, day); }
    ctx.out() += ' ';
    ctx.out() += '0' + day;
}

template<typename FmtCtx>
void format_chrono_day(FmtCtx& ctx, std::chrono::day d, const chrono_specs& specs) {
    if (!specs.modifier) {
        switch (specs.spec) {
            case chrono_specifier::day_zero: return format_chrono_day_dd(ctx, d);
            case chrono_specifier::day_space: return format_chrono_day_dd_space(ctx, d);
            default: break;
        }
    }
    if (!d.ok()) { format_chrono_out_of_bounds(); }
    std::tm tm{};
    tm.tm_mday = static_cast<unsigned>(d);
    format_chrono_locale(ctx, tm, specs);
}

// --- day of the week ---

template<typename FmtCtx>
void format_chrono_weekday_brief(FmtCtx& ctx, std::chrono::weekday wd) {
    using char_type = typename FmtCtx::char_type;
    assert(wd.c_encoding() <= 6);
    static UXS_CONSTEXPR std::basic_string_view<char_type> brief_name_list[] = {
        string_literal<char_type, 'S', 'u', 'n'>{}(), string_literal<char_type, 'M', 'o', 'n'>{}(),
        string_literal<char_type, 'T', 'u', 'e'>{}(), string_literal<char_type, 'W', 'e', 'd'>{}(),
        string_literal<char_type, 'T', 'h', 'u'>{}(), string_literal<char_type, 'F', 'r', 'i'>{}(),
        string_literal<char_type, 'S', 'a', 't'>{}(),
    };
    ctx.out() += brief_name_list[wd.c_encoding()];
}

template<typename FmtCtx>
void format_chrono_weekday_full(FmtCtx& ctx, std::chrono::weekday wd) {
    using char_type = typename FmtCtx::char_type;
    assert(wd.c_encoding() <= 6);
    static UXS_CONSTEXPR std::basic_string_view<char_type> full_name_list[] = {
        string_literal<char_type, 'S', 'u', 'n', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'M', 'o', 'n', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'T', 'u', 'e', 's', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'W', 'e', 'd', 'n', 'e', 's', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'T', 'h', 'u', 'r', 's', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'F', 'r', 'i', 'd', 'a', 'y'>{}(),
        string_literal<char_type, 'S', 'a', 't', 'u', 'r', 'd', 'a', 'y'>{}(),
    };
    ctx.out() += full_name_list[wd.c_encoding()];
}

template<typename FmtCtx>
void format_chrono_weekday(FmtCtx& ctx, std::chrono::weekday wd, const chrono_specs& specs) {
    if (!wd.ok()) { format_chrono_out_of_bounds(); }
    if (!specs.modifier) {
        const bool is_classic = is_locale_classic(ctx.locale(), specs.opts);
        switch (specs.spec) {
            case chrono_specifier::weekday_brief: {
                if (is_classic) { return format_chrono_weekday_brief(ctx, wd); }
            } break;
            case chrono_specifier::weekday_full: {
                if (is_classic) { return format_chrono_weekday_full(ctx, wd); }
            } break;
            case chrono_specifier::weekday_1_7: {
                ctx.out() += '0' + wd.iso_encoding();
                return;
            } break;
            case chrono_specifier::weekday_0_6: {
                ctx.out() += '0' + wd.c_encoding();
                return;
            } break;
            default: break;
        }
    }
    std::tm tm{};
    tm.tm_wday = wd.c_encoding();
    format_chrono_locale(ctx, tm, specs);
}

// --- date ---

template<typename FmtCtx>
void format_chrono_yyyy_mm_dd(FmtCtx& ctx, std::chrono::year_month_day ymd) {
    format_chrono_year_yyyy(ctx, ymd.year());
    ctx.out() += '-';
    format_chrono_month_mm(ctx, ymd.month());
    ctx.out() += '-';
    format_chrono_day_dd(ctx, ymd.day());
}

template<typename FmtCtx>
void format_chrono_mm_dd_yy(FmtCtx& ctx, std::chrono::year_month_day ymd) {
    format_chrono_month_mm(ctx, ymd.month());
    ctx.out() += '/';
    format_chrono_day_dd(ctx, ymd.day());
    ctx.out() += '/';
    format_chrono_year_yy(ctx, ymd.year());
}

template<typename FmtCtx>
void format_chrono_day_of_the_year(FmtCtx& ctx, std::chrono::year_month_day ymd) {
    const std::chrono::sys_days days{ymd};
    const int yday = (days - std::chrono::sys_days{ymd.year() / std::chrono::January / 0}).count();
    ctx.out() += '0' + yday / 100;
    format_append_2digs(ctx, yday % 100);
}

inline void make_tm_for_date(std::tm& tm, std::chrono::year_month_day ymd) {
    const std::chrono::sys_days days{ymd};
    tm.tm_year = static_cast<int>(ymd.year()) - 1900;
    tm.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
    tm.tm_mday = static_cast<unsigned>(ymd.day());
    tm.tm_yday = (days - std::chrono::sys_days{ymd.year() / std::chrono::January / 0}).count();
    tm.tm_wday = std::chrono::weekday{days}.c_encoding();
}

template<typename FmtCtx>
void format_chrono_date(FmtCtx& ctx, std::chrono::year_month_day ymd, const chrono_specs& specs) {
    switch (specs.spec) {
        // --- year ---
        case chrono_specifier::century:
        case chrono_specifier::year_yy:
        case chrono_specifier::year_yyyy: return format_chrono_year(ctx, ymd.year(), specs);
        // --- month ---
        case chrono_specifier::month_brief:
        case chrono_specifier::month_full:
        case chrono_specifier::month_mm: return format_chrono_month(ctx, ymd.month(), specs);
        // --- day ---
        case chrono_specifier::day_zero:
        case chrono_specifier::day_space: return format_chrono_day(ctx, ymd.day(), specs);
        // --- day of the week ---
        case chrono_specifier::weekday_brief:
        case chrono_specifier::weekday_full:
        case chrono_specifier::weekday_1_7:
        case chrono_specifier::weekday_0_6: return format_chrono_weekday(ctx, std::chrono::weekday{ymd}, specs);
        // --- day/week of the year ---
        case chrono_specifier::day_of_the_year: return format_chrono_day_of_the_year(ctx, ymd);
        // --- date ---
        case chrono_specifier::year_month_day: return format_chrono_yyyy_mm_dd(ctx, ymd);
        case chrono_specifier::month_day_year: return format_chrono_mm_dd_yy(ctx, ymd);
        default: break;
    }
    std::tm tm{};
    make_tm_for_date(tm, ymd);
    format_chrono_locale(ctx, tm, specs);
}

// --- time of day ---

template<typename FmtCtx>
void format_chrono_hours(FmtCtx& ctx, std::chrono::hours h) {
    const auto hours = h.count();
    assert(hours >= 0);
    if (hours >= 100) { scvt::fmt_integer(ctx.out(), hours / 100); }
    format_append_2digs(ctx, hours % 100);
}

template<typename FmtCtx>
void format_chrono_hours_12(FmtCtx& ctx, std::chrono::hours h) {
    const int hours = static_cast<int>(h.count() % 24);
    assert(hours >= 0);
    const int hours_12 = hours > 0 ? (hours <= 12 ? hours : hours - 12) : 12;
    format_append_2digs(ctx, hours_12);
}

template<typename FmtCtx>
void format_chrono_am_pm(FmtCtx& ctx, std::chrono::hours h) {
    using char_type = typename FmtCtx::char_type;
    const int hours = static_cast<int>(h.count() % 24);
    assert(hours >= 0);
    ctx.out() += hours < 12 ? string_literal<char_type, 'A', 'M'>{}() : string_literal<char_type, 'P', 'M'>{}();
}

template<typename FmtCtx>
void format_chrono_minutes(FmtCtx& ctx, std::chrono::minutes m) {
    const auto minutes = m.count();
    assert(minutes >= 0 && minutes < 60);
    format_append_2digs(ctx, static_cast<int>(minutes));
}

template<typename FmtCtx, typename Duration>
void format_chrono_seconds(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms, fmt_opts opts) {
    const auto seconds = hms.seconds().count();
    assert(seconds >= 0 && seconds < 60);
    format_append_2digs(ctx, static_cast<int>(seconds));
    if constexpr (std::chrono::hh_mm_ss<Duration>::fractional_width != 0) {
        const auto subsecs = static_cast<long long>(hms.subseconds().count());
        using char_type = typename FmtCtx::char_type;
        const char_type dec_point = !!(opts.flags & fmt_flags::localize) ?
                                        std::use_facet<std::numpunct<char_type>>(*ctx.locale()).decimal_point() :
                                        static_cast<char_type>('.');
        ctx.out() += dec_point;
        scvt::fmt_integer(ctx.out(), subsecs,
                          fmt_opts{fmt_flags::leading_zeroes, -1, std::chrono::hh_mm_ss<Duration>::fractional_width});
    }
}

template<typename FmtCtx, typename Duration>
void format_chrono_hh_mm(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms) {
    format_chrono_hours(ctx, hms.hours());
    ctx.out() += ':';
    format_chrono_minutes(ctx, hms.minutes());
}

template<typename FmtCtx, typename Duration>
void format_chrono_hh_mm_ss(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms, fmt_opts opts) {
    format_chrono_hours(ctx, hms.hours());
    ctx.out() += ':';
    format_chrono_minutes(ctx, hms.minutes());
    ctx.out() += ':';
    format_chrono_seconds(ctx, hms, opts);
}

template<typename Duration>
void make_tm_for_time(std::tm& tm, std::chrono::hh_mm_ss<Duration> hms) {
    tm.tm_hour = static_cast<int>(hms.hours().count() % 24);
    tm.tm_min = static_cast<int>(hms.minutes().count());
    tm.tm_sec = static_cast<int>(hms.seconds().count());
}

template<typename FmtCtx, typename Duration>
void format_chrono_time(FmtCtx& ctx, std::chrono::hh_mm_ss<Duration> hms, const chrono_specs& specs) {
    if (hms.is_negative()) { ctx.out() += '-'; }
    if (!specs.modifier) {
        const bool is_classic = is_locale_classic(ctx.locale(), specs.opts);
        switch (specs.spec) {
            case chrono_specifier::hours: return format_chrono_hours(ctx, hms.hours());
            case chrono_specifier::hours_12: return format_chrono_hours_12(ctx, hms.hours());
            case chrono_specifier::minutes: return format_chrono_minutes(ctx, hms.minutes());
            case chrono_specifier::seconds: return format_chrono_seconds(ctx, hms, specs.opts);
            case chrono_specifier::hours_am_pm: {
                if (is_classic) { return format_chrono_am_pm(ctx, hms.hours()); }
            } break;
            case chrono_specifier::hours_minutes: return format_chrono_hh_mm(ctx, hms);
            case chrono_specifier::hours_minutes_seconds: return format_chrono_hh_mm_ss(ctx, hms, specs.opts);
            default: break;
        }
    }
    std::tm tm{};
    make_tm_for_time(tm, hms);
    format_chrono_locale(ctx, tm, specs);
}

// --- date & time ---

template<typename FmtCtx, typename Duration>
void format_chrono_date_time(FmtCtx& ctx, std::chrono::sys_time<Duration> t, const chrono_specs& specs) {
    const auto days = std::chrono::floor<std::chrono::days>(t);
    if (specs.spec >= chrono_specifier::century && specs.spec <= chrono_specifier::locale_date) {
        return format_chrono_date(ctx, std::chrono::year_month_day{days}, specs);
    }
    if (specs.spec >= chrono_specifier::hours && specs.spec <= chrono_specifier::locale_time) {
        return format_chrono_time(ctx, std::chrono::hh_mm_ss{t - days}, specs);
    }
    std::tm tm{};
    make_tm_for_date(tm, std::chrono::year_month_day{days});
    make_tm_for_time(tm, std::chrono::hh_mm_ss{t - days});
    format_chrono_locale(ctx, tm, specs);
}

template<typename FmtCtx, typename Duration>
void format_chrono_yyyy_mm_dd_hh_mm_ss(FmtCtx& ctx, std::chrono::sys_time<Duration> t, fmt_opts opts) {
    const auto days = std::chrono::floor<std::chrono::days>(t);
    format_chrono_yyyy_mm_dd(ctx, std::chrono::year_month_day{days});
    ctx.out() += ' ';
    format_chrono_hh_mm_ss(ctx, std::chrono::hh_mm_ss{t - days}, opts);
}

// --------------------------

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
    std::size_t width_arg_id_ = unspecified_size;
    std::size_t prec_arg_id_ = unspecified_size;
    std::string_view fmt_;

    template<typename FmtCtx>
    void format_impl(FmtCtx& ctx, const Ty& val, chrono_specs& specs) const {
        if (fmt_.empty()) { return DeriverFormatterTy::template default_value_writer<FmtCtx>(ctx, val, specs.opts); }
        auto it0 = fmt_.begin();
        auto it = it0;
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
                        DeriverFormatterTy::template value_writer<FmtCtx>(ctx, val, specs);
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
        if (width_arg_id_ != unspecified_size) {
            specs.opts.width = ctx.arg(width_arg_id_).template get_unsigned<decltype(specs.opts.width)>();
        }
        if (prec_arg_id_ != unspecified_size) {
            specs.opts.prec = ctx.arg(prec_arg_id_).template get_unsigned<decltype(specs.opts.prec)>();
        }
        if (specs.opts.width == 0) { return format_impl(ctx, val, specs); }
        inline_basic_dynbuffer<CharT> buf;
        basic_format_context<CharT> buf_ctx{buf, ctx};
        format_impl(buf_ctx, val, specs);
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
        return spec >= detail::chrono_specifier::hours && spec <= detail::chrono_specifier::unit_suffix;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type d, const detail::chrono_specs& specs) {
        if (specs.spec == detail::chrono_specifier::ticks) {
            to_basic_string(ctx.out(), *ctx.locale(), d.count(), fmt_opts{specs.opts.flags, specs.opts.prec});
        } else if (specs.spec == detail::chrono_specifier::unit_suffix) {
            detail::duration_suffix_writer<typename Period::type>{}.write(ctx);
        } else {
            detail::format_chrono_time(ctx, std::chrono::hh_mm_ss{d}, specs);
        }
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type d, fmt_opts opts) {
        to_basic_string(ctx.out(), *ctx.locale(), d.count(), fmt_opts{opts.flags, opts.prec});
        detail::duration_suffix_writer<typename Period::type>{}.write(ctx);
    }
};

template<typename CharT>
struct formatter<std::chrono::year, CharT>
    : detail::chrono_formatter<formatter<std::chrono::year, CharT>, std::chrono::year, CharT> {
 private:
    using value_type = std::chrono::year;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;
    friend struct formatter<std::chrono::year_month, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::century && spec <= detail::chrono_specifier::year_yyyy;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type y, const detail::chrono_specs& specs) {
        detail::format_chrono_year(ctx, y, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type y, fmt_opts) {
        detail::format_chrono_year_yyyy(ctx, y);
        if (y.ok()) { return; }
        ctx.out() += string_literal<typename FmtCtx::char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v',
                                    'a', 'l', 'i', 'd', ' ', 'y', 'e', 'a', 'r'>{}();
    }
};

template<typename CharT>
struct formatter<std::chrono::month, CharT>
    : detail::chrono_formatter<formatter<std::chrono::month, CharT>, std::chrono::month, CharT> {
 private:
    using value_type = std::chrono::month;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;
    friend struct formatter<std::chrono::year_month, CharT>;
    friend struct formatter<std::chrono::month_day, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::month_brief && spec <= detail::chrono_specifier::month_mm;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type m, const detail::chrono_specs& specs) {
        detail::format_chrono_month(ctx, m, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type m, fmt_opts opts) {
        if (m.ok()) {
            if (detail::is_locale_classic(ctx.locale(), opts)) { return detail::format_chrono_month_brief(ctx, m); }
            std::tm tm{};
            tm.tm_mon = static_cast<unsigned>(m) - 1;
            return detail::format_chrono_locale(ctx, tm, 'b', '\0', opts);
        }
        scvt::fmt_integer(ctx.out(), static_cast<unsigned>(m));
        ctx.out() += string_literal<typename FmtCtx::char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v',
                                    'a', 'l', 'i', 'd', ' ', 'm', 'o', 'n', 't', 'h'>{}();
    }
};

template<typename CharT>
struct formatter<std::chrono::day, CharT>
    : detail::chrono_formatter<formatter<std::chrono::day, CharT>, std::chrono::day, CharT> {
 private:
    using value_type = std::chrono::day;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;
    friend struct formatter<std::chrono::month_day, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::day_zero && spec <= detail::chrono_specifier::day_space;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type d, const detail::chrono_specs& specs) {
        detail::format_chrono_day(ctx, d, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type d, fmt_opts) {
        detail::format_chrono_day_dd(ctx, d);
        if (d.ok()) { return; }
        ctx.out() += string_literal<typename FmtCtx::char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v',
                                    'a', 'l', 'i', 'd', ' ', 'd', 'a', 'y'>{}();
    }
};

template<typename CharT>
struct formatter<std::chrono::year_month, CharT>
    : detail::chrono_formatter<formatter<std::chrono::year_month, CharT>, std::chrono::year_month, CharT> {
 private:
    using value_type = std::chrono::year_month;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::century && spec <= detail::chrono_specifier::month_mm;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type ym, const detail::chrono_specs& specs) {
        if (specs.spec >= detail::chrono_specifier::century && specs.spec <= detail::chrono_specifier::year_yyyy) {
            detail::format_chrono_year(ctx, ym.year(), specs);
        } else {
            detail::format_chrono_month(ctx, ym.month(), specs);
        }
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type ym, fmt_opts opts) {
        formatter<std::chrono::year, CharT>::default_value_writer(ctx, ym.year(), opts);
        ctx.out() += '/';
        formatter<std::chrono::month, CharT>::default_value_writer(ctx, ym.month(), opts);
    }
};

template<typename CharT>
struct formatter<std::chrono::month_day, CharT>
    : detail::chrono_formatter<formatter<std::chrono::month_day, CharT>, std::chrono::month_day, CharT> {
 private:
    using value_type = std::chrono::month_day;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::month_brief && spec <= detail::chrono_specifier::day_space;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type md, const detail::chrono_specs& specs) {
        if (specs.spec >= detail::chrono_specifier::month_brief && specs.spec <= detail::chrono_specifier::month_mm) {
            detail::format_chrono_month(ctx, md.month(), specs);
        } else {
            detail::format_chrono_day(ctx, md.day(), specs);
        }
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type md, fmt_opts opts) {
        formatter<std::chrono::month, CharT>::default_value_writer(ctx, md.month(), opts);
        ctx.out() += '/';
        formatter<std::chrono::day, CharT>::default_value_writer(ctx, md.day(), opts);
    }
};

template<typename CharT>
struct formatter<std::chrono::weekday, CharT>
    : detail::chrono_formatter<formatter<std::chrono::weekday, CharT>, std::chrono::weekday, CharT> {
 private:
    using value_type = std::chrono::weekday;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::weekday_brief && spec <= detail::chrono_specifier::weekday_0_6;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type wd, const detail::chrono_specs& specs) {
        detail::format_chrono_weekday(ctx, wd, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type wd, fmt_opts opts) {
        if (wd.ok()) {
            if (detail::is_locale_classic(ctx.locale(), opts)) { return detail::format_chrono_weekday_brief(ctx, wd); }
            std::tm tm{};
            tm.tm_wday = wd.c_encoding();
            return detail::format_chrono_locale(ctx, tm, 'a', '\0', opts);
        }
        scvt::fmt_integer(ctx.out(), wd.c_encoding());
        ctx.out() += string_literal<typename FmtCtx::char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v',
                                    'a', 'l', 'i', 'd', ' ', 'w', 'e', 'e', 'k', 'd', 'a', 'y'>{}();
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
    static void value_writer(FmtCtx& ctx, value_type ymd, const detail::chrono_specs& specs) {
        if (!ymd.ok()) { detail::format_chrono_out_of_bounds(); }
        detail::format_chrono_date(ctx, ymd, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type ymd, fmt_opts) {
        detail::format_chrono_yyyy_mm_dd(ctx, ymd);
        if (ymd.ok()) { return; }
        ctx.out() += string_literal<typename FmtCtx::char_type, ' ', 'i', 's', ' ', 'n', 'o', 't', ' ', 'a', ' ', 'v',
                                    'a', 'l', 'i', 'd', ' ', 'd', 'a', 't', 'e'>{}();
    }
};

template<typename CharT, typename Duration>
struct formatter<std::chrono::hh_mm_ss<Duration>, CharT>
    : detail::chrono_formatter<formatter<std::chrono::hh_mm_ss<Duration>, CharT>, std::chrono::hh_mm_ss<Duration>, CharT> {
 private:
    using value_type = std::chrono::hh_mm_ss<Duration>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec >= detail::chrono_specifier::hours && spec <= detail::chrono_specifier::locale_time;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type hms, const detail::chrono_specs& specs) {
        detail::format_chrono_time(ctx, hms, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type hms, fmt_opts opts) {
        detail::format_chrono_hh_mm_ss(ctx, hms, opts);
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
    static void value_writer(FmtCtx& ctx, value_type t, const detail::chrono_specs& specs) {
        if (specs.spec == detail::chrono_specifier::time_zone) {
            ctx.out() += string_literal<typename FmtCtx::char_type, '+', '0', '0', '0', '0'>{}();
        } else if (specs.spec == detail::chrono_specifier::time_zone_abbreviation) {
            ctx.out() += string_literal<typename FmtCtx::char_type, 'U', 'T', 'C'>{}();
        } else {
            detail::format_chrono_date_time(ctx, t, specs);
        }
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type t, fmt_opts opts) {
        detail::format_chrono_yyyy_mm_dd_hh_mm_ss(ctx, t, opts);
    }
};

template<typename CharT, typename Duration>
struct formatter<std::chrono::local_time<Duration>, CharT>
    : detail::chrono_formatter<formatter<std::chrono::local_time<Duration>, CharT>, std::chrono::local_time<Duration>,
                               CharT> {
 private:
    using value_type = std::chrono::local_time<Duration>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec != detail::chrono_specifier::ticks && spec != detail::chrono_specifier::unit_suffix &&
               spec != detail::chrono_specifier::time_zone && spec != detail::chrono_specifier::time_zone_abbreviation;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type t, const detail::chrono_specs& specs) {
        detail::format_chrono_date_time(ctx, std::chrono::sys_time<Duration>{t.time_since_epoch()}, specs);
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type t, fmt_opts opts) {
        detail::format_chrono_yyyy_mm_dd_hh_mm_ss(ctx, std::chrono::sys_time<Duration>{t.time_since_epoch()}, opts);
    }
};

template<typename CharT, typename Duration>
struct formatter<detail::local_time_format_t<Duration>, CharT>
    : detail::chrono_formatter<formatter<detail::local_time_format_t<Duration>, CharT>,
                               detail::local_time_format_t<Duration>, CharT> {
 private:
    using value_type = detail::local_time_format_t<Duration>;
    friend struct detail::chrono_formatter<formatter, value_type, CharT>;

    static UXS_CONSTEXPR bool spec_checker(detail::chrono_specifier spec) {
        return spec != detail::chrono_specifier::ticks && spec != detail::chrono_specifier::unit_suffix;
    }

    template<typename FmtCtx>
    static void value_writer(FmtCtx& ctx, value_type t, const detail::chrono_specs& specs) {
        if (specs.spec == detail::chrono_specifier::time_zone) {
            std::chrono::hh_mm_ss offset{t.offset};
            ctx.out() += offset.is_negative() ? '-' : '+';
            detail::format_append_2digs(ctx, static_cast<int>(offset.hours().count() % 100));
            detail::format_append_2digs(ctx, static_cast<int>(offset.minutes().count()));
        } else if (specs.spec == detail::chrono_specifier::time_zone_abbreviation) {
            ctx.out() += t.abbrev;
        } else {
            detail::format_chrono_date_time(ctx, t.time, specs);
        }
    }

    template<typename FmtCtx>
    static void default_value_writer(FmtCtx& ctx, value_type t, fmt_opts opts) {
        detail::format_chrono_yyyy_mm_dd_hh_mm_ss(ctx, t.time, opts);
        ctx.out() += ' ';
        ctx.out() += t.abbrev;
    }
};

#if _MSC_VER >= 1930 || __GLIBCXX__ >= 20240904
template<typename CharT, typename Duration>
struct formatter<std::chrono::utc_time<Duration>, CharT> : formatter<std::chrono::sys_time<Duration>, CharT> {
    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::utc_time<Duration> t) const {
        formatter<std::chrono::sys_time<Duration>, CharT>::format(ctx, std::chrono::utc_clock::to_sys(t));
    }
};
#endif

namespace detail {
template<typename Clock, typename Duration>
auto cast_to_system_clock(std::chrono::time_point<Clock, Duration> t) -> decltype(Clock::to_sys(t)) {
    return Clock::to_sys(t);
}
template<typename Clock, typename Duration>
auto cast_to_system_clock(std::chrono::time_point<Clock, Duration> t)
    -> decltype(decltype(Clock::to_utc(t))::clock::to_sys(Clock::to_utc(t))) {
    return decltype(Clock::to_utc(t))::clock::to_sys(Clock::to_utc(t));
}
}  // namespace detail

template<typename CharT, typename Duration>
struct formatter<std::chrono::file_time<Duration>, CharT> : formatter<std::chrono::sys_time<Duration>, CharT> {
    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::file_time<Duration> t) const {
        formatter<std::chrono::sys_time<Duration>, CharT>::format(ctx, detail::cast_to_system_clock(t));
    }
};

#if _MSC_VER >= 1930 || __GLIBCXX__ >= 20240904
template<typename CharT, typename Duration, typename TimeZonePtr>
struct formatter<std::chrono::zoned_time<Duration, TimeZonePtr>, CharT>
    : formatter<detail::local_time_format_t<std::common_type_t<Duration, std::chrono::seconds>>, CharT> {
    template<typename FmtCtx>
    void format(FmtCtx& ctx, std::chrono::zoned_time<Duration, TimeZonePtr> t) const {
        using common_duration_type = std::common_type_t<Duration, std::chrono::seconds>;
        using formatter_type = formatter<detail::local_time_format_t<common_duration_type>, CharT>;
        const auto zone_info = t.get_info();
        formatter_type::format(ctx, {std::chrono::sys_time<Duration>{t.get_local_time().time_since_epoch()},
                                     zone_info.abbrev, zone_info.offset});
    }
};
#endif

}  // namespace uxs
