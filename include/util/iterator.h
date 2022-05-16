#pragma once

#include "utility.h"

#include <iterator>
#include <memory>

#if _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) assert(cond)
#else  // _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) ((void)0)
#endif  // _ITERATOR_DEBUG_LEVEL != 0

#if defined(_MSC_VER) && _MSC_VER < 1920
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
// Iterator facade

template<typename Iter, typename ValTy, typename Tag,  //
         typename RefTy, typename PtrTy, typename DiffTy = std::ptrdiff_t>
class iterator_facade {
 public:
    using iterator_category = Tag;
    using value_type = ValTy;
    using difference_type = DiffTy;
    using reference = RefTy;
    using pointer = PtrTy;

    Iter& operator++() NOEXCEPT {
        static_cast<Iter&>(*this).increment();
        return static_cast<Iter&>(*this);
    }

    Iter operator++(int) NOEXCEPT {
        auto it = static_cast<Iter&>(*this);
        static_cast<Iter&>(*this).increment();
        return it;
    }

    Iter& operator--() NOEXCEPT {
        static_cast<Iter&>(*this).decrement();
        return static_cast<Iter&>(*this);
    }

    Iter operator--(int) NOEXCEPT {
        auto it = static_cast<Iter&>(*this);
        static_cast<Iter&>(*this).decrement();
        return it;
    }

    template<typename Iter_ = Iter>
    auto operator+=(difference_type j) NOEXCEPT
        -> util::type_identity_t<Iter&, decltype(std::declval<Iter_>().advance(j))> {
        static_cast<Iter&>(*this).advance(j);
        return static_cast<Iter&>(*this);
    }

    template<typename Iter_ = Iter>
    auto operator+(difference_type j) const NOEXCEPT
        -> util::type_identity_t<Iter, decltype(std::declval<Iter_>().advance(j))> {
        auto it = static_cast<const Iter&>(*this);
        it.advance(j);
        return it;
    }

    template<typename Iter_ = Iter>
    auto operator-=(difference_type j) NOEXCEPT
        -> util::type_identity_t<Iter&, decltype(std::declval<Iter_>().advance(j))> {
        static_cast<Iter&>(*this).advance(-j);
        return static_cast<Iter&>(*this);
    }

    template<typename Iter_ = Iter>
    auto operator-(difference_type j) const NOEXCEPT
        -> util::type_identity_t<Iter, decltype(std::declval<Iter_>().advance(j))> {
        auto it = static_cast<const Iter&>(*this);
        it.advance(-j);
        return it;
    }

