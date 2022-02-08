#pragma once

#include "functional.h"
#include "rbtree_unique.h"

namespace util {

//-----------------------------------------------------------------------------
// Map front-end

template<typename Key, typename Ty, typename Comp, typename Alloc>
class multimap;

template<typename Key, typename Ty, typename Comp = std::less<Key>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>>
class map : public detail::rbtree_unique<detail::map_node_type<Key, Ty>, Alloc, Comp> {
 private:
    using super = detail::rbtree_unique<detail::map_node_type<Key, Ty>, Alloc, Comp>;
    using alloc_traits = typename super::alloc_traits;
    using alloc_type = typename super::alloc_type;
    using node_t = typename super::node_t;

 public:
    using allocator_type = typename super::allocator_type;
    using key_type = typename super::key_type;
    using mapped_type = typename node_t::mapped_type;
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
    map(map&& other) NOEXCEPT : super(std::move(other)) {}
    map& operator=(map&& other) NOEXCEPT {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    map(std::initializer_list<value_type> l, const allocator_type& alloc) : super(alloc) {
        this->tidy_invoke([&]() { this->insert_impl(l.begin(), l.end()); });
    }

    map(std::initializer_list<value_type> l, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        this->tidy_invoke([&]() { this->insert_impl(l.begin(), l.end()); });
    }

    map& operator=(std::initializer_list<value_type> l) {
        this->assign_impl(l.begin(), l.end(), std::is_copy_assignable<typename node_t::writable_value_t>());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    map(InputIt first, InputIt last, const allocator_type& alloc) : super(alloc) {
        this->tidy_invoke([&]() { this->insert_impl(first, last); });
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    map(InputIt first, InputIt last, const key_compare& comp = key_compare(),
        const allocator_type& alloc = allocator_type())
        : super(comp, alloc) {
        this->tidy_invoke([&]() { this->insert_impl(first, last); });
    }

    map(const map& other, const allocator_type& alloc) : super(other, alloc) {}
    map(map&& other, const allocator_type& alloc) : super(std::move(other), alloc) {}

    void swap(map& other) NOEXCEPT_IF(std::is_nothrow_swappable<key_compare>::value) {
        if (std::addressof(other) == this) { return; }
        this->swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    value_compare value_comp() const { return value_compare(this->get_compare()); }

    const mapped_type& at(const key_type& key) const {
        auto it = this->find(key);
        if (it == this->end()) { throw std::out_of_range("invalid map key"); }
        return it->second;
    }

    mapped_type& at(const key_type& key) {
        auto it = this->find(key);
        if (it == this->end()) { throw std::out_of_range("invalid map key"); }
        return it->second;
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

    template<typename... Args>
    iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
        return try_emplace_hint_impl(hint, key, std::forward<Args>(args)...).first;
    }

    template<typename... Args>
    iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
        return try_emplace_hint_impl(hint, std::move(key), std::forward<Args>(args)...).first;
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

 protected:
    template<typename Key2, typename... Args>
    std::pair<iterator, bool> try_emplace_impl(Key2&& key, Args&&... args) {
        auto result = rbtree_find_insert_unique_pos<node_t>(std::addressof(this->head_), key, this->get_compare());
        if (result.second == 0) { return std::make_pair(iterator(result.first), false); }
        auto node = this->new_node(std::piecewise_construct, std::forward_as_tuple(std::forward<Key2>(key)),
                                   std::forward_as_tuple(std::forward<Args>(args)...));
        node_t::set_head(node, std::addressof(this->head_));
        ++this->size_;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second < 0);
        return std::make_pair(iterator(node), true);
    }

    template<typename Key2, typename... Args>
    std::pair<iterator, bool> try_emplace_hint_impl(const_iterator hint, Key2&& key, Args&&... args) {
        auto result = rbtree_find_insert_unique_pos<node_t>(std::addressof(this->head_), this->to_ptr(hint), key,
                                                            this->get_compare());
        if (result.second == 0) { return std::make_pair(iterator(result.first), false); }
        auto node = this->new_node(std::piecewise_construct, std::forward_as_tuple(std::forward<Key2>(key)),
                                   std::forward_as_tuple(std::forward<Args>(args)...));
        node_t::set_head(node, std::addressof(this->head_));
        ++this->size_;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second < 0);
        return std::make_pair(iterator(node), true);
    }
};

#if __cplusplus >= 201703L
template<typename InputIt,
         typename Comp = std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
         typename Alloc =
             std::allocator<std::pair<std::add_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                                      typename std::iterator_traits<InputIt>::value_type::second_type>>>
map(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
    -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
           typename std::iterator_traits<InputIt>::value_type::second_type, Comp, Alloc>;
template<typename Key, typename Ty, typename Comp = std::less<Key>,
         typename Alloc = std::allocator<std::pair<const Key, Ty>>>
map(std::initializer_list<std::pair<Key, Ty>>, Comp = Comp(), Alloc = Alloc()) -> map<Key, Ty, Comp, Alloc>;
template<typename InputIt, typename Alloc>
map(InputIt, InputIt, Alloc)
    -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
           typename std::iterator_traits<InputIt>::value_type::second_type,
           std::less<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>, Alloc>;
template<typename Key, typename Ty, typename Allocator>
map(std::initializer_list<std::pair<Key, Ty>>, Allocator) -> map<Key, Ty, std::less<Key>, Allocator>;
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

}  // namespace util

namespace std {
template<typename Key, typename Ty, typename Comp, typename Alloc>
void swap(util::map<Key, Ty, Comp, Alloc>& m1, util::map<Key, Ty, Comp, Alloc>& m2)
    NOEXCEPT_IF(NOEXCEPT_IF(m1.swap(m2))) {
    m1.swap(m2);
}
}  // namespace std
