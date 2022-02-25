#pragma once

#include "functional.h"
#include "string_view.h"
#include "utf_cvt.h"

#include <cctype>
#include <vector>

namespace util {

enum class split_flags : unsigned { kNoFlags = 0, kSkipEmpty = 1 };

inline bool operator&(split_flags flags, split_flags mask) {
    return (static_cast<unsigned>(flags) & static_cast<unsigned>(mask)) != 0;
}

namespace detail {
template<typename Ty>
struct string_finder;
template<typename Ty>
struct reversed_string_finder;

template<>
struct string_finder<char> {
    char ch;
    using is_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit string_finder(char in_ch) : ch(in_ch) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        for (; begin < end; ++begin) {
            if (*begin == '\\') {
                if (++begin == end) { break; }
            } else if (*begin == ch) {
                return std::make_pair(begin, begin + 1);
            }
        }
        return std::make_pair(end, end);
    }
};

template<>
struct reversed_string_finder<char> {
    char ch;
    using is_reversed_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit reversed_string_finder(char in_ch) : ch(in_ch) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        while (begin < end) {
            if (begin < --end && *(end - 1) == '\\') {
            } else if (*end == ch) {
                return std::make_pair(end, end + 1);
            }
        }
        return std::make_pair(begin, begin);
    }
};

template<>
struct string_finder<std::string_view> {
    std::string_view s;
    using is_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit string_finder(std::string_view in_s) : s(in_s) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<size_t>(end - begin) < s.size()) { return std::make_pair(end, end); }
        for (iterator last = end - s.size(); begin <= last; ++begin) {
            if (std::equal(begin, begin + s.size(), s.begin())) { return std::make_pair(begin, begin + s.size()); }
        }
        return std::make_pair(end, end);
    }
};

template<>
struct reversed_string_finder<std::string_view> {
    std::string_view s;
    using is_reversed_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit reversed_string_finder(std::string_view in_s) : s(in_s) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<size_t>(end - begin) < s.size()) { return std::make_pair(begin, begin); }
        for (iterator last = begin + s.size(); last <= end; --end) {
            if (std::equal(end - s.size(), end, s.begin())) { return std::make_pair(end - s.size(), end); }
        }
        return std::make_pair(begin, begin);
    }
};
}  // namespace detail

template<typename Ty, typename = std::void_t<typename detail::string_finder<Ty>::is_finder>>
detail::string_finder<Ty> sfind(const Ty& v) {
    return detail::string_finder<Ty>(v);
}

template<typename Ty, typename = std::void_t<typename detail::reversed_string_finder<Ty>::is_reversed_finder>>
detail::reversed_string_finder<Ty> rsfind(const Ty& v) {
    return detail::reversed_string_finder<Ty>(v);
}

inline detail::string_finder<char> sfind(char ch) { return detail::string_finder<char>(ch); }
inline detail::reversed_string_finder<char> rsfind(char ch) { return detail::reversed_string_finder<char>(ch); }
inline detail::string_finder<std::string_view> sfind(std::string_view s) {
    return detail::string_finder<std::string_view>(s);
}
inline detail::reversed_string_finder<std::string_view> rsfind(std::string_view s) {
    return detail::reversed_string_finder<std::string_view>(s);
}

template<typename Finder, typename = std::void_t<typename Finder::is_finder>>
std::string replace_strings(std::string_view s, Finder finder, std::string_view with) {
    std::string result;
    result.reserve(s.size());
    for (auto p = s.begin(); p < s.end();) {
        auto sub = finder(p, s.end());
        result.append(p, sub.first);
        if (sub.first != sub.second) { result += with; }
        p = sub.second;
    }
    return result;
}

template<typename Range, typename SepTy, typename StrTy, typename JoinFn = grow>
StrTy& join_strings_append(StrTy& s, const Range& r, SepTy sep, JoinFn fn = JoinFn{}) {
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
    return std::move(join_strings_append(prefix, r, sep, fn));
}

template<split_flags flags = split_flags::kNoFlags, typename Finder, typename OutputFn,  //
         typename OutputIt, typename = std::void_t<typename Finder::is_finder>>
