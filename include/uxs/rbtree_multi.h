#pragma once

#include "rbtree_base.h"

namespace uxs {

namespace detail {

//-----------------------------------------------------------------------------
// Red-black tree with multiple keys implementation

template<typename NodeTraits, typename Alloc, typename Comp>
class rbtree_multi : public rbtree_base<NodeTraits, Alloc, Comp> {
 protected:
    using node_traits = NodeTraits;
    using super = rbtree_base<node_traits, Alloc, Comp>;
    using alloc_type = typename super::alloc_type;

 public:
    using allocator_type = typename super::allocator_type;
    using value_type = typename super::value_type;
    using key_compare = typename super::key_compare;
    using iterator = typename super::iterator;
    using const_iterator = typename super::const_iterator;
    using node_type = typename super::node_type;

    rbtree_multi() NOEXCEPT_IF(noexcept(super())) : super() {}
    explicit rbtree_multi(const allocator_type& alloc) NOEXCEPT_IF(noexcept(super(alloc))) : super(alloc) {}
    explicit rbtree_multi(const key_compare& comp, const allocator_type& alloc) : super(comp, alloc) {}
    rbtree_multi(const rbtree_multi& other, const allocator_type& alloc) : super(other, alloc) {}
    rbtree_multi(rbtree_multi&& other, const allocator_type& alloc)
        NOEXCEPT_IF(is_alloc_always_equal<alloc_type>::value)
        : super(std::move(other), alloc) {}

#if __cplusplus < 201703L
    ~rbtree_multi() = default;
    rbtree_multi(const rbtree_multi&) = default;
    rbtree_multi& operator=(const rbtree_multi&) = default;
    rbtree_multi(rbtree_multi&& other) NOEXCEPT : super(std::move(other)) {}
    rbtree_multi& operator=(rbtree_multi&& other) NOEXCEPT {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    void assign(std::initializer_list<value_type> l) {
        assign_impl(l.begin(), l.end(), std::is_copy_assignable<typename node_traits::writable_value_t>());
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_impl(first, last, std::is_assignable<typename node_traits::writable_value_t&, decltype(*first)>());
    }

    iterator insert(const value_type& val) { return emplace(val); }
    iterator insert(value_type&& val) { return emplace(std::move(val)); }
    template<typename... Args>
    iterator emplace(Args&&... args) {
        auto* node = this->new_node(std::forward<Args>(args)...);
        try {
            auto result = rbtree_find_insert_pos<node_traits>(
                std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
            node_traits::set_head(node, std::addressof(this->head_));
            ++this->size_;
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
        } catch (...) {
            super::helpers::delete_node(*this, node);
            throw;
        }
        return iterator(node);
    }

    iterator insert(const_iterator hint, const value_type& val) { return emplace_hint(hint, val); }
    iterator insert(const_iterator hint, value_type&& val) { return emplace_hint(hint, std::move(val)); }
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        auto* node = this->new_node(std::forward<Args>(args)...);
        try {
            auto result = rbtree_find_insert_pos<node_traits>(std::addressof(this->head_), this->to_ptr(hint),
                                                              node_traits::get_key(node_traits::get_value(node)),
                                                              this->get_compare());
            node_traits::set_head(node, std::addressof(this->head_));
            ++this->size_;
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
        } catch (...) {
            super::helpers::delete_node(*this, node);
            throw;
        }
        return iterator(node);
    }

    iterator insert(node_type&& nh) {
        if (nh.empty()) { return this->end(); }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto* node = nh.node_;
        auto result = rbtree_find_insert_pos<node_traits>(
            std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
        node_traits::set_head(node, std::addressof(this->head_));
        ++this->size_;
        nh.node_ = nullptr;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
        return iterator(node);
    }

    iterator insert(const_iterator hint, node_type&& nh) {
        if (nh.empty()) { return this->end(); }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto* node = nh.node_;
        auto result = rbtree_find_insert_pos<node_traits>(std::addressof(this->head_), this->to_ptr(hint),
                                                          node_traits::get_key(node_traits::get_value(node)),
                                                          this->get_compare());
        node_traits::set_head(node, std::addressof(this->head_));
        ++this->size_;
        nh.node_ = nullptr;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
        return iterator(node);
    }

    template<typename Val, typename = std::enable_if_t<std::is_constructible<value_type, Val&&>::value>>
    iterator insert(Val&& val) {
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
    void merge_impl(rbtree_base<node_traits, Alloc, Comp2>&& other);
    template<typename InputIt>
    void insert_impl(InputIt first, InputIt last) {
        assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        for (; first != last; ++first) { emplace_hint(this->end(), *first); }
    }
};

template<typename node_traits, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_multi<node_traits, Alloc, Comp>::assign_impl(InputIt first, InputIt last, std::true_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (first == last) {
        this->clear();
        return;
    }
    if (this->size_) {
        auto* reuse = super::reuse_first(this->head_.parent);
        this->reset();
        try {
            do {
                node_traits::get_writable_value(reuse) = *first;
                ++first;
                auto result = rbtree_find_insert_pos<node_traits>(
                    std::addressof(this->head_), std::addressof(this->head_),
                    node_traits::get_key(node_traits::get_value(reuse)), this->get_compare());
                auto* next = super::reuse_next(reuse);
                ++this->size_;
                rbtree_insert(std::addressof(this->head_), reuse, result.first, result.second);
                reuse = next;
            } while (reuse != std::addressof(this->head_) && first != last);
        } catch (...) {
            this->delete_node_chain(reuse);
            throw;
        }
        this->delete_node_chain(reuse);
    }
    insert_impl(first, last);
}

template<typename node_traits, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_multi<node_traits, Alloc, Comp>::assign_impl(InputIt first, InputIt last, std::false_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (first == last) {
        this->clear();
        return;
    }
    if (this->size_) {
        auto *reuse = reuse_first(this->head_.parent), *node = reuse;
        this->reset();
        reuse = super::reuse_next(node);
        try {
            do {
                super::helpers::reconstruct_node(*this, node, *first);
                ++first;
                auto* next = reuse;
                reuse = node;
                auto result = rbtree_find_insert_pos<node_traits>(
                    std::addressof(this->head_), std::addressof(this->head_),
                    node_traits::get_key(node_traits::get_value(node)), this->get_compare());
                ++this->size_;
                rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
                node = next;
                if (node == std::addressof(this->head_)) { break; }
                reuse = super::reuse_next(node);
            } while (first != last);
        } catch (...) {
            this->delete_node_chain(reuse);
            if (node != reuse) {
                super::alloc_traits::deallocate(*this, static_cast<typename node_traits::node_t*>(node), 1);
            }
            throw;
        }
        this->delete_node_chain(node);
    }
    insert_impl(first, last);
}

template<typename node_traits, typename Alloc, typename Comp>
template<typename Comp2>
void rbtree_multi<node_traits, Alloc, Comp>::merge_impl(rbtree_base<node_traits, Alloc, Comp2>&& other) {
    if (!other.size_ || std::addressof(other) == static_cast<alloc_type*>(this)) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for merge");
    }
    auto* node = other.head_.parent;
    do {
        auto result = rbtree_find_insert_pos<node_traits>(
            std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
        --other.size_;
        auto* next = rbtree_remove(std::addressof(other.head_), node);
        node_traits::set_head(node, std::addressof(this->head_));
        ++this->size_;
        rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
        node = next;
    } while (node != std::addressof(other.head_));
}

}  // namespace detail

}  // namespace uxs
