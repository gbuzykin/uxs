#pragma once

#include "utility.h"

#include <iterator>
#include <memory>

#if _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) assert(cond)
#else  // _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) ((void)0)
#endif  // _ITERATOR_DEBUG_LEVEL != 0

#if defined(_MSC_VER)
#    define USE_CHECKED_ITERATORS
#endif  // defined(_MSC_VER)

namespace util {

template<typename Iter>
using is_input_iterator =
    std::is_base_of<std::input_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

template<typename Iter>
using is_random_access_iterator =
    std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

//-----------------------------------------------------------------------------
// Iterator range

template<typename Iter, typename = void>
class iterator_range;
template<typename Iter>
class iterator_range<Iter, std::enable_if_t<is_input_iterator<Iter>::value>> {
 public:
    using iterator = Iter;
    iterator_range(Iter from, Iter to) : from_(from), to_(to) {}
    Iter begin() const { return from_; }
    Iter end() const { return to_; }
    bool empty() const { return from_ == to_; }

 private:
    Iter from_, to_;
};

template<typename Iter>
iterator_range<Iter> make_range(Iter from, Iter to) {
    return {from, to};
}

template<typename Iter>
iterator_range<Iter> make_range(const std::pair<Iter, Iter>& p) {
    return {p.first, p.second};
}

template<typename Iter>
iterator_range<std::reverse_iterator<Iter>> reverse_range(Iter from, Iter to) {
    return {std::reverse_iterator<Iter>(to), std::reverse_iterator<Iter>(from)};
}

template<typename Iter>
iterator_range<std::reverse_iterator<Iter>> reverse_range(const std::pair<Iter, Iter>& p) {
    return {std::reverse_iterator<Iter>(p.second), std::reverse_iterator<Iter>(p.first)};
}

template<typename Range>
auto reverse_range(Range&& r) -> iterator_range<std::reverse_iterator<decltype(std::end(r))>> {
    return {std::reverse_iterator<decltype(std::end(r))>(std::end(r)),
            std::reverse_iterator<decltype(std::begin(r))>(std::begin(r))};
}

template<typename IterL, typename IterR>
bool operator==(const iterator_range<IterL>& lhs, const iterator_range<IterR>& rhs) {
    return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
}
template<typename IterL, typename IterR>
bool operator!=(const iterator_range<IterL>& lhs, const iterator_range<IterR>& rhs) {
    return !(lhs == rhs);
}

//-----------------------------------------------------------------------------
// Normal iterator facade

template<typename BaseIter, typename Container = void>
class normal_iterator {
 public:
    using iterator_category = typename BaseIter::iterator_category;
    using value_type = typename BaseIter::value_type;
    using difference_type = typename BaseIter::difference_type;
    using reference = typename BaseIter::reference;
    using pointer = typename BaseIter::pointer;

    normal_iterator() NOEXCEPT : base_() {}
    normal_iterator(const normal_iterator& it) NOEXCEPT : base_(it.base()) {}
    normal_iterator& operator=(const normal_iterator& it) NOEXCEPT {
        base_ = it.base();
        return *this;
    }

    template<typename... Args>
    explicit normal_iterator(const BaseIter& base) NOEXCEPT : base_(base) {}

    template<typename... Args>
    static normal_iterator from_base(Args&&... args) NOEXCEPT {
        return normal_iterator(BaseIter(std::forward<Args>(args)...));
    }

    // Note: iterator can be copied if the underlying iterator can be converted to the type of
    // current underlying iterator
    template<typename BaseIter2>
    normal_iterator(
        const normal_iterator<BaseIter2, std::enable_if_t<std::is_convertible<BaseIter2, BaseIter>::value, Container>>&
            it) NOEXCEPT : base_(it.base()) {}
    template<typename BaseIter2>
    normal_iterator& operator=(
        const normal_iterator<BaseIter2, std::enable_if_t<std::is_convertible<BaseIter2, BaseIter>::value, Container>>&
            it) NOEXCEPT {
        base_ = it.base();
        return *this;
    }

    normal_iterator& operator++() NOEXCEPT {
        base_.increment();
        return *this;
    }

    normal_iterator operator++(int) NOEXCEPT {
        BaseIter base = base_;
        base_.increment();
        return normal_iterator(base);
    }

    normal_iterator& operator+=(difference_type j) NOEXCEPT {
        base_.advance(j);
        return *this;
    }

    normal_iterator operator+(difference_type j) const NOEXCEPT {
        BaseIter base = base_;
        base.advance(j);
        return normal_iterator(base);
    }

    normal_iterator& operator--() NOEXCEPT {
        base_.decrement();
        return *this;
    }

