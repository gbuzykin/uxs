#pragma once

#include "iterator.h"

#include <cctype>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#ifdef USE_QT
#    include <QHash>
#    include <QString>
#    ifndef QT_SPECIALIZE_STD_HASH_TO_CALL_QHASH
namespace std {
template<>
struct hash<QString> {
    size_t operator()(const QString& k) const { return qHash(k); }
};
}  // namespace std
#    endif  // !QT_SPECIALIZE_STD_HASH_TO_CALL_QHASH
#endif      // USE_QT

#if __cplusplus < 201703L
namespace std {
template<typename CharT>
class basic_string_view {
 public:
    using traits_type = std::char_traits<CharT>;
    using value_type = CharT;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using reference = CharT&;
    using const_reference = const CharT&;
    using const_iterator = util::array_iterator<basic_string_view, true>;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    static const size_type npos = size_type(-1);

    basic_string_view() = default;
    basic_string_view(const CharT* s, size_type count) : begin_(s), size_(count) {}
    basic_string_view(const CharT* s) : begin_(s) {
        for (; *s; ++s, ++size_) {}
    }
    basic_string_view(const basic_string<CharT>& s) : basic_string_view(s.data(), s.size()) {}

    size_type size() const { return size_; }
    size_type length() const { return size_; }
    bool empty() const { return size_ == 0; }

    explicit operator basic_string<CharT>() const { return basic_string<CharT>(begin_, size_); }

    const_iterator begin() const { return const_iterator{begin_, begin_, begin_ + size_}; }
    const_iterator cbegin() const { return const_iterator{begin_, begin_, begin_ + size_}; }

    const_iterator end() const { return const_iterator{begin_ + size_, begin_, begin_ + size_}; }
    const_iterator cend() const { return const_iterator{begin_ + size_, begin_, begin_ + size_}; }

    const_reverse_iterator rbegin() const { return const_reverse_iterator{end()}; }
    const_reverse_iterator crbegin() const { return const_reverse_iterator{end()}; }

    const_reverse_iterator rend() const { return const_reverse_iterator{begin()}; }
    const_reverse_iterator crend() const { return const_reverse_iterator{begin()}; }

    const_reference operator[](size_type pos) const {
        assert(pos < size_);
        return begin_[pos];
    }
    const_reference at(size_type pos) const {
        if (pos >= size_) { throw out_of_range("invalid pos"); }
        return begin_[pos];
    }
    const_reference front() const {
        assert(size_ > 0);
        return begin_[0];
    }
    const_reference back() const {
        assert(size_ > 0);
        return *(begin_ + size_ - 1);
    }
    const_pointer data() const { return begin_; }

    basic_string_view substr(size_type pos, size_type count = npos) const {
        pos = std::min(pos, size_);
        return basic_string_view(begin_ + pos, std::min(count, size_ - pos));
    }

    int compare(basic_string_view s) const;
    size_type find(CharT ch, size_type pos = 0) const;
    size_type find(basic_string_view s, size_type pos = 0) const;
    size_type rfind(CharT ch, size_type pos = npos) const;
    size_type rfind(basic_string_view s, size_type pos = npos) const;

    friend basic_string<CharT>& operator+=(basic_string<CharT>& lhs, basic_string_view rhs) {
        lhs.append(rhs.data(), rhs.size());
        return lhs;
    }

 private:
    const CharT* begin_ = nullptr;
    size_t size_ = 0;
};

template<typename CharT>
int basic_string_view<CharT>::compare(basic_string_view<CharT> s) const {
    int result = traits_type::compare(begin_, s.begin_, std::min(size_, s.size_));
    if (result != 0) {
        return result;
    } else if (size_ < s.size_) {
        return -1;
    } else if (s.size_ < size_) {
        return 1;
    }
    return 0;
}

template<typename CharT>
auto basic_string_view<CharT>::find(CharT ch, size_type pos) const -> size_type {
    for (auto p = begin_ + pos; p < begin_ + size_; ++p) {
        if (traits_type::eq(*p, ch)) { return p - begin_; }
    }
    return npos;
}

template<typename CharT>
auto basic_string_view<CharT>::find(basic_string_view s, size_type pos) const -> size_type {
    for (auto p = begin_ + pos; p + s.size_ <= begin_ + size_; ++p) {
        if (std::equal(p, p + s.size_, s.begin_, traits_type::eq)) { return p - begin_; }
    }
    return npos;
}

template<typename CharT>
auto basic_string_view<CharT>::rfind(CharT ch, size_type pos) const -> size_type {
    for (auto p = begin_ + std::min(pos + 1, size_); p > begin_; --p) {
        if (traits_type::eq(*(p - 1), ch)) { return p - begin_ - 1; }
    }
    return npos;
}

template<typename CharT>
auto basic_string_view<CharT>::rfind(basic_string_view s, size_type pos) const -> size_type {
    for (auto p = begin_ + std::min(pos + s.size_, size_); p >= begin_ + s.size_; --p) {
        if (std::equal(p - s.size_, p, s.begin_, traits_type::eq)) { return p - begin_ - s.size_; }
    }
    return npos;
}

template<typename CharT>
bool operator==(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}
template<typename CharT>
bool operator!=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.size() != rhs.size() || lhs.compare(rhs) != 0;
}
template<typename CharT>
bool operator<(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.compare(rhs) < 0;
}
template<typename CharT>
bool operator<=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.compare(rhs) <= 0;
}
template<typename CharT>
bool operator>(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.compare(rhs) > 0;
}
template<typename CharT>
bool operator>=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) {
    return lhs.compare(rhs) >= 0;
}

