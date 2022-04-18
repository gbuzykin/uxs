#pragma once

#include "allocator.h"
#include "dllist.h"
#include "iterator.h"

namespace util {

//-----------------------------------------------------------------------------
// List implementation

namespace detail {
struct list_links_type : dllist_node_t {
    static dllist_node_t* get_next(dllist_node_t* node) { return node->next; }
    static dllist_node_t* get_prev(dllist_node_t* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) { static_cast<list_links_type*>(node)->head = head; }
    static void set_head(dllist_node_t* first, dllist_node_t* last, dllist_node_t* head) {
        for (auto* p = first; p != last; p = get_next(p)) { set_head(p, head); }
    }
    static dllist_node_t* get_head(dllist_node_t* node) { return static_cast<list_links_type*>(node)->head; }
    static dllist_node_t* get_front(dllist_node_t* head) { return head->next; }
    dllist_node_t* head;
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) {}
    static void set_head(dllist_node_t* first, dllist_node_t* last, dllist_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Ty>
struct list_node_type : list_links_type {
    using links_t = list_links_type;
    using iterator_node_t = dllist_node_t;
    static Ty& get_value(dllist_node_t* node) { return static_cast<list_node_type*>(node)->value; }
    Ty value;
};
}  // namespace detail

template<typename Ty, typename Alloc = std::allocator<Ty>>
class list : protected std::allocator_traits<Alloc>::template rebind_alloc<detail::list_node_type<Ty>> {
 private:
    using node_t = detail::list_node_type<Ty>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<node_t>;
    using alloc_traits = std::allocator_traits<alloc_type>;
    using value_alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
    using value_alloc_traits = std::allocator_traits<value_alloc_type>;

