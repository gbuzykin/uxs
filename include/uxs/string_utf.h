#pragma once

#include "string_util.h"
#include "utf.h"

#include <algorithm>

namespace uxs {

template<typename CharT>
class utf_string_adapter {
 public:
    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
             typename = std::enable_if_t<
                 std::is_same<typename string_char_traits_t<std::remove_cvref_t<StrLikeTy>>::char_type, CharT>::value>>
    auto operator()(StrLikeTy&& s) const -> decltype(std::forward<StrLikeTy>(s)) {
        return std::forward<StrLikeTy>(s);
    }

    template<typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
             typename = std::enable_if_t<!std::is_same<string_char_type<StrLikeTy>, CharT>::value>>
    std::basic_string<CharT> operator()(const StrLikeTy& s) const {
        std::basic_string<CharT> result;
        const auto sv = to_string_view(s);
        result.reserve(sv.size());
        append_dispatch(result, sv.begin(), sv.end(), std::true_type(),
                        std::is_same<string_char_type<StrLikeTy>, CharT>());
        return result;
    }

    template<typename StrTy, typename StrLikeTy, typename = std::enable_if_t<is_string_like<StrLikeTy>::value>,
             typename = std::enable_if_t<std::is_same<typename StrTy::value_type, CharT>::value>>
    void append(StrTy& out, const StrLikeTy& s) const {
        const auto sv = to_string_view(s);
        append_dispatch(out, sv.begin(), sv.end(), std::true_type(), std::is_same<string_char_type<StrLikeTy>, CharT>());
    }

    template<typename StrTy, typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>,
             typename = std::enable_if_t<std::is_same<typename StrTy::value_type, CharT>::value>>
    void append(StrTy& out, InputIt first, InputIt last) {
        append_dispatch(out, first, last, est::is_random_access_iterator<InputIt>(),
                        std::is_same<est::iterator_value_t<InputIt>, CharT>());
    }

    template<typename InputIt, typename OutputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>,
             typename = std::enable_if_t<est::is_output_iterator<OutputIt, CharT>::value>>
    OutputIt transform(InputIt first, InputIt last, OutputIt out) {
        return transform_dispatch(first, last, std::move(out), std::is_same<est::iterator_value_t<InputIt>, CharT>());
    }

    template<typename InputIt, typename = std::enable_if_t<est::is_input_iterator<InputIt>::value>>
    std::size_t count(InputIt first, InputIt last) {
        return count_dispatch(first, last, std::is_same<est::iterator_value_t<InputIt>, CharT>());
    }

 private:
    template<typename StrTy, typename InputIt>
    static void append_dispatch(StrTy& out, InputIt first, InputIt last, std::true_type /* random access iterator */,
                                std::true_type /* same char type */) {
        out.append(first, last);
    }

    template<typename StrTy, typename InputIt>
    static void append_dispatch(StrTy& out, InputIt first, InputIt last, std::false_type /* random access iterator */,
                                std::true_type /* same char type */) {
        for (; first != last; ++first) { out += *first; }
    }

    template<typename StrTy, typename InputIt, typename Bool>
    static void append_dispatch(StrTy& out, InputIt first, InputIt last, Bool /* random access iterator */,
                                std::false_type /* same char type */) {
        while (first != last) {
            std::uint32_t code = 0;
            first = utf_codec<est::iterator_value_t<InputIt>>{}.decode(first, last, code).iter;
            utf_codec<CharT>{}.encode(code, std::back_inserter(out));
        }
    }

    template<typename InputIt, typename OutputIt>
    static OutputIt transform_dispatch(InputIt first, InputIt last, OutputIt out, std::true_type /* same char type */) {
        return std::copy(first, last, std::move(out));
    }

    template<typename InputIt, typename OutputIt>
    static OutputIt transform_dispatch(InputIt first, InputIt last, OutputIt out,
                                       std::false_type /* same char type */) {
        while (first != last) {
            std::uint32_t code = 0;
            first = utf_codec<est::iterator_value_t<InputIt>>{}.decode(first, last, code).iter;
            out = std::move(utf_codec<CharT>{}.encode(code, std::move(out)).out);
        }
        return out;
    }

    template<typename InputIt>
    static std::size_t count_dispatch(InputIt first, InputIt last, std::true_type /* same char type */) {
        return static_cast<std::size_t>(std::distance(first, last));
    }

    template<typename InputIt>
    static std::size_t count_dispatch(InputIt first, InputIt last, std::false_type /* same char type */) {
        std::size_t count = 0;
        while (first != last) {
            std::uint32_t code = 0;
            first = utf_codec<est::iterator_value_t<InputIt>>{}.decode(first, last, code).iter;
            count += utf_codec<CharT>{}.count(code);
        }
        return count;
    }
};

// --------------------------

template<typename StrTy, typename InputIt>
std::size_t append_escaped_string(StrTy& out, InputIt first, InputIt last, std::uint8_t quot_char,
                                  std::size_t max_width = std::numeric_limits<std::size_t>::max()) {
    using char_type = typename StrTy::value_type;
    std::basic_string_view<char_type> special;
    std::array<char_type, 12> special_buf;
    if (max_width == 0) { return 0; }
    out += quot_char;
    std::size_t width = 1;
    auto first0 = first;
    while (first != last) {
        std::uint32_t code = 0;
        const auto result = utf_codec<est::iterator_value_t<InputIt>>{}.decode(first, last, code);
        const bool is_printable = result && ((code < 0x80 && code >= 0x20) || is_utf_printable(code));
        if (is_printable && code != '\\' && code != quot_char) {
            const unsigned w = get_utf_printable_width(code);
            if (max_width - width < w) { break; }
            width += w;
            first = result.iter;
            continue;
        }
        if (code == '\t') {
            special = string_literal<char_type, '\\', 't'>{}();
        } else if (code == '\n') {
            special = string_literal<char_type, '\\', 'n'>{}();
        } else if (code == '\r') {
            special = string_literal<char_type, '\\', 'r'>{}();
        } else if (code == '\\') {
            special = string_literal<char_type, '\\', '\\'>{}();
        } else if (code == quot_char) {
            special_buf[0] = '\\';
            special_buf[1] = quot_char;
            special = to_string_view(special_buf.data(), 2);
        } else {
            char_type* p = special_buf.data() + special_buf.size();
            *--p = '}';
            do { *--p = "0123456789abcdef"[code & 0xf]; } while ((code >>= 4));
            *--p = '{';
            *--p = result ? 'u' : 'x';
            *--p = '\\';
            special = to_string_view(p, special_buf.data() + special_buf.size());
        }
        utf_string_adapter<char_type>{}.append(out, first0, first);
        if (max_width - width < special.size()) { break; }
        width += special.size();
        out += special;
        first = first0 = result.iter;
    }
    utf_string_adapter<char_type>{}.append(out, first0, first);
    if (width == max_width) { return width; }
    out += quot_char;
    return width + 1;
}

}  // namespace uxs
