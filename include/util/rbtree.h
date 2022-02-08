#pragma once

#include "common.h"

namespace util {

//-----------------------------------------------------------------------------
// Red-black tree functions

struct rbtree_node_t {
    rbtree_node_t* left;
    rbtree_node_t* parent;
    rbtree_node_t* right;
    enum class color_t : char { kBlack = 0, kRed = 1 } color;
};

inline bool rbtree_is_empty(const rbtree_node_t* head) { return head->left == nullptr; }

inline void rbtree_init_head(rbtree_node_t* head) {
    head->left = nullptr;
    head->right = head->parent = head;
    head->color = rbtree_node_t::color_t::kBlack;
}

inline rbtree_node_t* rbtree_right_bound(rbtree_node_t* node) {
    while (node->right) { node = node->right; }
    return node;
}

inline rbtree_node_t* rbtree_left_bound(rbtree_node_t* node) {
    while (node->left) { node = node->left; }
    return node;
}

inline rbtree_node_t* rbtree_right_parent(rbtree_node_t* node) {
    auto parent = node->parent;
    while (node != parent->left) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

inline rbtree_node_t* rbtree_left_parent(rbtree_node_t* node) {
    auto parent = node->parent;
    while (node == parent->left) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

inline rbtree_node_t* rbtree_next(rbtree_node_t* node) {
    if (node->right) { return rbtree_left_bound(node->right); }
    return rbtree_right_parent(node);
}

inline rbtree_node_t* rbtree_prev(rbtree_node_t* node) {
    if (node->left) { return rbtree_right_bound(node->left); }
    return rbtree_left_parent(node);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, bool> rbtree_find_insert_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto pos = head->left;
    if (!pos) { return std::make_pair(head, true); }

    while (true) {
        if (comp(k, Traits::get_key(Traits::get_value(pos)))) {
            if (!pos->left) { return std::make_pair(pos, true); }
            pos = pos->left;
        } else {
            if (!pos->right) { break; }
            pos = pos->right;
        }
    }

    return std::make_pair(pos, false);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, bool> rbtree_find_insert_leftish_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto pos = head->left;
    if (!pos) { return std::make_pair(head, true); }

    while (true) {
        if (!comp(Traits::get_key(Traits::get_value(pos)), k)) {
            if (!pos->left) { return std::make_pair(pos, true); }
            pos = pos->left;
        } else {
            if (!pos->right) { break; }
            pos = pos->right;
        }
    }

    return std::make_pair(pos, false);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, bool> rbtree_find_insert_pos(rbtree_node_t* head, rbtree_node_t* hint, const Key& k,
                                                       const Comp& comp) {
    if (hint == head) {
        if (!head->left) { return std::make_pair(head, true); }
        if (!comp(k, Traits::get_key(Traits::get_value(head->right)))) { return std::make_pair(head->right, false); }
    } else if (!comp(Traits::get_key(Traits::get_value(hint)), k)) {
        if (hint == head->parent) { return std::make_pair(hint, true); }
        auto prev = rbtree_prev(hint);
        if (!comp(k, Traits::get_key(Traits::get_value(prev)))) {
            if (!prev->right) { return std::make_pair(prev, false); }
            return std::make_pair(hint, true);
        }
    } else if (hint == head->right) {
        return std::make_pair(hint, false);
    } else {
        auto next = rbtree_next(hint);
        if (comp(Traits::get_key(Traits::get_value(next)), k)) {
            return rbtree_find_insert_leftish_pos<Traits>(head, k, comp);
        }
        if (!next->left) { return std::make_pair(next, true); }
        return std::make_pair(hint, false);
    }

    return rbtree_find_insert_pos<Traits>(head, k, comp);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_unique_pos(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto pos = head->left;
    if (!pos) { return std::make_pair(head, -1); }

    while (true) {
        if (comp(k, Traits::get_key(Traits::get_value(pos)))) {
            if (!pos->left) {
                if (pos != head->parent) {
                    auto prev = rbtree_prev(pos);
                    if (!comp(Traits::get_key(Traits::get_value(prev)), k)) { return std::make_pair(prev, 0); }
                }
                return std::make_pair(pos, -1);
            }
            pos = pos->left;
        } else {
            if (!pos->right) { break; }
            pos = pos->right;
        }
    }

    if (!comp(Traits::get_key(Traits::get_value(pos)), k)) { return std::make_pair(pos, 0); }
    return std::make_pair(pos, 1);
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, int> rbtree_find_insert_unique_pos(rbtree_node_t* head, rbtree_node_t* hint, const Key& k,
                                                             const Comp& comp) {
    if (hint == head) {
        if (!head->left) { return std::make_pair(head, -1); }
        if (comp(Traits::get_key(Traits::get_value(head->right)), k)) { return std::make_pair(head->right, 1); }
    } else if (comp(k, Traits::get_key(Traits::get_value(hint)))) {
        if (hint == head->parent) { return std::make_pair(hint, -1); }
        auto prev = rbtree_prev(hint);
        if (comp(Traits::get_key(Traits::get_value(prev)), k)) {
            if (!prev->right) { return std::make_pair(prev, 1); }
            return std::make_pair(hint, -1);
        }
    } else if (comp(Traits::get_key(Traits::get_value(hint)), k)) {
        if (hint == head->right) { return std::make_pair(hint, 1); }
        auto next = rbtree_next(hint);
        if (comp(k, Traits::get_key(Traits::get_value(next)))) {
            if (!next->left) { return std::make_pair(next, -1); }
            return std::make_pair(hint, 1);
        }
    } else {
        return std::make_pair(hint, 0);
    }

    return rbtree_find_insert_unique_pos<Traits>(head, k, comp);
}

template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_lower_bound(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto node = head->left;
    auto lower = head;
    while (node) {
        if (comp(Traits::get_key(Traits::get_value(node)), k)) {
            node = node->right;
        } else {
            lower = node;
            node = node->left;
        }
    }
    return lower;
}

template<typename Traits, typename Key, typename Comp>
rbtree_node_t* rbtree_upper_bound(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto node = head->left;
    auto upper = head;
    while (node) {
        if (!comp(k, Traits::get_key(Traits::get_value(node)))) {
            node = node->right;
        } else {
            upper = node;
            node = node->left;
        }
    }
    return upper;
}

template<typename Traits, typename Key, typename Comp>
std::pair<rbtree_node_t*, rbtree_node_t*> rbtree_equal_range(rbtree_node_t* head, const Key& k, const Comp& comp) {
    auto node = head->left;
    auto upper = head;

    while (node) {
        if (comp(Traits::get_key(Traits::get_value(node)), k)) {
            node = node->right;
        } else if (comp(k, Traits::get_key(Traits::get_value(node)))) {
            upper = node;
            node = node->left;
        } else {
            auto lower = rbtree_lower_bound<Traits>(node, k, comp);
            node = node->right;
            while (node) {
                if (!comp(k, Traits::get_key(Traits::get_value(node)))) {
                    node = node->right;
                } else {
                    upper = node;
                    node = node->left;
                }
            }
            return std::make_pair(lower, upper);
        }
    }

    return std::make_pair(upper, upper);
}

CORE_EXPORT void rbtree_insert(rbtree_node_t* head, rbtree_node_t* node, rbtree_node_t* pos, bool left);
CORE_EXPORT rbtree_node_t* rbtree_remove(rbtree_node_t* head, rbtree_node_t* pos);

}  // namespace util
