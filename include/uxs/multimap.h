#pragma once

#include "rbtree_multi.h"

#include <algorithm>
#include <functional>

namespace uxs {

//-----------------------------------------------------------------------------
// Multimap front-end

template<typename Key, typename Ty, typename Comp, typename Alloc>
class map;

template<typename Key, typename Ty, typename Comp = std::less<Key>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>>
class multimap : public detail::rbtree_multi<detail::map_node_traits<Key, Ty>, Alloc, Comp> {
 private:
    using node_traits = detail::map_node_traits<Key, Ty>;
    using super = detail::rbtree_multi<node_traits, Alloc, Comp>;
    using alloc_traits = typename super::alloc_traits;
    using alloc_type = typename super::alloc_type;

 public:
    using allocator_type = typename super::allocator_type;
    using mapped_type = typename node_traits::mapped_type;
    using value_type = typename super::value_type;
    using key_compare = typename super::key_compare;
    using value_compare = typename super::value_compare_func;
    using iterator = typename super::iterator;
    using const_iterator = typename super::const_iterator;

    multimap() noexcept(noexcept(super())) : super() {}
    explicit multimap(const allocator_type& alloc) noexcept(noexcept(super(alloc))) : super(alloc) {}
    explicit multimap(const key_compare& comp, const allocator_type& alloc = allocator_type()) : super(comp, alloc) {}

#if __cplusplus < 201703L
    ~multimap() = default;
    multimap(const multimap&) = default;
    multimap& operator=(const multimap&) = default;
    multimap(multimap&& other) noexcept(noexcept(super(std::move(other)))) : super(std::move(other)) {}
    multimap& operator=(multimap&& other) noexcept(std::is_nothrow_move_assignable<super>::value) {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    multimap(std::initializer_list<value_type> l, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    multimap(std::initializer_list<value_type> l, const key_compare& comp = key_compare(),
             const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    multimap& operator=(std::initializer_list<value_type> l) {
        this->assign_range(l.begin(), l.end());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    multimap(InputIt first, InputIt last, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    multimap(InputIt first, InputIt last, const key_compare& comp = key_compare(),
             const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    multimap(const multimap& other, const allocator_type& alloc) : super(other, alloc) {}
    multimap(multimap&& other, const allocator_type& alloc) noexcept(noexcept(super(std::move(other), alloc)))
        : super(std::move(other), alloc) {}

    void swap(multimap& other) noexcept(std::is_nothrow_swappable<key_compare>::value) {
        if (std::addressof(other) == this) { return; }
        this->swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    value_compare value_comp() const { return value_compare(this->get_compare()); }

    template<typename Comp2>
    void merge(map<Key, Ty, Comp2, Alloc>& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(map<Key, Ty, Comp2, Alloc>&& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(multimap<Key, Ty, Comp2, Alloc>& other) {
        this->merge_impl(std::move(other));
    }
    template<typename Comp2>
    void merge(multimap<Key, Ty, Comp2, Alloc>&& other) {
        this->merge_impl(std::move(other));
    }
};

#if __cplusplus >= 201703L
template<typename InputIt, typename Comp = std::less<detail::iter_key_t<InputIt>>,
         typename Alloc = std::allocator<detail::iter_to_alloc_t<InputIt>>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
multimap(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
    -> multimap<detail::iter_key_t<InputIt>, detail::iter_val_t<InputIt>, Comp, Alloc>;
template<typename Key, typename Ty, typename Comp = std::less<est::remove_const_t<Key>>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
multimap(std::initializer_list<std::pair<Key, Ty>>, Comp = Comp(), Alloc = Alloc())
    -> multimap<est::remove_const_t<Key>, Ty, Comp, Alloc>;
template<typename InputIt, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
multimap(InputIt, InputIt, Alloc)
    -> multimap<detail::iter_key_t<InputIt>, detail::iter_val_t<InputIt>, std::less<detail::iter_key_t<InputIt>>, Alloc>;
template<typename Key, typename Ty, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
multimap(std::initializer_list<std::pair<Key, Ty>>, Alloc)
    -> multimap<est::remove_const_t<Key>, Ty, std::less<est::remove_const_t<Key>>, Alloc>;
#endif  // __cplusplus >= 201703L

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator==(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator<(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator!=(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    return !(lhs == rhs);
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator<=(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    return !(rhs < lhs);
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator>(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    return rhs < lhs;
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator>=(const multimap<Key, Ty, Comp, Alloc>& lhs, const multimap<Key, Ty, Comp, Alloc>& rhs) {
    return !(lhs < rhs);
}

}  // namespace uxs

namespace std {
template<typename Key, typename Ty, typename Comp, typename Alloc>
void swap(uxs::multimap<Key, Ty, Comp, Alloc>& m1,
          uxs::multimap<Key, Ty, Comp, Alloc>& m2) noexcept(noexcept(m1.swap(m2))) {
    m1.swap(m2);
}
}  // namespace std
