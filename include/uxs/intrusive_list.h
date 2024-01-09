#pragma once

#include "dllist.h"
#include "iterator.h"

namespace uxs {

namespace detail {

struct intrusive_list_base_hook_t : dllist_node_t {
#if _ITERATOR_DEBUG_LEVEL != 0
    dllist_node_t* head;
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Ty, typename HookTy>
struct intrusive_list_node_traits {
    using iterator_node_t = dllist_node_t;
    using links_t = intrusive_list_base_hook_t;
    static dllist_node_t* get_next(dllist_node_t* node) { return node->next; }
    static dllist_node_t* get_prev(dllist_node_t* node) { return node->prev; }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) { static_cast<links_t*>(node)->head = head; }
    static dllist_node_t* get_head(dllist_node_t* node) { return static_cast<links_t*>(node)->head; }
    static dllist_node_t* get_front(dllist_node_t* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    static Ty& get_value(dllist_node_t* node) { return *static_cast<HookTy*>(node)->ptr; }
};

}  // namespace detail

template<typename Ty, typename PtrTy>
struct intrusive_list_hook_t : detail::intrusive_list_base_hook_t {
    using internal_pointer_t = PtrTy;
    internal_pointer_t ptr;
};

template<typename Ty, typename HookTy, HookTy Ty::*PtrToMember>
class intrusive_list {
 private:
    using node_traits = detail::intrusive_list_node_traits<Ty, HookTy>;

 public:
    using value_type = Ty;
    using hook_t = HookTy;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = Ty&;
    using const_reference = const Ty&;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using iterator = list_iterator<intrusive_list, node_traits, false>;
    using const_iterator = list_iterator<intrusive_list, node_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    intrusive_list() {
        dllist_make_cycle(&head_);
        node_traits::set_head(&head_, &head_);
    }

    intrusive_list(const intrusive_list&) = delete;
    intrusive_list& operator=(const intrusive_list&) = delete;
    ~intrusive_list() { clear(); }

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    iterator begin() { return iterator(head_.next); }
    const_iterator begin() const { return const_iterator(head_.next); }
    const_iterator cbegin() const { return const_iterator(head_.next); }

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

    void clear() {
        auto* item = head_.next;
        while (item != &head_) {
            auto* next = item->next;
            node_traits::set_head(item, nullptr);
            static_cast<hook_t*>(item)->ptr = nullptr;
            item = next;
        }
        size_ = 0;
        dllist_make_cycle(&head_);
    }

    iterator insert(const_iterator pos, typename hook_t::internal_pointer_t ptr) {
        auto* item = &((*ptr).*PtrToMember);
        item->ptr = std::move(ptr);
        node_traits::set_head(item, &head_);
        auto* after = pos.node();
        iterator_assert(node_traits::get_head(after) == &head_);
        dllist_insert_before<dllist_node_t>(after, item);
        ++size_;
        return iterator(item);
    }

    iterator erase(const_iterator pos) {
        auto* item = pos.node();
        iterator_assert(node_traits::get_head(item) == &head_);
        assert(item != &head_);
        --size_;
        auto* next = dllist_remove(item);
        node_traits::set_head(item, nullptr);
        static_cast<hook_t*>(item)->ptr = nullptr;
        return iterator(next);
    }

    reference push_front(typename hook_t::internal_pointer_t ptr) { return *insert(begin(), std::move(ptr)); }
    reference push_back(typename hook_t::internal_pointer_t ptr) { return *insert(end(), std::move(ptr)); }
    void pop_front() { erase(begin()); }
    void pop_back() { erase(std::prev(end())); }

    typename hook_t::internal_pointer_t extract(const_iterator pos) {
        auto* item = pos.node();
        iterator_assert(node_traits::get_head(item) == &head_);
        assert(item != &head_);
        --size_;
        dllist_remove(item);
        node_traits::set_head(item, nullptr);
        return std::move(static_cast<hook_t*>(item)->ptr);
    }

    typename hook_t::internal_pointer_t extract_front() { return extract(begin()); }
    typename hook_t::internal_pointer_t extract_back() { return extract(std::prev(end())); }

    const_iterator to_iterator(const_reference r) const noexcept { return const_iterator(&(r.*PtrToMember)); }
    iterator to_iterator(reference r) noexcept { return iterator(&(r.*PtrToMember)); }

 private:
    size_t size_ = 0;
    mutable typename node_traits::links_t head_;
};

}  // namespace uxs
