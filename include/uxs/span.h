#pragma once

#include "utility.h"

#if __cplusplus >= 202002L && UXS_HAS_INCLUDE(<span>)

#    include <span>

namespace est {
template<typename Ty>
using span = std::span<Ty, std::dynamic_extent>;
}  // namespace est

#else  // span

#    include "iterator.h"

#    include <cassert>
#    include <stdexcept>

namespace est {

template<typename Ty>
class span {
 public:
    using value_type = std::remove_cv_t<Ty>;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using reference = Ty&;
    using const_reference = const Ty&;
    using iterator = uxs::array_iterator<span, pointer, false>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    UXS_CONSTEXPR span() noexcept = default;
    template<typename Ty2, typename = std::enable_if_t<std::is_convertible<Ty2*, Ty*>::value>>
    UXS_CONSTEXPR span(Ty2* v, size_type count) noexcept : begin_(v), size_(count) {}
    template<typename Ty2, std::size_t N, typename = std::enable_if_t<std::is_convertible<Ty2*, Ty*>::value>>
    explicit UXS_CONSTEXPR span(Ty2 (&v)[N]) noexcept : begin_(v), size_(N) {}
    template<typename Range,
             typename = std::enable_if_t<uxs::is_contiguous_range<std::remove_reference_t<Range>, Ty>::value>>
    UXS_CONSTEXPR span(Range&& r) noexcept : begin_(r.data()), size_(r.size()) {}

    UXS_CONSTEXPR size_type size() const noexcept { return size_; }
    UXS_CONSTEXPR bool empty() const noexcept { return size_ == 0; }
    UXS_CONSTEXPR pointer data() const noexcept { return begin_; }

    UXS_CONSTEXPR iterator begin() const noexcept { return iterator(begin_, begin_, begin_ + size_); }
    UXS_CONSTEXPR iterator end() const noexcept { return iterator(begin_ + size_, begin_, begin_ + size_); }

    UXS_CONSTEXPR reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
    UXS_CONSTEXPR reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }

    UXS_CONSTEXPR reference operator[](size_type pos) const {
        assert(pos < size_);
        return begin_[pos];
    }
    reference at(size_type pos) const {
        if (pos < size_) { return begin_[pos]; }
        throw std::out_of_range("index out of range");
    }
    UXS_CONSTEXPR reference front() const {
        assert(size_ > 0);
        return begin_[0];
    }
    UXS_CONSTEXPR reference back() const {
        assert(size_ > 0);
        return *(begin_ + size_ - 1);
    }

    UXS_CONSTEXPR span subspan(size_type offset, size_type count = dynamic_extent) const {
        if (offset > size_) { offset = size_; }
        return span(begin_ + offset, count < size_ - offset ? count : size_ - offset);
    }

 private:
    Ty* begin_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace est

#endif  // span

namespace est {

template<typename Ty>
UXS_CONSTEXPR span<Ty> as_span(Ty* v, typename span<Ty>::size_type count) noexcept {
    return span<Ty>(v, count);
}

template<typename Ty, std::size_t N>
UXS_CONSTEXPR span<Ty> as_span(Ty (&v)[N]) noexcept {
    return span<Ty>(v, N);
}

template<typename Range>
UXS_CONSTEXPR auto as_span(Range&& r) noexcept -> span<std::remove_pointer_t<decltype(r.data() + r.size())>> {
    return span<std::remove_pointer_t<decltype(r.data())>>(r.data(), r.size());
}

}  // namespace est
