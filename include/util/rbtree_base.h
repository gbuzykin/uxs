#pragma once

#include "iterator.h"
#include "rbtree_node_handle.h"

namespace util {

namespace detail {

//-----------------------------------------------------------------------------
// Red-black tree implementation

struct rbtree_links_type : rbtree_node_t {
    static rbtree_node_t* get_next(rbtree_node_t* node) { return rbtree_next(node); }
    static rbtree_node_t* get_prev(rbtree_node_t* node) { return rbtree_prev(node); }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(rbtree_node_t* node, rbtree_node_t* head) {
        static_cast<rbtree_links_type*>(node)->head = head;
    }
    static void set_head(rbtree_node_t* first, rbtree_node_t* last, rbtree_node_t* head) {
        for (auto p = first; p != last; p = get_next(p)) { set_head(p, head); }
    }
    static rbtree_node_t* get_head(rbtree_node_t* node) { return static_cast<rbtree_links_type*>(node)->head; }
    static rbtree_node_t* get_front(rbtree_node_t* head) { return head->parent; }
    rbtree_node_t* head;
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(rbtree_node_t* node, rbtree_node_t* head) {}
    static void set_head(rbtree_node_t* first, rbtree_node_t* last, rbtree_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Key>
struct set_node_type : rbtree_links_type {
    using key_type = Key;
    using value_type = Key;
    using links_t = rbtree_links_type;
    using iterator_node_t = rbtree_node_t;
    using is_value_copy_assignable = std::is_copy_assignable<Key>;
    using is_value_move_assignable = std::is_move_assignable<Key>;
    template<typename Ty2>
    using is_value_assignable = std::is_assignable<key_type&, Ty2>;
    static const key_type& get_key(const value_type& v) { return v; }
    static key_type& get_value(rbtree_node_t* node) { return static_cast<set_node_type*>(node)->value; }
    static key_type& get_writable_value(rbtree_node_t* node) { return get_value(node); }
    value_type value;
};

template<typename Key, typename Ty>
struct map_node_type : rbtree_links_type {
    using key_type = Key;
    using mapped_type = Ty;
    using value_type = std::pair<const Key, Ty>;
    using links_t = rbtree_links_type;
    using iterator_node_t = rbtree_node_t;
    using is_value_copy_assignable = std::is_copy_assignable<std::pair<Key, Ty>>;
    using is_value_move_assignable = std::is_move_assignable<std::pair<Key, Ty>>;
    template<typename Ty2>
    using is_value_assignable = std::is_assignable<std::pair<Key, Ty>&, Ty2>;
    static const key_type& get_key(const value_type& v) { return v.first; }
    static value_type& get_value(rbtree_node_t* node) { return static_cast<map_node_type*>(node)->value; }
    static std::pair<Key&, Ty&> get_writable_value(rbtree_node_t* node) {
        auto& v = get_value(node);
        return std::pair<Key&, Ty&>{const_cast<Key&>(v.first), v.second};
    }
    value_type value;
};

template<typename NodeTy, typename Alloc, typename Comp, typename = void>
class rbtree_compare : public std::allocator_traits<Alloc>::template rebind_alloc<NodeTy> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<NodeTy>;

 public:
    rbtree_compare() NOEXCEPT_IF((std::is_nothrow_default_constructible<Comp>::value) &&
                                 (std::is_nothrow_default_constructible<alloc_type>::value))
        : alloc_type(Alloc()), func_() {}
    explicit rbtree_compare(const Alloc& alloc) NOEXCEPT_IF(std::is_nothrow_default_constructible<Comp>::value)
        : alloc_type(alloc), func_() {}
    explicit rbtree_compare(const Alloc& alloc, const Comp& func) : alloc_type(alloc), func_(func) {}
    explicit rbtree_compare(const Alloc& alloc, Comp&& func) : alloc_type(alloc), func_(std::move(func)) {}
#if __cplusplus < 201703L
    ~rbtree_compare() = default;
    rbtree_compare(const rbtree_compare&) = default;
    rbtree_compare& operator=(const rbtree_compare&) = default;
    rbtree_compare(rbtree_compare&& other) : alloc_type(std::move(other)), func_(std::move(other.func_)) {}
    rbtree_compare& operator=(rbtree_compare&& other) {
        alloc_type::operator=(std::move(other));
        func_ = std::move(other.func_);
        return *this;
    }
#endif  // __cplusplus < 201703L

    const Comp& get_compare() const { return func_; }
    Comp& get_compare() { return func_; }
    void change_compare(const Comp& func) { func_ = func; }
    void change_compare(Comp&& func) { func_ = std::move(func); }
    void swap_compare(Comp& func) { std::swap(func_, func); }

 private:
    Comp func_;
};

template<typename NodeTy, typename Alloc, typename Comp>
class rbtree_compare<NodeTy, Alloc, Comp,
                     std::enable_if_t<(std::is_empty<Comp>::value &&  //
                                       std::is_nothrow_default_constructible<Comp>::value)>>
    : public std::allocator_traits<Alloc>::template rebind_alloc<NodeTy> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<NodeTy>;