template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator==(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs == basic_string_view<CharT>(rhs);
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator!=(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs != basic_string_view<CharT>(rhs);
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator<(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs < basic_string_view<CharT>(rhs);
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator<=(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs <= basic_string_view<CharT>(rhs);
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator>(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs > basic_string_view<CharT>(rhs);
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator>=(basic_string_view<CharT> lhs, const Ty& rhs) {
    return lhs >= basic_string_view<CharT>(rhs);
}

template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator==(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) == rhs;
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator!=(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) != rhs;
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator<(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) < rhs;
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator<=(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) <= rhs;
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator>(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) > rhs;
}
template<typename CharT, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT>>::value>>
bool operator>=(const Ty& lhs, basic_string_view<CharT> rhs) {
    return basic_string_view<CharT>(lhs) >= rhs;
}

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;

#    if defined(_MSC_VER)
template<typename CharT>
struct hash<basic_string_view<CharT>> {
    size_t operator()(basic_string_view<CharT> s) const { return _Hash_seq((const unsigned char*)s.data(), s.size()); }
};
#    else   // defined(_MSC_VER)
template<typename CharT>
struct hash<basic_string_view<CharT>> {
    size_t operator()(basic_string_view<CharT> s) const {
        return std::hash<std::basic_string<CharT>>{}(static_cast<std::basic_string<CharT>>(s));
    }
};
#    endif  // defined(_MSC_VER)
}  // namespace std
#endif  // __cplusplus < 201703L

namespace util {

template<typename InputIt>
InputIt from_utf8(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in >= in_end) { return in; }
    uint32_t code = static_cast<uint8_t>(*in);
    if ((code & 0xC0) == 0xC0) {
        static const uint32_t mask_tbl[] = {0xFF, 0x1F, 0xF, 0x7};
        static const uint32_t count_tbl[] = {1, 1, 1, 1, 2, 2, 3, 0};
        uint32_t count = count_tbl[(code >> 3) & 7];  // continuation byte count
        if (in_end - in <= count) { return in; }
        code &= mask_tbl[count];
        while (count > 0) {
            code = (code << 6) | ((*++in) & 0x3F);
            --count;
        }
    }
    *pcode = code;
    return ++in;
}

template<typename InputIt>
InputIt from_utf16(InputIt in, InputIt in_end, uint32_t* pcode) {
    if (in >= in_end) { return in; }
    uint32_t code = static_cast<uint16_t>(*in);
    if ((code & 0xDC00) == 0xD800) {
        if (in_end - in <= 1) { return in; }
        code = 0x10000 + ((code & 0x3FF) << 10) | ((*++in) & 0x3FF);
    }
    *pcode = code;
    return ++in;
}

template<typename OutputIt>
uint32_t to_utf8(uint32_t code, OutputIt out,  //
                 size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code > 0x7F) {
        static const uint32_t mask_tbl[] = {0xFF, 0x1F, 0xF, 0x7};
        static const uint32_t hdr_tbl[] = {0, 0xC0, 0xE0, 0xF0};
        uint8_t ch[4];
        uint32_t count = 0;  // continuation byte count
        if (code > 0x10FFFF) { code = 0xFFFD; }
        do {
            ch[count++] = 0x80 | (code & 0x3F);
            code >>= 6;
        } while (code > mask_tbl[count]);
        if (count >= max_count) { return 0; }
        *out++ = static_cast<uint8_t>(hdr_tbl[count] | code);
        uint32_t n = count;
        do { *out++ = ch[--n]; } while (n > 0);
        return count + 1;
    }
    *out++ = static_cast<uint8_t>(code);
    return 1;
}

template<typename OutputIt>
uint32_t to_utf16(uint32_t code, OutputIt out,  //
                  size_t max_count = std::numeric_limits<size_t>::max()) {
    if (!max_count) { return 0; }
    if (code >= 0x10000) {
        if (code <= 0x10FFFF) {
            if (max_count <= 1) { return 0; }
            code -= 0x10000;
            *out++ = static_cast<uint16_t>(0xD800 | (code >> 10));
            *out++ = static_cast<uint16_t>(0xDC00 | (code & 0x3FF));
            return 2;
        }
        code = 0xFFFD;
    } else if ((code & 0xD800) == 0xD800) {
        code = 0xFFFD;
    }
    *out++ = static_cast<uint16_t>(code);
    return 1;
}

inline bool is_leading_utf8_byte(char c) { return (c & 0xC0) != 0x80; }
inline void pop_utf8(std::string& s) {
    while (!s.empty()) {
        char c = s.back();
        s.pop_back();
        if (is_leading_utf8_byte(c)) { break; }
    }
}

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, int digs, bool* ok = nullptr, InputFn fn = InputFn{}) {
    unsigned val = 0;
    while (digs > 0) {
        char ch = fn(*in++);
        val <<= 4;
        --digs;
        if ((ch >= '0') && (ch <= '9')) {
            val |= ch - '0';
        } else if ((ch >= 'A') && (ch <= 'F')) {
            val |= ch - 'A' + 10;
        } else if ((ch >= 'a') && (ch <= 'f')) {
            val |= ch - 'a' + 10;
        } else {
            if (ok) { *ok = false; }
            return val;
        }
    }
    if (ok) { *ok = true; }
    return val;
}

template<typename OutputIt, typename OutputFn = nofunc>
void to_hex(unsigned val, OutputIt out, int digs, OutputFn fn = OutputFn{}) {
    int shift = digs << 2;
    while (shift > 0) {
        shift -= 4;
        *out++ = fn("0123456789ABCDEF"[(val >> shift) & 0xf]);
    }
}

enum class split_flags : unsigned { kNoFlags = 0, kSkipEmpty = 1 };

inline bool operator&(split_flags flags, split_flags mask) {
    return (static_cast<unsigned>(flags) & static_cast<unsigned>(mask)) != 0;
}

enum class scvt_fp {
    kScientific = 1,
    kFixed = 2,
    kGeneral = kScientific | kFixed,
};

enum class scvt_base { kBase8 = 0, kBase10 = 1, kBase16 = 2 };

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
            if ((begin < --end) && (*(end - 1) == '\\')) {
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
        for (auto last = end - s.size(); begin <= last; ++begin) {
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
        for (auto last = begin + s.size(); last <= end; --end) {
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

template<typename Range, typename InputFn = nofunc>
std::string join_strings(const Range& r, std::string_view sep, InputFn fn = InputFn{}) {
    if (std::begin(r) == std::end(r)) { return {}; }
    std::string s;
    for (auto it = std::begin(r);;) {
        s += fn(*it);
        if (++it != std::end(r)) {
            s += sep;
        } else {
            break;
        }
    }
    return s;
}

template<split_flags flags = split_flags::kNoFlags, typename Finder, typename OutputIt,  //
         typename OutputFn = nofunc, typename = std::void_t<typename Finder::is_finder>>
std::enable_if_t<!is_function_pointer<OutputIt>::value &&  //
                     is_output_iterator<OutputIt>::value,  //
                 size_t>
split_string(std::string_view s, Finder finder, OutputIt out,  //
             size_t max_count = std::numeric_limits<size_t>::max(), OutputFn fn = OutputFn{}) {
    if (!max_count) { return 0; }
    size_t count = 0;
    for (auto p = s.begin();;) {
        auto sub = finder(p, s.end());
        if (!(flags & split_flags::kSkipEmpty) || (p < sub.first)) {
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
    split_string<flags>(s, finder, std::back_inserter(result), std::numeric_limits<size_t>::max(), fn);
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
        if (!(flags & split_flags::kSkipEmpty) || (p < sub.first)) {
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
        if (!(flags & split_flags::kSkipEmpty) || (sub.second < p)) {
            if (count == fin) { to = p; }
            if (count++ == start) { return s.substr(sub.second - s.begin(), to - sub.second); }
        }
        if (sub.second == s.begin()) { break; }
        p = sub.first;
    }
    return s.substr(0, to - s.begin());
}

template<typename OutputIt, typename OutputFn = nofunc>
std::enable_if_t<!is_function_pointer<OutputIt>::value &&  //
                     is_output_iterator<OutputIt>::value,  //
                 size_t>
separate_words(std::string_view s, char sep, OutputIt out,  //
               size_t max_count = std::numeric_limits<size_t>::max(), OutputFn fn = OutputFn{}) {
    size_t count = 0;
    enum class state_t : char { kStart = 0, kSepFound, kSkipSep } state = state_t::kStart;
    if (!max_count) { return 0; }
    for (auto p = s.begin();; ++p) {
        while ((p < s.end()) && std::isblank(*p)) { ++p; }  // skip blanks
        auto p0 = p;
        if (p == s.end()) {
            if (state != state_t::kSepFound) { break; }
        } else {
            auto prev_state = state;
            do {  // find separator or blank
                if (*p == '\\') {
                    if (++p == s.end()) { break; }
                } else if (std::isblank(*p)) {
                    state = state_t::kSkipSep;
                    break;
                } else if (*p == sep) {
                    state = state_t::kSepFound;
                    break;
                }
            } while (++p < s.end());
            if ((p == p0) && (prev_state == state_t::kSkipSep)) { continue; }
        }
        *out++ = fn(s.substr(p0 - s.begin(), p - p0));
        if ((++count == max_count) || (p == s.end())) { break; }
    }
    return count;
}

template<typename OutputFn = nofunc>
auto separate_words(std::string_view s, char sep, OutputFn fn = OutputFn{})
    -> std::vector<std::decay_t<decltype(fn(s))>> {
    std::vector<std::decay_t<decltype(fn(s))>> result;
    separate_words(s, sep, std::back_inserter(result), std::numeric_limits<size_t>::max(), fn);
    return result;
}

template<typename Range, typename InputFn = nofunc>
std::string pack_strings(const Range& r, char sep, InputFn fn = InputFn{}) {
    if (std::begin(r) == std::end(r)) { return {}; }
    std::string s;
    for (auto it = std::begin(r);;) {
        const auto& el = fn(*it);
        auto p = std::begin(el), p0 = p;
        for (; p != std::end(el); ++p) {
            if ((*p == '\\') || (*p == sep)) {
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
    return s;
}

template<typename OutputIt, typename OutputFn = nofunc>
std::enable_if_t<!is_function_pointer<OutputIt>::value &&  //
                     is_output_iterator<OutputIt>::value,  //
                 size_t>
unpack_strings(std::string_view s, char sep, OutputIt out,  //
               size_t max_count = std::numeric_limits<size_t>::max(), OutputFn fn = OutputFn{}) {
    size_t count = 0;
    if (!max_count) { return 0; }
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
        if ((p < s.end()) || !result.empty()) {
            *out++ = fn(std::move(result));
            if (++count == max_count) { break; }
        }
        if (p == s.end()) { break; }
    }
    return count;
}

CORE_EXPORT std::wstring from_utf8_to_wide(std::string_view s);
CORE_EXPORT std::string from_wide_to_utf8(std::wstring_view s);
CORE_EXPORT std::string_view trim_string(std::string_view s);
CORE_EXPORT std::vector<std::string> unpack_strings(std::string_view s, char sep);
CORE_EXPORT std::string encode_escapes(std::string_view s, std::string_view symb, std::string_view code);
CORE_EXPORT std::string decode_escapes(std::string_view s, std::string_view symb, std::string_view code);
CORE_EXPORT std::pair<unsigned, unsigned> parse_flag_string(
    std::string_view s, const std::vector<std::pair<std::string_view, unsigned>>& flag_tbl);

CORE_EXPORT int compare_strings_nocase(std::string_view lhs, std::string_view rhs);
CORE_EXPORT std::string to_lower(std::string s);

//-------------------------------------------------------------------
// String converters

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

template<typename Ty>
struct string_converter;

template<>
struct CORE_EXPORT string_converter<int16_t> : string_converter_base<int16_t> {
    static const char* from_string(std::string_view s, int16_t& val, bool* ok = nullptr);
    static std::string to_string(int16_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<int32_t> : string_converter_base<int32_t> {
    static const char* from_string(std::string_view s, int32_t& val, bool* ok = nullptr);
    static std::string to_string(int32_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<int64_t> : string_converter_base<int64_t> {
    static const char* from_string(std::string_view s, int64_t& val, bool* ok = nullptr);
    static std::string to_string(int64_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<uint16_t> : string_converter_base<uint16_t> {
    static const char* from_string(std::string_view s, uint16_t& val, bool* ok = nullptr);
    static std::string to_string(uint16_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<uint32_t> : string_converter_base<uint32_t> {
    static const char* from_string(std::string_view s, uint32_t& val, bool* ok = nullptr);
    static std::string to_string(uint32_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<uint64_t> : string_converter_base<uint64_t> {
    static const char* from_string(std::string_view s, uint64_t& val, bool* ok = nullptr);
    static std::string to_string(uint64_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct CORE_EXPORT string_converter<float> : string_converter_base<float> {
    static const char* from_string(std::string_view s, float& val, bool* ok = nullptr);
    static std::string to_string(float val, scvt_fp fmt = scvt_fp::kGeneral, int prec = -1);
};

template<>
struct CORE_EXPORT string_converter<double> : string_converter_base<double> {
    static const char* from_string(std::string_view s, double& val, bool* ok = nullptr);
    static std::string to_string(double val, scvt_fp fmt = scvt_fp::kGeneral, int prec = -1);
};

template<>
struct CORE_EXPORT string_converter<bool> : string_converter_base<bool> {
    static const char* from_string(std::string_view s, bool& val, bool* ok = nullptr);
    static std::string to_string(bool val) { return val ? "true" : "false"; }
};

template<typename Ty, typename Def, typename... Args>
Ty from_string(std::string_view s, Def&& def, Args&&... args) {
    Ty result(std::forward<Def>(def));
    string_converter<Ty>::from_string(s, result, std::forward<Args>(args)...);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    return string_converter<Ty>::to_string(val, std::forward<Args>(args)...);
}

#ifdef USE_QT
template<>
struct qt_type_converter<QString> {
    using is_qt_type_converter = int;
    QString to_qt(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<int>(s.size())); }
    template<typename Ty, typename = std::void_t<typename string_converter<Ty>::is_string_converter>, typename... Args>
    QString to_qt(const Ty& val, Args&&... args) {
        return QString::fromStdString(to_string(val, std::forward<Args>(args)...));
    }
    template<typename Ty>
    Ty from_qt(const QString& s) {
        return from_string<Ty>(s.toStdString());
    }
};

template<>
inline std::string qt_type_converter<QString>::from_qt(const QString& s) {
    return s.toStdString();
}

inline int compare_strings_nocase(const QString& lhs, const QString& rhs) {
    return lhs.compare(rhs, Qt::CaseInsensitive);
}
#endif  // USE_QT

//-------------------------------------------------------------------
// String formatter

struct sfield {
    explicit sfield(int in_width, char in_fill = ' ') : width(in_width), fill(in_fill) {}
    int width;
    char fill;
};

class CORE_EXPORT sformat {
 public:
    explicit sformat(std::string_view fmt) : fmt_(fmt) {}
    std::string str() const;
    operator std::string() const { return str(); }
#ifdef USE_QT
    operator QString() const { return to_qt<QString>(str()); }
#endif  // USE_QT

    template<typename Ty, typename... Args>
    sformat& arg(Ty&& v, Args&&... args) {
        args_.emplace_back(arg_cvt(std::forward<Ty>(v), std::forward<Args>(args)...));
        return *this;
    }

    template<typename Ty, typename... Args>
    sformat& arg(Ty&& v, sfield field, Args&&... args) {
        auto s = arg_cvt(std::forward<Ty>(v), std::forward<Args>(args)...);
        auto width = static_cast<size_t>(field.width);
        if (width > s.size()) { s.insert(0, width - s.size(), field.fill); }
        args_.emplace_back(std::move(s));
        return *this;
    }

 private:
    std::string arg_cvt(std::string_view s) { return static_cast<std::string>(s); }
    std::string arg_cvt(std::string s) { return std::move(s); }
    std::string arg_cvt(const char* s) { return std::string(s); }
    std::string arg_cvt(const sformat& fmt) { return fmt.str(); }
    std::string arg_cvt(void* p) { return arg_cvt(reinterpret_cast<uintptr_t>(p), scvt_base::kBase16); }
#ifdef USE_QT
    std::string arg_cvt(const QString& s) { return from_qt<std::string>(s); }
#endif  // USE_QT

    template<typename Ty, typename = std::void_t<typename string_converter<Ty>::is_string_converter>, typename... Args>
    std::string arg_cvt(const Ty& v, Args&&... args) {
        return to_string(v, std::forward<Args>(args)...);
    }

    std::string_view fmt_;
    std::vector<std::string> args_;
};

}  // namespace util
