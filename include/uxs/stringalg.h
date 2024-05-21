#pragma once

#include "chars.h"
#include "functional.h"
#include "string_view.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace uxs {

enum class split_opts : unsigned { no_opts = 0, skip_empty = 1 };
UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(split_opts, unsigned);

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
std::basic_string<CharT, Traits> replace_basic_strings(std::basic_string_view<CharT, Traits> s, Finder finder,
                                                       std::basic_string_view<CharT, Traits> with) {
    std::basic_string<CharT, Traits> result;
    result.reserve(s.size());
    for (auto p = s.begin(); p != s.end();) {
        auto sub = finder(p, s.end());
        result.append(p, sub.first);
        if (sub.first != sub.second) { result += with; }
        p = sub.second;
    }
    return result;
}

template<typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::string replace_strings(std::string_view s, Finder finder, std::string_view with) {
    return replace_basic_strings(s, finder, with);
}

template<typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::wstring replace_strings(std::wstring_view s, Finder finder, std::wstring_view with) {
    return replace_basic_strings(s, finder, with);
}

// --------------------------

template<typename StrTy, typename Range, typename SepTy, typename JoinFn = grow>
StrTy& join_basic_strings(StrTy& s, const Range& r, SepTy sep, JoinFn fn = JoinFn{}) {
    if (std::begin(r) != std::end(r)) {
        for (auto it = std::begin(r);;) {
            fn(s, *it);
            if (++it != std::end(r)) {
                s += sep;
            } else {
                break;
            }
        }
    }
    return s;
}

template<typename Range, typename SepTy, typename JoinFn = grow>
std::string join_strings(const Range& r, SepTy sep, std::string prefix, JoinFn fn = JoinFn{}) {
    return std::move(join_basic_strings(prefix, r, sep, fn));
}

template<typename Range, typename SepTy, typename JoinFn = grow>
std::wstring join_strings(const Range& r, SepTy sep, std::wstring prefix, JoinFn fn = JoinFn{}) {
    return std::move(join_basic_strings(prefix, r, sep, fn));
}

// --------------------------

template<split_opts opts, typename CharT, typename Traits, typename Finder, typename OutputFn, typename OutputIt,
         typename = std::void_t<typename Finder::is_finder>>
