#pragma once

#include "string_view.h"

#include <regex>
#include <utility>

namespace uxs {

namespace detail {
template<typename CharT, typename Traits>
struct regex_finder {
    const std::basic_regex<CharT, Traits>& regex;
    using is_finder = int;
    using iterator = typename std::basic_string_view<CharT, typename Traits::string_type::traits_type>::const_iterator;
    explicit regex_finder(const std::basic_regex<CharT, Traits>& tgt) : regex(tgt) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        std::match_results<iterator> m;
        if (std::regex_search(begin, end, m, regex)) { return m[0]; }
        return std::make_pair(end, end);
    }
};
}  // namespace detail

template<typename CharT, typename Traits>
detail::regex_finder<CharT, Traits> sfinder(const std::basic_regex<CharT, Traits>& re) {
    return detail::regex_finder<CharT, Traits>(re);
}

}  // namespace uxs
