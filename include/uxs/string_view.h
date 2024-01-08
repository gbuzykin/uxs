#pragma once

#include "common.h"

#if (__cplusplus >= 201703L || defined(_LIBCPP_VERSION)) && UXS_HAS_INCLUDE(<string_view>)
#    include <string_view>
#else  // string view

#    include "iterator.h"

#    include <algorithm>
#    include <cassert>
#    include <functional>
#    include <stdexcept>
#    include <string>

namespace std {
template<typename CharT, typename Traits = std::char_traits<CharT>>
class basic_string_view {
    static_assert(is_same<CharT, typename Traits::char_type>::value, "bad char_traits for basic_string_view");
    static_assert(!is_array<CharT>::value && is_trivial<CharT>::value && is_standard_layout<CharT>::value,
                  "the character type of basic_string_view must be a non-array trivial standard-layout type");

 public:
    using traits_type = Traits;
    using value_type = CharT;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using reference = CharT&;
    using const_reference = const CharT&;
    using const_iterator = uxs::array_iterator<basic_string_view, true>;
    using iterator = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    static const size_type npos = std::string::npos;

    basic_string_view() NOEXCEPT {}
    basic_string_view(const CharT* s, size_type count) : begin_(s), size_(count) {}
    basic_string_view(const CharT* s) : begin_(s) {
        for (; *s; ++s, ++size_) {}
    }
    template<typename Alloc>
    basic_string_view(const basic_string<CharT, Traits, Alloc>& s) NOEXCEPT : basic_string_view(s.data(), s.size()) {}

    size_type size() const NOEXCEPT { return size_; }
    size_type length() const NOEXCEPT { return size_; }
    bool empty() const NOEXCEPT { return size_ == 0; }

    explicit operator basic_string<CharT, Traits>() const { return basic_string<CharT, Traits>(begin_, size_); }

    const_iterator begin() const NOEXCEPT { return const_iterator(begin_, begin_, begin_ + size_); }
    const_iterator cbegin() const NOEXCEPT { return const_iterator(begin_, begin_, begin_ + size_); }

    const_iterator end() const NOEXCEPT { return const_iterator(begin_ + size_, begin_, begin_ + size_); }
    const_iterator cend() const NOEXCEPT { return const_iterator(begin_ + size_, begin_, begin_ + size_); }

    const_reverse_iterator rbegin() const NOEXCEPT { return const_reverse_iterator{end()}; }
    const_reverse_iterator crbegin() const NOEXCEPT { return const_reverse_iterator{end()}; }

    const_reverse_iterator rend() const NOEXCEPT { return const_reverse_iterator{begin()}; }
    const_reverse_iterator crend() const NOEXCEPT { return const_reverse_iterator{begin()}; }

    const_reference operator[](size_type pos) const {
        assert(pos < size_);
        return begin_[pos];
    }
    const_reference at(size_type pos) const {
        if (pos < size_) { return begin_[pos]; }
        throw out_of_range("index out of range");
    }
    const_reference front() const {
        assert(size_ > 0);
        return begin_[0];
    }
    const_reference back() const {
        assert(size_ > 0);
        return *(begin_ + size_ - 1);
    }
    const_pointer data() const NOEXCEPT { return begin_; }

    basic_string_view substr(size_type pos, size_type count = npos) const {
        if (pos > size_) { pos = size_; }
        return basic_string_view(begin_ + pos, count < size_ - pos ? count : size_ - pos);
    }

    int compare(basic_string_view s) const;
    size_type find(CharT ch, size_type pos = 0) const;
    size_type find(basic_string_view s, size_type pos = 0) const;
    size_type rfind(CharT ch, size_type pos = npos) const;
    size_type rfind(basic_string_view s, size_type pos = npos) const;

    template<typename Traits2, typename Alloc>
    friend basic_string<CharT, Traits2, Alloc>& operator+=(basic_string<CharT, Traits2, Alloc>& lhs,
                                                           basic_string_view rhs) {
        lhs.append(rhs.data(), rhs.size());
        return lhs;
    }

