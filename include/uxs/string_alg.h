#pragma once

#include "chars.h"
#include "string_util.h"

#include <algorithm>
#include <vector>

namespace uxs {

enum class split_opts { no_opts = 0, skip_empty = 1 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(split_opts);

namespace detail {

template<typename CharT, typename Traits>
struct string_finder {
    CharT ch;
    using is_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit string_finder(CharT tgt) : ch(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        for (; begin != end; ++begin) {
            if (Traits::eq(*begin, '\\')) {
                if (++begin == end) { break; }
            } else if (Traits::eq(*begin, ch)) {
                return std::make_pair(begin, begin + 1);
            }
        }
        return std::make_pair(end, end);
    }
};

template<typename CharT, typename Traits>
struct reversed_string_finder {
    CharT ch;
    using is_reversed_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit reversed_string_finder(CharT tgt) : ch(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        while (begin != end) {
            if (begin != --end && Traits::eq(*(end - 1), '\\')) {
            } else if (Traits::eq(*end, ch)) {
                return std::make_pair(end, end + 1);
            }
        }
        return std::make_pair(begin, begin);
    }
};

template<typename CharT, typename Traits>
struct string_finder<std::basic_string_view<CharT, Traits>, Traits> {
    std::basic_string_view<CharT, Traits> s;
    using is_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit string_finder(std::basic_string_view<CharT, Traits> tgt) : s(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<std::size_t>(end - begin) < s.size()) { return std::make_pair(end, end); }
        if (!s.size()) { return std::make_pair(begin, begin); }
        for (iterator last = end - s.size() + 1; begin != last; ++begin) {
            if (std::equal(s.begin(), s.end(), begin, Traits::eq)) { return std::make_pair(begin, begin + s.size()); }
        }
        return std::make_pair(end, end);
    }
};

template<typename CharT, typename Traits>
struct reversed_string_finder<std::basic_string_view<CharT, Traits>, Traits> {
    std::basic_string_view<CharT, Traits> s;
    using is_reversed_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit reversed_string_finder(std::basic_string_view<CharT, Traits> tgt) : s(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<std::size_t>(end - begin) < s.size()) { return std::make_pair(begin, begin); }
        if (!s.size()) { return std::make_pair(end, end); }
        for (end -= s.size() - 1; begin != end; --end) {
            if (std::equal(s.begin(), s.end(), end - 1, Traits::eq)) {
                return std::make_pair(end - 1, end - 1 + s.size());
            }
        }
        return std::make_pair(begin, begin);
    }
};

}  // namespace detail

inline detail::string_finder<char, std::char_traits<char>> sfinder(char ch) {
    return detail::string_finder<char, std::char_traits<char>>(ch);
}
inline detail::reversed_string_finder<char, std::char_traits<char>> rsfinder(char ch) {
    return detail::reversed_string_finder<char, std::char_traits<char>>(ch);
}
inline detail::string_finder<std::string_view, std::char_traits<char>> sfinder(std::string_view s) {
    return detail::string_finder<std::string_view, std::char_traits<char>>(s);
}
inline detail::reversed_string_finder<std::string_view, std::char_traits<char>> rsfinder(std::string_view s) {
    return detail::reversed_string_finder<std::string_view, std::char_traits<char>>(s);
}

inline detail::string_finder<wchar_t, std::char_traits<wchar_t>> sfinder(wchar_t ch) {
    return detail::string_finder<wchar_t, std::char_traits<wchar_t>>(ch);
}
inline detail::reversed_string_finder<wchar_t, std::char_traits<wchar_t>> rsfinder(wchar_t ch) {
    return detail::reversed_string_finder<wchar_t, std::char_traits<wchar_t>>(ch);
}
inline detail::string_finder<std::wstring_view, std::char_traits<wchar_t>> sfinder(std::wstring_view s) {
    return detail::string_finder<std::wstring_view, std::char_traits<wchar_t>>(s);
}
inline detail::reversed_string_finder<std::wstring_view, std::char_traits<wchar_t>> rsfinder(std::wstring_view s) {
    return detail::reversed_string_finder<std::wstring_view, std::char_traits<wchar_t>>(s);
}

// --------------------------

template<typename CharT, typename Traits, typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::basic_string<CharT, Traits> replace_strings_generic(std::basic_string_view<CharT, Traits> s, Finder finder,
                                                         std::basic_string_view<CharT, Traits> with) {
    std::basic_string<CharT, Traits> result;
    result.reserve(s.size());
    auto p = s.begin();
    while (p != s.end()) {
        const auto sub = finder(p, s.end());
        result += to_string_view(p, sub.first);
        if (sub.first != sub.second) { result += with; }
        p = sub.second;
    }
    return result;
}