size_t split_string(std::string_view s, Finder finder, OutputFn fn, OutputIt out,
                    size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    size_t count = 0;
    for (auto p = s.begin();;) {
        auto sub = finder(p, s.end());
        if (!(flags & split_flags::kSkipEmpty) || p < sub.first) {
            *out++ = fn(s.substr(p - s.begin(), sub.first - p));
            if (++count == max_count) { break; }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return count;
}

template<split_flags flags = split_flags::kNoFlags, typename Finder, typename OutputFn = nofunc>
auto split_string(std::string_view s, Finder finder, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    split_string<flags>(s, finder, fn, std::back_inserter(result));
    return result;
}

template<split_flags flags = split_flags::kNoFlags, typename Finder>
type_identity_t<std::string_view, typename Finder::is_finder> string_section(  //
    std::string_view s, Finder finder, size_t start, size_t fin = std::numeric_limits<size_t>::max()) {
    if (fin < start) { fin = start; }
    size_t count = 0;
    auto p = s.begin(), from = s.end();
    for (;;) {
        auto sub = finder(p, s.end());
        if (!(flags & split_flags::kSkipEmpty) || p < sub.first) {
            if (count == start) { from = p; }
            if (count++ == fin) { return s.substr(from - s.begin(), sub.first - from); }
        }
        if (sub.first == s.end()) { break; }
        p = sub.second;
    }
    return s.substr(from - s.begin(), s.end() - from);
}

template<split_flags flags = split_flags::kNoFlags, typename Finder>
type_identity_t<std::string_view, typename Finder::is_reversed_finder> string_section(  //
    std::string_view s, Finder finder, size_t start, size_t fin = 0) {
    if (fin > start) { fin = start; }
    size_t count = 0;
    auto p = s.end(), to = s.begin();
    for (;;) {
        auto sub = finder(s.begin(), p);
        if (!(flags & split_flags::kSkipEmpty) || sub.second < p) {
            if (count == fin) { to = p; }
            if (count++ == start) { return s.substr(sub.second - s.begin(), to - sub.second); }
        }
        if (sub.second == s.begin()) { break; }
        p = sub.first;
    }
    return s.substr(0, to - s.begin());
}

template<typename OutputFn, typename OutputIt>
size_t separate_words(std::string_view s, char sep, OutputFn fn, OutputIt out,
                      size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    size_t count = 0;
    enum class state_t : char { kStart = 0, kSepFound, kSkipSep } state = state_t::kStart;
    for (auto p = s.begin();; ++p) {
        while (p < s.end() && std::isspace(static_cast<unsigned char>(*p))) { ++p; }  // skip spaces
        auto p0 = p;
        if (p == s.end()) {
            if (state != state_t::kSepFound) { break; }
        } else {
            state_t prev_state = state;
            do {  // find separator or blank
                if (*p == '\\') {
                    if (++p == s.end()) { break; }
                } else if (std::isspace(static_cast<unsigned char>(*p))) {
                    state = state_t::kSkipSep;
                    break;
                } else if (*p == sep) {
                    state = state_t::kSepFound;
                    break;
                }
            } while (++p < s.end());
            if (p == p0 && prev_state == state_t::kSkipSep) { continue; }
        }
        *out++ = fn(s.substr(p0 - s.begin(), p - p0));
        if (++count == max_count || p == s.end()) { break; }
    }
    return count;
}

template<typename OutputFn = nofunc>
auto separate_words(std::string_view s, char sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    separate_words(s, sep, fn, std::back_inserter(result));
    return result;
}

template<typename Range, typename StrTy, typename InputFn = nofunc>
StrTy& pack_strings_append(StrTy& s, const Range& r, char sep, InputFn fn = InputFn{}) {
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
    return std::move(pack_strings_append(prefix, r, sep, fn));
}

template<typename OutputFn, typename OutputIt>
size_t unpack_strings(std::string_view s, char sep, OutputFn fn, OutputIt out,
                      size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    size_t count = 0;
    for (auto p = s.begin();; ++p) {
        std::string result;
        auto p0 = p;  // append chars till separator
        for (; p < s.end(); ++p) {
            if (*p == '\\') {
                result.append(p0, p);
                p0 = p + 1;
                if (++p == s.end()) { break; }
            } else if (*p == sep) {
                break;
            }
        }
        result.append(p0, p);
        if (p < s.end() || !result.empty()) {
            *out++ = fn(std::move(result));
            if (++count == max_count) { break; }
        }
        if (p == s.end()) { break; }
    }
    return count;
}

UTIL_EXPORT std::wstring from_utf8_to_wide(std::string_view s);
UTIL_EXPORT std::string from_wide_to_utf8(std::wstring_view s);
UTIL_EXPORT std::string_view trim_string(std::string_view s);
UTIL_EXPORT std::vector<std::string> unpack_strings(std::string_view s, char sep);
UTIL_EXPORT std::string encode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UTIL_EXPORT std::string decode_escapes(std::string_view s, std::string_view symb, std::string_view code);
UTIL_EXPORT std::pair<unsigned, unsigned> parse_flag_string(
    std::string_view s, const std::vector<std::pair<std::string_view, unsigned>>& flag_tbl);

UTIL_EXPORT int compare_strings_nocase(std::string_view lhs, std::string_view rhs);
UTIL_EXPORT std::string to_lower(std::string_view s);
UTIL_EXPORT std::string to_upper(std::string_view s);

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
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return compare_strings_nocase(lhs, rhs) == 0;
    }
};

template<>
struct less_nocase<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return compare_strings_nocase(lhs, rhs) < 0;
    }
};

template<typename Str, typename Func = nofunc>
is_equal_to_predicate<Str, Func, equal_to_nocase<>> is_equal_to_nocase(const Str& s, const Func& fn = Func{}) {
    return is_equal_to_predicate<Str, Func, equal_to_nocase<>>(s, fn);
}

}  // namespace util
