#pragma once

#include "uxs/dllist.h"
#include "uxs/iterator.h"

namespace uxs {
namespace intrusive {

struct list_links_t {
    list_links_t* next;
    list_links_t* prev;
#if _ITERATOR_DEBUG_LEVEL != 0
    list_links_t* head;
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
    using owning_pointer_t = std::remove_cv_t<Ty>*;
    Ty& get_value(hook_t* h) const { return *static_cast<owning_pointer_t>(h); }
    owning_pointer_t release_pointer(hook_t* h) const { return static_cast<owning_pointer_t>(h); }
};

template<typename Ty, typename HookTy, typename Caster>
struct list_hook_traits<Ty, HookTy, opt_hook_caster<Caster>> {
    using hook_t = HookTy;
    using owning_pointer_t = decltype(Caster{}.cast(nullptr));
    Ty& get_value(hook_t* h) const { return *Caster{}.cast(h); }
    owning_pointer_t release_pointer(hook_t* h) const { return Caster{}.cast(h); }
};

template<typename Ty, typename HookTy, typename InternalPtrTy, InternalPtrTy HookTy::*InternalPointerMember>
struct list_hook_traits<Ty, HookTy, opt_internal_pointer<HookTy, InternalPtrTy, InternalPointerMember>> {
    using hook_t = HookTy;
    using owning_pointer_t = InternalPtrTy;
    Ty& get_value(hook_t* h) const { return *(h->*InternalPointerMember); }
    owning_pointer_t release_pointer(hook_t* h) const { return std::move(h->*InternalPointerMember); }
    void reset_pointer(hook_t* h, owning_pointer_t p) const { h->*InternalPointerMember = std::move(p); }
};

template<typename ParentTy, typename HookTy, HookTy ParentTy::*HookMember>
struct list_hook_getter<opt_member_hook<ParentTy, HookTy, HookMember>> {
    HookTy* get_hook(ParentTy* parent) const { return std::addressof(parent->*HookMember); }
};

namespace detail {

template<typename Ty, typename HookTraits>
struct list_node_traits {
    using iterator_node_t = list_links_t;
    using hook_t = typename HookTraits::hook_t;
    using owning_pointer_t = typename HookTraits::owning_pointer_t;
    static list_links_t* get_next(list_links_t* node) { return node->next; }
    static list_links_t* get_prev(list_links_t* node) { return node->prev; }
    static Ty& get_value(list_links_t* node) { return HookTraits{}.get_value(static_cast<hook_t*>(node)); }

    static auto release_pointer(hook_t* h) -> decltype(HookTraits{}.release_pointer(h)) {
        return HookTraits{}.release_pointer(h);
    }

    template<typename HookTraits_ = HookTraits>
    static auto reset_pointer(hook_t* h, owning_pointer_t p) -> decltype(HookTraits_{}.reset_pointer(h, std::move(p)),
                                                                         int{}) {
        HookTraits{}.reset_pointer(h, std::move(p));
        return 0;
    }
    template<typename... Dummy>
    static void reset_pointer(hook_t* h, owning_pointer_t p, Dummy&&...) {}

    template<typename HookTraits_ = HookTraits>
    static auto dispose(owning_pointer_t p) -> decltype(HookTraits_{}.dispose(std::move(p)), int{}) {
        HookTraits{}.dispose(std::move(p));
        return 0;
    }
    template<typename... Dummy>
    static void dispose(Dummy&&...) {}

    using has_reset_pointer = std::is_same<decltype(reset_pointer(nullptr, std::declval<owning_pointer_t>())), int>;
    using has_dispose = std::is_same<decltype(dispose(std::declval<owning_pointer_t>())), int>;

    template<typename Traits = list_node_traits,
             typename = std::enable_if_t<Traits::has_reset_pointer::value || Traits::has_dispose::value>>
    static void dispose_all(list_links_t* head) {
        auto* item = head->next;
        while (item != head) {
            auto* next = item->next;
            set_head(item, nullptr);
            auto p = HookTraits{}.release_pointer(static_cast<hook_t*>(item));
            reset_pointer(static_cast<hook_t*>(item), nullptr);
            dispose(std::move(p));
            item = next;
        }
    }
    template<typename... Dummy>
    static void dispose_all(list_links_t* head, Dummy&&...) {
#if _ITERATOR_DEBUG_LEVEL != 0
        auto* item = head->next;
        while (item != head) {
            auto* next = item->next;
            set_head(item, nullptr);
            item = next;
        }
#endif  // _ITERATOR_DEBUG_LEVEL != 0
    }

#if _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) { node->head = head; }
    static list_links_t* get_head(list_links_t* node) { return node->head; }
    static list_links_t* get_front(list_links_t* head) { return head->next; }
#else   // _ITERATOR_DEBUG_LEVEL != 0
    static void set_head(list_links_t* node, list_links_t* head) {}
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