    reference operator*() const NOEXCEPT { return static_cast<const Iter&>(*this).dereference(); }
    pointer operator->() const NOEXCEPT { return std::addressof(**this); }
    template<typename Iter_ = Iter>
    auto operator[](difference_type j) const NOEXCEPT
        -> util::type_identity_t<reference, decltype(std::declval<Iter_>().advance(j))> {
        return *(*this + j);
    }
};

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator==(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs))) {
    return static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator!=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs))) {
    return !static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator<(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs))) {
    return static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator<=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs))) {
    return !static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator>(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs))) {
    return static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator>=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs))) {
    return !static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator-(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const IterR&>(rhs).distance_to(static_cast<const IterL&>(lhs))) {
    return static_cast<const IterR&>(rhs).distance_to(static_cast<const IterL&>(lhs));
}

template<typename Iter, typename ValTy, typename Tag, typename RefTy, typename PtrTy, typename DiffTy>
auto operator+(typename iterator_facade<Iter, ValTy, Tag, RefTy, PtrTy, DiffTy>::difference_type j,
               const iterator_facade<Iter, ValTy, Tag, RefTy, PtrTy, DiffTy>& it) NOEXCEPT
    -> util::type_identity_t<Iter, decltype(std::declval<Iter>().advance(j))> {
    auto result = static_cast<const Iter&>(it);
    result.advance(j);
    return result;
}

template<typename Traits, typename Iter, typename Tag, bool Const>
using container_iterator_facade =
    iterator_facade<Iter, typename Traits::value_type, Tag,
                    std::conditional_t<Const, typename Traits::const_reference, typename Traits::reference>,
                    std::conditional_t<Const, typename Traits::const_pointer, typename Traits::pointer>,
                    typename Traits::difference_type>;

//-----------------------------------------------------------------------------
// Array iterator

template<typename Traits, bool Const>
class array_iterator : public container_iterator_facade<Traits, array_iterator<Traits, Const>,  //
                                                        std::random_access_iterator_tag, Const> {
 private:
    using super = container_iterator_facade<Traits, array_iterator, std::random_access_iterator_tag, Const>;

 public:
    using reference = typename super::reference;
    using pointer = typename super::pointer;
    using difference_type = typename super::difference_type;

    template<typename, bool>
    friend class array_iterator;

    array_iterator() NOEXCEPT {}
    ~array_iterator() {}  // explicit destructor is required by standard

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
    bool is_equal_to(const array_iterator<Traits, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ == it.ptr_;
    }

    template<bool Const2>
    bool is_less_than(const array_iterator<Traits, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ < it.ptr_;
    }

    template<bool Const2>
    difference_type distance_to(const array_iterator<Traits, Const2>& it) const {
        iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return it.ptr_ - ptr_;
    }

    pointer ptr() const { return ptr_; }

    // explicit non-trivial copy operations are required by standard
#if _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(pointer ptr, pointer begin, pointer end) NOEXCEPT : ptr_(ptr), begin_(begin), end_(end) {}
    array_iterator(const array_iterator& it) NOEXCEPT : ptr_(it.ptr_), begin_(it.begin_), end_(it.end_) {}
    array_iterator& operator=(const array_iterator& it) NOEXCEPT {
        ptr_ = it.ptr_, begin_ = it.begin_, end_ = it.end_;
        return *this;
    }
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT : ptr_(it.ptr_),
                                                                                                 begin_(it.begin_),
                                                                                                 end_(it.end_) {}
    template<bool Const_ = Const>
    array_iterator& operator=(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT {
        ptr_ = it.ptr_, begin_ = it.begin_, end_ = it.end_;
        return *this;
    }
    pointer begin() const { return begin_; }
    pointer end() const { return end_; }

 private:
    pointer ptr_{nullptr}, begin_{nullptr}, end_{nullptr};
#else   // _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(pointer ptr, pointer begin, pointer end) NOEXCEPT : ptr_(ptr) { (void)begin, (void)end; }
    array_iterator(const array_iterator& it) NOEXCEPT : ptr_(it.ptr_) {}
    array_iterator& operator=(const array_iterator& it) NOEXCEPT {
        ptr_ = it.ptr_;
        return *this;
    }
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT : ptr_(it.ptr_) {}
    template<bool Const_ = Const>
    array_iterator& operator=(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT {
        ptr_ = it.ptr_;
        return *this;
    }

 private:
    pointer ptr_{nullptr};
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

#ifdef USE_CHECKED_ITERATORS
template<typename Traits, bool Const>
struct std::_Is_checked_helper<array_iterator<Traits, Const>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

//-----------------------------------------------------------------------------
// List iterator

template<typename Traits, typename NodeTraits, bool Const>
class list_iterator : public container_iterator_facade<Traits, list_iterator<Traits, NodeTraits, Const>,  //
                                                       std::bidirectional_iterator_tag, Const> {
 private:
    using super = container_iterator_facade<Traits, list_iterator, std::bidirectional_iterator_tag, Const>;

 public:
    using reference = typename super::reference;
    using node_type = typename NodeTraits::iterator_node_t;

    template<typename, typename, bool>
    friend class list_iterator;

    list_iterator() NOEXCEPT {}
    explicit list_iterator(node_type* node) NOEXCEPT : node_(node) {}

    // this iterator consists of one natural pointer,
    // so explicit copy constructor and operator are not needed

    template<bool Const_ = Const>
    list_iterator(const std::enable_if_t<Const_, list_iterator<Traits, NodeTraits, false>>& it) NOEXCEPT
        : node_(it.node_) {}
    template<bool Const_ = Const>
    list_iterator& operator=(const std::enable_if_t<Const_, list_iterator<Traits, NodeTraits, false>>& it) NOEXCEPT {
        node_ = it.node_;
        return *this;
    }

    void increment() NOEXCEPT {
        iterator_assert(node_ && (node_ != NodeTraits::get_head(node_)));
        node_ = NodeTraits::get_next(node_);
    }

    void decrement() NOEXCEPT {
        iterator_assert(node_ && (node_ != NodeTraits::get_front(NodeTraits::get_head(node_))));
        node_ = NodeTraits::get_prev(node_);
    }

    template<bool Const2>
    bool is_equal_to(const list_iterator<Traits, NodeTraits, Const2>& it) const NOEXCEPT {
        iterator_assert(node_ && it.node_ && (NodeTraits::get_head(node_) == NodeTraits::get_head(it.node_)));
        return node_ == it.node_;
    }

    reference dereference() const NOEXCEPT {
        iterator_assert(node_);
        return NodeTraits::get_value(node_);
    }

    node_type* node() const { return node_; }

 private:
    node_type* node_ = nullptr;
};

#ifdef USE_CHECKED_ITERATORS
template<typename Traits, typename NodeTraits, bool Const>
struct std::_Is_checked_helper<list_iterator<Traits, NodeTraits, Const>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

//-----------------------------------------------------------------------------
// Const value iterator

template<typename Val>
class const_value_iterator : public iterator_facade<const_value_iterator<Val>, Val,  //
                                                    std::input_iterator_tag, const Val&, const Val*> {
 public:
    explicit const_value_iterator(const Val& v) NOEXCEPT : v_(std::addressof(v)) {}
    ~const_value_iterator() {}
    const_value_iterator(const const_value_iterator& it) NOEXCEPT : v_(it.v_) {}
    const_value_iterator& operator=(const const_value_iterator& it) NOEXCEPT {
        v_ = it.v_;
        return *this;
    }

    void increment() NOEXCEPT {}
    void advance(std::ptrdiff_t j) NOEXCEPT {}
    const Val& dereference() const NOEXCEPT { return *v_; }
    bool is_equal_to(const const_value_iterator& it) const NOEXCEPT {
        iterator_assert(v_ == it.v_);
        return true;
    }

 private:
    const Val* v_;
};

template<typename Val>
const_value_iterator<Val> const_value(const Val& v) NOEXCEPT {
    return const_value_iterator<Val>(v);
}

#ifdef USE_CHECKED_ITERATORS
template<typename Val>
struct std::_Is_checked_helper<const_value_iterator<Val>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

}  // namespace util
