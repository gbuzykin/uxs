#include "uxs/string_alg.h"

std::wstring uxs::from_utf8_to_wide(std::string_view s) {
    std::wstring result;
    result.reserve(s.size());
    utf_string_adapter<wchar_t>{}.append(result, s);
    return result;
}

std::string uxs::from_wide_to_utf8(std::wstring_view s) {
    std::string result;
    result.reserve(s.size());
    utf_string_adapter<char>{}.append(result, s);
    return result;
}

// --------------------------

namespace {
template<typename CharT>
std::basic_string<CharT> encode_string_escapes_generic(std::basic_string_view<CharT> s,
                                                       std::basic_string_view<CharT> symb,
                                                       std::basic_string_view<CharT> code) {
    std::basic_string<CharT> result;
    result.reserve(s.size());
    auto p0 = s.begin();
    auto p = p0;
    for (; p != s.end(); ++p) {
        auto pos = symb.find(*p);
        if (pos != std::basic_string_view<CharT>::npos) {
            result += uxs::to_string_view(p0, p);
            result += '\\';
            result += code[pos];
            p0 = p + 1;
        }
    }
    result += uxs::to_string_view(p0, p);
    return result;
}
}  // namespace

std::string uxs::encode_string_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    return encode_string_escapes_generic(s, symb, code);
}

std::wstring uxs::encode_string_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code) {
    return encode_string_escapes_generic(s, symb, code);
}

// --------------------------

namespace {
template<typename CharT>
std::basic_string<CharT> decode_string_escapes_generic(std::basic_string_view<CharT> s,
                                                       std::basic_string_view<CharT> symb,
                                                       std::basic_string_view<CharT> code) {
    std::basic_string<CharT> result;
    result.reserve(s.size());
    auto p0 = s.begin();
    auto p = p0;
    for (; p != s.end(); ++p) {
        if (*p != '\\') { continue; }
        result += uxs::to_string_view(p0, p);
        p0 = p + 1;
        if (++p == s.end()) { break; }
        auto pos = code.find(*p);
        if (pos != std::basic_string_view<CharT>::npos) {
            result += symb[pos];
            p0 = p + 1;
        }
    }
    result += uxs::to_string_view(p0, p);
    return result;
}
}  // namespace

std::string uxs::decode_string_escapes(std::string_view s, std::string_view symb, std::string_view code) {
    return decode_string_escapes_generic(s, symb, code);
}

std::wstring uxs::decode_string_escapes(std::wstring_view s, std::wstring_view symb, std::wstring_view code) {
    return decode_string_escapes_generic(s, symb, code);
}
