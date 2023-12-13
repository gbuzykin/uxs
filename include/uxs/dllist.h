#pragma once

namespace uxs {

struct dllist_node_t {
    dllist_node_t* next;
    dllist_node_t* prev;
};

template<typename Ty>
bool dllist_is_empty(const Ty* node) {
    return node->next == node;
}

template<typename Ty>
void dllist_make_cycle(Ty* node) {
    node->next = node->prev = node;
}

template<typename Ty>
void dllist_make_cycle(Ty* first, Ty* last) {
    first->prev = last;
    last->next = first;
}

template<typename Ty>
auto dllist_remove(Ty* node) -> decltype(node->next) {
    auto* next = node->next;
    node->prev->next = next;
    next->prev = node->prev;
    return next;
}

template<typename Ty>
void dllist_remove(Ty* first, Ty* last) {
    first->prev->next = last;
    last->prev = first->prev;
}

template<typename Ty>
void dllist_insert_before(Ty* pos, Ty* node) {
    node->next = pos;
    node->prev = pos->prev;
    pos->prev->next = node;
    pos->prev = node;
}

template<typename Ty>
void dllist_insert_before(Ty* pos, Ty* first, Ty* last) {
    last->next = pos;
    first->prev = pos->prev;
    pos->prev->next = first;
    pos->prev = last;
}

template<typename Ty>
void dllist_insert_after(Ty* pos, Ty* node) {
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
}

template<typename Ty>
void dllist_insert_after(Ty* pos, Ty* first, Ty* last) {
    last->next = pos->next;
    first->prev = pos;
    pos->next->prev = last;
    pos->next = first;
}

}  // namespace uxs
