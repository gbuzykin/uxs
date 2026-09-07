#pragma once

#include "string_view.h"

#include <regex>
#include <utility>

namespace uxs {

namespace detail {
template<typename CharTraits, typename RegexTraits>
struct regex_finder {
    using is_finder = int;
    using char_type = typename CharTraits::char_type;
    using iterator = typename std::basic_string_view<char_type, CharTraits>::const_iterator;
    const std::basic_regex<char_type, RegexTraits>& regex;
    explicit regex_finder(const std::basic_regex<char_type, RegexTraits>& tgt) : regex(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        std::match_results<iterator> m;
        if (std::regex_search(begin, end, m, regex)) { return m[0]; }
        return std::make_pair(end, end);
    }
};
}  // namespace detail

template<typename CharT, typename Traits>
detail::regex_finder<typename Traits::string_type::traits_type, Traits> sfinder(
    const std::basic_regex<CharT, Traits>& re) {
    return detail::regex_finder<typename Traits::string_type::traits_type, Traits>(re);
}

}  // namespace uxs
