#pragma once

#include "dllist.h"
#include "iterator.h"
#include "memory.h"

#include <algorithm>
#include <stdexcept>

namespace uxs {

//-----------------------------------------------------------------------------
// List implementation

namespace detail {

struct list_links_t {
    list_links_t* next;
    list_links_t* prev;
#if _ITERATOR_DEBUG_LEVEL != 0
    list_links_t* head;
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Ty>
struct list_node_type : list_links_t {
    Ty value;
};

template<typename Ty>
struct list_node_traits {
    using iterator_node_t = list_links_t;
    using node_t = list_node_type<Ty>;
    static list_links_t* get_next(list_links_t* node) { return node->next; }
    static list_links_t* get_prev(list_links_t* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) { node->head = head; }
    static void set_head(list_links_t* first, list_links_t* last, list_links_t* head) {
        for (auto* p = first; p != last; p = get_next(p)) { set_head(p, head); }
    }
    static list_links_t* get_head(list_links_t* node) { return node->head; }
    static list_links_t* get_front(list_links_t* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) {}
    static void set_head(list_links_t* first, list_links_t* last, list_links_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    static Ty& get_value(list_links_t* node) { return static_cast<node_t*>(node)->value; }
};

}  // namespace detail

template<typename Ty, typename Alloc = std::allocator<Ty>>
class list : protected std::allocator_traits<Alloc>::template rebind_alloc<  //
                 typename detail::list_node_traits<Ty>::node_t> {
 private:
    using list_links_t = detail::list_links_t;
    using node_traits = detail::list_node_traits<Ty>;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<typename node_traits::node_t>;
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
    using iterator = list_iterator<list, node_traits, false>;
    using const_iterator = list_iterator<list, node_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    list() noexcept(std::is_nothrow_default_constructible<alloc_type>::value) : alloc_type(allocator_type()) { init(); }
    explicit list(const allocator_type& alloc) noexcept : alloc_type(alloc) { init(); }
    explicit list(size_type sz, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init();
            insert_default(std::addressof(head_), sz);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(size_type sz, const value_type& val, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init();
            insert_fill(std::addressof(head_), sz, val);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(std::initializer_list<value_type> l, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init();
            insert_impl(std::addressof(head_), l.begin(), l.end());
        } catch (...) {
            tidy();
            throw;
        }
    }

    list& operator=(std::initializer_list<value_type> l) {
        assign_range(l.begin(), l.end());
        return *this;
    }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    list(InputIt first, InputIt last, const allocator_type& alloc = allocator_type()) : alloc_type(alloc) {
        try {
            init();
            insert_impl(std::addressof(head_), first, last);
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(const list& other) : alloc_type(alloc_traits::select_on_container_copy_construction(other)) {
        try {
            init();
            insert_impl(std::addressof(head_), other.begin(), other.end());
        } catch (...) {
            tidy();
            throw;
        }
    }

    list(const list& other, const allocator_type& alloc) : alloc_type(alloc) {
        try {
            init();
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

    list(list&& other) noexcept : alloc_type(std::move(other)) {
        init();
        steal_data(other);
    }

    list(list&& other, const allocator_type& alloc) noexcept(is_alloc_always_equal<alloc_type>::value)
        : alloc_type(alloc) {
        construct_impl(std::move(other), alloc, is_alloc_always_equal<alloc_type>());
    }

    list& operator=(list&& other) noexcept(alloc_traits::propagate_on_container_move_assignment::value ||
                                           is_alloc_always_equal<alloc_type>::value) {
        if (std::addressof(other) == this) { return *this; }
        assign_impl(std::move(other), std::bool_constant<(alloc_traits::propagate_on_container_move_assignment::value ||
                                                          is_alloc_always_equal<alloc_type>::value)>());
        return *this;
    }

    ~list() { tidy(); }

    void swap(list& other) noexcept {
        if (std::addressof(other) == this) { return; }
        swap_impl(other, typename alloc_traits::propagate_on_container_swap());
    }

    allocator_type get_allocator() const noexcept { return allocator_type(*this); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }
    size_type max_size() const noexcept { return alloc_traits::max_size(*this); }

    iterator begin() noexcept { return iterator(head_.next); }
    const_iterator begin() const noexcept { return const_iterator(head_.next); }
    const_iterator cbegin() const noexcept { return begin(); }

    iterator end() noexcept { return iterator(std::addressof(head_)); }
    const_iterator end() const noexcept { return const_iterator(std::addressof(head_)); }
    const_iterator cend() const noexcept { return end(); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }

    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    reference front() {
        assert(size_);
        return node_traits::get_value(head_.next);
    }
    const_reference front() const {
        assert(size_);
        return node_traits::get_value(head_.next);
    }

    reference back() {
        assert(size_);
        return node_traits::get_value(head_.prev);
    }
    const_reference back() const {
        assert(size_);
        return node_traits::get_value(head_.prev);
    }

    void assign(size_type sz, const value_type& val) { assign_fill(sz, val); }

    void assign(std::initializer_list<value_type> l) { assign_range(l.begin(), l.end()); }

    template<typename InputIt, typename = std::enable_if_t<is_input_iterator<InputIt>::value>>
    void assign(InputIt first, InputIt last) {
        assign_range(first, last);
    }

    void clear() noexcept { tidy(); }

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
            insert_fill(std::addressof(head_), sz - size_, val);
        }
    }

    iterator insert(const_iterator pos, size_type count, const value_type& val) {
        return iterator(insert_fill(to_ptr(pos), count, val));
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
        ++size_;
        return iterator(node);
    }

    void push_front(const value_type& val) { emplace_front(val); }
    void push_front(value_type&& val) { emplace_front(std::move(val)); }
    template<typename... Args>
    reference emplace_front(Args&&... args) {
        auto* node = new_node(std::forward<Args>(args)...);
        dllist_insert_after<list_links_t>(std::addressof(head_), node);
        ++size_;
        return node_traits::get_value(node);
    }

    void pop_front() {
        assert(size_);
        auto* p = head_.next;
        dllist_remove(p);
        this->delete_node(p);
        --size_;
    }

    void push_back(const value_type& val) { emplace_back(val); }
    void push_back(value_type&& val) { emplace_back(std::move(val)); }
    template<typename... Args>
    reference emplace_back(Args&&... args) {
        auto* node = new_node(std::forward<Args>(args)...);
        dllist_insert_before<list_links_t>(std::addressof(head_), node);
        ++size_;
        return node_traits::get_value(node);
    }

    void pop_back() {
        assert(size_);
        auto* p = head_.prev;
        dllist_remove(p);
        this->delete_node(p);
        --size_;
    }

    iterator erase(const_iterator pos) {
        auto* p = to_ptr(pos);
        assert(p != std::addressof(head_));
        auto* next = dllist_remove(p);
        this->delete_node(p);
        --size_;
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
        list_links_t* removed = nullptr;
        try {
            for (auto* p = head_.next; p != std::addressof(head_); p = p->next) {
                if (node_traits::get_value(p) == val) {
                    dllist_remove(p);
                    p->prev = removed;
                    --size_, removed = p;
                }
            }
            delete_removed(removed);
            return old_sz - size_;
        } catch (...) {
            delete_removed(removed);
            throw;
        }
    }

    template<typename Pred>
    size_type remove_if(Pred pred) {
        size_type old_sz = size_;
        list_links_t* removed = nullptr;
        try {
            for (auto* p = head_.next; p != std::addressof(head_); p = p->next) {
                if (pred(node_traits::get_value(p))) {
                    dllist_remove(p);
                    p->prev = removed;
                    --size_, removed = p;
                }
            }
            delete_removed(removed);
            return old_sz - size_;
        } catch (...) {
            delete_removed(removed);
            throw;
        }
    }

    size_type unique() { return unique(equal_to_op()); }
    template<typename Pred>
    size_type unique(Pred pred) {
        size_type old_sz = size_;
        list_links_t* removed = nullptr;
        try {
            if (!old_sz) { return 0; }
            for (auto *p0 = head_.next, *p = p0->next; p != std::addressof(head_); p = p->next) {
                if (pred(node_traits::get_value(p0), node_traits::get_value(p))) {
                    dllist_remove(p);
                    p->prev = removed;
                    --size_, removed = p;
                } else {
                    p0 = p;
                }
            }
            delete_removed(removed);
            return old_sz - size_;
        } catch (...) {
            delete_removed(removed);
            throw;
        }
    }

    void reverse() {
        if (!size_) { return; }
        auto* p = static_cast<list_links_t*>(std::addressof(head_));
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

    void merge(list& other) { merge_impl(std::move(other), less_op()); }
    void merge(list&& other) { merge_impl(std::move(other), less_op()); }
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
    void sort() { sort(less_op()); }

 private:
    mutable list_links_t head_;
    size_type size_ = 0;

    bool is_same_alloc(const alloc_type& alloc) { return static_cast<alloc_type&>(*this) == alloc; }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::true_type /* random access iterator */) {
        return first <= last;
    }

    template<typename InputIt>
    static bool check_iterator_range(InputIt first, InputIt last, std::false_type /* random access iterator */) {
        return true;
    }

    struct less_op {
        template<typename Ty1, typename Ty2>
        bool operator()(Ty1& lhs, Ty2& rhs) const {
            return lhs < rhs;
        }
    };

    struct equal_to_op {
        template<typename Ty1, typename Ty2>
        bool operator()(Ty1& lhs, Ty2& rhs) const {
            return lhs == rhs;
        }
    };

    list_links_t* to_ptr(const_iterator it) const {
        auto* node = it.node();
        uxs_iterator_assert(node_traits::get_head(node) == std::addressof(head_));
        return node;
    }

    template<typename... Args>
    list_links_t* new_node(Args&&... args) {
        auto* node = static_cast<list_links_t*>(std::addressof(*alloc_traits::allocate(*this, 1)));
        try {
            alloc_traits::construct(*this, std::addressof(node_traits::get_value(node)), std::forward<Args>(args)...);
            node_traits::set_head(node, std::addressof(head_));
            return node;
        } catch (...) {
            alloc_traits::deallocate(*this, static_cast<typename node_traits::node_t*>(node), 1);
            throw;
        }
    }

    void delete_node(list_links_t* node) {
        alloc_traits::destroy(*this, std::addressof(node_traits::get_value(node)));
        alloc_traits::deallocate(*this, static_cast<typename node_traits::node_t*>(node), 1);
    }

    void init() {
        dllist_make_cycle(std::addressof(head_));
        node_traits::set_head(std::addressof(head_), std::addressof(head_));
    }

    void reset() {
        dllist_make_cycle(std::addressof(head_));
        size_ = 0;
    }

    void tidy() {
        auto* node = head_.next;
        reset();
        while (node != std::addressof(head_)) {
            auto* next = node->next;
            this->delete_node(node);
            node = next;
        }
    }

    void delete_removed(list_links_t* node) {
        while (node) {
            auto* prev = node->prev;
            this->delete_node(node);
            node = prev;
        }
    }

    void steal_data(list& other) {
        if (!other.size_) { return; }
        head_.next = other.head_.next;
        head_.next->prev = std::addressof(head_);
        head_.prev = other.head_.prev;
        head_.prev->next = std::addressof(head_);
        size_ = other.size_;
        other.reset();
        node_traits::set_head(head_.next, std::addressof(head_), std::addressof(head_));
    }

    void construct_impl(list&& other, const allocator_type& alloc, std::true_type) noexcept {
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

    void assign_impl(const list& other, std::true_type) { assign_range(other.begin(), other.end()); }

    void assign_impl(const list& other, std::false_type) {
        if (is_same_alloc(other)) {
            assign_range(other.begin(), other.end());
        } else {
            tidy();
            alloc_type::operator=(other);
            insert_impl(std::addressof(head_), other.begin(), other.end());
        }
    }

    void assign_impl(list&& other, std::true_type) noexcept {
        tidy();
        if (alloc_traits::propagate_on_container_move_assignment::value) { alloc_type::operator=(std::move(other)); }
        steal_data(other);
    }

    void assign_impl(list&& other, std::false_type) {
        if (is_same_alloc(other)) {
            tidy();
            steal_data(other);
        } else {
            assign_range(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
    }

    void swap_impl(list& other, std::true_type) noexcept {
        std::swap(static_cast<alloc_type&>(*this), static_cast<alloc_type&>(other));
        swap_impl(other, std::false_type());
    }

    void swap_impl(list& other, std::false_type) noexcept {
        if (!size_) {
            steal_data(other);
            return;
        } else if (other.size_) {
            std::swap(head_.next, other.head_.next);
            std::swap(head_.prev, other.head_.prev);
            std::swap(head_.next->prev, other.head_.next->prev);
            std::swap(head_.prev->next, other.head_.prev->next);
            std::swap(size_, other.size_);
            node_traits::set_head(head_.next, std::addressof(head_), std::addressof(head_));
        } else {
            dllist_insert_after<list_links_t>(std::addressof(other.head_), head_.next, head_.prev);
            other.size_ = size_;
            reset();
        }
        node_traits::set_head(other.head_.next, std::addressof(other.head_), std::addressof(other.head_));
    }

    template<typename InputIt>
    void assign_range(InputIt first, InputIt last) {
        assert(check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        auto* p = head_.next;
        for (; p != std::addressof(head_) && first != last; ++first) {
            node_traits::get_value(p) = *first;
            p = p->next;
        }
        if (p != std::addressof(head_)) {
            erase_impl(p, std::addressof(head_));
        } else {
            insert_impl(std::addressof(head_), first, last);
        }
    }

    void assign_fill(size_type sz, const value_type& val) {
        auto* p = head_.next;
        for (; p != std::addressof(head_) && sz; --sz) {
            node_traits::get_value(p) = val;
            p = p->next;
        }
        if (p != std::addressof(head_)) {
            erase_impl(p, std::addressof(head_));
        } else {
            insert_fill(std::addressof(head_), sz, val);
        }
    }

    template<typename InputIt>
    list_links_t* insert_impl(list_links_t* pos, InputIt first, InputIt last) {
        assert(check_iterator_range(first, last, is_random_access_iterator<InputIt>()));
        auto* pre_first = pos->prev;
        for (; first != last; ++first) {
            auto* node = new_node(*first);
            dllist_insert_before(pos, node);
            ++size_;
        }
        return pre_first->next;
    }

    list_links_t* insert_fill(list_links_t* pos, size_type sz, const value_type& val) {
        auto* pre_first = pos->prev;
        for (; sz; --sz) {
            auto* node = new_node(val);
            dllist_insert_before(pos, node);
            ++size_;
        }
        return pre_first->next;
    }

    list_links_t* insert_default(list_links_t* pos, size_type sz) {
        auto* pre_first = pos->prev;
        for (; sz; --sz) {
            auto* node = new_node();
            dllist_insert_before(pos, node);
            ++size_;
        }
        return pre_first->next;
    }

    void erase_impl(list_links_t* first, list_links_t* last) {
        assert(first != last);
        dllist_remove(first, last);
        do {
            assert(first != std::addressof(head_));
            auto* next = first->next;
            this->delete_node(first);
            --size_, first = next;
        } while (first != last);
    }

    void splice_impl(const_iterator pos, list&& other);
    void splice_impl(const_iterator pos, list&& other, const_iterator it);
    void splice_impl(const_iterator pos, list&& other, const_iterator first, const_iterator last);

    template<typename Comp>
    void merge_impl(list&& other, Comp comp);
    template<typename Comp>
    void merge_impl(list_links_t* head_tgt, list_links_t* head_src, Comp comp);
};

template<typename Ty, typename Alloc>
void list<Ty, Alloc>::splice_impl(const_iterator pos, list&& other) {
    assert(std::addressof(other) != this || pos == end());
    if (!other.size_ || std::addressof(other) == this) { return; }
    if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
        throw std::logic_error("allocators incompatible for splice");
    }
    node_traits::set_head(other.head_.next, std::addressof(other.head_), std::addressof(head_));
    dllist_insert_before(to_ptr(pos), other.head_.next, other.head_.prev);
    size_ += other.size_;
    other.reset();
}

template<typename Ty, typename Alloc>
void list<Ty, Alloc>::splice_impl(const_iterator pos, list&& other, const_iterator it) {
    auto* p = other.to_ptr(it);
    if (std::addressof(other) != this) {
        if (!is_alloc_always_equal<alloc_type>::value && !is_same_alloc(other)) {
            throw std::logic_error("allocators incompatible for splice");
        }
        node_traits::set_head(p, std::addressof(head_));
        ++size_, --other.size_;
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
            node_traits::set_head(p, std::addressof(head_));
            p = p->next;
            ++count;
        } while (p != p_last);
        size_ += count, other.size_ -= count;
    } else {
        if (last == pos) { return; }
#if !defined(NDEBUG)
        for (auto it = first; it != last; ++it) { assert(it != pos); }
#endif  // !defined(NDEBUG)
    }
    auto* pre_last = p_last->prev;
    dllist_remove(p_first, p_last);
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
    node_traits::set_head(head_src->next, head_src, std::addressof(head_));
    size_ += other.size_;
    other.size_ = 0;
    try {
        merge_impl(std::addressof(head_), head_src, comp);
    } catch (...) {
        dllist_insert_before<list_links_t>(std::addressof(head_), head_src->next, head_src->prev);
        dllist_make_cycle(head_src);
        throw;
    }
}

template<typename Ty, typename Alloc>
template<typename Comp>
void list<Ty, Alloc>::merge_impl(list_links_t* head_tgt, list_links_t* head_src, Comp comp) {
    auto* p_first = head_src->next;
    auto* p_last = p_first;
    for (auto* p = head_tgt->next; p != head_tgt && p_last != head_src; p = p->next) {
        while (p_last != head_src && comp(node_traits::get_value(p_last), node_traits::get_value(p))) {
            p_last = p_last->next;
        }
        if (p_first != p_last) {
            auto* pre_last = p_last->prev;
            dllist_remove(p_first, p_last);
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
    list_links_t tmp_list, bin_lists[max_bins];

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
        dllist_insert_before<list_links_t>(std::addressof(head_), bin_lists[maxbin - 1].next,
                                           bin_lists[maxbin - 1].prev);
    } catch (...) {
        // collect all stuff
        dllist_insert_before<list_links_t>(std::addressof(head_), tmp_list.next, tmp_list.prev);
        for (size_type bin = 0; bin < maxbin; ++bin) {
            dllist_insert_before<list_links_t>(std::addressof(head_), bin_lists[bin].next, bin_lists[bin].prev);
        }
        throw;
    }
}

#if __cplusplus >= 201703L
template<typename InputIt, typename Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>,
         typename = std::enable_if_t<is_allocator<Alloc>::value>>
list(InputIt, InputIt, Alloc = Alloc()) -> list<typename std::iterator_traits<InputIt>::value_type, Alloc>;
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

}  // namespace uxs

namespace std {
template<typename Ty, typename Alloc>
void swap(uxs::list<Ty, Alloc>& l1, uxs::list<Ty, Alloc>& l2) noexcept(noexcept(l1.swap(l2))) {
    l1.swap(l2);
}
}  // namespace std
