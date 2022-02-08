#include "util/rbtree.h"

using namespace util;

inline void rbtree_rotate_left(rbtree_node_t* node) {
    auto* right = node->right;
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

inline void rbtree_rotate_right(rbtree_node_t* node) {
    auto* left = node->left;
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

void util::rbtree_insert(rbtree_node_t* head, rbtree_node_t* node, rbtree_node_t* pos, bool left) {
    node->left = nullptr;
    node->right = nullptr;
    node->parent = pos;
    if (pos == head) {
        head->left = node;
        head->parent = node;
        head->right = node;
        node->color = rbtree_node_t::color_t::kBlack;
        return;
    }

    if (left) {
        if (pos == head->parent) { head->parent = node; }
        pos->left = node;
    } else {
        if (pos == head->right) { head->right = node; }
        pos->right = node;
    }
    node->color = rbtree_node_t::color_t::kRed;
    if (pos->color == rbtree_node_t::color_t::kBlack) { return; }

    do {
        auto* parent = pos->parent;
        parent->color = rbtree_node_t::color_t::kRed;

        if (parent->left == pos) {
            if (!parent->right || parent->right->color == rbtree_node_t::color_t::kBlack) {
                if (node == pos->right) {
                    rbtree_rotate_left(pos);
                    pos = node;
                }
                rbtree_rotate_right(parent);
                pos->color = rbtree_node_t::color_t::kBlack;
                return;
            }

            parent->right->color = rbtree_node_t::color_t::kBlack;
        } else {
            if (!parent->left || parent->left->color == rbtree_node_t::color_t::kBlack) {
                if (node == pos->left) {
                    rbtree_rotate_right(pos);
                    pos = node;
                }
                rbtree_rotate_left(parent);
                pos->color = rbtree_node_t::color_t::kBlack;
                return;
            }

            parent->left->color = rbtree_node_t::color_t::kBlack;
        }

        pos->color = rbtree_node_t::color_t::kBlack;
        node = parent;
        pos = parent->parent;
    } while (pos->color != rbtree_node_t::color_t::kBlack);

    head->left->color = rbtree_node_t::color_t::kBlack;
}

rbtree_node_t* util::rbtree_remove(rbtree_node_t* head, rbtree_node_t* pos) {
    auto* fix = pos->right;
    auto* parent = pos->parent;
    rbtree_node_t::color_t color = pos->color;

    if (fix) {
        auto* next = rbtree_left_bound(fix);

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

    if (color != rbtree_node_t::color_t::kBlack) { return pos; }

    while (!fix || (parent != head && fix->color == rbtree_node_t::color_t::kBlack)) {
        if (parent->left == fix) {
            auto* node = parent->right;

            if (node->color != rbtree_node_t::color_t::kBlack) {
                node->color = rbtree_node_t::color_t::kBlack;
                parent->color = rbtree_node_t::color_t::kRed;
                rbtree_rotate_left(parent);
                node = parent->right;
            }

            if ((node->left && node->left->color != rbtree_node_t::color_t::kBlack) ||
                (node->right && node->right->color != rbtree_node_t::color_t::kBlack)) {
                if (!node->right || node->right->color == rbtree_node_t::color_t::kBlack) {
                    node->left->color = rbtree_node_t::color_t::kBlack;
                    node->color = rbtree_node_t::color_t::kRed;
                    rbtree_rotate_right(node);
                    node = parent->right;
                }

                node->color = parent->color;
                parent->color = rbtree_node_t::color_t::kBlack;
                node->right->color = rbtree_node_t::color_t::kBlack;
                rbtree_rotate_left(parent);
                return pos;
            }

            node->color = rbtree_node_t::color_t::kRed;
        } else {
            auto* node = parent->left;

            if (node->color != rbtree_node_t::color_t::kBlack) {
                node->color = rbtree_node_t::color_t::kBlack;
                parent->color = rbtree_node_t::color_t::kRed;
                rbtree_rotate_right(parent);
                node = parent->left;
            }

            if ((node->left && node->left->color != rbtree_node_t::color_t::kBlack) ||
                (node->right && node->right->color != rbtree_node_t::color_t::kBlack)) {
                if (!node->left || node->left->color == rbtree_node_t::color_t::kBlack) {
                    node->right->color = rbtree_node_t::color_t::kBlack;
                    node->color = rbtree_node_t::color_t::kRed;
                    rbtree_rotate_left(node);
                    node = parent->left;
                }

                node->color = parent->color;
                parent->color = rbtree_node_t::color_t::kBlack;
                node->left->color = rbtree_node_t::color_t::kBlack;
                rbtree_rotate_right(parent);
                return pos;
            }

            node->color = rbtree_node_t::color_t::kRed;
        }

        fix = parent;
        parent = fix->parent;
    }

    fix->color = rbtree_node_t::color_t::kBlack;
    return pos;
}