 public:
    using value_type = Ty;
    using allocator_type = Alloc;
    using size_type = typename value_alloc_traits::size_type;
    using difference_type = typename value_alloc_traits::difference_type;
    using pointer = typename value_alloc_traits::pointer;
    using const_pointer = typename value_alloc_traits::const_pointer;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = list_iterator<list, node_t, false>;
    using const_iterator = list_iterator<list, node_t, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    list() NOEXCEPT_IF(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(allocator_type()) {
        init();
    }
    explicit list(const allocator_type& alloc) NOEXCEPT : alloc_type(alloc) { init(); }
    explicit list(size_type sz, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init();
        try {
            insert_default(std::addressof(head_), sz);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(size_type sz, const value_type& val, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init();
        try {
            insert_const(std::addressof(head_), sz, val);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(std::initializer_list<value_type> l, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init();
        try {
            insert_impl(std::addressof(head_), l.begin(), l.end());
        } catch (...) {
            tidy();
            throw;
        }
    }

    list& operator=(std::initializer_list<value_type> l) {
        assign_impl(l.begin(), l.end(), std::is_copy_assignable<Ty>());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    list(InputIt first, InputIt last, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        init();
        try {
            insert_impl(std::addressof(head_), first, last);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(const list& other) : alloc_type(alloc_traits::select_on_container_copy_construction(other)) {
        init();
        try {
            insert_impl(std::addressof(head_), other.begin(), other.end());
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(const list& other, const allocator_type& alloc) : alloc_type(alloc) {
        init();
        try {
            insert_impl(std::addressof(head_), other.begin(), other.end());
        } catch (...) {
            tidy();
            throw;
        }
    }

    list& operator=(const list& other) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(other, std::bool_constant<(!alloc_traits::propagate_on_container_copy_assignment::value ||
                                               is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    list(list&& other) NOEXCEPT : alloc_type(std::move(other)) {
        init();
        steal_data(other);
    }

    list(list&& other, const allocator_type& alloc) NOEXCEPT_IF(is_alloc_always_equal<alloc_type>::value)
        : alloc_type(alloc) {
        construct_impl(std::move(other), alloc, is_alloc_always_equal<alloc_type>());
    }

    list& operator=(list&& other) NOEXCEPT_IF(alloc_traits::propagate_on_container_move_assignment::value ||
                                              is_alloc_always_equal<alloc_type>::value) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(std::move(other), std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    ~list() { tidy(); }

    void swap(list& other) NOEXCEPT {
        if (std::addressof(other) == this) { return; }
        swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    allocator_type get_allocator() const { return static_cast<const alloc_type&>(*this); }

    bool empty() const NOEXCEPT { return size_ == 0; }
    size_type size() const NOEXCEPT { return size_; }
    size_type max_size() const NOEXCEPT { return alloc_traits::max_size(*this); }

    iterator begin() NOEXCEPT { return iterator(head_.next); }
    const_iterator begin() const NOEXCEPT { return const_iterator(head_.next); }
    const_iterator cbegin() const NOEXCEPT { return const_iterator(head_.next); }

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
        return node_t::get_value(head_.next);
    }
    const_reference front() const {
        assert(size_);
        return node_t::get_value(head_.next);
    }

    reference back() {
        assert(size_);
        return node_t::get_value(head_.prev);
    }
    const_reference back() const {
        assert(size_);
        return node_t::get_value(head_.prev);
    }

    void assign(size_type sz, const value_type& val) { assign_const(sz, val, std::is_copy_assignable<Ty>()); }

    void assign(std::initializer_list<value_type> l) { assign_impl(l.begin(), l.end(), std::is_copy_assignable<Ty>()); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_impl(first, last, std::is_assignable<Ty&, decltype(*first)>());
    }

    void clear() NOEXCEPT { tidy(); }

    void resize(size_type sz) {
        if (sz < size_) {
            auto* p = head_.prev;
            while (++sz < size_) { p = p->prev; }
            erase_impl(p, std::addressof(head_));
        } else {
            insert_default(std::addressof(head_), sz - size_);
        }
    }

    void resize(size_type sz, const value_type& val) {
        if (sz < size_) {
            auto* p = head_.prev;
            while (++sz < size_) { p = p->prev; }
            erase_impl(p, std::addressof(head_));
        } else {
            insert_const(std::addressof(head_), sz - size_, val);
        }
    }

    iterator insert(const_iterator pos, size_type count, const value_type& val) {
        return iterator(insert_const(to_ptr(pos), count, val));
    }

    iterator insert(const_iterator pos, std::initializer_list<value_type> l) {
        return iterator(insert_impl(to_ptr(pos), l.begin(), l.end()));
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        return iterator(insert_impl(to_ptr(pos), first, last));
    }

    iterator insert(const_iterator pos, const value_type& val) { return emplace(pos, val); }
    iterator insert(const_iterator pos, value_type&& val) { return emplace(pos, std::move(val)); }
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        auto* node = new_node(std::forward<Args>(args)...);
        dllist_insert_before(to_ptr(pos), node);
        return iterator(node);
    }

    void push_front(const value_type& val) { emplace_front(val); }
    void push_front(value_type&& val) { emplace_front(std::move(val)); }
    template<typename... Args>
    reference emplace_front(Args&&... args) {
        auto* node = new_node(std::forward<Args>(args)...);
        dllist_insert_after<dllist_node_t>(std::addressof(head_), node);
        return node_t::get_value(node);
    }

    void pop_front() {
        assert(size_);
        --size_;
        auto* p = head_.next;
        dllist_remove(p);
        helpers::delete_node(*this, p);
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        auto* node = new_node(std::forward<Args>(args)...);
        dllist_insert_before<dllist_node_t>(std::addressof(head_), node);
        return node_t::get_value(node);
    }

    void pop_back() {
        assert(size_);
        --size_;
        auto* p = head_.prev;
        dllist_remove(p);
        helpers::delete_node(*this, p);
    }

    iterator erase(const_iterator pos) {
        auto* p = to_ptr(pos);
        assert(p != std::addressof(head_));
        --size_;
        auto* next = dllist_remove(p);
        helpers::delete_node(*this, p);
        return iterator(next);
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto* p_first = to_ptr(first);
        auto* p_last = to_ptr(last);
        if (p_first != p_last) { erase_impl(p_first, p_last); }
        return iterator(p_last);
    }

    size_type remove(const value_type& val) {
        size_type old_sz = size_;
        for (auto* p = head_.next; p != std::addressof(head_);) {
            if (node_t::get_value(p) == val) {
                --size_;
                auto* next = dllist_remove(p);
                helpers::delete_node(*this, p);
                p = next;
            } else {
                p = p->next;
            }
        }
        return old_sz - size_;
    }

    template<typename Pred>
    size_type remove_if(Pred pred) {
        size_type old_sz = size_;
        for (auto* p = head_.next; p != std::addressof(head_);) {
            if (pred(node_t::get_value(p))) {
                --size_;
                auto* next = dllist_remove(p);
                helpers::delete_node(*this, p);
                p = next;
            } else {
                p = p->next;
            }
        }
        return old_sz - size_;
    }

    size_type unique() { return unique(std::equal_to<value_type>()); }
    template<typename Pred>
    size_type unique(Pred pred) {
        size_type old_sz = size_;
        if (!old_sz) { return 0; }
        for (auto *p0 = head_.next, *p = p0->next; p != std::addressof(head_);) {
            if (pred(node_t::get_value(p0), node_t::get_value(p))) {
                --size_;
                auto* next = dllist_remove(p);
                helpers::delete_node(*this, p);
                p = next;
            } else {
                p0 = p;
                p = p->next;
            }
        }
        return old_sz - size_;
    }

    void reverse() {
        if (!size_) { return; }
        auto* p = static_cast<dllist_node_t*>(std::addressof(head_));
        do {
            std::swap(p->next, p->prev);
            p = p->prev;
        } while (p != std::addressof(head_));
    }

    void splice(const_iterator pos, list& other) { splice_impl(pos, std::move(other)); }
    void splice(const_iterator pos, list&& other) { splice_impl(pos, std::move(other)); }
    void splice(const_iterator pos, list& other, const_iterator it) { splice_impl(pos, std::move(other), it); }
    void splice(const_iterator pos, list&& other, const_iterator it) { splice_impl(pos, std::move(other), it); }
    void splice(const_iterator pos, list& other, const_iterator first, const_iterator last) {
        splice_impl(pos, std::move(other), first, last);
    }
    void splice(const_iterator pos, list&& other, const_iterator first, const_iterator last) {
        splice_impl(pos, std::move(other), first, last);
    }

    void merge(list& other) { merge_impl(std::move(other), std::less<value_type>()); }
    void merge(list&& other) { merge_impl(std::move(other), std::less<value_type>()); }
    template<typename Comp>
    void merge(list& other, Comp comp) {
        merge_impl(std::move(other), comp);
    }
    template<typename Comp>
    void merge(list&& other, Comp comp) {
        merge_impl(std::move(other), comp);
    }

    template<typename Comp>
    void sort(Comp comp);
    void sort() { sort(std::less<value_type>()); }

 private:
    mutable typename node_t::links_t head_;
    size_type size_ = 0;

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    dllist_node_t* to_ptr(const_iterator it) const {
        auto* node = it.node();
        iterator_assert(node_t::get_head(node) == std::addressof(head_));
        return node;
    }

    template<typename... Args>
    dllist_node_t* new_node(Args&&... args) {
        auto* node = helpers::new_node(*this, std::is_nothrow_constructible<Ty, Args...>(), std::forward<Args>(args)...);
        node_t::set_head(node, std::addressof(head_));
        ++size_;
        return node;
    }

    void init() {
        dllist_make_cycle(std::addressof(head_));
        node_t::set_head(std::addressof(head_), std::addressof(head_));
    }

    void reset() {
        size_ = 0;
        dllist_make_cycle(std::addressof(head_));
    }

    void delete_node_chain(dllist_node_t* node) {
        while (node != std::addressof(head_)) {
            auto* next = node->next;
            helpers::delete_node(*this, node);
            node = next;
        }
    }

    void tidy() {
        delete_node_chain(head_.next);
        reset();
    }

    void steal_data(list& other) {
        if (!other.size_) { return; }
        size_ = other.size_;
        head_.next = other.head_.next;
        head_.next->prev = std::addressof(head_);
        head_.prev = other.head_.prev;
        head_.prev->next = std::addressof(head_);
        other.reset();
        node_t::set_head(head_.next, std::addressof(head_), std::addressof(head_));
    }

    void construct_impl(list&& other, const allocator_type& alloc, std::true_type) NOEXCEPT {
        init();
        steal_data(other);
    }

    void construct_impl(list&& other, const allocator_type& alloc, std::false_type) {
        init();
        if (is_same_alloc(other)) {
            steal_data(other);
        } else {
            try {
                insert_impl(std::addressof(head_), std::make_move_iterator(other.begin()),
                            std::make_move_iterator(other.end()));
            } catch (...) {
                tidy();
                throw;
            }
        }
    }

    void assign_impl(const list& other, std::true_type) {
        assign_impl(other.begin(), other.end(), std::is_copy_assignable<Ty>());
    }

    void assign_impl(const list& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_impl(other.begin(), other.end(), std::is_copy_assignable<Ty>());
        } else {
            tidy();
            alloc_type::operator=(other);
            insert_impl(std::addressof(head_), other.begin(), other.end());
        }
    }

    void assign_impl(list&& other, std::true_type) NOEXCEPT {
        tidy();
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        steal_data(other);
    }

    void assign_impl(list&& other, std::false_type) {
        if (is_same_alloc(other)) {
            tidy();
            steal_data(other);
        } else {
            assign_impl(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()),
                        std::is_move_assignable<Ty>());
        }
    }

    void swap_impl(list& other, std::true_type) NOEXCEPT {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        swap_impl(other, std::false_type());
    }

    void swap_impl(list& other, std::false_type) NOEXCEPT {
        if (!size_) {
            steal_data(other);
            return;
        } else if (other.size_) {
            std::swap(size_, other.size_);
            std::swap(head_.next, other.head_.next);
            std::swap(head_.prev, other.head_.prev);
            std::swap(head_.next->prev, other.head_.next->prev);
            std::swap(head_.prev->next, other.head_.prev->next);
            node_t::set_head(head_.next, std::addressof(head_), std::addressof(head_));
        } else {
            other.size_ = size_;
            dllist_insert_after<dllist_node_t>(std::addressof(other.head_), head_.next, head_.prev);
            reset();
        }
        node_t::set_head(other.head_.next, std::addressof(other.head_), std::addressof(other.head_));
    }

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::true_type /* assignable */) {
        assert(helpers::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        auto* p = head_.next;
        for (; p != std::addressof(head_) && first != last; ++first) {
            node_t::get_value(p) = *first;
            p = p->next;
        }
        if (p != std::addressof(head_)) {
            erase_impl(p, std::addressof(head_));
        } else {
            insert_impl(std::addressof(head_), first, last);
        }
    }

    template<typename InputIt>
    void assign_impl(InputIt first, InputIt last, std::false_type /* assignable */) {
        assert(helpers::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        auto* reuse = head_.next;
        reset();
        try {
            for (; reuse != std::addressof(head_) && first != last; ++first) {
                auto* node = helpers::reconstruct_node(*this, reuse, *first);
                reuse = reuse->next;
                ++size_;
                dllist_insert_before<dllist_node_t>(std::addressof(head_), node);
            }
        } catch (...) {
            delete_node_chain(reuse->next);
            alloc_traits::deallocate(*this, static_cast<node_t*>(reuse), 1);
            throw;
        }
        if (reuse == std::addressof(head_)) {
            insert_impl(std::addressof(head_), first, last);
        } else {
            delete_node_chain(reuse);
        }
    }

    void assign_const(size_type sz, const value_type& val, std::true_type /* assignable */) {
        auto* p = head_.next;
        for (; p != std::addressof(head_) && sz; --sz) {
            node_t::get_value(p) = val;
            p = p->next;
        }
        if (p != std::addressof(head_)) {
            erase_impl(p, std::addressof(head_));
        } else {
            insert_const(std::addressof(head_), sz, val);
        }
    }

    void assign_const(size_type sz, const value_type& val, std::false_type /* assignable */) {
        auto* reuse = head_.next;
        reset();
        try {
            for (; reuse != std::addressof(head_) && sz; --sz) {
                auto* node = helpers::reconstruct_node(*this, reuse, val);
                reuse = reuse->next;
                ++size_;
                dllist_insert_before<dllist_node_t>(std::addressof(head_), node);
            }
        } catch (...) {
            delete_node_chain(reuse->next);
            alloc_traits::deallocate(*this, static_cast<node_t*>(reuse), 1);
            throw;
        }
        if (reuse == std::addressof(head_)) {
            insert_const(std::addressof(head_), sz, val);
        } else {
            delete_node_chain(reuse);
        }
    }

    template<typename InputIt>
    dllist_node_t* insert_impl(dllist_node_t* pos, InputIt first, InputIt last) {
        assert(helpers::check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        auto* pre_first = pos->prev;
        for (; first != last; ++first) { dllist_insert_before(pos, new_node(*first)); }
        return pre_first->next;
    }

    dllist_node_t* insert_const(dllist_node_t* pos, size_type sz, const value_type& val) {
        auto* pre_first = pos->prev;
        for (; sz; --sz) { dllist_insert_before(pos, new_node(val)); }
        return pre_first->next;
    }

    dllist_node_t* insert_default(dllist_node_t* pos, size_type sz) {
        auto* pre_first = pos->prev;
        for (; sz; --sz) { dllist_insert_before(pos, new_node()); }
        return pre_first->next;
    }

    void erase_impl(dllist_node_t* first, dllist_node_t* last) {
        assert(first != last);
        dllist_remove(first, last);
        do {
            assert(first != std::addressof(head_));
            --size_;
            auto* next = first->next;
            helpers::delete_node(*this, first);
            first = next;
        } while (first != last);
    }

    void splice_impl(const_iterator pos, list&& other);
    void splice_impl(const_iterator pos, list&& other, const_iterator it);
    void splice_impl(const_iterator pos, list&& other, const_iterator first, const_iterator last);

    template<typename Comp>
    void merge_impl(list&& other, Comp comp);
    template<typename Comp>
    void merge_impl(dllist_node_t* head_tgt, dllist_node_t* head_src, Comp comp);

    struct helpers {
        template<typename InputIt>
        static bool check_iterator_range(InputIt first, InputIt last, std::true_type /* random access iterator */) {
            return first <= last;
        }

        template<typename InputIt>
        static bool check_iterator_range(InputIt first, InputIt last, std::false_type /* random access iterator */) {
            return true;
        }

        template<typename... Args>
        static dllist_node_t* new_node(alloc_type& alloc, std::true_type /* nothrow constructible */,
                                       Args&&... args) NOEXCEPT {
            auto* node = static_cast<dllist_node_t*>(std::addressof(*alloc_traits::allocate(alloc, 1)));
            alloc_traits::construct(alloc, std::addressof(node_t::get_value(node)), std::forward<Args>(args)...);
            return node;
        }

        template<typename... Args>
        static dllist_node_t* new_node(alloc_type& alloc, std::false_type /* nothrow constructible */, Args&&... args) {
            auto* node = static_cast<dllist_node_t*>(std::addressof(*alloc_traits::allocate(alloc, 1)));
            try {
                alloc_traits::construct(alloc, std::addressof(node_t::get_value(node)), std::forward<Args>(args)...);
            } catch (...) {
                alloc_traits::deallocate(alloc, static_cast<node_t*>(node), 1);
                throw;
            }
            return node;
        }

        template<typename... Args>
        static dllist_node_t* reconstruct_node(alloc_type& alloc, dllist_node_t* node, Args&&... args) {
            alloc_traits::destroy(alloc, std::addressof(node_t::get_value(node)));
            alloc_traits::construct(alloc, std::addressof(node_t::get_value(node)), std::forward<Args>(args)...);
            return node;
        }

        static void delete_node(alloc_type& alloc, dllist_node_t* node) {
            alloc_traits::destroy(alloc, std::addressof(node_t::get_value(node)));
            alloc_traits::deallocate(alloc, static_cast<node_t*>(node), 1);
        }
    };
};  // namespace util

template<typename Ty, typename Alloc>
void list<Ty, Alloc>::splice_impl(const_iterator pos, list&& other) {
    assert(std::addressof(other) != this || pos == end());
    if (!other.size_ || std::addressof(other) == this) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for splice");
    }
    node_t::set_head(other.head_.next, std::addressof(other.head_), std::addressof(head_));
    size_ += other.size_;
    dllist_insert_before(to_ptr(pos), other.head_.next, other.head_.prev);
    other.reset();
}

template<typename Ty, typename Alloc>
void list<Ty, Alloc>::splice_impl(const_iterator pos, list&& other, const_iterator it) {
    auto* p = other.to_ptr(it);
    if (std::addressof(other) != this) {
        if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
            throw std::logic_error("allocators incompatible for splice");
        }
        node_t::set_head(p, std::addressof(head_));
        ++size_;
        --other.size_;
    } else if (it == pos) {
        return;
    }
    dllist_remove(p);
    dllist_insert_before(to_ptr(pos), p);
}

template<typename Ty, typename Alloc>
void list<Ty, Alloc>::splice_impl(const_iterator pos, list&& other, const_iterator first, const_iterator last) {
    auto* p_first = other.to_ptr(first);
    auto* p_last = other.to_ptr(last);
    if (p_first == p_last) { return; }
    if (std::addressof(other) != this) {
        if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
            throw std::logic_error("allocators incompatible for splice");
        }
        size_type count = 0;
        auto* p = p_first;
        do {
            assert(p != std::addressof(other.head_));
            node_t::set_head(p, std::addressof(head_));
            p = p->next;
            ++count;
        } while (p != p_last);
        size_ += count;
        other.size_ -= count;
    } else {
        if (last == pos) { return; }
#if !defined(NDEBUG)
        for (auto it = first; it != last; ++it) { assert(it != pos); }
#endif  // !defined(NDEBUG)
    }
    auto* pre_last = dllist_remove(p_first, p_last);
    dllist_insert_before(to_ptr(pos), p_first, pre_last);
}

template<typename Ty, typename Alloc>
template<typename Comp>
void list<Ty, Alloc>::merge_impl(list&& other, Comp comp) {
    if (!other.size_ || std::addressof(other) == this) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for merge");
    }
    auto* head_src = std::addressof(other.head_);
    node_t::set_head(head_src->next, head_src, std::addressof(head_));
    size_ += other.size_;
    other.size_ = 0;
    try {
        merge_impl(std::addressof(head_), head_src, comp);
    } catch (...) {
        node_t::set_head(head_src->next, head_src, head_src);
        for (auto* p = head_src->next; p != head_src; p = p->next, --size_, ++other.size_) {}
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename Comp>
void list<Ty, Alloc>::merge_impl(dllist_node_t* head_tgt, dllist_node_t* head_src, Comp comp) {
    auto* p_first = head_src->next;
    auto* p_last = p_first;
    for (auto* p = head_tgt->next; p != head_tgt && p_last != head_src; p = p->next) {
        while (p_last != head_src && comp(node_t::get_value(p_last), node_t::get_value(p))) { p_last = p_last->next; }
        if (p_first != p_last) {
            auto* pre_last = dllist_remove(p_first, p_last);
            dllist_insert_before(p, p_first, pre_last);
            p_first = p_last;
        }
    }
    if (p_first != head_src) {
        dllist_insert_before(head_tgt, p_first, head_src->prev);
        dllist_make_cycle(head_src);
    }
}

template<typename Ty, typename Alloc>
template<typename Comp>
void list<Ty, Alloc>::sort(Comp comp) {
    if (size_ < 2) { return; }

    // worth sorting, do it
    const size_type max_bins = 25;
    size_type maxbin = 0;
    dllist_node_t tmp_list, bin_lists[max_bins];

    dllist_make_cycle(std::addressof(tmp_list));

    try {
        while (!dllist_is_empty(std::addressof(head_))) {
            // sort another element, using bins
            auto* p = head_.next;
            dllist_remove(p);
            dllist_insert_before(std::addressof(tmp_list), p);

            size_type bin;

            // merge into ever larger bins
            for (bin = 0; bin < maxbin && !dllist_is_empty(std::addressof(bin_lists[bin])); ++bin) {
                merge_impl(std::addressof(tmp_list), std::addressof(bin_lists[bin]), comp);
            }

            if (bin == max_bins) {
                merge_impl(std::addressof(bin_lists[bin - 1]), std::addressof(tmp_list), comp);
            } else {
                if (bin == maxbin) { dllist_make_cycle(std::addressof(bin_lists[maxbin++])); }
                assert(dllist_is_empty(std::addressof(bin_lists[bin])));
                dllist_insert_after(std::addressof(bin_lists[bin]), tmp_list.next, tmp_list.prev);
                dllist_make_cycle(std::addressof(tmp_list));
            }
        }

        // merge up
        for (size_type bin = 1; bin < maxbin; ++bin) {
            merge_impl(std::addressof(bin_lists[bin]), std::addressof(bin_lists[bin - 1]), comp);
        }

        // result in last bin
        dllist_insert_before<dllist_node_t>(std::addressof(head_), bin_lists[maxbin - 1].next,
                                            bin_lists[maxbin - 1].prev);
    } catch (...) {
        // collect all stuff
        dllist_insert_before<dllist_node_t>(std::addressof(head_), tmp_list.next, tmp_list.prev);
        for (size_type bin = 0; bin < maxbin; ++bin) {
            dllist_insert_before<dllist_node_t>(std::addressof(head_), bin_lists[bin].next, bin_lists[bin].prev);
        }
        throw;
    }
}

#if __cplusplus >= 201703L
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
list(InputIt, InputIt, Alloc = Alloc()) -> list<typename std::iterator_traits<InputIt>::value_type, Alloc>;
template<typename Ty, typename Alloc = std::allocator<Ty>>
list(typename list<Ty>::size_type, Ty, Alloc = Alloc()) -> list<Ty, Alloc>;
#endif  // __cplusplus >= 201703L

template<typename Ty, typename Alloc>
bool operator==(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template<typename Ty, typename Alloc>
bool operator<(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template<typename Ty, typename Alloc>
bool operator!=(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    return !(lhs == rhs);
}
template<typename Ty, typename Alloc>
bool operator<=(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    return !(rhs < lhs);
}
template<typename Ty, typename Alloc>
bool operator>(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    return rhs < lhs;
}
template<typename Ty, typename Alloc>
bool operator>=(const list<Ty, Alloc>& lhs, const list<Ty, Alloc>& rhs) {
    return !(lhs < rhs);
}

}  // namespace util

namespace std {
template<typename Ty, typename Alloc>
void swap(util::list<Ty, Alloc>& l1, util::list<Ty, Alloc>& l2) NOEXCEPT_IF(NOEXCEPT_IF(l1.swap(l2))) {
    l1.swap(l2);
}
}  // namespace std
