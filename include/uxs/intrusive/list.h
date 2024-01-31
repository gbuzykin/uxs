#pragma once

#include "uxs/dllist.h"
#include "uxs/iterator.h"

namespace uxs {
namespace intrusive {

struct list_links_t : dllist_node_t {
#if _ITERATOR_DEBUG_LEVEL != 0
    dllist_node_t* head;
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

template<typename Ty, typename HookTy, typename... Opts>
struct list_hook_traits;

template<typename... Opts>
struct list_hook_getter;

template<typename StaticCaster>
struct opt_hook_caster;

template<typename HookTy, typename InternalPtrTy, InternalPtrTy HookTy::*>
struct opt_internal_pointer;

template<typename ParentTy, typename HookTy, HookTy ParentTy::*>
struct opt_member_hook;

template<typename Ty, typename HookTy>
struct list_hook_traits<Ty, HookTy> {
    using hook_t = HookTy;
    using holding_pointer_t = std::remove_cv_t<Ty>*;
    using is_release_modifying = std::false_type;
    static Ty& get_value(hook_t* h) { return *static_cast<holding_pointer_t>(h); }
    static void hold(hook_t* h, holding_pointer_t p) {}
    static holding_pointer_t release(hook_t* h) { return static_cast<holding_pointer_t>(h); }
};

template<typename Ty, typename HookTy, typename Caster>
struct list_hook_traits<Ty, HookTy, opt_hook_caster<Caster>> {
    using hook_t = HookTy;
    using holding_pointer_t = decltype(Caster::cast(nullptr));
    using is_release_modifying = std::false_type;
    static Ty& get_value(hook_t* h) { return *Caster::cast(h); }
    static void hold(hook_t* h, holding_pointer_t p) {}
    static holding_pointer_t release(hook_t* h) { return Caster::cast(h); }
};

template<typename Ty, typename HookTy, typename InternalPtrTy, InternalPtrTy HookTy::*InternalPointerMember>
struct list_hook_traits<Ty, HookTy, opt_internal_pointer<HookTy, InternalPtrTy, InternalPointerMember>> {
    using hook_t = HookTy;
    using holding_pointer_t = InternalPtrTy;
    static Ty& get_value(hook_t* h) { return *(h->*InternalPointerMember); }
    static void hold(hook_t* h, holding_pointer_t p) { h->*InternalPointerMember = std::move(p); }
    static holding_pointer_t release(hook_t* h) {
        holding_pointer_t p = std::move(h->*InternalPointerMember);
        h->*InternalPointerMember = nullptr;
        return p;
    }
};

template<typename ParentTy, typename HookTy, HookTy ParentTy::*HookMember>
struct list_hook_getter<opt_member_hook<ParentTy, HookTy, HookMember>> {
    static HookTy* get_hook(ParentTy& parent) { return &(parent.*HookMember); }
};

namespace detail {

template<typename Ty, typename HookTraits>
struct list_node_traits {
    using iterator_node_t = dllist_node_t;
    using links_t = list_links_t;
    static dllist_node_t* get_next(dllist_node_t* node) { return node->next; }
    static dllist_node_t* get_prev(dllist_node_t* node) { return node->prev; }
    static Ty& get_value(dllist_node_t* node) {
        return HookTraits::get_value(static_cast<typename HookTraits::hook_t*>(node));
    }
#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) { static_cast<links_t*>(node)->head = head; }
    static dllist_node_t* get_head(dllist_node_t* node) { return static_cast<links_t*>(node)->head; }
    static dllist_node_t* get_front(dllist_node_t* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(dllist_node_t* node, dllist_node_t* head) {}
#endif  // _ITERATOR_DEBUG_LEVEL != 0
};

}  // namespace detail

template<typename, typename, typename>
class list;

template<typename Ty, typename HookTraits>
class list_enumerator {
 private:
    using node_traits = detail::list_node_traits<Ty, HookTraits>;

 public:
    using value_type = Ty;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = list_iterator<list_enumerator, node_traits, false>;
    using const_iterator = list_iterator<list_enumerator, node_traits, true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    list_enumerator() {
        dllist_make_cycle(&head_);
        node_traits::set_head(&head_, &head_);
    }

