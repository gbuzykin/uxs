#pragma once

#include "rbtree_base.h"

namespace util {

namespace detail {

//-----------------------------------------------------------------------------
// Red-black tree with unique keys implementation

template<typename NodeTy, typename Alloc, typename Comp>
class rbtree_unique : public rbtree_base<NodeTy, Alloc, Comp> {
 protected:
    using super = rbtree_base<NodeTy, Alloc, Comp>;
    using alloc_type = typename super::alloc_type;
    using node_t = typename super::node_t;

 public:
    using allocator_type = typename super::allocator_type;
    using value_type = typename super::value_type;
    using key_compare = typename super::key_compare;
    using iterator = typename super::iterator;
    using const_iterator = typename super::const_iterator;
    using node_type = typename super::node_type;

    struct insert_return_type {
#if __cplusplus < 201703L
        insert_return_type(iterator in_position, bool in_inserted, node_type&& in_node)
            : position(in_position), inserted(in_inserted), node(std::move(in_node)) {}
        insert_return_type(insert_return_type&& rt)
            : position(rt.position), inserted(rt.inserted), node(std::move(rt.node)) {}
        insert_return_type& operator=(insert_return_type&& rt) {
            position = rt.position, inserted = rt.inserted, node = std::move(rt.node);
            return *this;
        }
#endif  // __cplusplus < 201703L
        iterator position;
        bool inserted;
        node_type node;
    };

    rbtree_unique() NOEXCEPT_IF(noexcept(super())) : super() {}
    explicit rbtree_unique(const allocator_type& alloc) NOEXCEPT_IF(noexcept(super(alloc))) : super(alloc) {}
    explicit rbtree_unique(const key_compare& comp, const allocator_type& alloc) : super(comp, alloc) {}
    rbtree_unique(const rbtree_unique& other, const allocator_type& alloc) : super(other, alloc) {}
    rbtree_unique(rbtree_unique&& other, const allocator_type& alloc) : super(std::move(other), alloc) {}

#if __cplusplus < 201703L
    ~rbtree_unique() = default;
    rbtree_unique(const rbtree_unique&) = default;
    rbtree_unique& operator=(const rbtree_unique&) = default;
    rbtree_unique(rbtree_unique&& other) NOEXCEPT : super(std::move(other)) {}
    rbtree_unique& operator=(rbtree_unique&& other) NOEXCEPT {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    void assign(std::initializer_list<value_type> l) {
        assign_impl(l.begin(), l.end(), typename node_t::is_value_copy_assignable());
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_impl(first, last, typename node_t::template is_value_assignable<decltype(*std::declval<InputIt>())>());
    }

    std::pair<iterator, bool> insert(const value_type& val) { return emplace(val); }
    std::pair<iterator, bool> insert(value_type&& val) { return emplace(std::move(val)); }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        typename super::delete_guard_t g(*this, super::helpers::new_node(*this, std::forward<Args>(args)...));
        auto result = rbtree_find_insert_unique_pos<node_t>(
            std::addressof(this->head_), node_t::get_key(node_t::get_value(g.node)), this->get_compare());
        if (result.second == 0) { return std::make_pair(iterator(result.first), false); }
        node_t::set_head(g.node, std::addressof(this->head_));
        ++this->size_;
        rbtree_insert(std::addressof(this->head_), g.node, result.first, result.second < 0);
        return std::make_pair(iterator(g.release()), true);
    }

    iterator insert(const_iterator hint, const value_type& val) { return emplace_hint(hint, val); }
    iterator insert(const_iterator hint, value_type&& val) { return emplace_hint(hint, std::move(val)); }
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        typename super::delete_guard_t g(*this, super::helpers::new_node(*this, std::forward<Args>(args)...));
        auto result = rbtree_find_insert_unique_pos<node_t>(std::addressof(this->head_), this->to_ptr(hint),
                                                            node_t::get_key(node_t::get_value(g.node)),
                                                            this->get_compare());
        if (result.second == 0) { return iterator(result.first); }
        node_t::set_head(g.node, std::addressof(this->head_));
        ++this->size_;
        rbtree_insert(std::addressof(this->head_), g.node, result.first, result.second < 0);
        return iterator(g.release());
    }

