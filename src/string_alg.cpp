#include "uxs/string_alg.h"

namespace uxs {

std::wstring from_utf8_to_wide(std::string_view s) {
    std::wstring result;
    result.reserve(s.size());
    utf_string_adapter<wchar_t>{}.append(result, s);
    return result;
}

std::string from_wide_to_utf8(std::wstring_view s) {
    std::string result;
    result.reserve(s.size());
    utf_string_adapter<char>{}.append(result, s);
    return result;
}

// --------------------------

template<typename CharT>
std::basic_string_view<CharT> basic_trim_string(std::basic_string_view<CharT> s) {
    auto p1 = s.begin();
    auto p2 = s.end();
    while (p1 != p2 && is_space(*p1)) { ++p1; }
    while (p1 != p2 && is_space(*(p2 - 1))) { --p2; }
    return s.substr(p1 - s.begin(), p2 - p1);
}

std::string_view trim_string(std::string_view s) { return basic_trim_string(s); }
std::wstring_view trim_string(std::wstring_view s) { return basic_trim_string(s); }

// --------------------------

std::vector<std::string> unpack_strings(std::string_view s, char sep) {
    std::vector<std::string> result;
    unpack_strings(s, sep, nofunc(), std::back_inserter(result));
    return result;
}

std::vector<std::wstring> unpack_strings(std::wstring_view s, char sep) {
    std::vector<std::wstring> result;
    unpack_strings(s, sep, nofunc(), std::back_inserter(result));
    return result;
}

// --------------------------

template<typename CharT, typename Traits>
std::basic_string<CharT, Traits> basic_encode_escapes(std::basic_string_view<CharT, Traits> s,
                                                      std::basic_string_view<CharT, Traits> symb,
                                                      std::basic_string_view<CharT, Traits> code) {
    std::basic_string<CharT, Traits> result;
    result.reserve(s.size());
    auto p0 = s.begin();
    auto p = p0;
    for (; p != s.end(); ++p) {
        auto pos = symb.find(*p);
        if (pos != std::basic_string_view<CharT, Traits>::npos) {
            result += to_string_view(p0, p);
            result += '\\';
            result += code[pos];
            p0 = p + 1;
        }
    }
    result += to_string_view(p0, p);
    return result;
}

std::string encode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    return basic_encode_escapes(s, symb, code);
}

std::wstring encode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code) {
    return basic_encode_escapes(s, symb, code);
}

// --------------------------

template<typename CharT, typename Traits>
std::basic_string<CharT, Traits> basic_decode_escapes(std::basic_string_view<CharT, Traits> s,
                                                      std::basic_string_view<CharT, Traits> symb,
                                                      std::basic_string_view<CharT, Traits> code) {
    std::basic_string<CharT, Traits> result;
    result.reserve(s.size());
    auto p0 = s.begin();
    auto p = p0;
    for (; p != s.end(); ++p) {
        if (*p != '\\') { continue; }
        result += to_string_view(p0, p);
        p0 = p + 1;
        if (++p == s.end()) { break; }
        auto pos = code.find(*p);
        if (pos != std::basic_string_view<CharT, Traits>::npos) {
            result += symb[pos];
            p0 = p + 1;
        }
    }
    result += to_string_view(p0, p);
    return result;
}

std::string decode_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    return basic_decode_escapes(s, symb, code);
}

std::wstring decode_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code) {
    return basic_decode_escapes(s, symb, code);
}

// --------------------------

template<typename CharT>
int basic_compare_strings_nocase(std::basic_string_view<CharT> lhs, std::basic_string_view<CharT> rhs) {
    auto p1_end = lhs.begin() + std::min(lhs.size(), rhs.size());
    for (auto p1 = lhs.begin(), p2 = rhs.begin(); p1 != p1_end; ++p1, ++p2) {
        CharT ch1 = to_lower(*p1);
        CharT ch2 = to_lower(*p2);
        if (std::basic_string_view<CharT>::traits_type::lt(ch1, ch2)) { return -1; }
        if (std::basic_string_view<CharT>::traits_type::lt(ch2, ch1)) { return 1; }
    }
    if (lhs.size() < rhs.size()) { return -1; }
    if (rhs.size() < lhs.size()) { return 1; }
    return 0;
}

int compare_strings_nocase(std::string_view lhs, std::string_view rhs) {
    return basic_compare_strings_nocase(lhs, rhs);
}

int compare_strings_nocase(std::wstring_view lhs, std::wstring_view rhs) {
    return basic_compare_strings_nocase(lhs, rhs);
}

// --------------------------

std::string to_lower(std::string_view s) {
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](int c) { return to_lower(c); });
    return lower;
}

std::wstring to_lower(std::wstring_view s) {
    std::wstring lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](int c) { return to_lower(c); });
    return lower;
}

// --------------------------

std::string to_upper(std::string_view s) {
    std::string upper(s);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](int c) { return to_upper(c); });
    return upper;
}

std::wstring to_upper(std::wstring_view s) {
    std::wstring upper(s);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](int c) { return to_upper(c); });
    return upper;
}

}  // namespace uxs
