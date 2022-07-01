#pragma once

#include "rbtree_base.h"

namespace uxs {

namespace detail {

//-----------------------------------------------------------------------------
// Red-black tree with unique keys implementation

template<typename NodeTraits, typename Alloc, typename Comp>
class rbtree_unique : public rbtree_base<NodeTraits, Alloc, Comp> {
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
    rbtree_unique(rbtree_unique&& other, const allocator_type& alloc)
        NOEXCEPT_IF(noexcept(super(std::move(other), alloc)))
        : super(std::move(other), alloc) {}

#if __cplusplus < 201703L
    ~rbtree_unique() = default;
    rbtree_unique(const rbtree_unique&) = default;
    rbtree_unique& operator=(const rbtree_unique&) = default;
    rbtree_unique(rbtree_unique&& other) NOEXCEPT_IF(noexcept(super(std::move(other)))) : super(std::move(other)) {}
    rbtree_unique& operator=(rbtree_unique&& other) NOEXCEPT_IF(std::is_nothrow_move_assignable<super>::value) {
        super::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    void assign(std::initializer_list<value_type> l) {
        assign_range(l.begin(), l.end(), std::is_copy_assignable<typename node_traits::writable_value_t>());
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_range(first, last, std::is_assignable<typename node_traits::writable_value_t&, decltype(*first)>());
    }

    std::pair<iterator, bool> insert(const value_type& val) { return emplace(val); }
    std::pair<iterator, bool> insert(value_type&& val) { return emplace(std::move(val)); }
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        auto* node = this->new_node(std::forward<Args>(args)...);
        try {
            auto result = rbtree_find_insert_unique_pos<node_traits>(
                std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
            if (result.second) {
                rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
                ++this->size_;
                return std::make_pair(iterator(node), true);
            }
            this->delete_node(node);
            return std::make_pair(iterator(result.first), false);
        } catch (...) {
            this->delete_node(node);
            throw;
        }
    }

    iterator insert(const_iterator hint, const value_type& val) { return emplace_hint(hint, val); }
    iterator insert(const_iterator hint, value_type&& val) { return emplace_hint(hint, std::move(val)); }
    template<typename... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        auto* node = this->new_node(std::forward<Args>(args)...);
        try {
            auto result = rbtree_find_insert_unique_pos<node_traits>(std::addressof(this->head_), this->to_ptr(hint),
                                                                     node_traits::get_key(node_traits::get_value(node)),
                                                                     this->get_compare());
            if (result.second) {
                rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
                ++this->size_;
                return iterator(node);
            }
            this->delete_node(node);
            return iterator(result.first);
        } catch (...) {
            this->delete_node(node);
            throw;
        }
    }

    insert_return_type insert(node_type&& nh) {
        if (nh.empty()) { return {this->end(), false, node_type(*this)}; }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto* node = nh.node_;
        auto result = rbtree_find_insert_unique_pos<node_traits>(
            std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
        if (result.second) {
            nh.node_ = nullptr;
            node_traits::set_head(node, std::addressof(this->head_));
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
            ++this->size_;
            return {iterator(node), true, node_type(*this)};
        }
        return {iterator(result.first), false, std::move(nh)};
    }

    iterator insert(const_iterator hint, node_type&& nh) {
        if (nh.empty()) { return this->end(); }
        if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(nh)) {
            throw std::logic_error("allocators incompatible for insert");
        }
        auto* node = nh.node_;
        auto result = rbtree_find_insert_unique_pos<node_traits>(std::addressof(this->head_), this->to_ptr(hint),
                                                                 node_traits::get_key(node_traits::get_value(node)),
                                                                 this->get_compare());
        if (result.second) {
            nh.node_ = nullptr;
            node_traits::set_head(node, std::addressof(this->head_));
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
            ++this->size_;
            return iterator(node);
        }
        return iterator(result.first);
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
    void assign_range(InputIt first, InputIt last, std::true_type);
    template<typename InputIt>
    void assign_range(InputIt first, InputIt last, std::false_type);
    template<typename Comp2>
    void merge_impl(rbtree_base<NodeTraits, Alloc, Comp2>&& other);
    template<typename InputIt>
    void insert_impl(InputIt first, InputIt last) {
        assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        for (; first != last; ++first) { emplace_hint(this->end(), *first); }
    }
};  // namespace detail

template<typename NodeTraits, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_unique<NodeTraits, Alloc, Comp>::assign_range(InputIt first, InputIt last, std::true_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (this->size_) {
        auto* reuse = super::reuse_first(this->head_.parent);
        this->reset();
        try {
            for (; reuse != std::addressof(this->head_) && first != last; ++first) {
                auto* node = reuse;
                node_traits::get_writable_value(node) = *first;
                auto result = rbtree_find_insert_unique_pos<node_traits>(
                    std::addressof(this->head_), std::addressof(this->head_),
                    node_traits::get_key(node_traits::get_value(node)), this->get_compare());
                if (result.second) {
                    reuse = super::reuse_next(reuse);
                    rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
                    ++this->size_;
                }
            }
            this->delete_node_chain(reuse);
        } catch (...) {
            this->delete_node_chain(reuse);
            throw;
        }
    }
    insert_impl(first, last);
}

template<typename NodeTraits, typename Alloc, typename Comp>
template<typename InputIt>
void rbtree_unique<NodeTraits, Alloc, Comp>::assign_range(InputIt first, InputIt last, std::false_type) {
    assert(super::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
    if (this->size_) {
        auto* node = super::reuse_first(this->head_.parent);
        auto* reuse = super::reuse_next(node);
        this->reset();
        try {
            for (; reuse != std::addressof(this->head_) && first != last; ++first) {
                this->reconstruct_node(node, *first);
                auto* tmp_reuse = reuse;
                reuse = node;
                auto result = rbtree_find_insert_unique_pos<node_traits>(
                    std::addressof(this->head_), std::addressof(this->head_),
                    node_traits::get_key(node_traits::get_value(node)), this->get_compare());
                reuse = tmp_reuse;
                if (result.second) {
                    rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
                    ++this->size_;
                    node = tmp_reuse, reuse = super::reuse_next(tmp_reuse);
                }
            }
            this->delete_node_chain(reuse);
        } catch (...) {
            this->delete_node_chain(reuse);
            throw;
        }
    }
    insert_impl(first, last);
}

template<typename NodeTraits, typename Alloc, typename Comp>
template<typename Comp2>
void rbtree_unique<NodeTraits, Alloc, Comp>::merge_impl(rbtree_base<NodeTraits, Alloc, Comp2>&& other) {
    if (!other.size_ || std::addressof(other) == static_cast<alloc_type*>(this)) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !this->is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for merge");
    }
    auto* node = other.head_.parent;
    do {
        auto result = rbtree_find_insert_unique_pos<node_traits>(
            std::addressof(this->head_), node_traits::get_key(node_traits::get_value(node)), this->get_compare());
        if (result.second) {
            auto* next = rbtree_remove(std::addressof(other.head_), node);
            node_traits::set_head(node, std::addressof(this->head_));
            rbtree_insert(std::addressof(this->head_), node, result.first, result.second);
            ++this->size_, --other.size_;
            node = next;
        } else {
            node = rbtree_next(node);
        }
    } while (node != std::addressof(other.head_));
}

}  // namespace detail

}  // namespace uxs