    insert_return_type insert(node_type&& nh) {
        if (nh.empty()) { return {this->end(), false, node_type(*this)}; }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto node = nh.node_;
        auto result = rbtree_find_insert_unique_pos<node_t>(
            std::addressof(this->head_), node_t::get_key(node_t::get_value(node)), this->get_compare());
        if (result.second == 0) { return {iterator(result.first), false, std::move(nh)}; }
        node_t::set_head(node, std::addressof(this->head_));
        ++this->size_;
        nh.node_ = nullptr;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second < 0);
        return {iterator(node), true, node_type(*this)};
    }

    iterator insert(const_iterator hint, node_type&& nh) {
        if (nh.empty()) { return this->end(); }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto node = nh.node_;
        auto result = rbtree_find_insert_unique_pos<node_t>(std::addressof(this->head_), this->to_ptr(hint),
                                                            node_t::get_key(node_t::get_value(node)),
                                                            this->get_compare());
        if (result.second == 0) { return iterator(result.first); }
        node_t::set_head(node, std::addressof(this->head_));
        ++this->size_;
        nh.node_ = nullptr;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second < 0);
        return iterator(node);
    }

    template<typename Val, typename = std::enable_if_t<std::is_constructible<value_type, Val&&>::value>>
    std::pair<iterator, bool> insert(Val&& val) {
        return emplace(std::forward<Val>(val));
    }

    template<typename Val, typename = std::enable_if_t<std::is_constructible<value_type, Val&&>::value>>
    iterator insert(const_iterator hint, Val&& val) {
        return emplace_hint(hint, std::forward<Val>(val));
    }

    void insert(std::initializer_list<value_type> l) { insert_impl(l.begin(), l.end()); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void insert(InputIt first, InputIt last) {
        insert_impl(first, last);
    }

 protected:
    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::true_type);
    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::false_type);
    template<typename Comp2>
    void merge_impl(rbtree_base<NodeTy, Alloc, Comp2>&& other);
    template<typename InputIt>
    void insert_impl(InputIt first, InputIt last) {
        assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        for (; first != last; ++first) { emplace_hint(this->end(), *first); }
    }
};

template<typename NodeTy, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_unique<NodeTy, Alloc, Comp>::assign_impl(InputIt first, InputIt last, std::true_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (this->size_) {
        typename super::temp_chain_t tmp(std::move(*this));
        for (; !tmp.empty() && (first != last); ++first) {
            node_t::get_writable_value(tmp.first) = *first;
            auto result = rbtree_find_insert_unique_pos<node_t>(
                std::addressof(this->head_), std::addressof(this->head_), node_t::get_key(node_t::get_value(tmp.first)),
                this->get_compare());
            if (result.second != 0) {
                ++this->size_;
                rbtree_insert(std::addressof(this->head_), tmp.get(), result.first, result.second < 0);
            }
        }
    }
    insert_impl(first, last);
}

template<typename NodeTy, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_unique<NodeTy, Alloc, Comp>::assign_impl(InputIt first, InputIt last, std::false_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (this->size_) {
        typename super::temp_chain_t tmp(std::move(*this));
        typename super::delete_guard_t g(*this, tmp.get());
        for (; g.node && (first != last); ++first) {
            g.node = super::helpers::reconstruct_node(*this, g.release(), *first);
            auto result = rbtree_find_insert_unique_pos<node_t>(
                std::addressof(this->head_), std::addressof(this->head_), node_t::get_key(node_t::get_value(g.node)),
                this->get_compare());
            if (result.second != 0) {
                ++this->size_;
                rbtree_insert(std::addressof(this->head_), g.release(), result.first, result.second < 0);
                if (!tmp.empty()) { g.node = tmp.get(); }
            }
        }
    }
    insert_impl(first, last);
}

template<typename NodeTy, typename Alloc, typename Comp>
template<typename Comp2>
void rbtree_unique<NodeTy, Alloc, Comp>::merge_impl(rbtree_base<NodeTy, Alloc, Comp2>&& other) {
    if (!other.size_ || (std::addressof(other) == static_cast<alloc_type*>(this))) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for merge");
    }
    auto node = other.head_.parent;
    do {
        auto result = rbtree_find_insert_unique_pos<node_t>(
            std::addressof(this->head_), node_t::get_key(node_t::get_value(node)), this->get_compare());
        if (result.second != 0) {
            --other.size_;
            auto next = rbtree_remove(std::addressof(other.head_), node);
            node_t::set_head(node, std::addressof(this->head_));
            ++this->size_;
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second < 0);
            node = next;
        } else {
            node = rbtree_next(node);
        }
    } while (node != std::addressof(other.head_));
}

}  // namespace detail

}  // namespace util