template<typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::string replace_strings(std::string_view s, Finder finder, std::string_view with) {
    return replace_strings_generic(s, finder, with);
}

template<typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::wstring replace_strings(std::wstring_view s, Finder finder, std::wstring_view with) {
    return replace_strings_generic(s, finder, with);
}

// --------------------------

template<typename StrTy, typename Range, typename SepTy, typename JoinFn = grow>
void join_strings_append(StrTy& out, const Range& r, SepTy sep, JoinFn fn = JoinFn{}) {
    auto first = std::begin(r);
    const auto last = std::end(r);
    if (first == last) { return; }
    while (true) {
        fn(out, *first);
        if (++first == last) { break; }
        out += sep;
    }
}

template<typename Range, typename SepTy, typename JoinFn = grow>
std::string join_strings(const Range& r, SepTy sep, std::string prefix = {}, JoinFn fn = JoinFn{}) {
    join_strings_append(prefix, r, sep, fn);
    return prefix;
}

template<typename Range, typename SepTy, typename JoinFn = grow>
std::wstring join_strings(const Range& r, SepTy sep, std::wstring prefix = {}, JoinFn fn = JoinFn{}) {
    join_strings_append(prefix, r, sep, fn);
    return prefix;
}

// --------------------------

template<typename OutputIt>
struct split_string_result {
#if __cplusplus < 201703L
    split_string_result(OutputIt out, std::size_t count) : out(out), count(count) {}
#endif  // __cplusplus < 201703L
    OutputIt out;
    std::size_t count;
};

template<split_opts Opts, typename CharT, typename Traits, typename Finder, typename OutputIt,
         typename OutputFn = nofunc, typename = std::void_t<typename Finder::is_finder>>
