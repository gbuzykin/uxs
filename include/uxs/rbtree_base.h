#pragma once

#include "iterator.h"
#include "memory.h"
#include "rbtree_node_handle.h"

namespace uxs {

namespace detail {

//-----------------------------------------------------------------------------
// Red-black tree implementation

struct rbtree_links_type : rbtree_node_t {
#if _ITERATOR_DEBUG_LEVEL != 0
    rbtree_node_t* head;
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Key>
struct set_node_type : rbtree_links_type {
    Key value;
};

template<typename Key, typename Ty>
struct map_node_type : rbtree_links_type {
    std::pair<const Key, Ty> value;
};

struct rbtree_node_traits {
    using iterator_node_t = rbtree_node_t;
    using links_t = rbtree_links_type;
    static rbtree_node_t* get_next(rbtree_node_t* node) { return rbtree_next(node); }
    static rbtree_node_t* get_prev(rbtree_node_t* node) { return rbtree_prev(node); }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(rbtree_node_t* node, rbtree_node_t* head) { static_cast<links_t*>(node)->head = head; }
    static void set_head(rbtree_node_t* first, rbtree_node_t* last, rbtree_node_t* head) {
        for (auto* p = first; p != last; p = get_next(p)) { set_head(p, head); }
    }
    static rbtree_node_t* get_head(rbtree_node_t* node) { return static_cast<links_t*>(node)->head; }
    static rbtree_node_t* get_front(rbtree_node_t* head) { return head->parent; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(rbtree_node_t* node, rbtree_node_t* head) {}
    static void set_head(rbtree_node_t* first, rbtree_node_t* last, rbtree_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Key>
struct set_node_traits : rbtree_node_traits {
    using key_type = Key;
    using value_type = Key;
    using node_t = set_node_type<Key>;
    static const key_type& get_key(const value_type& v) { return v; }
    static key_type& get_value(rbtree_node_t* node) { return static_cast<node_t*>(node)->value; }
    static key_type& get_lref_value(rbtree_node_t* node) { return get_value(node); }
    static key_type&& get_rref_value(rbtree_node_t* node) { return std::move(get_value(node)); }
};

template<typename Key, typename Ty>
struct map_node_traits : rbtree_node_traits {
    using key_type = Key;
    using mapped_type = Ty;
    using value_type = std::pair<const Key, Ty>;
    using node_t = map_node_type<Key, Ty>;
    static const key_type& get_key(const value_type& v) { return v.first; }
    static value_type& get_value(rbtree_node_t* node) { return static_cast<node_t*>(node)->value; }
    static std::pair<Key&, Ty&> get_lref_value(rbtree_node_t* node) {
        value_type& v = get_value(node);
        return std::pair<Key&, Ty&>(const_cast<Key&>(v.first), v.second);
    }
    static std::pair<Key&&, Ty&&> get_rref_value(rbtree_node_t* node) {
        value_type& v = get_value(node);
        return std::pair<Key&&, Ty&&>(std::move(const_cast<Key&>(v.first)), std::move(v.second));
    }
};

template<typename NodeTraits, typename Alloc, typename Comp, typename = void>
class rbtree_compare : protected std::allocator_traits<Alloc>::template rebind_alloc<typename NodeTraits::node_t> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<typename NodeTraits::node_t>;

 public:
    rbtree_compare() noexcept((std::is_nothrow_default_constructible<Comp>::value &&
                               std::is_nothrow_default_constructible<Alloc>::value))
        : alloc_type(Alloc()), func_() {}
    explicit rbtree_compare(const Alloc& alloc) noexcept(std::is_nothrow_default_constructible<Comp>::value)
        : alloc_type(alloc), func_() {}
    explicit rbtree_compare(const Alloc& alloc, const Comp& func) : alloc_type(alloc), func_(func) {}
    explicit rbtree_compare(const Alloc& alloc, Comp&& func) noexcept(std::is_nothrow_move_constructible<Comp>::value)
        : alloc_type(alloc), func_(std::move(func)) {}
    ~rbtree_compare() = default;
    rbtree_compare(const rbtree_compare&) = default;
    rbtree_compare& operator=(const rbtree_compare&) = default;
    rbtree_compare(rbtree_compare&& other) noexcept(std::is_nothrow_move_constructible<Comp>::value)
        : alloc_type(std::move(other)), func_(std::move(other.func_)) {}
    rbtree_compare& operator=(rbtree_compare&& other) noexcept(std::is_nothrow_move_assignable<Comp>::value) {
        alloc_type::operator=(std::move(other));
        func_ = std::move(other.func_);
        return *this;
    }

    const Comp& get_compare() const { return func_; }
    Comp& get_compare() { return func_; }
    void change_compare(const Comp& func) { func_ = func; }
    void change_compare(Comp&& func) { func_ = std::move(func); }
    void swap_compare(Comp& func) { std::swap(func_, func); }

 private:
    Comp func_;
};

template<typename NodeTraits, typename Alloc, typename Comp>
class rbtree_compare<NodeTraits, Alloc, Comp,
                     std::enable_if_t<(std::is_empty<Comp>::value &&  //
                                       std::is_nothrow_default_constructible<Comp>::value)>>
    : protected std::allocator_traits<Alloc>::template rebind_alloc<typename NodeTraits::node_t> {
 protected:
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<typename NodeTraits::node_t>;

 public:
    rbtree_compare() noexcept((std::is_nothrow_default_constructible<Comp>::value &&
                               std::is_nothrow_default_constructible<Alloc>::value))
        : alloc_type(Alloc()) {}
    explicit rbtree_compare(const Alloc& alloc) noexcept(std::is_nothrow_default_constructible<Comp>::value)
        : alloc_type(alloc) {}
    explicit rbtree_compare(const Alloc& alloc, const Comp&) noexcept(std::is_nothrow_move_constructible<Comp>::value)
        : alloc_type(alloc) {}
    ~rbtree_compare() = default;
    rbtree_compare(const rbtree_compare&) = default;
    rbtree_compare& operator=(const rbtree_compare&) = default;
    rbtree_compare(rbtree_compare&& other) noexcept(std::is_nothrow_move_constructible<Comp>::value)
        : alloc_type(std::move(other)) {}
    rbtree_compare& operator=(rbtree_compare&& other) noexcept(std::is_nothrow_move_assignable<Comp>::value) {
        alloc_type::operator=(std::move(other));
        return *this;
    }

    Comp get_compare() const { return Comp(); }
    void change_compare(const Comp&) {}
    void swap_compare(const Comp&) {}
};

template<typename NodeTraits, typename Alloc, typename Comp>
class rbtree_base : protected rbtree_compare<NodeTraits, Alloc, Comp> {
 protected:
    using node_traits = NodeTraits;
    using super = rbtree_compare<node_traits, Alloc, Comp>;
    using alloc_type = typename super::alloc_type;
    using alloc_traits = std::allocator_traits<alloc_type>;
    using value_alloc_type =
        typename std::allocator_traits<Alloc>::template rebind_alloc<typename node_traits::value_type>;
    using value_alloc_traits = std::allocator_traits<value_alloc_type>;

 public:
    using key_type = typename node_traits::key_type;
    using value_type = typename node_traits::value_type;
    using allocator_type = Alloc;
    using key_compare = Comp;
    using size_type = typename alloc_traits::size_type;
    using difference_type = typename alloc_traits::difference_type;
    using pointer = typename value_alloc_traits::pointer;
    using const_pointer = typename value_alloc_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = uxs::list_iterator<rbtree_base, node_traits, std::is_same<key_type, value_type>::value>;
    using const_iterator = uxs::list_iterator<rbtree_base, node_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using node_type = rbtree_node_handle<node_traits, Alloc>;

    rbtree_base() noexcept(noexcept(super())) : super() { init(); }
    explicit rbtree_base(const allocator_type& alloc) noexcept(noexcept(super(alloc))) : super(alloc) { init(); }
    explicit rbtree_base(const key_compare& comp, const allocator_type& alloc) : super(alloc, comp) { init(); }

    rbtree_base(const rbtree_base& other)
        : super(alloc_traits::select_on_container_copy_construction(other), other.get_compare()) {
        init_impl(other, node_traits::get_value);
    }

    rbtree_base(const rbtree_base& other, const allocator_type& alloc) : super(alloc, other.get_compare()) {
        init_impl(other, node_traits::get_value);
    }

    rbtree_base& operator=(const rbtree_base& other) {
        if (std::addressof(other) == this) { return *this; }
        this->change_compare(other.get_compare());
        assign_impl(other, std::bool_constant<(!alloc_traits::propagate_on_container_copy_assignment::value ||
                                               is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    rbtree_base(rbtree_base&& other) noexcept(noexcept(super(std::move(other)))) : super(std::move(other)) {
        init();
        steal_data(other);
    }

    rbtree_base(rbtree_base&& other, const allocator_type& alloc) noexcept(noexcept(super(std::move(other))) &&
                                                                           is_alloc_always_equal<alloc_type>())
        : super(alloc, std::move(other.get_compare())) {
        construct_impl(std::move(other), alloc, is_alloc_always_equal<alloc_type>());
    }

    rbtree_base& operator=(rbtree_base&& other) noexcept(std::is_nothrow_move_assignable<super>::value &&
                                                         (alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)) {
        if (std::addressof(other) == this) { return *this; }
        this->change_compare(std::move(other.get_compare()));
        assign_impl(std::move(other), std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    ~rbtree_base() { tidy(); }

    allocator_type get_allocator() const noexcept { return allocator_type(*this); }
    key_compare key_comp() const { return this->get_compare(); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept { return alloc_traits::max_size(*this); }

    iterator begin() noexcept { return iterator(head_.parent); }
    const_iterator begin() const noexcept { return const_iterator(head_.parent); }
    const_iterator cbegin() const noexcept { return const_iterator(head_.parent); }

    iterator end() noexcept { return iterator(std::addressof(head_)); }
    const_iterator end() const noexcept { return const_iterator(std::addressof(head_)); }
    const_iterator cend() const noexcept { return const_iterator(std::addressof(head_)); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    reference front() {
        assert(size_);
        return node_traits::get_value(head_.parent);
    }
    const_reference front() const {
        assert(size_);
        return node_traits::get_value(head_.parent);
    }

    reference back() {
        assert(size_);
        return node_traits::get_value(head_.right);
    }
    const_reference back() const {
        assert(size_);
        return node_traits::get_value(head_.right);
    }

    // - find

    iterator find(const key_type& key) {
        auto* p = rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare());
        if (p == std::addressof(head_) || this->get_compare()(key, node_traits::get_key(node_traits::get_value(p)))) {
            return end();
        }
        return iterator(p);
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<iterator, typename Comp_::is_transparent> find(const Key& key) {
        auto* p = rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare());
        if (p == std::addressof(head_) || this->get_compare()(key, node_traits::get_key(node_traits::get_value(p)))) {
            return end();
        }
        return iterator(p);
    }

    const_iterator find(const key_type& key) const {
        auto* p = rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare());
        if (p == std::addressof(head_) || this->get_compare()(key, node_traits::get_key(node_traits::get_value(p)))) {
            return end();
        }
        return const_iterator(p);
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<const_iterator, typename Comp_::is_transparent> find(const Key& key) const {
        auto* p = rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare());
        if (p == std::addressof(head_) || this->get_compare()(key, node_traits::get_key(node_traits::get_value(p)))) {
            return end();
        }
        return const_iterator(p);
    }

    // - lower_bound

    iterator lower_bound(const key_type& key) {
        return iterator(rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<iterator, typename Comp_::is_transparent> lower_bound(const Key& key) {
        return iterator(rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    const_iterator lower_bound(const key_type& key) const {
        return const_iterator(rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<const_iterator, typename Comp_::is_transparent> lower_bound(const Key& key) const {
        return const_iterator(rbtree_lower_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    // - upper_bound

    iterator upper_bound(const key_type& key) {
        return iterator(rbtree_upper_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<iterator, typename Comp_::is_transparent> upper_bound(const Key& key) {
        return iterator(rbtree_upper_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    const_iterator upper_bound(const key_type& key) const {
        return const_iterator(rbtree_upper_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<const_iterator, typename Comp_::is_transparent> upper_bound(const Key& key) const {
        return const_iterator(rbtree_upper_bound<node_traits>(std::addressof(head_), key, this->get_compare()));
    }

    // - equal_range

    std::pair<iterator, iterator> equal_range(const key_type& key) {
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(iterator(result.first), iterator(result.second));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<std::pair<iterator, iterator>, typename Comp_::is_transparent> equal_range(const Key& key) {
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(iterator(result.first), iterator(result.second));
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(const_iterator(result.first), const_iterator(result.second));
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<std::pair<const_iterator, const_iterator>, typename Comp_::is_transparent> equal_range(
        const Key& key) const {
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        return std::make_pair(const_iterator(result.first), const_iterator(result.second));
    }

    // - count

    size_type count(const key_type& key) const {
        size_type count = 0;
        for (auto it : make_range(equal_range(key))) { ++count, (void)it; }
        return count;
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<size_type, typename Comp_::is_transparent> count(const Key& key) const {
        size_type count = 0;
        for (auto it : make_range(equal_range(key))) { ++count, (void)it; }
        return count;
    }

    // - contains

    bool contains(const key_type& key) const { return find(key) != end(); }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<bool, typename Comp_::is_transparent> contains(const Key& key) const {
        return find(key) != end();
    }

    // - clear, swap, erase

    void clear() { tidy(); }

    iterator erase(const_iterator pos) {
        auto* p = to_ptr(pos);
        assert(p != std::addressof(head_));
        auto* next = rbtree_remove(std::addressof(head_), p);
        delete_node(p);
        --size_;
        return iterator(next);
    }

    template<typename Key_ = key_type>
    iterator erase(std::enable_if_t<!std::is_same<Key_, value_type>::value, iterator> pos) {
        return erase(static_cast<const_iterator>(pos));
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto* p_first = to_ptr(first);
        auto* p_last = to_ptr(last);
        if (p_first != p_last) { erase_impl(p_first, p_last); }
        return iterator(p_last);
    }

    size_type erase(const key_type& key) {
        size_type old_size = size_;
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        if (result.first != result.second) { erase_impl(result.first, result.second); }
        return old_size - size_;
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<size_type, typename Comp_::is_transparent> erase(const Key& key) const {
        size_type old_size = size_;
        auto result = rbtree_equal_range<node_traits>(std::addressof(head_), key, this->get_compare());
        if (result.first != result.second) { erase_impl(result.first, result.second); }
        return old_size - size_;
    }

    node_type extract(const_iterator pos) {
        auto* p = to_ptr(pos);
        assert(p != std::addressof(head_));
        rbtree_remove(std::addressof(head_), p);
        node_traits::set_head(p, nullptr);
        --size_;
        return node_type(*this, p);
    }

    node_type extract(const key_type& key) {
        auto it = find(key);
        if (it != end()) { return extract(it); }
        return node_type(*this);
    }

    template<typename Key, typename Comp_ = key_compare>
    type_identity_t<node_type, typename Comp_::is_transparent> extract(const Key& key) {
        auto it = find(key);
        if (it != end()) { return extract(it); }
        return node_type(*this);
    }

 protected:
    struct value_compare_func {
        using result_type = bool;
        using first_argument_type = value_type;
        using second_argument_type = value_type;
        value_compare_func(const key_compare& comp_) : comp(comp_) {}
        bool operator()(const value_type& lhs, const value_type& rhs) const {
            return comp(node_traits::get_key(lhs), node_traits::get_key(rhs));
        }
        key_compare comp;
    };

    void tidy() {
        if (!size_) { return; }
        auto* root = head_.left;
        reset();
        delete_recursive(root);
    }

    void swap_impl(rbtree_base& other, std::true_type) noexcept(std::is_nothrow_swappable<key_compare>::value) {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        swap_impl(other, std::false_type());
    }

    void swap_impl(rbtree_base& other, std::false_type) noexcept(std::is_nothrow_swappable<key_compare>::value) {
        this->swap_compare(other.get_compare());
        if (!size_) {
            steal_data(other);
            return;
        } else if (other.size_) {
            std::swap(head_.left, other.head_.left);
            std::swap(head_.right, other.head_.right);
            std::swap(head_.parent, other.head_.parent);
            std::swap(head_.left->parent, other.head_.left->parent);
            std::swap(size_, other.size_);
            node_traits::set_head(head_.parent, std::addressof(head_), std::addressof(head_));
        } else {
            other.head_.left = head_.left;
            head_.left = nullptr;
            other.head_.right = head_.right;
            other.head_.parent = head_.parent;
            other.head_.left->parent = std::addressof(other.head_);
            head_.right = head_.parent = std::addressof(head_);
            other.size_ = size_;
            size_ = 0;
        }
        node_traits::set_head(other.head_.parent, std::addressof(other.head_), std::addressof(other.head_));
    }

 private:
    mutable typename node_traits::links_t head_;
    size_type size_ = 0;

    template<typename, typename>
    friend class rbtree_node_handle;
    template<typename, typename, typename>
    friend class rbtree_unique;
    template<typename, typename, typename>
    friend class rbtree_multi;

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::true_type) {
        return first <= last;
    }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::false_type) {
        return true;
    }

    rbtree_node_t* to_ptr(const_iterator it) const {
        auto* node = it.node();
        iterator_assert(node_traits::get_head(node) == std::addressof(head_));
        return node;
    }

    template<typename... Args>
    rbtree_node_t* new_node(Args&&... args) {
        auto* node = static_cast<rbtree_node_t*>(std::addressof(*alloc_traits::allocate(*this, 1)));
        try {
            alloc_traits::construct(*this, std::addressof(node_traits::get_value(node)), std::forward<Args>(args)...);
            node_traits::set_head(node, std::addressof(head_));
            return node;
        } catch (...) {
            alloc_traits::deallocate(*this, static_cast<typename node_traits::node_t*>(node), 1);
            throw;
        }
    }

    void delete_node(rbtree_node_t* node) {
        alloc_traits::destroy(*this, std::addressof(node_traits::get_value(node)));
        alloc_traits::deallocate(*this, static_cast<typename node_traits::node_t*>(node), 1);
    }

    void init() {
        rbtree_init_head(std::addressof(head_));
        node_traits::set_head(std::addressof(head_), std::addressof(head_));
    }

    void reset() {
        head_.left = nullptr;
        head_.right = head_.parent = std::addressof(head_);
        size_ = 0;
    }

    void delete_recursive(rbtree_node_t* node) {
        if (node->left) { delete_recursive(node->left); }
        if (node->right) { delete_recursive(node->right); }
        delete_node(node);
    }

    void steal_data(rbtree_base& other) {
        if (!other.size_) { return; }
        head_.left = other.head_.left;
        other.head_.left = nullptr;
        head_.right = other.head_.right;
        head_.parent = other.head_.parent;
        head_.left->parent = std::addressof(head_);
        other.head_.right = other.head_.parent = std::addressof(other.head_);
        size_ = other.size_;
        other.size_ = 0;
        node_traits::set_head(head_.parent, std::addressof(head_), std::addressof(head_));
    }

    struct reuse_cache_t {
        rbtree_base* tree;
        rbtree_node_t* reuse;
        explicit reuse_cache_t(rbtree_base* t) : tree(t), reuse(get_first(t->head_.parent)) {}
        ~reuse_cache_t() {
            while (reuse != std::addressof(tree->head_)) {
                auto* next = get_next(reuse);
                tree->delete_node(reuse);
                reuse = next;
            }
        }
        rbtree_node_t* operator*() const { return reuse; }
        operator bool() const { return reuse != std::addressof(tree->head_); }
        rbtree_node_t* advance() {
            auto* node = reuse;
            reuse = get_next(reuse);
            return node;
        }
        static rbtree_node_t* get_first(rbtree_node_t* node) {
            while (node->right) { node = rbtree_left_bound(node->right); }
            return node;
        }
        static rbtree_node_t* get_next(rbtree_node_t* node) {
            auto* next = node->parent;
            if (next->left == node) { return get_first(next); }
            return next;
        }
    };

    void construct_impl(rbtree_base&& other, const allocator_type& alloc, std::true_type) noexcept {
        init();
        steal_data(other);
    }

    void construct_impl(rbtree_base&& other, const allocator_type& alloc, std::false_type) {
        if (is_same_alloc(other)) {
            init();
            steal_data(other);
        } else {
            init_impl(other, node_traits::get_rref_value);
        }
    }

    void assign_impl(const rbtree_base& other, std::true_type) { assign_impl(other, node_traits::get_value); }

    void assign_impl(const rbtree_base& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_impl(other, node_traits::get_value);
        } else {
            tidy();
            alloc_type::operator=(other);
            init_impl(other, node_traits::get_value);
        }
    }

    void assign_impl(rbtree_base&& other, std::true_type) noexcept {
        tidy();
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        steal_data(other);
    }

    void assign_impl(rbtree_base&& other, std::false_type) {
        if (is_same_alloc(other)) {
            tidy();
            steal_data(other);
        } else {
            assign_impl(other, node_traits::get_rref_value);
        }
    }

    template<typename Other>
    static void tidy_other(Other&& other, std::true_type /* copy other */) {}
    template<typename Other>
    static void tidy_other(Other&& other, std::false_type /* move other */) {
        other.tidy();
    }

    template<typename Other, typename CopyFunc>
    void init_impl(Other&& other, CopyFunc fn) {
        init();
        if (!other.size_) { return; }
        try {
            head_.left = new_node(fn(other.head_.left));
            head_.left->parent = std::addressof(head_);
            copy_node(head_.left, other.head_.left, fn);
            head_.parent = rbtree_left_bound(head_.left);
            head_.right = rbtree_right_bound(head_.left);
            size_ = other.size_;
            tidy_other(other, std::is_const<std::remove_reference_t<Other>>());
        } catch (...) {
            if (head_.left) { delete_recursive(head_.left); }
            tidy_other(other, std::is_const<std::remove_reference_t<Other>>());
            throw;
        }
    }

    template<typename Other, typename CopyFunc>
    void assign_impl(Other&& other, CopyFunc fn) {
        if (!other.size_) {
            tidy();
            return;
        }
        try {
            if (size_) {
                reuse_cache_t cache(this);
                reset();
                node_traits::get_lref_value(*cache) = fn(other.head_.left);
                head_.left = cache.advance();
                head_.left->parent = std::addressof(head_);
                copy_node_reuse(head_.left, other.head_.left, fn, cache);
            } else {
                head_.left = new_node(fn(other.head_.left));
                head_.left->parent = std::addressof(head_);
                copy_node(head_.left, other.head_.left, fn);
            }
            head_.parent = rbtree_left_bound(head_.left);
            head_.right = rbtree_right_bound(head_.left);
            size_ = other.size_;
            tidy_other(other, std::is_const<std::remove_reference_t<Other>>());
        } catch (...) {
            if (head_.left) { delete_recursive(head_.left); }
            tidy_other(other, std::is_const<std::remove_reference_t<Other>>());
            throw;
        }
    }

    void erase_impl(rbtree_node_t* first, rbtree_node_t* last) {
        do {
            assert(first != std::addressof(head_));
            auto* next = rbtree_remove(std::addressof(head_), first);
            delete_node(first);
            --size_, first = next;
        } while (first != last);
    }

    template<typename CopyFunc>
    void copy_node(rbtree_node_t* node, rbtree_node_t* src_node, CopyFunc fn);
    template<typename CopyFunc>
    void copy_node_reuse(rbtree_node_t* node, rbtree_node_t* src_node, CopyFunc fn, reuse_cache_t& cache);
};

template<typename NodeTraits, typename Alloc, typename Comp>
template<typename CopyFunc>
void rbtree_base<NodeTraits, Alloc, Comp>::copy_node(rbtree_node_t* node, rbtree_node_t* src_node, CopyFunc fn) {
    node->left = node->right = nullptr;
    node->color = src_node->color;
    if (src_node->left) {
        node->left = new_node(fn(src_node->left));
        node->left->parent = node;
        copy_node(node->left, src_node->left, fn);
    }
    if (src_node->right) {
        node->right = new_node(fn(src_node->right));
        node->right->parent = node;
        copy_node(node->right, src_node->right, fn);
    }
}

template<typename NodeTraits, typename Alloc, typename Comp>
template<typename CopyFunc>
void rbtree_base<NodeTraits, Alloc, Comp>::copy_node_reuse(rbtree_node_t* node, rbtree_node_t* src_node, CopyFunc fn,
                                                           reuse_cache_t& cache) {
    node->left = node->right = nullptr;
    node->color = src_node->color;
    if (src_node->left) {
        if (cache) {
            node_traits::get_lref_value(*cache) = fn(src_node->left);
            node->left = cache.advance();
            node->left->parent = node;
            copy_node_reuse(node->left, src_node->left, fn, cache);
        } else {
            node->left = new_node(fn(src_node->left));
            node->left->parent = node;
            copy_node(node->left, src_node->left, fn);
        }
    }
    if (src_node->right) {
        if (cache) {
            node_traits::get_lref_value(*cache) = fn(src_node->right);
            node->right = cache.advance();
            node->right->parent = node;
            copy_node_reuse(node->right, src_node->right, fn, cache);
        } else {
            node->right = new_node(fn(src_node->right));
            node->right->parent = node;
            copy_node(node->right, src_node->right, fn);
        }
    }
}

#if __cplusplus >= 201703L
template<typename InputIt>
using iter_key_t = remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>;
template<typename InputIt>
using iter_val_t = typename std::iterator_traits<InputIt>::value_type::second_type;
template<typename InputIt>
using iter_to_alloc_t = std::pair<std::add_const_t<iter_key_t<InputIt>>, iter_val_t<InputIt>>;
#endif  // __cplusplus >= 201703L

#if __cplusplus >= 201703L
template<typename InputIt>
using iter_key_t = remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>;
template<typename InputIt>
using iter_val_t = typename std::iterator_traits<InputIt>::value_type::second_type;
template<typename InputIt>
using iter_to_alloc_t = std::pair<std::add_const_t<iter_key_t<InputIt>>, iter_val_t<InputIt>>;
#endif  // __cplusplus >= 201703L

}  // namespace detail

}  // namespace uxs
