#pragma once

#include "utility.h"

#include <cassert>  // NOLINT
#include <iterator>
#include <limits>
#include <memory>

#if _ITERATOR_DEBUG_LEVEL != 0
#    define uxs_iterator_assert(cond) assert(cond)
#else  // _ITERATOR_DEBUG_LEVEL != 0
#    define uxs_iterator_assert(cond) ((void)0)
#endif  // _ITERATOR_DEBUG_LEVEL != 0

namespace uxs {

template<typename Iter>
using is_input_iterator =
    std::is_base_of<std::input_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

template<typename Iter>
using is_forward_iterator =
    std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

template<typename Iter>
using is_bidirectional_iterator =
    std::is_base_of<std::bidirectional_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

template<typename Iter>
using is_random_access_iterator =
    std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>;

template<typename Iter, typename Ty, typename = void>
struct is_output_iterator : std::false_type {};

template<typename Iter, typename Ty>
struct is_output_iterator<Iter, Ty, std::void_t<decltype(*std::declval<Iter&>()++ = std::declval<Ty>())>>
    : std::true_type {};

template<typename Iter>
using iterator_value_t = std::decay_t<decltype(*std::declval<Iter>())>;

//-----------------------------------------------------------------------------
// Iterator range

#if __cplusplus < 201703L
const std::size_t dynamic_extent = ~std::size_t(0);
#else   // __cplusplus < 201703L
inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();
#endif  // __cplusplus < 201703L

template<typename Range, typename = void>
struct range_element {};
template<typename Range>
struct range_element<Range, std::void_t<decltype(*std::end(std::declval<const Range&>()))>> {
    using type = std::remove_cvref_t<decltype(*std::end(std::declval<const Range&>()))>;
};
template<typename Range>
using range_element_t = typename range_element<Range>::type;

template<typename Range, typename Ty, typename = void>
struct is_contiguous_range : std::false_type {};
template<typename Range, typename Ty>
struct is_contiguous_range<Range, Ty,
                           std::enable_if_t<std::is_convertible<
                               decltype(std::declval<Range&>().data() + std::declval<Range&>().size()), Ty*>::value>>
    : std::true_type {};

template<typename Iter, typename = void>
class iterator_range;
template<typename Iter>
class iterator_range<Iter, std::enable_if_t<is_input_iterator<Iter>::value>> {
 public:
    using iterator = Iter;
    iterator_range(Iter from, Iter to) noexcept : from_(from), to_(to) {}
    Iter begin() const noexcept { return from_; }
    Iter end() const noexcept { return to_; }
    bool empty() const noexcept { return from_ == to_; }

