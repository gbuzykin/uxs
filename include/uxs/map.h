#pragma once

#include "rbtree_unique.h"

#include <algorithm>
#include <functional>

namespace uxs {

//-----------------------------------------------------------------------------
// Map front-end

template<typename Key, typename Ty, typename Comp, typename Alloc>
class multimap;

template<typename Key, typename Ty, typename Comp = std::less<Key>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>>
class map : public detail::rbtree_unique<detail::map_node_traits<Key, Ty>, Alloc, Comp> {
 private:
    using node_traits = detail::map_node_traits<Key, Ty>;
    using super = detail::rbtree_unique<node_traits, Alloc, Comp>;
    using alloc_traits = typename super::alloc_traits;
    using alloc_type = typename super::alloc_type;

 public:
    using allocator_type = typename super::allocator_type;
    using key_type = typename super::key_type;
    using mapped_type = typename node_traits::mapped_type;
    using value_type = typename super::value_type;
    using key_compare = typename super::key_compare;
    using value_compare = typename super::value_compare_func;
    using iterator = typename super::iterator;
    using const_iterator = typename super::const_iterator;

    map() NOEXCEPT_IF(noexcept(super())) : super() {}
    explicit map(const allocator_type& alloc) NOEXCEPT_IF(noexcept(super(alloc))) : super(alloc) {}
    explicit map(const key_compare& comp, const allocator_type& alloc = allocator_type()) : super(comp, alloc) {}

#if __cplusplus < 201703L
    ~map() = default;
    map(const map&) = default;
    map& operator=(const map&) = default;
    map(map&& other) NOEXCEPT_IF(noexcept(super(std::move(other)))) : super(std::move(other)) {}
    map& operator=(map&& other) NOEXCEPT_IF(std::is_nothrow_move_assignable<super>::value) {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    map(std::initializer_list<value_type> l, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    map(std::initializer_list<value_type> l, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(l.begin(), l.end());
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    map& operator=(std::initializer_list<value_type> l) {
        this->assign_range(l.begin(), l.end());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    map(InputIt first, InputIt last, const allocator_type& alloc) : super(alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    map(InputIt first, InputIt last, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        try {
            this->insert_impl(first, last);
        } catch (...) {
            this->tidy();
            throw;
        }
    }

    map(const map& other, const allocator_type& alloc) : super(other, alloc) {}
    map(map&& other, const allocator_type& alloc) NOEXCEPT_IF(noexcept(super(std::move(other), alloc)))
        : super(std::move(other), alloc) {}

    void swap(map& other) NOEXCEPT_IF(std::is_nothrow_swappable<key_compare>::value) {
        if (std::addressof(other) == this) { return; }
        this->swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    value_compare value_comp() const { return value_compare(this->get_compare()); }

    const mapped_type& at(const key_type& key) const {
        auto it = this->find(key);
        if (it != this->end()) { return it->second; }
        throw std::out_of_range("invalid map key");
    }

    mapped_type& at(const key_type& key) {
        auto it = this->find(key);
        if (it != this->end()) { return it->second; }
        throw std::out_of_range("invalid map key");
    }

    mapped_type& operator[](const key_type& key) { return try_emplace_impl(key).first->second; }
    mapped_type& operator[](key_type&& key) { return try_emplace_impl(std::move(key)).first->second; }

    template<typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
        return try_emplace_impl(key, std::forward<Args>(args)...);
    }

    template<typename... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
        return try_emplace_impl(std::move(key), std::forward<Args>(args)...);
    }

    template<typename Key2, typename Comp_ = key_compare, typename... Args>
    type_identity_t<std::pair<iterator, bool>, typename Comp_::is_transparent> try_emplace(Key2&& key, Args&&... args) {
        return try_emplace_impl(std::forward<Key2>(key), std::forward<Args>(args)...);
    }

    template<typename... Args>
    iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
        return try_emplace_hint_impl(hint, key, std::forward<Args>(args)...).first;
    }

    template<typename... Args>
    iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
        return try_emplace_hint_impl(hint, std::move(key), std::forward<Args>(args)...).first;
    }

    template<typename Key2, typename Comp_ = key_compare, typename... Args>
    type_identity_t<iterator, typename Comp_::is_transparent> try_emplace_hint(const_iterator hint, Key2&& key,
                                                                               Args&&... args) {
        return try_emplace_hint_impl(hint, std::forward<Key2>(key), std::forward<Args>(args)...).first;
    }

    template<typename Ty2>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, Ty2&& obj) {
        auto result = try_emplace_impl(key, std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result;
    }

    template<typename Ty2>
    std::pair<iterator, bool> insert_or_assign(key_type&& key, Ty2&& obj) {
        auto result = try_emplace_impl(std::move(key), std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result;
    }

    template<typename Key2, typename Ty2, typename Comp_ = key_compare>
    type_identity_t<std::pair<iterator, bool>, typename Comp_::is_transparent> insert_or_assign(Key2&& key, Ty2&& obj) {
        auto result = try_emplace_impl(std::forward<Key2>(key), std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result;
    }

    template<typename Ty2>
    iterator insert_or_assign(const_iterator hint, const key_type& key, Ty2&& obj) {
        auto result = try_emplace_hint_impl(hint, key, std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result.first;
    }

    template<typename Ty2>
    iterator insert_or_assign(const_iterator hint, key_type&& key, Ty2&& obj) {
        auto result = try_emplace_hint_impl(hint, std::move(key), std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result.first;
    }

    template<typename Key2, typename Ty2, typename Comp_ = key_compare>
    type_identity_t<iterator, typename Comp_::is_transparent> insert_or_assign(const_iterator hint, Key2&& key,
                                                                               Ty2&& obj) {
        auto result = try_emplace_hint_impl(hint, std::forward<Key2>(key), std::forward<Ty2>(obj));
        if (!result.second) { result.first->second = std::forward<Ty2>(obj); }
        return result.first;
    }

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

 private:
    template<typename Key2, typename... Args>
    std::pair<iterator, bool> try_emplace_impl(Key2&& key, Args&&... args) {
        auto result = rbtree_find_insert_unique_pos<node_traits>(std::addressof(this->head_), key, this->get_compare());
        if (result.second) {
            auto* node = this->new_node(std::piecewise_construct, std::forward_as_tuple(std::forward<Key2>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
            ++this->size_;
            return std::make_pair(iterator(node), true);
        }
        return std::make_pair(iterator(result.first), false);
    }

    template<typename Key2, typename... Args>
    std::pair<iterator, bool> try_emplace_hint_impl(const_iterator hint, Key2&& key, Args&&... args) {
        auto result = rbtree_find_insert_unique_pos<node_traits>(std::addressof(this->head_), this->to_ptr(hint), key,
                                                                 this->get_compare());
        if (result.second) {
            auto* node = this->new_node(std::piecewise_construct, std::forward_as_tuple(std::forward<Key2>(key)),
                                        std::forward_as_tuple(std::forward<Args>(args)...));
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
            ++this->size_;
            return std::make_pair(iterator(node), true);
        }
        return std::make_pair(iterator(result.first), false);
    }
};

#if __cplusplus >= 201703L
template<typename InputIt, typename Comp = std::less<detail::iter_key_t<InputIt>>,
         typename Alloc = std::allocator<detail::iter_to_alloc_t<InputIt>>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
map(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
    -> map<detail::iter_key_t<InputIt>, detail::iter_val_t<InputIt>, Comp, Alloc>;
template<typename Key, typename Ty, typename Comp = std::less<remove_const_t<Key>>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>,
         typename = std::enable_if_t<!is_allocator<Comp>::value>, typename = std::enable_if_t<is_allocator<Alloc>::value>>
map(std::initializer_list<std::pair<Key, Ty>>, Comp = Comp(), Alloc = Alloc())
    -> map<remove_const_t<Key>, Ty, Comp, Alloc>;
template<typename InputIt, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
map(InputIt, InputIt, Alloc)
    -> map<detail::iter_key_t<InputIt>, detail::iter_val_t<InputIt>, std::less<detail::iter_key_t<InputIt>>, Alloc>;
template<typename Key, typename Ty, typename Alloc, typename = std::enable_if_t<is_allocator<Alloc>::value>>
map(std::initializer_list<std::pair<Key, Ty>>, Alloc)
    -> map<remove_const_t<Key>, Ty, std::less<remove_const_t<Key>>, Alloc>;
#endif  // __cplusplus >= 201703L

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator==(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator<(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator!=(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    return !(lhs == rhs);
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator<=(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    return !(rhs < lhs);
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator>(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    return rhs < lhs;
}
template<typename Key, typename Ty, typename Comp, typename Alloc>
bool operator>=(const map<Key, Ty, Comp, Alloc>& lhs, const map<Key, Ty, Comp, Alloc>& rhs) {
    return !(lhs < rhs);
}

}  // namespace uxs

namespace std {
template<typename Key, typename Ty, typename Comp, typename Alloc>
void swap(uxs::map<Key, Ty, Comp, Alloc>& m1, uxs::map<Key, Ty, Comp, Alloc>& m2)
    NOEXCEPT_IF(NOEXCEPT_IF(m1.swap(m2))) {
    m1.swap(m2);
}
}  // namespace std