 public:
    rbtree_compare() NOEXCEPT_IF(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(Alloc()) {}
    explicit rbtree_compare(const Alloc& alloc) NOEXCEPT : alloc_type(alloc) {}
    explicit rbtree_compare(const Alloc& alloc, const Comp&) : alloc_type(alloc) {}
#if __cplusplus < 201703L
    ~rbtree_compare() = default;
    rbtree_compare(const rbtree_compare&) = default;
    rbtree_compare& operator=(const rbtree_compare&) = default;
    rbtree_compare(rbtree_compare&& other) : alloc_type(std::move(other)) {}
    rbtree_compare& operator=(rbtree_compare&& other) {
        alloc_type::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    Comp get_compare() const { return Comp(); }
    void change_compare(const Comp&) {}
    void swap_compare(const Comp&) {}
};

template<typename NodeTy, typename Alloc, typename Comp>
class rbtree_base : protected rbtree_compare<NodeTy, Alloc, Comp> {
 protected:
    using super = rbtree_compare<NodeTy, Alloc, Comp>;
    using alloc_type = typename super::alloc_type;
    using alloc_traits = std::allocator_traits<alloc_type>;
    using value_alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<typename NodeTy::value_type>;
    using value_alloc_traits = std::allocator_traits<value_alloc_type>;
    using node_t = NodeTy;

 public:
    using key_type = typename NodeTy::key_type;
    using value_type = typename NodeTy::value_type;
    using allocator_type = Alloc;
    using key_compare = Comp;
    using size_type = typename value_alloc_traits::size_type;
    using difference_type = typename value_alloc_traits::difference_type;
    using pointer = typename value_alloc_traits::pointer;
    using const_pointer = typename value_alloc_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = list_iterator<rbtree_base, node_t, std::is_same<key_type, value_type>::value>;
    using const_iterator = list_iterator<rbtree_base, node_t, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using node_type = rbtree_node_handle<rbtree_base, node_t>;

    rbtree_base() NOEXCEPT_IF(noexcept(super())) : super() { init(); }
    explicit rbtree_base(const allocator_type& alloc) NOEXCEPT_IF(noexcept(super(alloc))) : super(alloc) { init(); }
    explicit rbtree_base(const key_compare& comp, const allocator_type& alloc) : super(alloc, comp) { init(); }

    rbtree_base(const rbtree_base& other)
        : super(alloc_traits::select_on_container_copy_construction(other), other.get_compare()) {
        init();
        init_from(other, copy_value);
    }

    rbtree_base(const rbtree_base& other, const allocator_type& alloc) : super(alloc, other.get_compare()) {
        init();
        init_from(other, copy_value);
    }

    rbtree_base& operator=(const rbtree_base& other) {
        if (std::addressof(other) == this) { return *this; }
        this->change_compare(other.get_compare());
        assign_impl(other, std::bool_constant<(!alloc_traits::propagate_on_container_copy_assignment::value ||
                                               is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    rbtree_base(rbtree_base&& other) NOEXCEPT : super(std::move(other)) {
        init();
        steal_data(other);
    }

    rbtree_base(rbtree_base&& other, const allocator_type& alloc) : super(alloc, std::move(other.get_compare())) {
        init();
        if (is_alloc_always_equal<alloc_type>::value || is_same_alloc(other)) {
            steal_data(other);
        } else {
            init_from(other, move_value);
        }
    }

    rbtree_base& operator=(rbtree_base&& other)
        NOEXCEPT_IF(std::is_nothrow_move_assignable<key_compare>::value &&
                    (alloc_traits::propagate_on_container_move_assignment::value ||
                     is_alloc_always_equal<alloc_type>::value)) {
        if (std::addressof(other) == this) { return *this; }
        this->change_compare(std::move(other.get_compare()));
        assign_impl(std::move(other), std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    ~rbtree_base() { tidy(); }

    allocator_type get_allocator() const { return static_cast<const alloc_type&>(*this); }
    key_compare key_comp() const { return this->get_compare(); }

    bool empty() const NOEXCEPT { return size_ == 0; }
    size_type size() const NOEXCEPT { return size_; }
    size_type max_size() const NOEXCEPT { return alloc_traits::max_size(*this); }

    iterator begin() NOEXCEPT { return iterator(head_.parent); }
    const_iterator begin() const NOEXCEPT { return const_iterator(head_.parent); }
    const_iterator cbegin() const NOEXCEPT { return const_iterator(head_.parent); }

    iterator end() NOEXCEPT { return iterator(std::addressof(head_)); }
    const_iterator end() const NOEXCEPT { return const_iterator(std::addressof(head_)); }
    const_iterator cend() const NOEXCEPT { return const_iterator(std::addressof(head_)); }

    reverse_iterator rbegin() NOEXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const NOEXCEPT { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const NOEXCEPT { return const_reverse_iterator(end()); }

    reverse_iterator rend() NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const NOEXCEPT { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const NOEXCEPT { return const_reverse_iterator(begin()); }

    reference front() {
        assert(size_);
        return node_t::get_value(head_.parent);
    }
    const_reference front() const {
        assert(size_);
        return node_t::get_value(head_.parent);
    }

    reference back() {
        assert(size_);
        return node_t::get_value(head_.right);
    }
    const_reference back() const {
        assert(size_);
        return node_t::get_value(head_.right);
    }

    // - find

    iterator find(const key_type& key) {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        if ((p == std::addressof(head_)) || this->get_compare()(key, node_t::get_key(node_t::get_value(p)))) {
            return end();
        }
        return iterator(p);
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    iterator find(const Key& key) {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        if ((p == std::addressof(head_)) || this->get_compare()(key, node_t::get_key(node_t::get_value(p)))) {
            return end();
        }
        return iterator(p);
    }

    const_iterator find(const key_type& key) const {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        if ((p == std::addressof(head_)) || this->get_compare()(key, node_t::get_key(node_t::get_value(p)))) {
            return end();
        }
        return const_iterator(p);
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    const_iterator find(const Key& key) const {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        if ((p == std::addressof(head_)) || this->get_compare()(key, node_t::get_key(node_t::get_value(p)))) {
            return end();
        }
        return const_iterator(p);
    }

    // - lower_bound

    iterator lower_bound(const key_type& key) {
        return iterator(rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    iterator lower_bound(const Key& key) {
        return iterator(rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    const_iterator lower_bound(const key_type& key) const {
        return const_iterator(rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    const_iterator lower_bound(const Key& key) const {
        return const_iterator(rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    // - upper_bound

    iterator upper_bound(const key_type& key) {
        return iterator(rbtree_upper_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    iterator upper_bound(const Key& key) {
        return iterator(rbtree_upper_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    const_iterator upper_bound(const key_type& key) const {
        return const_iterator(rbtree_upper_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    const_iterator upper_bound(const Key& key) const {
        return const_iterator(rbtree_upper_bound<node_t>(std::addressof(head_), key, this->get_compare()));
    }

    // - equal_range

    std::pair<iterator, iterator> equal_range(const key_type& key) {
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(iterator(result.first), iterator(result.second));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    std::pair<iterator, iterator> equal_range(const Key& key) {
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(iterator(result.first), iterator(result.second));
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(const_iterator(result.first), const_iterator(result.second));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const {
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(const_iterator(result.first), const_iterator(result.second));
    }

    // - count

    size_type count(const key_type& key) const {
        size_type count = 0;
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        while (result.first != result.second) {
            result.first = rbtree_next(result.first);
            ++count;
        }
        return count;
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    size_type count(const Key& key) const {
        size_type count = 0;
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        while (result.first != result.second) {
            result.first = rbtree_next(result.first);
            ++count;
        }
        return count;
    }

    // - contains

    bool contains(const key_type& key) const {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        return (p != std::addressof(head_)) && !this->get_compare()(key, node_t::get_key(node_t::get_value(p)));
    }

    template<typename Key, typename Comp_ = key_compare, typename = std::void_t<typename Comp_::is_transparent>>
    bool contains(const Key& key) const {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        return (p != std::addressof(head_)) && !this->get_compare()(key, node_t::get_key(node_t::get_value(p)));
    }

    // - clear, swap, erase

    void clear() { tidy(); }

    iterator erase(const_iterator pos) {
        auto p = to_ptr(pos);
        assert(p != std::addressof(head_));
        --size_;
        auto next = rbtree_remove(std::addressof(head_), p);
        helpers::delete_node(*this, p);
        return iterator(next);
    }

    template<typename Key_ = key_type>
    iterator erase(std::enable_if_t<!std::is_same<Key_, value_type>::value, iterator> pos) {
        return erase(static_cast<const_iterator>(pos));
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto p_first = to_ptr(first);
        auto p_last = to_ptr(last);
        if (p_first != p_last) { erase_impl(p_first, p_last); }
        return iterator(p_last);
    }

    size_type erase(const key_type& key) {
        auto old_size = size_;
        auto result = rbtree_equal_range<node_t>(std::addressof(head_), key, this->get_compare());
        if (result.first != result.second) { erase_impl(result.first, result.second); }
        return old_size - size_;
    }

    node_type extract(const_iterator pos) {
        auto p = to_ptr(pos);
        assert(p != std::addressof(head_));
        --size_;
        rbtree_remove(std::addressof(head_), p);
        node_t::set_head(p, nullptr);
        return node_type(*this, p);
    }

    node_type extract(const key_type& key) {
        auto p = rbtree_lower_bound<node_t>(std::addressof(head_), key, this->get_compare());
        if ((p == std::addressof(head_)) || this->get_compare()(key, node_t::get_key(node_t::get_value(p)))) {
            return node_type(*this);
        }
        --size_;
        rbtree_remove(std::addressof(head_), p);
        node_t::set_head(p, nullptr);
        return node_type(*this, p);
    }

 protected:
    mutable typename node_t::links_t head_;
    size_type size_ = 0;

    template<typename, typename>
    friend class rbtree_node_handle;
    template<typename, typename, typename>
    friend class rbtree_unique;
    template<typename, typename, typename>
    friend class rbtree_multi;

    struct value_compare_func {
        using result_type = bool;
        using first_argument_type = value_type;
        using second_argument_type = value_type;
        value_compare_func(const key_compare& comp_) : comp(comp_) {}
        bool operator()(const value_type& lhs, const value_type& rhs) const {
            return comp(node_t::get_key(lhs), node_t::get_key(rhs));
        }
        key_compare comp;
    };

    struct dealloc_guard_t : nocopy_t {
        alloc_type& alloc;
        rbtree_node_t* node;
        dealloc_guard_t(alloc_type& alloc_, rbtree_node_t* node_) : alloc(alloc_), node(node_) {}
        rbtree_node_t* release() { return get_and_set(node, nullptr); }
        ~dealloc_guard_t() {
            if (node) { alloc_traits::deallocate(alloc, static_cast<node_t*>(node), 1); }
        }
    };

    struct delete_guard_t : nocopy_t {
        alloc_type& alloc;
        rbtree_node_t* node;
        delete_guard_t(alloc_type& alloc_, rbtree_node_t* node_) : alloc(alloc_), node(node_) {}
        rbtree_node_t* release() { return get_and_set(node, nullptr); }
        ~delete_guard_t() {
            if (node) { helpers::delete_node(alloc, node); }
        }
    };

    struct delete_recursive_guard_t : nocopy_t {
        alloc_type& alloc;
        rbtree_node_t* node;
        delete_recursive_guard_t(alloc_type& alloc_, rbtree_node_t* node_) : alloc(alloc_), node(node_) {}
        rbtree_node_t* release() { return get_and_set(node, nullptr); }
        void delete_recursive(rbtree_node_t* node_) {
            if (node_->left) { delete_recursive(node_->left); }
            if (node_->right) { delete_recursive(node_->right); }
            helpers::delete_node(alloc, node_);
        }
        ~delete_recursive_guard_t() {
            if (node) { delete_recursive(node); }
        }
    };

    struct temp_chain_t : nocopy_t {
        alloc_type& alloc;
        rbtree_node_t* first;
        rbtree_node_t* last;
        temp_chain_t(rbtree_base&& t) : alloc(t), first(reuse_first(t.head_.parent)), last(std::addressof(t.head_)) {
            t.size_ = 0;
            t.head_.left = nullptr;
            t.head_.right = t.head_.parent = std::addressof(t.head_);
        }
        bool empty() const { return first == last; }
        rbtree_node_t* get() {
            auto p = first;
            first = reuse_next(first);
            return p;
        }
        ~temp_chain_t() {
            while (first != last) {
                auto next = reuse_next(first);
                helpers::delete_node(alloc, first);
                first = next;
            }
        }
    };

    template<typename Func>
    void tidy_invoke(Func fn) {
        try {
            fn();
        } catch (...) {
            tidy();
            throw;
        }
    }

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::true_type) {
        return first <= last;
    }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::false_type) {
        return true;
    }

    static rbtree_node_t* reuse_first(rbtree_node_t* node) {
        while (node->right) { node = rbtree_left_bound(node->right); }
        return node;
    }

    static rbtree_node_t* reuse_next(rbtree_node_t* node) {
        auto next = node->parent;
        if (next->left == node) { return reuse_first(next); }
        return next;
    }

    static const value_type& copy_value(rbtree_node_t* node) { return node_t::get_value(node); }
    static auto move_value(rbtree_node_t* node) -> decltype(std::move(node_t::get_writable_value(node))) {
        return std::move(node_t::get_writable_value(node));
    }

    rbtree_node_t* to_ptr(const_iterator it) const { return it.node(std::addressof(head_)); }

    void init() {
        rbtree_init_head(std::addressof(head_));
        node_t::set_head(std::addressof(head_), std::addressof(head_));
    }

    void tidy() {
        if (size_) { temp_chain_t tmp(std::move(*this)); }
    }

    void steal_data(rbtree_base& other) {
        if (!other.size_) { return; }
        size_ = other.size_;
        other.size_ = 0;
        head_.left = other.head_.left;
        other.head_.left = nullptr;
        head_.right = other.head_.right;
        head_.parent = other.head_.parent;
        head_.left->parent = std::addressof(head_);
        other.head_.right = other.head_.parent = std::addressof(other.head_);
        node_t::set_head(head_.parent, std::addressof(head_), std::addressof(head_));
    }

    void assign_impl(const rbtree_base& other, std::true_type) {
        assign_from(other, copy_value, typename node_t::is_value_copy_assignable());
    }

    void assign_impl(const rbtree_base& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_from(other, copy_value, typename node_t::is_value_copy_assignable());
        } else {
            tidy();
            alloc_type::operator=(other);
            init_from(other, copy_value);
        }
    }

    void assign_impl(rbtree_base&& other, std::true_type) NOEXCEPT {
        tidy();
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        steal_data(other);
    }

    void assign_impl(rbtree_base&& other, std::false_type) {
        if (is_same_alloc(other)) {
            tidy();
            steal_data(other);
        } else {
            assign_from(other, move_value, typename node_t::is_value_move_assignable());
        }
    }

    void swap_impl(rbtree_base& other, std::true_type) NOEXCEPT_IF(std::is_nothrow_swappable<key_compare>::value) {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        swap_impl(other, std::false_type());
    }

    void swap_impl(rbtree_base& other, std::false_type) NOEXCEPT_IF(std::is_nothrow_swappable<key_compare>::value) {
        this->swap_compare(other.get_compare());
        if (!size_) {
            steal_data(other);
            return;
        } else if (other.size_) {
            std::swap(size_, other.size_);
            std::swap(head_.left, other.head_.left);
            std::swap(head_.right, other.head_.right);
            std::swap(head_.parent, other.head_.parent);
            std::swap(head_.left->parent, other.head_.left->parent);
            node_t::set_head(head_.parent, std::addressof(head_), std::addressof(head_));
        } else {
            other.size_ = size_;
            size_ = 0;
            other.head_.left = head_.left;
            head_.left = nullptr;
            other.head_.right = head_.right;
            other.head_.parent = head_.parent;
            other.head_.left->parent = std::addressof(other.head_);
            head_.right = head_.parent = std::addressof(head_);
        }
        node_t::set_head(other.head_.parent, std::addressof(other.head_), std::addressof(other.head_));
    }

    template<typename CopyFunc>
    void init_from(const rbtree_base& other, CopyFunc fn) {
        assert(!size_);
        if (!other.size_) { return; }
        head_.left = copy_node(other.head_.left, fn);
        head_.left->parent = std::addressof(head_);
        head_.parent = rbtree_left_bound(head_.left);
        head_.right = rbtree_right_bound(head_.left);
        size_ = other.size_;
    }

    template<typename CopyFunc, typename Bool>
    void assign_from(const rbtree_base& other, CopyFunc fn, Bool) {
        if (size_) {
            temp_chain_t tmp(std::move(*this));
            if (!other.size_) { return; }
            head_.left = copy_node_reuse(other.head_.left, fn, tmp, Bool());
        } else {
            if (!other.size_) { return; }
            head_.left = copy_node(other.head_.left, fn);
        }
        head_.left->parent = std::addressof(head_);
        head_.parent = rbtree_left_bound(head_.left);
        head_.right = rbtree_right_bound(head_.left);
        size_ = other.size_;
    }

    void erase_impl(rbtree_node_t* first, rbtree_node_t* last) {
        do {
            assert(first != std::addressof(head_));
            --size_;
            auto next = rbtree_remove(std::addressof(head_), first);
            helpers::delete_node(*this, first);
            first = next;
        } while (first != last);
    }

    template<typename CopyFunc>
    rbtree_node_t* reuse_node(rbtree_node_t* src_node, CopyFunc fn, temp_chain_t& tmp, std::true_type) {
        node_t::get_writable_value(tmp.first) = fn(src_node);
        return tmp.get();
    }

    template<typename CopyFunc>
    rbtree_node_t* reuse_node(rbtree_node_t* src_node, CopyFunc fn, temp_chain_t& tmp, std::false_type) {
        return helpers::reconstruct_node(*this, tmp.get(), fn(src_node));
    }

    template<typename CopyFunc, typename Bool>
    rbtree_node_t* copy_node_reuse(rbtree_node_t* src_node, CopyFunc fn, temp_chain_t& tmp, Bool);
    template<typename CopyFunc>
    rbtree_node_t* copy_node(rbtree_node_t* src_node, CopyFunc fn);

    struct helpers {
        template<typename... Args>
        static rbtree_node_t* new_node(alloc_type& alloc, Args&&... args) {
            dealloc_guard_t g(alloc, static_cast<rbtree_node_t*>(std::addressof(*alloc_traits::allocate(alloc, 1))));
            alloc_traits::construct(alloc, std::addressof(node_t::get_value(g.node)), std::forward<Args>(args)...);
            return g.release();
        }

        template<typename... Args>
        static rbtree_node_t* reconstruct_node(alloc_type& alloc, rbtree_node_t* node, Args&&... args) {
            dealloc_guard_t g(alloc, node);
            alloc_traits::destroy(alloc, std::addressof(node_t::get_value(g.node)));
            alloc_traits::construct(alloc, std::addressof(node_t::get_value(g.node)), std::forward<Args>(args)...);
            return g.release();
        }

        static void delete_node(alloc_type& alloc, rbtree_node_t* node) {
            alloc_traits::destroy(alloc, std::addressof(node_t::get_value(node)));
            alloc_traits::deallocate(alloc, static_cast<node_t*>(node), 1);
        }
    };
};

template<typename NodeTy, typename Alloc, typename Comp>
template<typename CopyFunc, typename Bool>
rbtree_node_t* rbtree_base<NodeTy, Alloc, Comp>::copy_node_reuse(rbtree_node_t* src_node, CopyFunc fn,
                                                                 temp_chain_t& tmp, Bool) {
    delete_recursive_guard_t g(*this, reuse_node(src_node, fn, tmp, Bool()));
    g.node->color = src_node->color;
    g.node->left = nullptr;
    g.node->right = nullptr;
    if (src_node->left) {
        if (tmp.empty()) {
            g.node->left = copy_node(src_node->left, fn);
        } else {
            g.node->left = copy_node_reuse(src_node->left, fn, tmp, Bool());
        }
        g.node->left->parent = g.node;
    }
    if (src_node->right) {
        if (tmp.empty()) {
            g.node->right = copy_node(src_node->right, fn);
        } else {
            g.node->right = copy_node_reuse(src_node->right, fn, tmp, Bool());
        }
        g.node->right->parent = g.node;
    }
    return g.release();
}

template<typename NodeTy, typename Alloc, typename Comp>
template<typename CopyFunc>
rbtree_node_t* rbtree_base<NodeTy, Alloc, Comp>::copy_node(rbtree_node_t* src_node, CopyFunc fn) {
    delete_recursive_guard_t g(*this, helpers::new_node(*this, fn(src_node)));
    g.node->color = src_node->color;
    g.node->left = nullptr;
    g.node->right = nullptr;
    if (src_node->left) {
        g.node->left = copy_node(src_node->left, fn);
        g.node->left->parent = g.node;
    }
    if (src_node->right) {
        g.node->right = copy_node(src_node->right, fn);
        g.node->right->parent = g.node;
    }
    node_t::set_head(g.node, std::addressof(this->head_));
    return g.release();
}

}  // namespace detail

}  // namespace util