    normal_iterator operator--(int) NOEXCEPT {
        BaseIter base = base_;
        base_.decrement();
        return normal_iterator(base);
    }

    normal_iterator& operator-=(difference_type j) NOEXCEPT {
        base_.advance(-j);
        return *this;
    }

    normal_iterator operator-(difference_type j) const NOEXCEPT {
        BaseIter base = base_;
        base.advance(-j);
        return normal_iterator(base);
    }

    reference operator*() const NOEXCEPT { return base_.dereference(); }
    pointer operator->() const NOEXCEPT { return std::addressof(**this); }
    reference operator[](difference_type j) const NOEXCEPT { return *(*this + j); }

    const BaseIter& base() const NOEXCEPT { return base_; }

 private:
    BaseIter base_;
};

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator==(const normal_iterator<BaseIterL, Container>& lhs,
                const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return lhs.base().is_equal_to(rhs.base());
}
template<typename BaseIter, typename Container>
bool operator==(const normal_iterator<BaseIter, Container>& lhs,
                const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return lhs.base().is_equal_to(rhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator!=(const normal_iterator<BaseIterL, Container>& lhs,
                const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return !lhs.base().is_equal_to(rhs.base());
}
template<typename BaseIter, typename Container>
bool operator!=(const normal_iterator<BaseIter, Container>& lhs,
                const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return !lhs.base().is_equal_to(rhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator<(const normal_iterator<BaseIterL, Container>& lhs,
               const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return lhs.base().is_less_than(rhs.base());
}
template<typename BaseIter, typename Container>
bool operator<(const normal_iterator<BaseIter, Container>& lhs,
               const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return lhs.base().is_less_than(rhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator<=(const normal_iterator<BaseIterL, Container>& lhs,
                const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return !rhs.base().is_less_than(lhs.base());
}
template<typename BaseIter, typename Container>
bool operator<=(const normal_iterator<BaseIter, Container>& lhs,
                const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return !rhs.base().is_less_than(lhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator>(const normal_iterator<BaseIterL, Container>& lhs,
               const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return rhs.base().is_less_than(lhs.base());
}
template<typename BaseIter, typename Container>
bool operator>(const normal_iterator<BaseIter, Container>& lhs,
               const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return rhs.base().is_less_than(lhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
bool operator>=(const normal_iterator<BaseIterL, Container>& lhs,
                const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT {
    return !lhs.base().is_less_than(rhs.base());
}
template<typename BaseIter, typename Container>
bool operator>=(const normal_iterator<BaseIter, Container>& lhs,
                const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return !lhs.base().is_less_than(rhs.base());
}

template<typename BaseIterL, typename BaseIterR, typename Container>
auto operator-(const normal_iterator<BaseIterL, Container>& lhs,
               const normal_iterator<BaseIterR, Container>& rhs) NOEXCEPT  //
    -> decltype(rhs.base().distance_to(lhs.base())) {
    return rhs.base().distance_to(lhs.base());
}
template<typename BaseIter, typename Container>
typename normal_iterator<BaseIter, Container>::difference_type operator-(
    const normal_iterator<BaseIter, Container>& lhs, const normal_iterator<BaseIter, Container>& rhs) NOEXCEPT {
    return rhs.base().distance_to(lhs.base());
}

template<typename BaseIter, typename Container>
normal_iterator<BaseIter, Container> operator+(typename normal_iterator<BaseIter, Container>::difference_type j,
                                               const normal_iterator<BaseIter, Container>& it) NOEXCEPT {
    BaseIter base = it.base();
    base.advance(j);
    return normal_iterator<BaseIter, Container>(base);
}

#ifdef USE_CHECKED_ITERATORS
template<typename BaseIter, typename Container>
struct std::_Is_checked_helper<normal_iterator<BaseIter, Container>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

//-----------------------------------------------------------------------------
// Array iterator

namespace detail {

template<typename Container, bool Const>
class array_iterator {
 public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = typename Container::value_type;
    using difference_type = typename Container::difference_type;
    using reference = std::conditional_t<Const, typename Container::const_reference, typename Container::reference>;
    using pointer = std::conditional_t<Const, typename Container::const_pointer, typename Container::pointer>;

    template<typename, bool>
    friend class array_iterator;

    array_iterator() = default;
    ~array_iterator() = default;
    array_iterator(const array_iterator&) = default;
    array_iterator& operator=(const array_iterator&) = default;

    void increment() {
        iterator_assert(ptr_ < end_);
        ++ptr_;
    }

    void decrement() {
        iterator_assert(ptr_ > begin_);
        --ptr_;
    }

    void advance(difference_type j) {
        iterator_assert(j >= 0 ? end_ - ptr_ >= j : ptr_ - begin_ >= -j);
        ptr_ += j;
    }

    reference dereference() const {
        iterator_assert(ptr_ < end_);
        return *ptr_;
    }

    template<bool Const2>
    bool is_equal_to(const array_iterator<Container, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ == it.ptr_;
    }

    template<bool Const2>
    bool is_less_than(const array_iterator<Container, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ < it.ptr_;
    }

    template<bool Const2>
    difference_type distance_to(const array_iterator<Container, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return it.ptr_ - ptr_;
    }

    pointer ptr() const { return ptr_; }

#if _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(pointer ptr, pointer begin, pointer end) : ptr_(ptr), begin_(begin), end_(end) {}
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Container, false>>& p)
        : ptr_(p.ptr_), begin_(p.begin_), end_(p.end_) {}

    pointer begin() const { return begin_; }
    pointer end() const { return end_; }

 private:
    pointer ptr_{nullptr}, begin_{nullptr}, end_{nullptr};
#else   // _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(pointer ptr, pointer begin, pointer end) : ptr_(ptr) { (void)begin, (void)end; }
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Container, false>>& p) : ptr_(p.ptr_) {}

 private:
    pointer ptr_{nullptr};
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

}  // namespace detail

template<typename Container, bool Const>
using array_iterator = normal_iterator<detail::array_iterator<Container, Const>, Container>;

//-----------------------------------------------------------------------------
// List iterator

namespace detail {

template<typename Container, typename NodeTy, bool Const>
class list_iterator {
 public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = typename Container::value_type;
    using difference_type = typename Container::difference_type;
    using reference = std::conditional_t<Const, typename Container::const_reference, typename Container::reference>;
    using pointer = std::conditional_t<Const, typename Container::const_pointer, typename Container::pointer>;
    using node_type = typename NodeTy::iterator_node_t;

    template<typename, typename, bool>
    friend class list_iterator;

    list_iterator() = default;
    explicit list_iterator(node_type* node) : node_(node) {}
    ~list_iterator() = default;
    list_iterator(const list_iterator&) = default;
    list_iterator& operator=(const list_iterator&) = default;

    template<bool Const_ = Const>
    list_iterator(const std::enable_if_t<Const_, list_iterator<Container, NodeTy, false>>& it) : node_(it.node_) {}
    template<bool Const_ = Const>
    list_iterator& operator=(const std::enable_if_t<Const_, list_iterator<Container, NodeTy, false>>& it) {
        node_ = it.node_;
        return *this;
    }

    void increment() {
        iterator_assert(node_ && node_ != NodeTy::get_head(node_));
        node_ = NodeTy::get_next(node_);
    }

    void decrement() {
        iterator_assert(node_ && node_ != NodeTy::get_front(NodeTy::get_head(node_)));
        node_ = NodeTy::get_prev(node_);
    }

    reference dereference() const {
        iterator_assert(node_);
        return NodeTy::get_value(node_);
    }

    template<bool Const2>
    bool is_equal_to(const list_iterator<Container, NodeTy, Const2>& it) const {
        iterator_assert(node_ && it.node_ && NodeTy::get_head(node_) == NodeTy::get_head(it.node_));
        return node_ == it.node_;
    }

    node_type* node() const { return node_; }

 private:
    node_type* node_ = nullptr;
};

}  // namespace detail

template<typename Container, typename NodeTy, bool Const>
using list_iterator = normal_iterator<detail::list_iterator<Container, NodeTy, Const>, Container>;

//-----------------------------------------------------------------------------
// Const value iterator

namespace detail {

template<typename Val>
class const_value_iterator {
 public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Val;
    using difference_type = std::ptrdiff_t;
    using reference = const Val&;
    using pointer = const Val*;

    explicit const_value_iterator(const Val& v) : v_(std::addressof(v)) {}
    ~const_value_iterator() = default;
    const_value_iterator(const const_value_iterator&) = default;
    const_value_iterator& operator=(const const_value_iterator&) = default;

    void increment() {}
    void advance(difference_type j) {}
    const Val& dereference() const { return *v_; }
    bool is_equal_to(const const_value_iterator& it) const {
        iterator_assert(v_ == it.v_);
        return true;
    }

 private:
    const Val* v_;
};

}  // namespace detail

template<typename Val>
using const_value_iterator = normal_iterator<detail::const_value_iterator<Val>>;

template<typename Val>
const_value_iterator<Val> const_value(const Val& v) NOEXCEPT {
    return const_value_iterator<Val>::from_base(v);
}

}  // namespace util