 private:
    const CharT* begin_ = nullptr;
    size_type size_ = 0;
};

template<typename CharT, typename Traits>
int basic_string_view<CharT, Traits>::compare(basic_string_view<CharT, Traits> s) const {
    int result = traits_type::compare(begin_, s.begin_, size_ < s.size_ ? size_ : s.size_);
    if (result != 0) {
        return result;
    } else if (size_ < s.size_) {
        return -1;
    } else if (s.size_ < size_) {
        return 1;
    }
    return 0;
}

template<typename CharT, typename Traits>
auto basic_string_view<CharT, Traits>::find(CharT ch, size_type pos) const -> size_type {
    if (pos >= size_) { return npos; }
    for (const auto* p = begin_ + pos; p != begin_ + size_; ++p) {
        if (traits_type::eq(*p, ch)) { return p - begin_; }
    }
    return npos;
}

template<typename CharT, typename Traits>
auto basic_string_view<CharT, Traits>::find(basic_string_view s, size_type pos) const -> size_type {
    if (size_ < s.size_ || pos > size_ - s.size_) { return npos; }
    if (!s.size_) { return pos; }
    for (const auto* p = begin_ + pos; p != begin_ + size_ - s.size_ + 1; ++p) {
        if (std::equal(s.begin_, s.begin_ + s.size_, p, traits_type::eq)) { return p - begin_; }
    }
    return npos;
}

template<typename CharT, typename Traits>
auto basic_string_view<CharT, Traits>::rfind(CharT ch, size_type pos) const -> size_type {
    if (!size_) { return npos; }
    if (pos >= size_) { pos = size_ - 1; }
    for (const auto* p = begin_ + pos + 1; p != begin_; --p) {
        if (traits_type::eq(*(p - 1), ch)) { return p - begin_ - 1; }
    }
    return npos;
}

template<typename CharT, typename Traits>
auto basic_string_view<CharT, Traits>::rfind(basic_string_view s, size_type pos) const -> size_type {
    if (size_ < s.size_) { return npos; }
    if (pos > size_ - s.size_) { pos = size_ - s.size_; }
    if (!s.size_) { return pos; }
    for (auto p = begin_ + pos + 1; p != begin_; --p) {
        if (std::equal(s.begin_, s.begin_ + s.size_, p - 1, traits_type::eq)) { return p - begin_ - 1; }
    }
    return npos;
}

template<typename CharT, typename Traits>
bool operator==(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}
template<typename CharT, typename Traits>
bool operator!=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.size() != rhs.size() || lhs.compare(rhs) != 0;
}
template<typename CharT, typename Traits>
bool operator<(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.compare(rhs) < 0;
}
template<typename CharT, typename Traits>
bool operator<=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.compare(rhs) <= 0;
}
template<typename CharT, typename Traits>
bool operator>(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.compare(rhs) > 0;
}
template<typename CharT, typename Traits>
bool operator>=(basic_string_view<CharT, Traits> lhs, basic_string_view<CharT, Traits> rhs) {
    return lhs.compare(rhs) >= 0;
}

template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator==(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs == basic_string_view<CharT, Traits>(rhs);
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator!=(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs != basic_string_view<CharT, Traits>(rhs);
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator<(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs < basic_string_view<CharT, Traits>(rhs);
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator<=(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs <= basic_string_view<CharT, Traits>(rhs);
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator>(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs > basic_string_view<CharT, Traits>(rhs);
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator>=(basic_string_view<CharT, Traits> lhs, const Ty& rhs) {
    return lhs >= basic_string_view<CharT, Traits>(rhs);
}

template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator==(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) == rhs;
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator!=(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) != rhs;
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator<(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) < rhs;
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator<=(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) <= rhs;
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator>(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) > rhs;
}
template<typename CharT, typename Traits, typename Ty,
         typename = std::enable_if_t<std::is_convertible<Ty, basic_string_view<CharT, Traits>>::value>>
bool operator>=(const Ty& lhs, basic_string_view<CharT, Traits> rhs) {
    return basic_string_view<CharT, Traits>(lhs) >= rhs;
}

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;

#    if defined(_MSC_VER)
template<typename CharT, typename Traits>
struct hash<basic_string_view<CharT, Traits>> {
    size_t operator()(basic_string_view<CharT, Traits> s) const {
        return _Hash_seq((const unsigned char*)s.data(), s.size());
    }
};
#    else   // defined(_MSC_VER)
template<typename CharT, typename Traits>
struct hash<basic_string_view<CharT, Traits>> {
    size_t operator()(basic_string_view<CharT, Traits> s) const {
        return std::hash<std::basic_string<CharT, Traits>>{}(static_cast<std::basic_string<CharT, Traits>>(s));
    }
};
#    endif  // defined(_MSC_VER)
}  // namespace std

#endif  // string view
