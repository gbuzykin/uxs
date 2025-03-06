#pragma once

#include "rbtree_unique.h"

#include <algorithm>
#include <functional>

namespace uxs {

//-----------------------------------------------------------------------------
// Set front-end

template<typename Key, typename Comp, typename Alloc>
class multiset;

template<typename Key, typename Comp = std::less<Key>, typename Alloc = std::allocator<Key>>
class set : public detail::rbtree_unique<detail::set_node_traits<Key>, Alloc, Comp> {
 private:
    using node_traits = detail::set_node_traits<Key>;
    using super = detail::rbtree_unique<node_traits, Alloc, Comp>;
    using alloc_traits = typename super::alloc_traits;
    using alloc_type = typename super::alloc_type;

 public:
    using allocator_type = typename super::allocator_type;
    using value_type = typename super::value_type;
    using key_compare = typename super::key_compare;
    using value_compare = typename super::key_compare;

    set() noexcept(noexcept(super())) : super() {}
    explicit set(const allocator_type& alloc) noexcept(noexcept(super(alloc))) : super(alloc) {}
    explicit set(const key_compare& comp, const allocator_type& alloc = allocator_type()) : super(comp, alloc) {}

#if __cplusplus < 201703L
    ~set() = default;
    set(const set&) = default;
    set& operator=(const set&) = default;
    set(set&& other) noexcept(noexcept(super(std::move(other)))) : super(std::move(other)) {}
    set& operator=(set&& other) noexcept(std::is_nothrow_move_assignable<super>::value) {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    set(std::initializer_list<value_type> l, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    set(std::initializer_list<value_type> l, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    set& operator=(std::initializer_list<value_type> l) {
        this->assign_range(l.begin(), l.end());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    set(InputIt first, InputIt last, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    set(InputIt first, InputIt last, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    set(const set& other, const allocator_type& alloc) : super(other, alloc) {}
    set(set&& other, const allocator_type& alloc) noexcept(noexcept(super(std::move(other), alloc)))
        : super(std::move(other), alloc) {}

    void swap(set& other) noexcept(std::is_nothrow_swappable<key_compare>::value) {
        if (std::addressof(other) == this) { return; }
        this->swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    value_compare value_comp() const { return this->get_compare(); }

    template<typename Comp2>
    void merge(set<Key, Comp2, Alloc>& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(set<Key, Comp2, Alloc>&& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(multiset<Key, Comp2, Alloc>& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(multiset<Key, Comp2, Alloc>&& other) {
        this->merge_impl(std::move(other));
    }
};

#if __cplusplus >= 201703L
template<typename InputIt, typename Comp = std::less<typename std::iterator_traits<InputIt>::value_type>,
         typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
set(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
    -> set<typename std::iterator_traits<InputIt>::value_type, Comp, Alloc>;
template<typename Key, typename Comp = std::less<Key>, typename Alloc = std::allocator<Key>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
set(std::initializer_list<Key>, Comp = Comp(), Alloc = Alloc()) -> set<Key, Comp, Alloc>;
template<typename InputIt, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
set(InputIt, InputIt, Alloc) -> set<typename std::iterator_traits<InputIt>::value_type,
                                    std::less<typename std::iterator_traits<InputIt>::value_type>, Alloc>;
template<typename Key, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
set(std::initializer_list<Key>, Alloc) -> set<Key, std::less<Key>, Alloc>;
#endif  // __cplusplus >= 201703L

template<typename Key, typename Comp, typename Alloc>
bool operator==(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename Key, typename Comp, typename Alloc>
bool operator<(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Key, typename Comp, typename Alloc>
bool operator!=(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    return !(lhs == rhs);
}
template<typename Key, typename Comp, typename Alloc>
bool operator<=(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    return !(rhs < lhs);
}
template<typename Key, typename Comp, typename Alloc>
bool operator>(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    return rhs < lhs;
}
template<typename Key, typename Comp, typename Alloc>
bool operator>=(const set<Key, Comp, Alloc>& lhs, const set<Key, Comp, Alloc>& rhs) {
    return !(lhs < rhs);
}

}  // namespace uxs

namespace std {
template<typename Key, typename Comp, typename Alloc>
void swap(uxs::set<Key, Comp, Alloc>& s1, uxs::set<Key, Comp, Alloc>& s2) noexcept(noexcept(s1.swap(s2))) {
    s1.swap(s2);
}
}  // namespace std