split_string_result<OutputIt> split_string_generic(std::basic_string_view<CharT, Traits> s, Finder finder, OutputIt out,
                                                   OutputFn fn = OutputFn{},
                                                   std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    if (!max_count) { return {out, 0}; }
    std::size_t count = 0;
    auto p = s.begin();
    while (true) {
        const auto sub = finder(p, s.end());
        if (!(Opts & split_opts::skip_empty) || p != sub.first) {
            *out++ = fn(s.substr(p - s.begin(), sub.first - p));
            if (++count == max_count) { break; }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return {out, count};
}

template<split_opts Opts = split_opts::no_opts, typename Finder, typename OutputIt, typename OutputFn = nofunc,
         typename = std::void_t<typename Finder::is_finder>>
split_string_result<OutputIt> split_string_to(std::string_view s, Finder finder, OutputIt out, OutputFn fn = OutputFn{},
                                              std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return split_string_generic<Opts>(s, finder, out, fn, max_count);
}

template<split_opts Opts = split_opts::no_opts, typename Finder, typename OutputFn = nofunc>
auto split_string(std::string_view s, Finder finder, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    split_string_generic<Opts>(s, finder, std::back_inserter(result), fn);
    return result;
}

template<split_opts Opts = split_opts::no_opts, typename Finder, typename OutputIt, typename OutputFn = nofunc,
         typename = std::void_t<typename Finder::is_finder>>
split_string_result<OutputIt> split_string_to(std::wstring_view s, Finder finder, OutputIt out,
                                              OutputFn fn = OutputFn{},
                                              std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return split_string_generic<Opts>(s, finder, out, fn, max_count);
}

template<split_opts Opts = split_opts::no_opts, typename Finder, typename OutputFn = nofunc>
auto split_string(std::wstring_view s, Finder finder, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    split_string_generic<Opts>(s, finder, std::back_inserter(result), fn);
    return result;
}

// --------------------------

template<split_opts Opts, typename CharT, typename Traits, typename Finder>
std::basic_string_view<CharT, Traits> string_section_generic(
    std::basic_string_view<CharT, Traits> s, Finder finder,
    est::type_identity_t<std::size_t, typename Finder::is_finder> start,
    std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    if (fin < start) { fin = start; }
    std::size_t count = 0;
    auto p = s.begin();
    auto from = s.end();
    while (true) {
        const auto sub = finder(p, s.end());
        if (!(Opts & split_opts::skip_empty) || p != sub.first) {
            if (count == start) { from = p; }
            if (count++ == fin) { return s.substr(from - s.begin(), sub.first - from); }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return s.substr(from - s.begin(), s.end() - from);
}

template<split_opts Opts = split_opts::no_opts, typename Finder>
std::string_view string_section(std::string_view s, Finder finder,
                                est::type_identity_t<std::size_t, typename Finder::is_finder> start,
                                std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    return string_section_generic<Opts>(s, finder, start, fin);
}

template<split_opts Opts = split_opts::no_opts, typename Finder>
std::wstring_view string_section(std::wstring_view s, Finder finder,
                                 est::type_identity_t<std::size_t, typename Finder::is_finder> start,
                                 std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    return string_section_generic<Opts>(s, finder, start, fin);
}

// --------------------------

template<split_opts Opts, typename CharT, typename Traits, typename Finder>
std::basic_string_view<CharT, Traits> string_section_generic(
    std::basic_string_view<CharT, Traits> s, Finder finder,
    est::type_identity_t<std::size_t, typename Finder::is_reversed_finder> start, std::size_t fin = 0) {
    if (fin > start) { fin = start; }
    std::size_t count = 0;
    auto p = s.end();
    auto to = s.begin();
    while (true) {
        const auto sub = finder(s.begin(), p);
        if (!(Opts & split_opts::skip_empty) || sub.second != p) {
            if (count == fin) { to = p; }
            if (count++ == start) { return s.substr(sub.second - s.begin(), to - sub.second); }
        }
        if (sub.second == s.begin()) { break; }
        p = sub.first;
    }
    return s.substr(0, to - s.begin());
}

template<split_opts Opts = split_opts::no_opts, typename Finder>
std::string_view string_section(std::string_view s, Finder finder,
                                est::type_identity_t<std::size_t, typename Finder::is_reversed_finder> start,
                                std::size_t fin = 0) {
    return string_section_generic<Opts>(s, finder, start, fin);
}

template<split_opts Opts = split_opts::no_opts, typename Finder>
std::wstring_view string_section(std::wstring_view s, Finder finder,
                                 est::type_identity_t<std::size_t, typename Finder::is_reversed_finder> start,
                                 std::size_t fin = 0) {
    return string_section_generic<Opts>(s, finder, start, fin);
}

// --------------------------

template<typename CharT, typename Traits, typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> string_to_words_generic(std::basic_string_view<CharT, Traits> s, CharT sep, OutputIt out,
                                                      OutputFn fn = OutputFn{},
                                                      std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    if (!max_count) { return {out, 0}; }
    std::size_t count = 0;
    enum class state_t { start = 0, sep_found, skip_sep } state = state_t::start;
    for (auto p = s.begin();; ++p) {
        while (p != s.end() && is_space(*p)) { ++p; }  // skip spaces
        const auto p0 = p;
        if (p == s.end()) {
            if (state != state_t::sep_found) { break; }
        } else {
            state_t prev_state = state;
            do {  // find separator or blank
                if (*p == '\\') {
                    if (++p == s.end()) { break; }
                } else if (is_space(*p)) {
                    state = state_t::skip_sep;
                    break;
                } else if (*p == sep) {
                    state = state_t::sep_found;
                    break;
                }
            } while (++p != s.end());
            if (p == p0 && prev_state == state_t::skip_sep) { continue; }
        }
        *out++ = fn(s.substr(p0 - s.begin(), p - p0));
        if (++count == max_count || p == s.end()) { break; }
    }
    return {out, count};
}

template<typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> string_to_words_to(std::string_view s, char sep, OutputIt out, OutputFn fn = OutputFn{},
                                                 std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return string_to_words_generic(s, sep, out, fn, max_count);
}

template<typename OutputFn = nofunc>
auto string_to_words(std::string_view s, char sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    string_to_words_generic(s, sep, std::back_inserter(result), fn);
    return result;
}

template<typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> string_to_words_to(std::wstring_view s, wchar_t sep, OutputIt out,
                                                 OutputFn fn = OutputFn{},
                                                 std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return string_to_words_generic(s, sep, out, fn, max_count);
}

template<typename OutputFn = nofunc>
auto string_to_words(std::wstring_view s, wchar_t sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    string_to_words_generic(s, sep, std::back_inserter(result), fn);
    return result;
}

// --------------------------

template<typename StrTy, typename Range, typename InputFn = nofunc>
void pack_strings_append(StrTy& out, const Range& r, typename StrTy::value_type sep, InputFn fn = InputFn{}) {
    auto first = std::begin(r);
    const auto last = std::end(r);
    if (first == last) { return; }
    while (true) {
        const auto el = fn(*first);
        auto p0 = std::begin(el);
        auto p = p0;
        const auto p_end = std::end(el);
        const bool is_empty = p0 == p_end;
        for (; p != p_end; ++p) {
            if (*p == '\\' || *p == sep) {
                out += to_string_view(p0, p);
                out += '\\';
                p0 = p;
            }
        }
        out += to_string_view(p0, p);
        if (++first == last) {
            if (is_empty) { out += sep; }
            break;
        }
        out += sep;
    }
}

template<typename Range, typename InputFn = nofunc>
std::string pack_strings(const Range& r, char sep, std::string prefix = {}, InputFn fn = InputFn{}) {
    pack_strings_append(prefix, r, sep, fn);
    return prefix;
}

template<typename Range, typename InputFn = nofunc>
std::wstring pack_strings(const Range& r, wchar_t sep, std::wstring prefix = {}, InputFn fn = InputFn{}) {
    pack_strings_append(prefix, r, sep, fn);
    return prefix;
}

// --------------------------

template<typename CharT, typename Traits, typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> unpack_strings_generic(std::basic_string_view<CharT, Traits> s, CharT sep, OutputIt out,
                                                     OutputFn fn = OutputFn{},
                                                     std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    if (!max_count) { return {out, 0}; }
    std::size_t count = 0;
    for (auto p = s.begin();; ++p) {
        std::basic_string<CharT, Traits> result;
        auto p0 = p;  // append chars till separator
        for (; p != s.end(); ++p) {
            if (*p == '\\') {
                result += to_string_view(p0, p);
                p0 = p + 1;
                if (++p == s.end()) { break; }
            } else if (*p == sep) {
                break;
            }
        }
        result += to_string_view(p0, p);
        if (p != s.end() || !result.empty()) {
            *out++ = fn(std::move(result));
            if (++count == max_count) { break; }
        }
        if (p == s.end()) { break; }
    }
    return {out, count};
}

template<typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> unpack_strings_to(std::string_view s, char sep, OutputIt out, OutputFn fn = OutputFn{},
                                                std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return unpack_strings_generic(s, sep, out, fn, max_count);
}

template<typename OutputFn = nofunc>
auto unpack_strings(std::string_view s, char sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(std::string()))>> {
    std::vector<std::decay_t<decltype(fn(std::string()))>> result;
    unpack_strings_generic(s, sep, std::back_inserter(result), fn);
    return result;
}

template<typename OutputIt, typename OutputFn = nofunc>
split_string_result<OutputIt> unpack_strings_to(std::wstring_view s, wchar_t sep, OutputIt out,
                                                OutputFn fn = OutputFn{},
                                                std::size_t max_count = std::numeric_limits<std::size_t>::max()) {
    return unpack_strings_generic(s, sep, out, fn, max_count);
}

template<typename OutputFn = nofunc>
auto unpack_strings(std::wstring_view s, wchar_t sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(std::wstring()))>> {
    std::vector<std::decay_t<decltype(fn(std::wstring()))>> result;
    unpack_strings_generic(s, sep, std::back_inserter(result), fn);
    return result;
}

// --------------------------

UXS_EXPORT std::string_view trim_string(std::string_view s);
UXS_EXPORT std::string encode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UXS_EXPORT std::string decode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UXS_EXPORT int compare_strings_nocase(std::string_view lhs, std::string_view rhs);
UXS_EXPORT std::string to_lower(std::string_view s);
UXS_EXPORT std::string to_upper(std::string_view s);

UXS_EXPORT std::wstring_view trim_string(std::wstring_view s);
UXS_EXPORT std::wstring encode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code);
UXS_EXPORT std::wstring decode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code);
UXS_EXPORT int compare_strings_nocase(std::wstring_view lhs, std::wstring_view rhs);
UXS_EXPORT std::wstring to_lower(std::wstring_view s);
UXS_EXPORT std::wstring to_upper(std::wstring_view s);

// --------------------------

template<typename Ty = void>
struct equal_to_nocase {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return compare_strings_nocase(lhs, rhs) == 0; }
};

template<typename Ty = void>
struct less_nocase {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return compare_strings_nocase(lhs, rhs) < 0; }
};

template<>
struct equal_to_nocase<void> {
    using is_transparent = int;
    template<typename TyL, typename TyR>
    bool operator()(const TyL& lhs, const TyR& rhs) const {
        return compare_strings_nocase(lhs, rhs) == 0;
    }
};

template<>
struct less_nocase<void> {
    using is_transparent = int;
    template<typename TyL, typename TyR>
    bool operator()(const TyL& lhs, const TyR& rhs) const {
        return compare_strings_nocase(lhs, rhs) < 0;
    }
};

}  // namespace uxs
