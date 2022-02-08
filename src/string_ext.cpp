#include "util/string_ext.h"

#include <algorithm>

using namespace util;

std::wstring util::from_utf8_to_wide(std::string_view s) {
    uint32_t code;
    std::wstring result;
    result.reserve(s.size());
    for (auto p = s.begin(), p1 = p; (p1 = from_utf8(p, s.end(), &code)) > p; p = p1) {
        to_utf16(code, std::back_inserter(result));
    }
    return result;
}

std::string util::from_wide_to_utf8(std::wstring_view s) {
    uint32_t code;
    std::string result;
    result.reserve(s.size());
    for (auto p = s.begin(), p1 = p; (p1 = from_utf16(p, s.end(), &code)) > p; p = p1) {
        to_utf8(code, std::back_inserter(result));
    }
    return result;
}

std::string_view util::trim_string(std::string_view s) {
    auto p1 = s.begin(), p2 = s.end();
    while (p1 < p2 && std::isspace(static_cast<unsigned char>(*p1))) { ++p1; }
    while (p1 < p2 && std::isspace(static_cast<unsigned char>(*(p2 - 1)))) { --p2; }
    return s.substr(p1 - s.begin(), p2 - p1);
}

std::vector<std::string> util::unpack_strings(std::string_view s, char sep) {
    std::vector<std::string> result;
    unpack_strings(s, sep, nofunc(), std::back_inserter(result));
    return result;
}

std::string util::encode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    std::string result;
    result.reserve(s.size());
    auto p = s.begin(), p0 = p;
    for (; p < s.end(); ++p) {
        auto pos = symb.find(*p);
        if (pos != std::string_view::npos) {
            result.append(p0, p);
            result += '\\';
            result += code[pos];
            p0 = p + 1;
        }
    }
    result.append(p0, p);
    return result;
}

std::string util::decode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    std::string result;
    result.reserve(s.size());
    auto p = s.begin(), p0 = p;
    for (; p < s.end(); ++p) {
        if (*p != '\\') { continue; }
        result.append(p0, p);
        p0 = p + 1;
        if (++p == s.end()) { break; }
        auto pos = code.find(*p);
        if (pos != std::string_view::npos) {
            result += symb[pos];
            p0 = p + 1;
        }
    }
    result.append(p0, p);
    return result;
}

std::pair<unsigned, unsigned> util::parse_flag_string(
    std::string_view s, const std::vector<std::pair<std::string_view, unsigned>>& flag_tbl) {
    std::pair<unsigned, unsigned> flags(0, 0);
    separate_words(s, ' ', nofunc(), function_caller([&](std::string_view flag) {
                       bool add_flag = (flag[0] != '-');
                       if (flag[0] == '+' || flag[0] == '-') { flag = flag.substr(1); }
                       auto it = std::find_if(flag_tbl.begin(), flag_tbl.end(),
                                              [flag](decltype(*flag_tbl.begin()) el) { return el.first == flag; });
                       if (it == flag_tbl.end()) {
                       } else if (add_flag) {
                           flags.first |= it->second;
                       } else {
                           flags.second |= it->second;
                       }
                   }));
    return flags;
}

int util::compare_strings_nocase(std::string_view lhs, std::string_view rhs) {
    auto p1_end = lhs.begin() + std::min(lhs.size(), rhs.size());
    for (auto p1 = lhs.begin(), p2 = rhs.begin(); p1 < p1_end; ++p1, ++p2) {
        char ch1 = std::tolower(static_cast<unsigned char>(*p1));
        char ch2 = std::tolower(static_cast<unsigned char>(*p2));
        if (std::string_view::traits_type::lt(ch1, ch2)) {
            return -1;
        } else if (std::string_view::traits_type::lt(ch2, ch1)) {
            return 1;
        }
    }
    if (lhs.size() < rhs.size()) {
        return -1;
    } else if (rhs.size() < lhs.size()) {
        return 1;
    }
    return 0;
}

std::string util::to_lower(std::string_view s) {
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower;
}

std::string util::to_upper(std::string_view s) {
    std::string upper(s);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return std::toupper(c); });
    return upper;
}