    list_enumerator(const list_enumerator&) = delete;
    list_enumerator& operator=(const list_enumerator&) = delete;

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

 private:
    template<typename, typename, typename>
    friend class list;

    size_t size_ = 0;
    mutable typename node_traits::links_t head_;
};

template<typename Ty, typename HookTraits = list_hook_traits<Ty, list_links_t>, typename HookGetter = void>
class list : public list_enumerator<Ty, HookTraits> {
 private:
    using hook_traits = HookTraits;
    using hook_t = typename HookTraits::hook_t;
    using holding_pointer_t = typename hook_traits::holding_pointer_t;
    using node_traits = typename list_enumerator<Ty, HookTraits>::node_traits;
    using parent_t = std::remove_reference_t<decltype(*std::declval<holding_pointer_t>())>;

 public:
    using reference = typename list_enumerator<Ty, HookTraits>::reference;
    using const_reference = typename list_enumerator<Ty, HookTraits>::const_reference;
    using iterator = typename list_enumerator<Ty, HookTraits>::iterator;
    using const_iterator = typename list_enumerator<Ty, HookTraits>::const_iterator;

    ~list() { release_all(); }

    void clear() {
        release_all();
        this->size_ = 0;
        dllist_make_cycle(&this->head_);
    }

    iterator insert(const_iterator pos, holding_pointer_t ptr) {
        auto* item = get_hook(*ptr);
        hook_traits::hold(item, std::move(ptr));
        node_traits::set_head(item, &this->head_);
        auto* after = pos.node();
        iterator_assert(node_traits::get_head(after) == &this->head_);
        ++this->size_;
        dllist_insert_before<dllist_node_t>(after, item);
        return iterator(item);
    }

    reference push_front(holding_pointer_t ptr) { return *insert(this->begin(), std::move(ptr)); }
    reference push_back(holding_pointer_t ptr) { return *insert(this->end(), std::move(ptr)); }

    std::pair<iterator, holding_pointer_t> extract(const_iterator pos) {
        auto* item = pos.node();
        assert(item != &this->head_);
        iterator_assert(node_traits::get_head(item) == &this->head_);
        --this->size_;
        auto* next = dllist_remove(item);
        node_traits::set_head(item, nullptr);
        return std::make_pair(iterator(next), hook_traits::release(static_cast<hook_t*>(item)));
    }

    holding_pointer_t extract_front() { return extract(this->begin()).second; }
    holding_pointer_t extract_back() { return extract(std::prev(this->end())).second; }
    iterator erase(const_iterator pos) { return extract(pos).first; }
    void pop_front() { extract(this->begin()); }
    void pop_back() { extract(std::prev(this->end())); }

    static const_iterator to_iterator(const parent_t& p) noexcept {
        return const_iterator(get_hook(const_cast<parent_t&>(p)));
    }
    static iterator to_iterator(parent_t& p) noexcept { return iterator(get_hook(p)); }

 private:
    template<typename ParentTy, typename HookGetter_ = HookGetter,
             typename = std::enable_if_t<!std::is_same<HookGetter_, void>::value>>
    static hook_t* get_hook(ParentTy& parent) {
        return HookGetter::get_hook(parent);
    }
    template<typename ParentTy, typename... Dummy>
    static hook_t* get_hook(ParentTy& parent, Dummy&&...) {
        return &parent;
    }

    template<typename HookTraits_ = HookTraits, typename = std::enable_if_t<HookTraits_::is_release_modifying::value>>
    void release_all() {
#if _ITERATOR_DEBUG_LEVEL != 0
        auto* item = this->head_.next;
        while (item != &this->head_) {
            auto* next = item->next;
            node_traits::set_head(item, nullptr);
            item = next;
        }
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    }
    template<typename... Dummy>
    void release_all(Dummy&&...) {
        auto* item = this->head_.next;
        while (item != &this->head_) {
            auto* next = item->next;
            node_traits::set_head(item, nullptr);
            hook_traits::release(static_cast<hook_t*>(item));
            item = next;
        }
    }
};

}  // namespace intrusive
}  // namespace uxs
