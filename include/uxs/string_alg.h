#pragma once

#include "chars.h"
#include "string_util.h"

#include <algorithm>
#include <vector>

namespace uxs {

namespace detail {

template<typename CharTraits>
struct char_finder {
    using is_finder = int;
    using char_type = typename CharTraits::char_type;
    using iterator = typename std::basic_string_view<char_type, CharTraits>::const_iterator;
    char_type ch;
    explicit UXS_CONSTEXPR char_finder(char_type tgt) : ch(tgt) {}
    UXS_CONSTEXPR std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        for (; begin != end; ++begin) {
            if (CharTraits::eq(*begin, '\\')) {
                if (++begin == end) { break; }
            } else if (CharTraits::eq(*begin, ch)) {
                return std::make_pair(begin, begin + 1);
            }
        }
        return std::make_pair(end, end);
    }
};

template<typename CharTraits>
struct reverse_char_finder {
    using is_reverse_finder = int;
    using char_type = typename CharTraits::char_type;
    using iterator = typename std::basic_string_view<char_type, CharTraits>::const_iterator;
    char_type ch;
    explicit UXS_CONSTEXPR reverse_char_finder(char_type tgt) : ch(tgt) {}
    UXS_CONSTEXPR std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        while (begin != end) {
            --end;
            if (begin != end && CharTraits::eq(*(end - 1), '\\')) {
            } else if (CharTraits::eq(*end, ch)) {
                return std::make_pair(end, end + 1);
            }
        }
        return std::make_pair(begin, begin);
    }
};

template<typename CharTraits>
struct string_finder {
    using is_finder = int;
    using char_type = typename CharTraits::char_type;
    using iterator = typename std::basic_string_view<char_type, CharTraits>::const_iterator;
    std::basic_string_view<char_type, CharTraits> s;
    explicit UXS_CONSTEXPR string_finder(std::basic_string_view<char_type, CharTraits> tgt) : s(tgt) {}
    UXS_CONSTEXPR std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<std::size_t>(end - begin) < s.size()) { return std::make_pair(end, end); }
        if (!s.size()) { return std::make_pair(begin, begin); }
        for (iterator last = end - s.size() + 1; begin != last; ++begin) {
            if (std::equal(s.begin(), s.end(), begin, CharTraits::eq)) {
                return std::make_pair(begin, begin + s.size());
            }
        }
        return std::make_pair(end, end);
    }
};

template<typename CharTraits>
struct reverse_string_finder {
    using is_reverse_finder = int;
    using char_type = typename CharTraits::char_type;
    using iterator = typename std::basic_string_view<char_type, CharTraits>::const_iterator;
    std::basic_string_view<char_type, CharTraits> s;
    explicit UXS_CONSTEXPR reverse_string_finder(std::basic_string_view<char_type, CharTraits> tgt) : s(tgt) {}
    UXS_CONSTEXPR std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        if (static_cast<std::size_t>(end - begin) < s.size()) { return std::make_pair(begin, begin); }
        if (!s.size()) { return std::make_pair(end, end); }
        for (end -= s.size() - 1; begin != end; --end) {
            if (std::equal(s.begin(), s.end(), end - 1, CharTraits::eq)) {
                return std::make_pair(end - 1, end - 1 + s.size());
            }
        }
        return std::make_pair(begin, begin);
    }
};

}  // namespace detail

template<typename CharT, typename Traits = std::char_traits<CharT>,
         typename = std::enable_if_t<est::is_character<CharT>::value>>
detail::char_finder<Traits> sfinder(CharT ch) {
    return detail::char_finder<Traits>(ch);
}

template<typename CharT, typename Traits = std::char_traits<CharT>,
         typename = std::enable_if_t<est::is_character<CharT>::value>>
