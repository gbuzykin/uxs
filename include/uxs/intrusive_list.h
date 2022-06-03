#pragma once

#include "dllist.h"
#include "iterator.h"

namespace uxs {

struct intrusive_list_base_hook_t : dllist_node_t {
    static dllist_node_t* get_next(dllist_node_t* node) { return node->next; }
    static dllist_node_t* get_prev(dllist_node_t* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) {
        static_cast<intrusive_list_base_hook_t*>(node)->head = head;
    }
    static dllist_node_t* get_head(dllist_node_t* node) { return static_cast<intrusive_list_base_hook_t*>(node)->head; }
    static dllist_node_t* get_front(dllist_node_t* head) { return head->next; }
    dllist_node_t* head;
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Ty, typename PtrTy>
struct intrusive_list_hook_t : intrusive_list_base_hook_t {
    using iterator_node_t = dllist_node_t;
    using internal_pointer_t = PtrTy;
    static Ty& get_value(dllist_node_t* node) { return *static_cast<intrusive_list_hook_t*>(node)->ptr; }
    internal_pointer_t ptr;
};

template<typename Ty, typename HookTy, HookTy Ty::*PtrToMember>
class intrusive_list {
 public:
    using value_type = Ty;
    using hook_t = HookTy;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = Ty&;
    using const_reference = const Ty&;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using iterator = list_iterator<intrusive_list, hook_t, false>;
    using const_iterator = list_iterator<intrusive_list, hook_t, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    intrusive_list() {
        dllist_make_cycle(&head_);
        intrusive_list_base_hook_t::set_head(&head_, &head_);
    }

    intrusive_list(const intrusive_list&) = delete;
    intrusive_list& operator=(const intrusive_list&) = delete;
    ~intrusive_list() { clear(); }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }

    iterator begin() { return iterator(head_.next); }
    const_iterator begin() const { return const_iterator(head_.next); }
    const_iterator cbegin() const { return const_iterator(head_.next); }

    iterator end() { return iterator(std::addressof(head_)); }
    const_iterator end() const { return const_iterator(std::addressof(head_)); }
    const_iterator cend() const { return const_iterator(std::addressof(head_)); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    reference front() {
        assert(size_);
        return hook_t::get_value(head_.next);
    }
    const_reference front() const {
        assert(size_);
        return hook_t::get_value(head_.next);
    }

    reference back() {
        assert(size_);
        return hook_t::get_value(head_.prev);
    }
    const_reference back() const {
        assert(size_);
        return hook_t::get_value(head_.prev);
    }

    void clear() {
        auto* item = head_.next;
        while (item != &head_) {
            auto* next = item->next;
            intrusive_list_base_hook_t::set_head(item, nullptr);
            static_cast<hook_t*>(item)->ptr = nullptr;
            item = next;
        }
        size_ = 0;
        dllist_make_cycle(&head_);
    }

    iterator insert(const_iterator pos, typename hook_t::internal_pointer_t ptr) {
        auto* item = &((*ptr).*PtrToMember);
        item->ptr = std::move(ptr);
        intrusive_list_base_hook_t::set_head(item, &head_);
        auto* after = pos.node();
        iterator_assert(intrusive_list_base_hook_t::get_head(after) == &head_);
        dllist_insert_before<dllist_node_t>(after, item);
        ++size_;
        return iterator(item);
    }

    iterator erase(const_iterator pos) {
        auto* item = pos.node();
        iterator_assert(intrusive_list_base_hook_t::get_head(item) == &head_);
        assert(item != &head_);
        --size_;
        auto* next = dllist_remove(item);
        intrusive_list_base_hook_t::set_head(item, nullptr);
        static_cast<hook_t*>(item)->ptr = nullptr;
        return iterator(next);
    }

    reference push_front(typename hook_t::internal_pointer_t ptr) { return *insert(begin(), std::move(ptr)); }
    reference push_back(typename hook_t::internal_pointer_t ptr) { return *insert(end(), std::move(ptr)); }
    void pop_front() { erase(begin()); }
    void pop_back() { erase(std::prev(end())); }

    typename hook_t::internal_pointer_t extract(const_iterator pos) {
        auto* item = pos.node();
        iterator_assert(intrusive_list_base_hook_t::get_head(item) == &head_);
        assert(item != &head_);
        --size_;
        dllist_remove(item);
        intrusive_list_base_hook_t::set_head(item, nullptr);
        return std::move(static_cast<hook_t*>(item)->ptr);
    }

    typename hook_t::internal_pointer_t extract_front() { return extract(begin()); }
    typename hook_t::internal_pointer_t extract_back() { return extract(std::prev(end())); }

    const_iterator to_iterator(const_pointer ptr) const { return const_iterator(&((*ptr).*PtrToMember)); }
    iterator to_iterator(pointer ptr) { return iterator(&((*ptr).*PtrToMember)); }

 private:
    size_t size_ = 0;
    intrusive_list_base_hook_t head_;
};

}  // namespace uxs