 private:
    Iter from_, to_;
};

template<typename Iter>
iterator_range<Iter> make_range(Iter from, Iter to) noexcept {
    return {from, to};
}

template<typename Iter>
iterator_range<Iter> make_range(const std::pair<Iter, Iter>& p) noexcept {
    return {p.first, p.second};
}

template<typename Range>
auto make_range(Range&& r) noexcept -> iterator_range<decltype(std::end(r))> {
    return {std::begin(r), std::end(r)};
}

template<typename Iter>
iterator_range<std::reverse_iterator<Iter>> make_reverse_range(Iter from, Iter to) noexcept {
    return {std::reverse_iterator<Iter>(to), std::reverse_iterator<Iter>(from)};
}

template<typename Iter>
iterator_range<std::reverse_iterator<Iter>> make_reverse_range(const std::pair<Iter, Iter>& p) noexcept {
    return {std::reverse_iterator<Iter>(p.second), std::reverse_iterator<Iter>(p.first)};
}

template<typename Range>
auto make_reverse_range(Range&& r) noexcept -> iterator_range<std::reverse_iterator<decltype(std::end(r))>> {
    return {std::reverse_iterator<decltype(std::end(r))>(std::end(r)),
            std::reverse_iterator<decltype(std::end(r))>(std::begin(r))};
}

template<typename Iter, typename = std::enable_if_t<is_random_access_iterator<Iter>::value>>
iterator_range<Iter> make_subrange(const std::pair<Iter, Iter>& p, std::size_t offset,
                                   std::size_t count = dynamic_extent) noexcept {
    std::size_t sz = p.second - p.first;
    if (offset > sz) { offset = sz; }
    return {p.first + offset, p.first + (count < sz - offset ? offset + count : sz)};
}

template<typename Range>
auto make_subrange(Range&& r, std::size_t offset, std::size_t count = dynamic_extent) noexcept
    -> std::enable_if_t<is_random_access_iterator<decltype(std::end(r))>::value, iterator_range<decltype(std::end(r))>> {
    std::size_t sz = std::end(r) - std::begin(r);
    if (offset > sz) { offset = sz; }
    return {std::begin(r) + offset, std::begin(r) + (count < sz - offset ? offset + count : sz)};
}

template<typename IterL, typename IterR>
bool operator==(const iterator_range<IterL>& lhs, const iterator_range<IterR>& rhs) {
    return std::begin(lhs) == std::begin(rhs) && std::end(lhs) == std::end(rhs);
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

    Iter& operator++() noexcept(noexcept(std::declval<Iter>().increment())) {
        static_cast<Iter&>(*this).increment();
        return static_cast<Iter&>(*this);
    }

    Iter operator++(int) noexcept(noexcept(std::declval<Iter>().increment())) {
        auto it = static_cast<Iter&>(*this);
        static_cast<Iter&>(*this).increment();
        return it;
    }

    Iter& operator--() noexcept(noexcept(std::declval<Iter>().decrement())) {
        static_cast<Iter&>(*this).decrement();
        return static_cast<Iter&>(*this);
    }

    Iter operator--(int) noexcept(noexcept(std::declval<Iter>().decrement())) {
        auto it = static_cast<Iter&>(*this);
        static_cast<Iter&>(*this).decrement();
        return it;
    }

    template<typename Iter_ = Iter>
    auto operator+=(difference_type j) noexcept(noexcept(std::declval<Iter>().advance(j)))
        -> uxs::type_identity_t<Iter&, decltype(std::declval<Iter_>().advance(j))> {
        static_cast<Iter&>(*this).advance(j);
        return static_cast<Iter&>(*this);
    }

    template<typename Iter_ = Iter>
    auto operator+(difference_type j) const noexcept(noexcept(std::declval<Iter>().advance(j)))
        -> uxs::type_identity_t<Iter, decltype(std::declval<Iter_>().advance(j))> {
        auto it = static_cast<const Iter&>(*this);
        it.advance(j);
        return it;
    }

    template<typename Iter_ = Iter>
    auto operator-=(difference_type j) noexcept(noexcept(std::declval<Iter>().advance(j)))
        -> uxs::type_identity_t<Iter&, decltype(std::declval<Iter_>().advance(j))> {
        static_cast<Iter&>(*this).advance(-j);
        return static_cast<Iter&>(*this);
    }

    template<typename Iter_ = Iter>
    auto operator-(difference_type j) const noexcept(noexcept(std::declval<Iter>().advance(j)))
        -> uxs::type_identity_t<Iter, decltype(std::declval<Iter_>().advance(j))> {
        auto it = static_cast<const Iter&>(*this);
        it.advance(-j);
        return it;
    }

    template<typename Iter_ = Iter>
    auto operator*() const
        noexcept(noexcept(std::declval<Iter>().dereference())) -> decltype(std::declval<Iter_>().dereference()) {
        return static_cast<const Iter&>(*this).dereference();
    }

    template<typename Iter_ = Iter>
    auto operator->() const noexcept(noexcept(std::addressof(std::declval<Iter>().dereference())))
        -> decltype(std::addressof(std::declval<Iter_>().dereference())) {
        return std::addressof(**this);
    }

    template<typename Iter_ = Iter>
    auto operator[](difference_type j) const
        noexcept(noexcept(std::declval<Iter>().dereference()) &&
                 noexcept(std::declval<Iter>().advance(j))) -> decltype(std::declval<Iter_>().dereference()) {
        return *(*this + j);
    }
};

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator==(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs))) {
    return static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator!=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs))) {
    return !static_cast<const IterL&>(lhs).is_equal_to(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator<(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs))) {
    return static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator<=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs))) {
    return !static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator>(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs))) {
    return static_cast<const IterR&>(rhs).is_less_than(static_cast<const IterL&>(lhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator>=(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
                const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs))) {
    return !static_cast<const IterL&>(lhs).is_less_than(static_cast<const IterR&>(rhs));
}

template<typename IterL, typename ValTy, typename Tag, typename RefTyL, typename PtrTyL, typename DiffTy,
         typename IterR, typename RefTyR, typename PtrTyR>
auto operator-(const iterator_facade<IterL, ValTy, Tag, RefTyL, PtrTyL, DiffTy>& lhs,
               const iterator_facade<IterR, ValTy, Tag, RefTyR, PtrTyR, DiffTy>& rhs) noexcept
    -> decltype(static_cast<const IterR&>(rhs).distance_to(static_cast<const IterL&>(lhs))) {
    return static_cast<const IterR&>(rhs).distance_to(static_cast<const IterL&>(lhs));
}

template<typename Iter, typename ValTy, typename Tag, typename RefTy, typename PtrTy, typename DiffTy>
auto operator+(typename iterator_facade<Iter, ValTy, Tag, RefTy, PtrTy, DiffTy>::difference_type j,
               const iterator_facade<Iter, ValTy, Tag, RefTy, PtrTy, DiffTy>& it) noexcept(noexcept(std::declval<Iter>()
                                                                                                        .advance(0)))
    -> uxs::type_identity_t<Iter, decltype(std::declval<Iter>().advance(j))> {
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

namespace detail {
#if __cplusplus >= 202002L && defined(__cpp_concepts) && defined(__cpp_lib_ranges)
using array_iterator_tag = std::contiguous_iterator_tag;
#else   // concepts
using array_iterator_tag = std::random_access_iterator_tag;
#endif  // concepts
}  // namespace detail

template<typename Traits, typename UnderlyingPtrTy, bool Const>
class array_iterator : public container_iterator_facade<Traits, array_iterator<Traits, UnderlyingPtrTy, Const>,
                                                        detail::array_iterator_tag, Const> {
 private:
    using super = container_iterator_facade<Traits, array_iterator, detail::array_iterator_tag, Const>;

 public:
    using reference = typename super::reference;
    using difference_type = typename super::difference_type;
    using underlying_ptr = UnderlyingPtrTy;

    template<typename, typename, bool>
    friend class array_iterator;

    array_iterator() noexcept = default;

    // Explicit non-trivial copy operations are required by standard
#if _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(underlying_ptr ptr, underlying_ptr begin, underlying_ptr end) noexcept
        : ptr_(ptr), begin_(begin), end_(end) {}
    array_iterator(const array_iterator& it) noexcept : ptr_(it.ptr_), begin_(it.begin_), end_(it.end_) {}
    array_iterator& operator=(const array_iterator& it) noexcept {
        ptr_ = it.ptr_, begin_ = it.begin_, end_ = it.end_;
        return *this;
    }
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Traits, UnderlyingPtrTy, false>>& it) noexcept
        : ptr_(it.ptr_), begin_(it.begin_), end_(it.end_) {}
    template<bool Const_ = Const>
    array_iterator& operator=(
        const std::enable_if_t<Const_, array_iterator<Traits, UnderlyingPtrTy, false>>& it) noexcept {
        ptr_ = it.ptr_, begin_ = it.begin_, end_ = it.end_;
        return *this;
    }
    underlying_ptr debug_begin() const { return begin_; }
    underlying_ptr debug_end() const { return end_; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    explicit array_iterator(underlying_ptr ptr, underlying_ptr begin, underlying_ptr end) noexcept : ptr_(ptr) {
        (void)begin, (void)end;
    }
    array_iterator(const array_iterator& it) noexcept : ptr_(it.ptr_) {}
    array_iterator& operator=(const array_iterator& it) noexcept {
        ptr_ = it.ptr_;
        return *this;
    }
    template<bool Const_ = Const>
    array_iterator(const std::enable_if_t<Const_, array_iterator<Traits, UnderlyingPtrTy, false>>& it) noexcept
        : ptr_(it.ptr_) {}
    template<bool Const_ = Const>
    array_iterator& operator=(
        const std::enable_if_t<Const_, array_iterator<Traits, UnderlyingPtrTy, false>>& it) noexcept {
        ptr_ = it.ptr_;
        return *this;
    }
#endif  // _ITERATOR_DEBUG_LEVEL != 0

    void increment() noexcept {
        uxs_iterator_assert(begin_ <= ptr_ && ptr_ < end_);
        ++ptr_;
    }

    void decrement() noexcept {
        uxs_iterator_assert(begin_ < ptr_ && ptr_ <= end_);
        --ptr_;
    }

    void advance(difference_type j) noexcept {
        uxs_iterator_assert(j >= 0 ? begin_ <= ptr_ && end_ - ptr_ >= j : ptr_ - begin_ >= -j && ptr_ <= end_);
        ptr_ += j;
    }

    reference dereference() const noexcept {
        uxs_iterator_assert(begin_ <= ptr_ && ptr_ < end_);
        return *ptr_;
    }

    template<bool Const2>
    bool is_equal_to(const array_iterator<Traits, UnderlyingPtrTy, Const2>& it) const noexcept {
        uxs_iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ == it.ptr_;
    }

    template<bool Const2>
    bool is_less_than(const array_iterator<Traits, UnderlyingPtrTy, Const2>& it) const noexcept {
        uxs_iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return ptr_ < it.ptr_;
    }

    template<bool Const2>
    difference_type distance_to(const array_iterator<Traits, UnderlyingPtrTy, Const2>& it) const noexcept {
        uxs_iterator_assert(begin_ == it.begin_ && end_ == it.end_);
        return it.ptr_ - ptr_;
    }

    underlying_ptr ptr() const noexcept { return ptr_; }

 private:
    underlying_ptr ptr_{nullptr};
#if _ITERATOR_DEBUG_LEVEL != 0
    underlying_ptr begin_{nullptr};
    underlying_ptr end_{nullptr};
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

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

    list_iterator() noexcept = default;
    explicit list_iterator(node_type* node) noexcept : node_(node) {}

    // This iterator consists of one natural pointer,
    // so explicit copy constructor and operator are not needed

    template<bool Const_ = Const>
    list_iterator(const std::enable_if_t<Const_, list_iterator<Traits, NodeTraits, false>>& it) noexcept
        : node_(it.node_) {}
    template<bool Const_ = Const>
    list_iterator& operator=(const std::enable_if_t<Const_, list_iterator<Traits, NodeTraits, false>>& it) noexcept {
        node_ = it.node_;
        return *this;
    }

    void increment() noexcept {
        uxs_iterator_assert(node_ && node_ != NodeTraits::get_head(node_));
        node_ = NodeTraits::get_next(node_);
    }

    void decrement() noexcept {
        uxs_iterator_assert(node_ && node_ != NodeTraits::get_front(NodeTraits::get_head(node_)));
        node_ = NodeTraits::get_prev(node_);
    }

    template<bool Const2>
    bool is_equal_to(const list_iterator<Traits, NodeTraits, Const2>& it) const noexcept {
        uxs_iterator_assert((!node_ && !it.node_) ||
                            (node_ && it.node_ && NodeTraits::get_head(node_) == NodeTraits::get_head(it.node_)));
        return node_ == it.node_;
    }

    reference dereference() const noexcept {
        uxs_iterator_assert(node_ && node_ != NodeTraits::get_head(node_));
        return NodeTraits::get_value(node_);
    }

    node_type* node() const noexcept { return node_; }

 private:
    node_type* node_ = nullptr;
};

//-----------------------------------------------------------------------------
// Const value iterator

template<typename Val>
class const_value_iterator : public iterator_facade<const_value_iterator<Val>, Val,  //
                                                    std::input_iterator_tag, const Val&, const Val*> {
 public:
    explicit const_value_iterator(const Val& v) noexcept : v_(std::addressof(v)) {}

    void increment() noexcept {}
    void advance(std::ptrdiff_t j) noexcept {}
    const Val& dereference() const noexcept { return *v_; }
    bool is_equal_to(const const_value_iterator& it) const noexcept {
        uxs_iterator_assert(v_ == it.v_);
        return true;
    }

 private:
    const Val* v_;
};

template<typename Val>
const_value_iterator<Val> const_value(const Val& v) noexcept {
    return const_value_iterator<Val>(v);
}

//-----------------------------------------------------------------------------
// Limited output iterator

template<typename BaseIt, typename = void>
class limited_output_iterator {
 public:
    using iterator_type = BaseIt;
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;

    limited_output_iterator() : base_(), limit_(0) {}
    limited_output_iterator(BaseIt base, difference_type limit) : base_(base), limit_(limit) {}

    template<typename Ty>
    limited_output_iterator& operator=(Ty&& v) {
        if (limit_) { *base_ = std::forward<Ty>(v); }
        return *this;
    }

    limited_output_iterator& operator*() { return *this; }
    limited_output_iterator& operator++() {
        ++base_, --limit_;
        return *this;
    }
    limited_output_iterator operator++(int) {
        limited_output_iterator it = *this;
        ++*this;
        return it;
    }

    iterator_type base() const { return base_; }

 private:
    iterator_type base_;
    difference_type limit_;
};

template<typename BaseIt>
class limited_output_iterator<BaseIt, std::enable_if_t<is_random_access_iterator<BaseIt>::value>> {
 public:
    using iterator_type = BaseIt;
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;

    limited_output_iterator() : first_(), last_() {}
    limited_output_iterator(iterator_type base, difference_type limit) : first_(base), last_(base + limit) {}

    template<typename Ty>
    limited_output_iterator& operator=(Ty&& v) {
        if (first_ != last_) { *first_ = std::forward<Ty>(v); }
        return *this;
    }

    limited_output_iterator& operator*() { return *this; }
    limited_output_iterator& operator++() {
        ++first_;
        return *this;
    }
    limited_output_iterator operator++(int) {
        limited_output_iterator it = *this;
        ++*this;
        return it;
    }

    iterator_type base() const { return first_; }

 private:
    iterator_type first_, last_;
};

template<typename BaseIt>
limited_output_iterator<BaseIt> limit_output_iterator(const BaseIt& base, std::ptrdiff_t limit) {
    return limited_output_iterator<BaseIt>(base, limit);
}

}  // namespace uxs

namespace std {
#if __cplusplus >= 202002L && defined(__cpp_concepts) && defined(__cpp_lib_addressof_constexpr)
template<typename Traits, typename UnderlyingPtrTy, bool Const>
struct pointer_traits<uxs::array_iterator<Traits, UnderlyingPtrTy, Const>> {
    using pointer = uxs::array_iterator<Traits, UnderlyingPtrTy, Const>;
    using element_type = std::conditional_t<Const, const typename pointer::value_type, typename pointer::value_type>;
    using difference_type = typename pointer::difference_type;
    [[nodiscard]] static constexpr element_type* to_address(const pointer iter) noexcept {
        uxs_iterator_assert(iter.debug_begin() <= iter.ptr() && iter.ptr() <= iter.debug_end());
        return std::to_address(iter.ptr());
    }
};
#endif  // pointer_traits
#if __cplusplus < 201402L && !defined(__cpp_lib_make_reverse_iterator)
template<typename Iter>
std::reverse_iterator<Iter> make_reverse_iterator(Iter it) {
    return std::reverse_iterator<Iter>(it);
}
#endif  // make reverse iterator
}  // namespace std
