#pragma once

#include "stringalg.h"

#include <regex>

namespace uxs {

namespace detail {
template<typename CharT, typename RegexTraits, typename Traits>
struct string_finder<std::basic_regex<CharT, RegexTraits>, Traits> {
    const std::basic_regex<CharT, RegexTraits>& regex;
    using is_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit string_finder(const std::basic_regex<CharT, RegexTraits>& tgt) : regex(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        std::match_results<iterator> m;
        if (std::regex_search(begin, end, m, regex)) { return m[0]; }
        return std::make_pair(end, end);
    }
};

template<typename CharT, typename RegexTraits, typename Traits>
struct reversed_string_finder<std::basic_regex<CharT, RegexTraits>, Traits> {
    const std::basic_regex<CharT, RegexTraits>& regex;
    using is_reversed_finder = int;
    using iterator = typename std::basic_string_view<CharT, Traits>::const_iterator;
    explicit reversed_string_finder(const std::basic_regex<CharT, RegexTraits>& tgt) : regex(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        std::match_results<iterator> m;
        auto result = std::make_pair(begin, begin);
        for (iterator p = end; begin != p;) {
            if (std::regex_search(--p, end, m, regex, std::regex_constants::match_continuous)) {
                if (m[0].second < result.second) {
                    break;
                } else {
                    result = m[0];
                }
            } else if (result.second > begin) {
                break;
            }
        }
        return result;
    }
};
}  // namespace detail

inline detail::string_finder<std::regex, std::char_traits<char>> sfinder(const std::regex& re) {
    return detail::string_finder<std::regex, std::char_traits<char>>(re);
}
inline detail::reversed_string_finder<std::regex, std::char_traits<char>> rsfinder(const std::regex& re) {
    return detail::reversed_string_finder<std::regex, std::char_traits<char>>(re);
}

inline detail::string_finder<std::wregex, std::char_traits<wchar_t>> sfinder(const std::wregex& re) {
    return detail::string_finder<std::wregex, std::char_traits<wchar_t>>(re);
}
inline detail::reversed_string_finder<std::wregex, std::char_traits<wchar_t>> rsfinder(const std::wregex& re) {
    return detail::reversed_string_finder<std::wregex, std::char_traits<wchar_t>>(re);
}

}  // namespace uxs