std::size_t split_basic_string(std::basic_string_view<CharT, Traits> s, Finder finder, OutputFn fn, OutputIt out,
                               std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (!n) { return 0; }
    std::size_t count = 0;
    for (auto p = s.begin();;) {
        auto sub = finder(p, s.end());
        if (!(opts & split_opts::skip_empty) || p != sub.first) {
            *out++ = fn(s.substr(p - s.begin(), sub.first - p));
            if (++count == n) { break; }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return count;
}

template<split_opts opts = split_opts::no_opts, typename Finder, typename OutputFn, typename OutputIt,
         typename = std::void_t<typename Finder::is_finder>>
std::size_t split_string(std::string_view s, Finder finder, OutputFn fn, OutputIt out,
                         std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return split_basic_string<opts>(s, finder, fn, out, n);
}

template<split_opts opts = split_opts::no_opts, typename Finder, typename OutputFn = nofunc>
auto split_string(std::string_view s, Finder finder, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    split_string<opts>(s, finder, fn, std::back_inserter(result));
    return result;
}

template<split_opts opts = split_opts::no_opts, typename Finder, typename OutputFn, typename OutputIt,
         typename = std::void_t<typename Finder::is_finder>>
std::size_t split_string(std::wstring_view s, Finder finder, OutputFn fn, OutputIt out,
                         std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return split_basic_string<opts>(s, finder, fn, out, n);
}

template<split_opts opts = split_opts::no_opts, typename Finder, typename OutputFn = nofunc>
auto split_string(std::wstring_view s, Finder finder, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    split_string<opts>(s, finder, fn, std::back_inserter(result));
    return result;
}

// --------------------------

template<split_opts opts, typename CharT, typename Traits, typename Finder>
type_identity_t<std::basic_string_view<CharT, Traits>, typename Finder::is_finder> basic_string_section(
    std::basic_string_view<CharT, Traits> s, Finder finder, std::size_t start,
    std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    if (fin < start) { fin = start; }
    std::size_t count = 0;
    auto p = s.begin(), from = s.end();
    for (;;) {
        auto sub = finder(p, s.end());
        if (!(opts & split_opts::skip_empty) || p != sub.first) {
            if (count == start) { from = p; }
            if (count++ == fin) { return s.substr(from - s.begin(), sub.first - from); }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return s.substr(from - s.begin(), s.end() - from);
}

template<split_opts opts = split_opts::no_opts, typename Finder>
type_identity_t<std::string_view, typename Finder::is_finder> string_section(  //
    std::string_view s, Finder finder, std::size_t start, std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    return basic_string_section<opts>(s, finder, start, fin);
}

template<split_opts opts = split_opts::no_opts, typename Finder>
type_identity_t<std::wstring_view, typename Finder::is_finder> string_section(  //
    std::wstring_view s, Finder finder, std::size_t start, std::size_t fin = std::numeric_limits<std::size_t>::max()) {
    return basic_string_section<opts>(s, finder, start, fin);
}

// --------------------------

template<split_opts opts, typename CharT, typename Traits, typename Finder>
type_identity_t<std::basic_string_view<CharT, Traits>, typename Finder::is_reversed_finder> basic_string_section(
    std::basic_string_view<CharT, Traits> s, Finder finder, std::size_t start, std::size_t fin = 0) {
    if (fin > start) { fin = start; }
    std::size_t count = 0;
    auto p = s.end(), to = s.begin();
    for (;;) {
        auto sub = finder(s.begin(), p);
        if (!(opts & split_opts::skip_empty) || sub.second != p) {
            if (count == fin) { to = p; }
            if (count++ == start) { return s.substr(sub.second - s.begin(), to - sub.second); }
        }
        if (sub.second == s.begin()) { break; }
        p = sub.first;
    }
    return s.substr(0, to - s.begin());
}

template<split_opts opts = split_opts::no_opts, typename Finder>
type_identity_t<std::string_view, typename Finder::is_reversed_finder> string_section(  //
    std::string_view s, Finder finder, std::size_t start, std::size_t fin = 0) {
    return basic_string_section<opts>(s, finder, start, fin);
}

template<split_opts opts = split_opts::no_opts, typename Finder>
type_identity_t<std::wstring_view, typename Finder::is_reversed_finder> string_section(  //
    std::wstring_view s, Finder finder, std::size_t start, std::size_t fin = 0) {
    return basic_string_section<opts>(s, finder, start, fin);
}

// --------------------------

template<typename CharT, typename Traits, typename OutputFn, typename OutputIt>
std::size_t basic_string_to_words(std::basic_string_view<CharT, Traits> s, CharT sep, OutputFn fn, OutputIt out,
                                  std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (!n) { return 0; }
    std::size_t count = 0;
    enum class state_t : char { start = 0, sep_found, skip_sep } state = state_t::start;
    for (auto p = s.begin();; ++p) {
        while (p != s.end() && is_space(*p)) { ++p; }  // skip spaces
        auto p0 = p;
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
        if (++count == n || p == s.end()) { break; }
    }
    return count;
}

template<typename OutputFn, typename OutputIt>
std::size_t string_to_words(std::string_view s, char sep, OutputFn fn, OutputIt out,
                            std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return basic_string_to_words(s, sep, fn, out, n);
}

template<typename OutputFn = nofunc>
auto string_to_words(std::string_view s, char sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    string_to_words(s, sep, fn, std::back_inserter(result));
    return result;
}

template<typename OutputFn, typename OutputIt>
std::size_t string_to_words(std::wstring_view s, wchar_t sep, OutputFn fn, OutputIt out,
                            std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return basic_string_to_words(s, sep, fn, out, n);
}

template<typename OutputFn = nofunc>
auto string_to_words(std::wstring_view s, wchar_t sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    string_to_words(s, sep, fn, std::back_inserter(result));
    return result;
}

// --------------------------

template<typename StrTy, typename Range, typename InputFn = nofunc>
StrTy& pack_basic_strings(StrTy& s, const Range& r, typename StrTy::value_type sep, InputFn fn = InputFn{}) {
    if (std::begin(r) != std::end(r)) {
        for (auto it = std::begin(r);;) {
            auto el = fn(*it);
            auto p = std::begin(el), p0 = p;
            for (; p != std::end(el); ++p) {
                if (*p == '\\' || *p == sep) {
                    s.append(p0, p);
                    s += '\\';
                    p0 = p;
                }
            }
            s.append(p0, p);
            if (++it != std::end(r)) {
                s += sep;
            } else {
                if (std::begin(el) == std::end(el)) { s += sep; }
                break;
            }
        }
    }
    return s;
}

template<typename Range, typename InputFn = nofunc>
std::string pack_strings(const Range& r, char sep, std::string prefix, InputFn fn = InputFn{}) {
    return std::move(pack_basic_strings(prefix, r, sep, fn));
}

template<typename Range, typename InputFn = nofunc>
std::wstring pack_strings(const Range& r, wchar_t sep, std::wstring prefix, InputFn fn = InputFn{}) {
    return std::move(pack_basic_strings(prefix, r, sep, fn));
}

// --------------------------

template<typename CharT, typename Traits, typename OutputFn, typename OutputIt>
std::size_t unpack_basic_strings(std::basic_string_view<CharT, Traits> s, CharT sep, OutputFn fn, OutputIt out,
                                 std::size_t n = std::numeric_limits<std::size_t>::max()) {
    if (!n) { return 0; }
    std::size_t count = 0;
    for (auto p = s.begin();; ++p) {
        std::basic_string<CharT, Traits> result;
        auto p0 = p;  // append chars till separator
        for (; p != s.end(); ++p) {
            if (*p == '\\') {
                result.append(p0, p);
                p0 = p + 1;
                if (++p == s.end()) { break; }
            } else if (*p == sep) {
                break;
            }
        }
        result.append(p0, p);
        if (p != s.end() || !result.empty()) {
            *out++ = fn(std::move(result));
            if (++count == n) { break; }
        }
        if (p == s.end()) { break; }
    }
    return count;
}

template<typename OutputFn, typename OutputIt>
std::size_t unpack_strings(std::string_view s, char sep, OutputFn fn, OutputIt out,
                           std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return unpack_basic_strings(s, sep, fn, out, n);
}

template<typename OutputFn, typename OutputIt>
std::size_t unpack_strings(std::wstring_view s, wchar_t sep, OutputFn fn, OutputIt out,
                           std::size_t n = std::numeric_limits<std::size_t>::max()) {
    return unpack_basic_strings(s, sep, fn, out, n);
}

// --------------------------

UXS_EXPORT std::wstring from_utf8_to_wide(std::string_view s);
UXS_EXPORT std::string from_wide_to_utf8(std::wstring_view s);

UXS_EXPORT std::string_view trim_string(std::string_view s);
UXS_EXPORT std::vector<std::string> unpack_strings(std::string_view s, char sep);
UXS_EXPORT std::string encode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UXS_EXPORT std::string decode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UXS_EXPORT std::pair<unsigned, unsigned> parse_flag_string(
    std::string_view s, const std::vector<std::pair<std::string_view, unsigned>>& flag_tbl);
UXS_EXPORT int compare_strings_nocase(std::string_view lhs, std::string_view rhs);
UXS_EXPORT std::string to_lower(std::string_view s);
UXS_EXPORT std::string to_upper(std::string_view s);

UXS_EXPORT std::wstring_view trim_string(std::wstring_view s);
UXS_EXPORT std::vector<std::wstring> unpack_strings(std::wstring_view s, wchar_t sep);
UXS_EXPORT std::wstring encode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code);
UXS_EXPORT std::wstring decode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code);
UXS_EXPORT std::pair<unsigned, unsigned> parse_flag_string(
    std::wstring_view s, const std::vector<std::pair<std::wstring_view, unsigned>>& flag_tbl);
UXS_EXPORT int compare_strings_nocase(std::wstring_view lhs, std::wstring_view rhs);
UXS_EXPORT std::string to_lower(std::string_view s);
UXS_EXPORT std::wstring to_upper(std::wstring_view s);

template<typename CharT>
struct utf8_string_converter;
template<>
struct utf8_string_converter<char> {
    static std::string_view from(std::string_view s) { return s; }
    static std::string_view to(std::string_view s) { return s; }
};
template<>
struct utf8_string_converter<wchar_t> {
    static std::wstring from(std::string_view s) { return from_utf8_to_wide(s); }
    static std::string to(std::wstring_view s) { return from_wide_to_utf8(s); }
};

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

template<typename StrTy, typename Func = nofunc>
is_equal_to_predicate<StrTy, Func, equal_to_nocase<>> is_equal_to_nocase(const StrTy& s, const Func& fn = Func{}) {
    return is_equal_to_predicate<StrTy, Func, equal_to_nocase<>>(s, fn);
}

}  // namespace uxs