    list_enumerator() noexcept {
        dllist_make_cycle(&head_);
        node_traits::set_head(&head_, &head_);
    }

    list_enumerator(const list_enumerator&) = delete;
    list_enumerator& operator=(const list_enumerator&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    size_type size() const noexcept { return size_; }

    iterator begin() { return iterator(head_.next); }
    const_iterator begin() const { return const_iterator(head_.next); }
    const_iterator cbegin() const { return begin(); }

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

 private:
    template<typename, typename, typename>
    friend class list;

    std::size_t size_ = 0;
    mutable list_links_t head_;
};

template<typename Ty, typename HookTraits = list_hook_traits<Ty, list_links_t>, typename HookGetter = void>
class list : public list_enumerator<Ty, HookTraits> {
 private:
    using hook_t = typename HookTraits::hook_t;
    using node_traits = typename list_enumerator<Ty, HookTraits>::node_traits;

 public:
    using reference = typename list_enumerator<Ty, HookTraits>::reference;
    using const_reference = typename list_enumerator<Ty, HookTraits>::const_reference;
    using iterator = typename list_enumerator<Ty, HookTraits>::iterator;
    using const_iterator = typename list_enumerator<Ty, HookTraits>::const_iterator;
    using owning_pointer_t = typename HookTraits::owning_pointer_t;
    using parent_object_t = std::remove_reference_t<decltype(*std::declval<owning_pointer_t>())>;

    ~list() { node_traits::dispose_all(&this->head_); }

    void clear() noexcept {
        node_traits::dispose_all(&this->head_);
        this->size_ = 0;
        dllist_make_cycle(&this->head_);
    }

    iterator insert(const_iterator pos, owning_pointer_t obj) {
        auto* item = get_hook(std::addressof(*obj));
        node_traits::reset_pointer(item, std::move(obj));
        node_traits::set_head(item, &this->head_);
        auto* after = pos.node();
        uxs_iterator_assert(node_traits::get_head(after) == &this->head_);
        ++this->size_;
        dllist_insert_before<list_links_t>(after, item);
        return iterator(item);
    }

    reference push_front(owning_pointer_t obj) { return *insert(this->begin(), std::move(obj)); }
    reference push_back(owning_pointer_t obj) { return *insert(this->end(), std::move(obj)); }

    std::pair<owning_pointer_t, iterator> extract(const_iterator pos) {
        auto* item = pos.node();
        assert(item != &this->head_);
        uxs_iterator_assert(node_traits::get_head(item) == &this->head_);
        --this->size_;
        auto* next = dllist_remove(item);
        node_traits::set_head(item, nullptr);
        auto obj = node_traits::release_pointer(static_cast<hook_t*>(item));
        node_traits::reset_pointer(static_cast<hook_t*>(item), nullptr);
        return std::make_pair(std::move(obj), iterator(next));
    }

    owning_pointer_t extract_front() { return extract(this->begin()).first; }
    owning_pointer_t extract_back() { return extract(std::prev(this->end())).first; }
    void pop_front() { node_traits::dispose(extract(this->begin()).first); }
    void pop_back() { node_traits::dispose(extract(std::prev(this->end())).first); }
    iterator erase(const_iterator pos) {
        auto result = extract(pos);
        node_traits::dispose(std::move(result.first));
        return result.second;
    }

    static const_iterator to_iterator(const parent_object_t* obj) noexcept {
        return const_iterator(get_hook(const_cast<parent_object_t*>(obj)));
    }
    static iterator to_iterator(parent_object_t* obj) noexcept { return iterator(get_hook(obj)); }

 private:
    template<typename ParentTy, typename HookGetter_ = HookGetter>
    static auto get_hook(ParentTy* obj) -> decltype(HookGetter_{}.get_hook(obj)) {
        return HookGetter{}.get_hook(obj);
    }
    template<typename ParentTy, typename... Dummy>
    static hook_t* get_hook(ParentTy* obj, Dummy&&...) {
        return obj;
    }
};

}  // namespace intrusive
}  // namespace uxs
