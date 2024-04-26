#pragma once

#include "common.h"

#include <utility>

namespace uxs {

struct rbtree_node_t {
    rbtree_node_t* left;
    rbtree_node_t* parent;
    rbtree_node_t* right;
    enum class color_t : char { black = 0, red = 1 } color;
};

inline bool rbtree_is_empty(const rbtree_node_t* head) noexcept { return head->left == nullptr; }

inline void rbtree_init_head(rbtree_node_t* head) noexcept {
    head->left = nullptr;
    head->right = head->parent = head;
    head->color = rbtree_node_t::color_t::black;
}

inline rbtree_node_t* rbtree_right_bound(rbtree_node_t* node) noexcept {
    while (node->right) { node = node->right; }
    return node;
}

inline rbtree_node_t* rbtree_left_bound(rbtree_node_t* node) noexcept {
    while (node->left) { node = node->left; }
    return node;
}

UXS_EXPORT NOALIAS rbtree_node_t* rbtree_next(rbtree_node_t* node) noexcept;
UXS_EXPORT NOALIAS rbtree_node_t* rbtree_prev(rbtree_node_t* node) noexcept;

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto* pos = head->left;
    if (!pos) { return std::make_pair(head, -1); }
    while (true) {
        if (comp(k, Traits::get_key(Traits::get_value(pos)))) {
            if (pos->left) {
                pos = pos->left;
            } else {
                return std::make_pair(pos, -1);
            }
        } else if (pos->right) {
            pos = pos->right;
        } else {
            return std::make_pair(pos, 1);
        }
    }
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_leftish_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto* pos = head->left;
    if (!pos) { return std::make_pair(head, -1); }
    while (true) {
        if (!comp(Traits::get_key(Traits::get_value(pos)), k)) {
            if (pos->left) {
                pos = pos->left;
            } else {
                return std::make_pair(pos, -1);
            }
        } else if (pos->right) {
            pos = pos->right;
        } else {
            return std::make_pair(pos, 1);
        }
    }
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_pos(rbtree_node_t* head, rbtree_node_t* hint, const Key& k,
                                                      const Comp& comp) {
    if (hint == head) {
        if (!head->left) { return std::make_pair(head, -1); }
        if (!comp(k, Traits::get_key(Traits::get_value(head->right)))) { return std::make_pair(head->right, 1); }
    } else if (!comp(Traits::get_key(Traits::get_value(hint)), k)) {
        if (hint == head->parent) { return std::make_pair(hint, -1); }
        auto* prev = rbtree_prev(hint);
        if (!comp(k, Traits::get_key(Traits::get_value(prev)))) {
            if (!prev->right) { return std::make_pair(prev, 1); }
            return std::make_pair(hint, -1);
        }
    } else if (hint == head->right) {
        return std::make_pair(hint, 1);
    } else {
        auto* next = rbtree_next(hint);
        if (comp(Traits::get_key(Traits::get_value(next)), k)) {
            return rbtree_find_insert_leftish_pos<Traits>(head, k, comp);
        }
        if (!next->left) { return std::make_pair(next, -1); }
        return std::make_pair(hint, 1);
    }
    return rbtree_find_insert_pos<Traits>(head, k, comp);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_unique_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto* pos = head->left;
    if (!pos) { return std::make_pair(head, -1); }
    while (true) {
        if (comp(k, Traits::get_key(Traits::get_value(pos)))) {
            if (pos->left) {
                pos = pos->left;
            } else {
                if (pos != head->parent) {
                    auto* prev = rbtree_prev(pos);
                    if (!comp(Traits::get_key(Traits::get_value(prev)), k)) { return std::make_pair(prev, 0); }
                }
                return std::make_pair(pos, -1);
            }
        } else if (pos->right) {
            pos = pos->right;
        } else {
            if (!comp(Traits::get_key(Traits::get_value(pos)), k)) { return std::make_pair(pos, 0); }
            return std::make_pair(pos, 1);
        }
    }
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_unique_pos(rbtree_node_t* head, rbtree_node_t* hint, const Key& k,
                                                             const Comp& comp) {
    if (hint == head) {
        if (!head->left) { return std::make_pair(head, -1); }
        if (comp(Traits::get_key(Traits::get_value(head->right)), k)) { return std::make_pair(head->right, 1); }
    } else if (comp(k, Traits::get_key(Traits::get_value(hint)))) {
        if (hint == head->parent) { return std::make_pair(hint, -1); }
        auto* prev = rbtree_prev(hint);
        if (comp(Traits::get_key(Traits::get_value(prev)), k)) {
            if (!prev->right) { return std::make_pair(prev, 1); }
            return std::make_pair(hint, -1);
        }
    } else if (comp(Traits::get_key(Traits::get_value(hint)), k)) {
        if (hint == head->right) { return std::make_pair(hint, 1); }
        auto* next = rbtree_next(hint);
        if (comp(k, Traits::get_key(Traits::get_value(next)))) {
            if (!next->left) { return std::make_pair(next, -1); }
            return std::make_pair(hint, 1);
        }
    } else {
        return std::make_pair(hint, 0);
    }
    return rbtree_find_insert_unique_pos<Traits>(head, k, comp);
}

namespace detail {
template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_lower_bound(rbtree_node_t* node, rbtree_node_t* head, const Key& k, const Comp& comp) {
    while (node) {
        if (!comp(Traits::get_key(Traits::get_value(node)), k)) {
            head = node, node = node->left;
        } else {
            node = node->right;
        }
    }
    return head;
}
template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_upper_bound(rbtree_node_t* node, rbtree_node_t* head, const Key& k, const Comp& comp) {
    while (node) {
        if (comp(k, Traits::get_key(Traits::get_value(node)))) {
            head = node, node = node->left;
        } else {
            node = node->right;
        }
    }
    return head;
}
}  // namespace detail

template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_lower_bound(rbtree_node_t* head, const Key& k, const Comp& comp) {
    return detail::rbtree_lower_bound<Traits>(head->left, head, k, comp);
}

template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_upper_bound(rbtree_node_t* head, const Key& k, const Comp& comp) {
    return detail::rbtree_upper_bound<Traits>(head->left, head, k, comp);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, rbtree_node_t*> rbtree_equal_range(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto* node = head->left;
    while (node) {
        if (comp(k, Traits::get_key(Traits::get_value(node)))) {
            head = node, node = node->left;
        } else if (comp(Traits::get_key(Traits::get_value(node)), k)) {
            node = node->right;
        } else {
            return std::make_pair(detail::rbtree_lower_bound<Traits>(node->left, node, k, comp),
                                  detail::rbtree_upper_bound<Traits>(node->right, head, k, comp));
        }
    }
    return std::make_pair(head, head);
}

UXS_EXPORT void rbtree_insert(rbtree_node_t* head, rbtree_node_t* node, rbtree_node_t* pos, int dir) noexcept;
UXS_EXPORT rbtree_node_t* rbtree_remove(rbtree_node_t* head, rbtree_node_t* pos) noexcept;

}  // namespace uxs
