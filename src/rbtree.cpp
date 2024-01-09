#include "uxs/rbtree.h"

using namespace uxs;

inline rbtree_node_t* rbtree_right_parent(rbtree_node_t* node) noexcept {
    rbtree_node_t* parent = node->parent;
    while (node != parent->left) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

inline rbtree_node_t* rbtree_left_parent(rbtree_node_t* node) noexcept {
    rbtree_node_t* parent = node->parent;
    while (node == parent->left) {
        node = parent;
        parent = node->parent;
    }
    return parent;
}

inline void rbtree_rotate_left(rbtree_node_t* node) noexcept {
    rbtree_node_t* right = node->right;
    node->right = right->left;
    right->parent = node->parent;
    if (right->left) { right->left->parent = node; }
    if (node->parent->left == node) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }
    right->left = node;
    node->parent = right;
}

inline void rbtree_rotate_right(rbtree_node_t* node) noexcept {
    rbtree_node_t* left = node->left;
    node->left = left->right;
    left->parent = node->parent;
    if (left->right) { left->right->parent = node; }
    if (node->parent->left == node) {
        node->parent->left = left;
    } else {
        node->parent->right = left;
    }
    left->right = node;
    node->parent = left;
}

NOALIAS rbtree_node_t* uxs::rbtree_next(rbtree_node_t* node) noexcept {
    if (node->right) { return rbtree_left_bound(node->right); }
    return rbtree_right_parent(node);
}

NOALIAS rbtree_node_t* uxs::rbtree_prev(rbtree_node_t* node) noexcept {
    if (node->left) { return rbtree_right_bound(node->left); }
    return rbtree_left_parent(node);
}

void uxs::rbtree_insert(rbtree_node_t* head, rbtree_node_t* node, rbtree_node_t* pos, int dir) noexcept {
    node->left = node->right = nullptr;
    node->parent = pos;
    node->color = rbtree_node_t::color_t::red;

    if (dir < 0) {
        pos->left = node;
        if (pos == head->parent) {
            head->parent = node;
            if (pos == head) { head->right = node; }
        }
    } else {
        pos->right = node;
        if (pos == head->right) { head->right = node; }
    }

    while (pos->color != rbtree_node_t::color_t::black) {
        rbtree_node_t* parent = pos->parent;
        parent->color = rbtree_node_t::color_t::red;

        if (parent->left == pos) {
            if (!parent->right || parent->right->color == rbtree_node_t::color_t::black) {
                if (node == pos->right) {
                    rbtree_rotate_left(pos);
                    pos = node;
                }
                rbtree_rotate_right(parent);
                pos->color = rbtree_node_t::color_t::black;
                return;
            }
            parent->right->color = rbtree_node_t::color_t::black;
        } else if (!parent->left || parent->left->color == rbtree_node_t::color_t::black) {
            if (node == pos->left) {
                rbtree_rotate_right(pos);
                pos = node;
            }
            rbtree_rotate_left(parent);
            pos->color = rbtree_node_t::color_t::black;
            return;
        } else {
            parent->left->color = rbtree_node_t::color_t::black;
        }

        pos->color = rbtree_node_t::color_t::black;
        node = parent;
        pos = parent->parent;
    }

    head->left->color = rbtree_node_t::color_t::black;
}

rbtree_node_t* uxs::rbtree_remove(rbtree_node_t* head, rbtree_node_t* pos) noexcept {
    rbtree_node_t* fix = pos->right;
    rbtree_node_t* parent = pos->parent;
    rbtree_node_t::color_t color = pos->color;

    if (fix) {
        rbtree_node_t* next = rbtree_left_bound(fix);

        if (pos->left) {
            std::swap(color, next->color);
            fix = next->right;
            if (parent->left == pos) {
                parent->left = next;
            } else {
                parent->right = next;
            }

            next->left = pos->left;
            pos->left->parent = next;
            if (pos->right == next) {
                next->parent = parent;
                parent = next;
            } else {
                std::swap(parent, next->parent);
                parent->left = fix;
                next->right = pos->right;
                pos->right->parent = next;
                if (fix) { fix->parent = parent; }
            }
        } else {
            fix->parent = parent;
            if (parent->left == pos) {
                parent->left = fix;
                if (pos == head->parent) { head->parent = next; }
            } else {
                parent->right = fix;
            }
        }

        pos = next;
    } else if (pos->left) {
        fix = pos->left;
        fix->parent = parent;

        if (parent->left == pos) {
            parent->left = fix;
            if (pos == head->right) {
                pos = head;
                head->right = rbtree_right_bound(fix);
            } else {
                pos = parent;
            }
        } else {
            parent->right = fix;
            if (pos == head->right) {
                pos = head;
                head->right = rbtree_right_bound(fix);
            } else {
                pos = rbtree_right_parent(parent);
            }
        }
    } else if (parent->left == pos) {
        parent->left = nullptr;
        if (pos == head->parent) {
            head->parent = parent;
            if (parent == head) {
                head->right = head;
                return head;
            }
        }

        pos = parent;
    } else {
        parent->right = nullptr;
        if (pos == head->right) {
            pos = head;
            head->right = parent;
        } else {
            pos = rbtree_right_parent(parent);
        }
    }

    if (color != rbtree_node_t::color_t::black) { return pos; }

    while (!fix || (parent != head && fix->color == rbtree_node_t::color_t::black)) {
        if (parent->left == fix) {
            rbtree_node_t* node = parent->right;

            if (node->color != rbtree_node_t::color_t::black) {
                node->color = rbtree_node_t::color_t::black;
                parent->color = rbtree_node_t::color_t::red;
                rbtree_rotate_left(parent);
                node = parent->right;
            }

            if ((node->left && node->left->color != rbtree_node_t::color_t::black) ||
                (node->right && node->right->color != rbtree_node_t::color_t::black)) {
                if (!node->right || node->right->color == rbtree_node_t::color_t::black) {
                    node->left->color = rbtree_node_t::color_t::black;
                    node->color = rbtree_node_t::color_t::red;
                    rbtree_rotate_right(node);
                    node = parent->right;
                }

                node->color = parent->color;
                parent->color = rbtree_node_t::color_t::black;
                node->right->color = rbtree_node_t::color_t::black;
                rbtree_rotate_left(parent);
                return pos;
            }

            node->color = rbtree_node_t::color_t::red;
        } else {
            rbtree_node_t* node = parent->left;

            if (node->color != rbtree_node_t::color_t::black) {
                node->color = rbtree_node_t::color_t::black;
                parent->color = rbtree_node_t::color_t::red;
                rbtree_rotate_right(parent);
                node = parent->left;
            }

            if ((node->left && node->left->color != rbtree_node_t::color_t::black) ||
                (node->right && node->right->color != rbtree_node_t::color_t::black)) {
                if (!node->left || node->left->color == rbtree_node_t::color_t::black) {
                    node->right->color = rbtree_node_t::color_t::black;
                    node->color = rbtree_node_t::color_t::red;
                    rbtree_rotate_left(node);
                    node = parent->left;
                }

                node->color = parent->color;
                parent->color = rbtree_node_t::color_t::black;
                node->left->color = rbtree_node_t::color_t::black;
                rbtree_rotate_right(parent);
                return pos;
            }

            node->color = rbtree_node_t::color_t::red;
        }

        fix = parent;
        parent = fix->parent;
    }

    fix->color = rbtree_node_t::color_t::black;
    return pos;
}
