#pragma once

#include "string_ext.h"

#include <regex>

namespace util {

namespace detail {
template<>
struct string_finder<std::regex> {
    const std::regex& regex;
    using is_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit string_finder(const std::regex& in_regex) : regex(in_regex) {}
    std::pair<iterator, iterator> operator()(iterator begin, iterator end) const {
        std::match_results<iterator> m;
        if (std::regex_search(begin, end, m, regex)) { return m[0]; }
        return std::make_pair(end, end);
    }
};

template<>
struct reversed_string_finder<std::regex> {
    const std::regex& regex;
    using is_reversed_finder = int;
    using iterator = std::string_view::const_iterator;
    explicit reversed_string_finder(const std::regex& in_regex) : regex(in_regex) {}
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

}  // namespace util
