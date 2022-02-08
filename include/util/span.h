#pragma once

#include "iterator.h"

#include <limits>
#include <stdexcept>

namespace util {

#if __cplusplus < 201703L
const size_t dynamic_extent = size_t(-1);
#else   // __cplusplus < 201703L
constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();
#endif  // __cplusplus < 201703L

namespace detail {
template<typename Range, typename Ty>
struct is_span_convertible {
    template<typename Range_>
    static auto test(Range_* r) -> std::is_same<std::decay_t<decltype(*(r->data() + r->size()))>, Ty>;
    template<typename Range_>
    static std::false_type test(...);
    using type = decltype(test<std::remove_reference_t<Range>>(nullptr));
};
}  // namespace detail

template<typename Range, typename Ty>
struct is_span_convertible : detail::is_span_convertible<Range, Ty>::type {};

template<typename Ty>
class span {
 public:
    using value_type = std::remove_cv_t<Ty>;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using reference = Ty&;
    using const_reference = const Ty&;
    using iterator = util::array_iterator<span, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;

    span() = default;
    template<typename Ty2, typename = std::enable_if_t<std::is_same<std::remove_cv_t<Ty2>, value_type>::value>>
    span(Ty2* v, size_type count) : begin_(v), size_(count) {}
    template<typename Range, typename = std::enable_if_t<is_span_convertible<Range, value_type>::value>>
    span(Range&& r) : begin_(r.data()), size_(r.size()) {}

    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }

    iterator begin() const { return iterator{begin_, begin_, begin_ + size_}; }
    iterator end() const { return iterator{begin_ + size_, begin_, begin_ + size_}; }

    reverse_iterator rbegin() const { return reverse_iterator{end()}; }
    reverse_iterator rend() const { return reverse_iterator{begin()}; }

    reference operator[](size_type pos) const {
        assert(pos < size_);
        return begin_[pos];
    }
    reference at(size_type pos) const {
        if (pos >= size_) { throw std::out_of_range("invalid pos"); }
        return begin_[pos];
    }
    reference front() const {
        assert(size_ > 0);
        return begin_[0];
    }
    reference back() const {
        assert(size_ > 0);
        return *(begin_ + size_ - 1);
    }
    pointer data() const { return begin_; }

    span subspan(size_type offset, size_type count = dynamic_extent) const {
        offset = std::min(offset, size_);
        return span(begin_ + offset, std::min(count, size_ - offset));
    }

 private:
    Ty* begin_ = nullptr;
    size_t size_ = 0;
};

}  // namespace util
