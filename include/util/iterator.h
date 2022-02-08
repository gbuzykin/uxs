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
using is_output_iterator = std::bool_constant<
    std::is_base_of<std::output_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>::value ||
    (std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>::value &&
     !std::is_const<std::remove_reference_t<typename std::iterator_traits<Iter>::reference>>::value)>;

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
        ++(*this);
        return it;
    }

    Iter& operator+=(difference_type j) NOEXCEPT {
        static_cast<Iter&>(*this).advance(j);
        return static_cast<Iter&>(*this);
    }

    Iter operator+(difference_type j) const NOEXCEPT {
        auto it = static_cast<const Iter&>(*this);
        it += j;
        return it;
    }

    Iter& operator--() NOEXCEPT {
        static_cast<Iter&>(*this).decrement();
        return static_cast<Iter&>(*this);
    }

    Iter operator--(int) NOEXCEPT {
        auto it = static_cast<Iter&>(*this);
        --(*this);
        return it;
    }

    Iter& operator-=(difference_type j) NOEXCEPT {
        static_cast<Iter&>(*this).advance(-j);
        return static_cast<Iter&>(*this);
    }

    Iter operator-(difference_type j) const NOEXCEPT {
        auto it = static_cast<const Iter&>(*this);
        it -= j;
        return it;
    }

    reference operator*() const NOEXCEPT { return static_cast<const Iter&>(*this).dereference(); }
    pointer operator->() const NOEXCEPT { return std::addressof(**this); }
    reference operator[](difference_type j) const NOEXCEPT { return *(*this + j); }
};

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator==(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
                const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter1&>(lhs).is_equal_to(static_cast<const Iter2&>(rhs))) {
    return static_cast<const Iter1&>(lhs).is_equal_to(static_cast<const Iter2&>(rhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator!=(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
                const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter1&>(lhs).is_equal_to(static_cast<const Iter2&>(rhs))) {
    return !static_cast<const Iter1&>(lhs).is_equal_to(static_cast<const Iter2&>(rhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator<(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
               const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter1&>(lhs).is_less_than(static_cast<const Iter2&>(rhs))) {
    return static_cast<const Iter1&>(lhs).is_less_than(static_cast<const Iter2&>(rhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator<=(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
                const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter2&>(rhs).is_less_than(static_cast<const Iter1&>(lhs))) {
    return !static_cast<const Iter2&>(rhs).is_less_than(static_cast<const Iter1&>(lhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator>(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
               const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter2&>(rhs).is_less_than(static_cast<const Iter1&>(lhs))) {
    return static_cast<const Iter2&>(rhs).is_less_than(static_cast<const Iter1&>(lhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator>=(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
                const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter1&>(lhs).is_less_than(static_cast<const Iter2&>(rhs))) {
    return !static_cast<const Iter1&>(lhs).is_less_than(static_cast<const Iter2&>(rhs));
}

template<typename Iter1, typename ValTy, typename Tag, typename RefTy1, typename PtrTy1, typename DiffTy,
         typename Iter2, typename RefTy2, typename PtrTy2>
auto operator-(const iterator_facade<Iter1, ValTy, Tag, RefTy1, PtrTy1, DiffTy>& lhs,
               const iterator_facade<Iter2, ValTy, Tag, RefTy2, PtrTy2, DiffTy>& rhs) NOEXCEPT
    -> decltype(static_cast<const Iter2&>(rhs).distance_to(static_cast<const Iter1&>(lhs))) {
    return static_cast<const Iter2&>(rhs).distance_to(static_cast<const Iter1&>(lhs));
}

template<typename Traits, typename Iter, typename Tag, bool Const>
using container_iterator_facade =
    iterator_facade<Iter, typename Traits::value_type, Tag,
                    std::conditional_t<Const, typename Traits::const_reference, typename Traits::reference>,
                    std::conditional_t<Const, typename Traits::const_pointer, typename Traits::pointer>,
                    typename Traits::difference_type>;

//-----------------------------------------------------------------------------
// Array iterator

template<typename PtrTy>
struct ranged_debug_pointer {
    ranged_debug_pointer() NOEXCEPT = default;
    ~ranged_debug_pointer() = default;
    ranged_debug_pointer(const ranged_debug_pointer&) NOEXCEPT = default;
    ranged_debug_pointer& operator=(const ranged_debug_pointer&) NOEXCEPT = default;
#if _ITERATOR_DEBUG_LEVEL != 0
    explicit ranged_debug_pointer(PtrTy in_ptr, PtrTy in_begin, PtrTy in_end) NOEXCEPT : ptr(in_ptr),
                                                                                         begin(in_begin),
                                                                                         end(in_end) {}
    template<typename PtrTy2>
    explicit ranged_debug_pointer(const ranged_debug_pointer<PtrTy2>& p) NOEXCEPT : ptr(p.ptr),
                                                                                    begin(p.begin),
                                                                                    end(p.end) {}
    PtrTy ptr{nullptr}, begin{nullptr}, end{nullptr};
#else   // _ITERATOR_DEBUG_LEVEL != 0
    explicit ranged_debug_pointer(PtrTy in_ptr, PtrTy in_begin, PtrTy in_end) NOEXCEPT : ptr(in_ptr) {
        (void)in_begin, (void)in_end;
    }
    template<typename PtrTy2>
    explicit ranged_debug_pointer(const ranged_debug_pointer<PtrTy2>& p) NOEXCEPT : ptr(p.ptr) {}
    PtrTy ptr{nullptr};
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Traits, bool Const>
class array_iterator : public container_iterator_facade<Traits, array_iterator<Traits, Const>,  //
                                                        std::random_access_iterator_tag, Const> {
 private:
    using super = container_iterator_facade<Traits, array_iterator, std::random_access_iterator_tag, Const>;

 public:
    using reference = typename super::reference;
    using pointer = typename super::pointer;
    using difference_type = typename super::difference_type;

    array_iterator() NOEXCEPT = default;
    explicit array_iterator(pointer ptr, pointer begin, pointer end) NOEXCEPT
        : ptr_(ranged_debug_pointer<pointer>(ptr, begin, end)) {}
    ~array_iterator() = default;
    array_iterator(const array_iterator&) NOEXCEPT = default;
    array_iterator& operator=(const array_iterator&) NOEXCEPT = default;

    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT
        : ptr_(static_cast<ranged_debug_pointer<typename Traits::const_pointer>>(it.ptr_)) {}

    template<bool Const_ = Const>
    array_iterator& operator=(const std::enable_if_t<Const_, array_iterator<Traits, false>>& it) NOEXCEPT {
        ptr_ = static_cast<ranged_debug_pointer<typename Traits::const_pointer>>(it.ptr_);
        return *this;
    }

    pointer ptr(pointer begin, pointer end) const NOEXCEPT {
        (void)begin, (void)end;
        iterator_assert((!begin || (ptr_.begin == begin)) && (!end || (ptr_.end == end)));
        return ptr_.ptr;
    }

    void increment() NOEXCEPT {
        iterator_assert(ptr_.ptr < ptr_.end);
        ++ptr_.ptr;
    }

    void decrement() NOEXCEPT {
        iterator_assert(ptr_.ptr > ptr_.begin);
        --ptr_.ptr;
    }

    void advance(difference_type j) NOEXCEPT {
        iterator_assert((j >= 0) ? (ptr_.end - ptr_.ptr >= j) : (ptr_.ptr - ptr_.begin >= -j));
        ptr_.ptr += j;
    }

    template<bool Const2>
    bool is_equal_to(const array_iterator<Traits, Const2>& it) const NOEXCEPT {
        iterator_assert((ptr_.begin == it.ptr_.begin) && (ptr_.end == it.ptr_.end));
        return ptr_.ptr == it.ptr_.ptr;
    }

    template<bool Const2>
    bool is_less_than(const array_iterator<Traits, Const2>& it) const NOEXCEPT {
        iterator_assert((ptr_.begin == it.ptr_.begin) && (ptr_.end == it.ptr_.end));
        return ptr_.ptr < it.ptr_.ptr;
    }

    template<bool Const2>
    difference_type distance_to(const array_iterator<Traits, Const2>& it) const NOEXCEPT {
        iterator_assert((ptr_.begin == it.ptr_.begin) && (ptr_.end == it.ptr_.end));
        return it.ptr_.ptr - ptr_.ptr;
    }

    reference dereference() const NOEXCEPT {
        iterator_assert(ptr_.ptr < ptr_.end);
        return *ptr_.ptr;
    }

 private:
    template<typename, bool>
    friend class array_iterator;
    ranged_debug_pointer<pointer> ptr_;
};

#ifdef USE_CHECKED_ITERATORS
template<typename Traits, bool Const>
struct std::_Is_checked_helper<array_iterator<Traits, Const>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

//-----------------------------------------------------------------------------
// List iterator

template<typename Traits, typename NodeTy, bool Const>
class list_iterator : public container_iterator_facade<Traits, list_iterator<Traits, NodeTy, Const>,  //
                                                       std::bidirectional_iterator_tag, Const> {
 private:
    using super = container_iterator_facade<Traits, list_iterator, std::bidirectional_iterator_tag, Const>;

 public:
    using reference = typename super::reference;
    using node_type = typename NodeTy::iterator_node_t;

    list_iterator() NOEXCEPT = default;
    explicit list_iterator(node_type* node) NOEXCEPT : node_(node) {}
    ~list_iterator() = default;
    list_iterator(const list_iterator&) NOEXCEPT = default;
    list_iterator& operator=(const list_iterator&) NOEXCEPT = default;

    template<bool Const_ = Const>
    list_iterator(const std::enable_if_t<Const_, list_iterator<Traits, NodeTy, false>>& it) NOEXCEPT : node_(it.node_) {
    }

    template<bool Const_ = Const>
    list_iterator& operator=(const std::enable_if_t<Const_, list_iterator<Traits, NodeTy, false>>& it) NOEXCEPT {
        node_ = it.node_;
        return *this;
    }

    node_type* node(node_type* head) const NOEXCEPT {
        (void)head;
        iterator_assert(!node_ || !head || (NodeTy::get_head(node_) == head));
        return node_;
    }

    void increment() NOEXCEPT {
        iterator_assert(node_ && (node_ != NodeTy::get_head(node_)));
        node_ = NodeTy::get_next(node_);
    }

    void decrement() NOEXCEPT {
        iterator_assert(node_ && (node_ != NodeTy::get_front(NodeTy::get_head(node_))));
        node_ = NodeTy::get_prev(node_);
    }

    template<bool Const2>
    bool is_equal_to(const list_iterator<Traits, NodeTy, Const2>& it) const NOEXCEPT {
        iterator_assert(node_ && it.node_ && (NodeTy::get_head(node_) == NodeTy::get_head(it.node_)));
        return node_ == it.node_;
    }

    reference dereference() const NOEXCEPT {
        iterator_assert(node_);
        return NodeTy::get_value(node_);
    }

 private:
    template<typename, typename, bool>
    friend class list_iterator;
    node_type* node_ = nullptr;
};

#ifdef USE_CHECKED_ITERATORS
template<typename Traits, typename NodeTy, bool Const>
struct std::_Is_checked_helper<list_iterator<Traits, NodeTy, Const>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

//-----------------------------------------------------------------------------
// Const value iterator

template<typename Val>
class const_value_iterator : public iterator_facade<const_value_iterator<Val>, Val,  //
                                                    std::input_iterator_tag, const Val&, const Val*> {
 public:
    explicit const_value_iterator(const Val& v) NOEXCEPT : v_(std::addressof(v)) {}
    ~const_value_iterator() = default;
    const_value_iterator(const const_value_iterator& it) NOEXCEPT = default;
    const_value_iterator& operator=(const const_value_iterator& it) NOEXCEPT = default;

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

template<typename Iter>
bool operator==(const util::iterator_range<Iter>& lhs, const util::iterator_range<Iter>& rhs) {
    return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
}
template<typename Iter>
bool operator!=(const util::iterator_range<Iter>& lhs, const util::iterator_range<Iter>& rhs) {
    return !(lhs == rhs);
}