detail::reverse_char_finder<Traits> rsfinder(CharT ch) {
    return detail::reverse_char_finder<Traits>(ch);
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
detail::string_finder<string_traits_t<StrLikeTy>> sfinder(const StrLikeTy& s) {
    return detail::string_finder<string_traits_t<StrLikeTy>>(to_string_view(s));
}

template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
detail::reverse_string_finder<string_traits_t<StrLikeTy>> rsfinder(const StrLikeTy& s) {
    return detail::reverse_string_finder<string_traits_t<StrLikeTy>>(to_string_view(s));
}

// --------------------------

template<typename StrTy, typename StrLikeTy, typename Finder, typename WithTy,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::void_t<typename Finder::is_finder>>
void replace_strings_append(StrTy& out, const StrLikeTy& s, Finder finder, const WithTy& with) {
    const auto sv = to_string_view(s);
    auto p = sv.begin();
    while (p != sv.end()) {
        const auto sub = finder(p, sv.end());
        out += sv.substr(p - sv.begin(), sub.first - p);
        if (sub.first != sub.second) { out += with; }
        p = sub.second;
    }
}

template<typename StrLikeTy, typename Finder, typename WithTy,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::void_t<typename Finder::is_finder>>
auto replace_strings(const StrLikeTy& s, Finder finder, const WithTy& with) -> decltype(make_string(s)) {
    decltype(make_string(s)) result;
    const auto sv = to_string_view(s);
    result.reserve(sv.size());
    replace_strings_append(result, sv, finder, with);
    return result;
}

// --------------------------

template<typename StrTy, typename Range, typename SepTy, typename JoinFn = est::grow>
void join_strings_append(StrTy& out, const Range& r, const SepTy& sep, JoinFn fn = JoinFn{}) {
    auto first = std::begin(r);
    const auto last = std::end(r);
    if (first == last) { return; }
    while (true) {
        fn(out, *first);
        ++first;
        if (first == last) { return; }
        out += sep;
    }
}

template<typename CharT = char, typename Range, typename SepTy, typename JoinFn = est::grow>
std::basic_string<CharT> join_strings(const Range& r, const SepTy& sep,
                                      est::type_identity_t<std::basic_string<CharT>> prefix = {},
                                      JoinFn fn = JoinFn{}) {
    join_strings_append(prefix, r, sep, fn);
    return prefix;
}

// --------------------------

template<typename StrLikeTy, typename Finder, typename OutputIt, typename OutputFn = est::identity,
         typename OutputPred = est::true_fn, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
         typename = std::void_t<typename Finder::is_finder>>
UXS_CONSTEXPR OutputIt split_string_to(const StrLikeTy& s, Finder finder, OutputIt out, OutputFn fn = OutputFn{},
                                       OutputPred pred = OutputPred{}) {
    const auto sv = to_string_view(s);
    auto p = sv.begin();
    while (true) {
        const auto sub = finder(p, sv.end());
        if (pred(p, sub.first)) {
            *out = fn(sv.substr(p - sv.begin(), sub.first - p));
            ++out;
        }
        if (sub.first == sv.end()) { return out; }
        p = sub.second;
    }
}

template<typename StrLikeTy, typename Finder, typename OutputFn = est::identity, typename OutputPred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
auto split_string(const StrLikeTy& s, Finder finder, OutputFn fn = OutputFn{}, OutputPred pred = OutputPred{})
    -> std::vector<std::decay_t<decltype(fn(to_string_view(s)))>> {
    std::vector<std::decay_t<decltype(fn(to_string_view(s)))>> result;
    split_string_to(s, finder, std::back_inserter(result), fn, pred);
    return result;
}

// --------------------------

template<typename StrLikeTy, typename Finder, typename Pred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR auto string_section(const StrLikeTy& s, Finder finder,
                                  est::type_identity_t<std::size_t, typename Finder::is_finder> start,
                                  std::size_t count = 0, Pred pred = Pred{}) -> decltype(to_string_view(s)) {
    std::size_t n = 0;
    const auto sv = to_string_view(s);
    auto p = sv.begin();
    auto from = sv.end();
    const std::size_t fin = count ? start + count - 1 : std::numeric_limits<std::size_t>::max();
    while (true) {
        const auto sub = finder(p, sv.end());
        if (pred(p, sub.first)) {
            if (n == start) { from = p; }
            if (n == fin) { return sv.substr(from - sv.begin(), sub.first - from); }
            ++n;
        }
        if (sub.first == sv.end()) { return sv.substr(from - sv.begin(), sv.end() - from); }
        p = sub.second;
    }
}

template<typename StrLikeTy, typename Finder, typename Pred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR auto string_section(const StrLikeTy& s, Finder finder,
                                  est::type_identity_t<std::size_t, typename Finder::is_reverse_finder> start,
                                  std::size_t count = 0, Pred pred = Pred{}) -> decltype(to_string_view(s)) {
    std::size_t n = 0;
    const auto sv = to_string_view(s);
    auto p = sv.end();
    auto to = sv.begin();
    const std::size_t fin = count ? start - count + 1 : 0;
    while (true) {
        const auto sub = finder(sv.begin(), p);
        if (pred(sub.second, p)) {
            if (n == fin) { to = p; }
            if (n == start) { return sv.substr(sub.second - sv.begin(), to - sub.second); }
            ++n;
        }
        if (sub.second == sv.begin()) { return sv.substr(0, to - sv.begin()); }
        p = sub.first;
    }
}

// --------------------------

template<typename StrLikeTy, typename OutputIt, typename OutputFn = est::identity, typename OutputPred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
UXS_CONSTEXPR OutputIt string_to_words_to(const StrLikeTy& s, typename string_traits_t<StrLikeTy>::char_type sep,
                                          OutputIt out, OutputFn fn = OutputFn{}, OutputPred pred = OutputPred{}) {
    enum class state_t { start = 0, sep_found, skip_sep } state = state_t::start;
    const auto sv = to_string_view(s);
    for (auto p = sv.begin();; ++p) {
        while (p != sv.end() && is_space(*p)) { ++p; }  // skip spaces
        const auto p0 = p;
        if (p == sv.end()) {
            if (state != state_t::sep_found) { return out; }
        } else {
            state_t prev_state = state;
            do {  // find separator or blank
                if (*p == '\\') {
                    if (++p == sv.end()) { break; }
                } else if (is_space(*p)) {
                    state = state_t::skip_sep;
                    break;
                } else if (*p == sep) {
                    state = state_t::sep_found;
                    break;
                }
            } while (++p != sv.end());
            if (p == p0 && prev_state == state_t::skip_sep) { continue; }
        }
        if (pred(p0, p)) {
            *out = fn(sv.substr(p0 - sv.begin(), p - p0));
            ++out;
        }
        if (p == sv.end()) { return out; }
    }
}

template<typename StrLikeTy, typename OutputFn = est::identity, typename OutputPred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
auto string_to_words(const StrLikeTy& s, typename string_traits_t<StrLikeTy>::char_type sep, OutputFn fn = OutputFn{},
                     OutputPred pred = OutputPred{}) -> std::vector<std::decay_t<decltype(fn(to_string_view(s)))>> {
    std::vector<std::decay_t<decltype(fn(to_string_view(s)))>> result;
    string_to_words_to(s, sep, std::back_inserter(result), fn, pred);
    return result;
}

// --------------------------

template<typename StrTy, typename Range, typename InputFn = est::identity>
auto pack_strings_append(StrTy& out, const Range& r, typename StrTy::value_type sep, InputFn fn = InputFn{})
    -> std::enable_if_t<is_string_like<std::decay_t<decltype(fn(*std::begin(r)))>>::value> {
    auto first = std::begin(r);
    const auto last = std::end(r);
    if (first == last) { return; }
    while (true) {
        const auto el = fn(*first);
        const auto sv = to_string_view(el);
        auto p0 = sv.begin();
        auto p = p0;
        for (; p != sv.end(); ++p) {
            if (*p == '\\' || *p == sep) {
                out += sv.substr(p0 - sv.begin(), p - p0);
                out += '\\';
                p0 = p;
            }
        }
        out += sv.substr(p0 - sv.begin(), p - p0);
        ++first;
        if (first == last) {
            if (sv.empty()) { out += sep; }
            return;
        }
        out += sep;
    }
}

template<typename CharT = char, typename Range, typename InputFn = est::identity>
auto pack_strings(const Range& r, est::type_identity_t<CharT> sep,
                  est::type_identity_t<std::basic_string<CharT>> prefix = {}, InputFn fn = InputFn{})
    -> std::enable_if_t<is_string_like<std::decay_t<decltype(fn(*std::begin(r)))>>::value, std::basic_string<CharT>> {
    pack_strings_append(prefix, r, sep, fn);
    return prefix;
}

// --------------------------

template<typename StrLikeTy, typename OutputIt, typename OutputFn = est::identity, typename OutputPred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
OutputIt unpack_strings_to(const StrLikeTy& s, typename string_traits_t<StrLikeTy>::char_type sep, OutputIt out,
                           OutputFn fn = OutputFn{}, OutputPred pred = OutputPred{}) {
    const auto sv = to_string_view(s);
    for (auto p = sv.begin();; ++p) {
        decltype(make_string(s)) val;
        auto p0 = p;  // append chars till separator
        for (; p != sv.end(); ++p) {
            if (*p == '\\') {
                val += sv.substr(p0 - sv.begin(), p - p0);
                p0 = p + 1;
                if (++p == sv.end()) { break; }
            } else if (*p == sep) {
                break;
            }
        }
        val += sv.substr(p0 - sv.begin(), p - p0);
        if ((p != sv.end() || !val.empty()) && pred(val.begin(), val.end())) {
            *out = fn(std::move(val));
            ++out;
        }
        if (p == sv.end()) { return out; }
    }
}

template<typename StrLikeTy, typename OutputFn = est::identity, typename OutputPred = est::true_fn,
         typename = std::enable_if_t<is_string_like<StrLikeTy>::value>>
auto unpack_strings(const StrLikeTy& s, typename string_traits_t<StrLikeTy>::char_type sep, OutputFn fn = OutputFn{},
                    OutputPred pred = OutputPred{}) -> std::vector<std::decay_t<decltype(fn(make_string(s)))>> {
    std::vector<std::decay_t<decltype(fn(make_string(s)))>> result;
    unpack_strings_to(s, sep, std::back_inserter(result), fn, pred);
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
