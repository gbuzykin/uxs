#pragma once

#include "pool_allocator.h"

namespace uxs {
namespace detail {

template<typename Alloc>
/*static*/ void pool<Alloc>::tidy(pool_desc_t* desc) {
    alloc_type al(*desc);
    auto* desc0 = desc;
    do {
        auto* next = desc->next_pool;
        if (desc->size_and_alignment) { desc->tidy_pool(desc); }
        al.deallocate(desc, 1);
        desc = next;
    } while (desc != desc0);
}

template<typename Alloc>
/*static*/ typename pool<Alloc>::pool_desc_t* pool<Alloc>::find_pool(pool_desc_t* desc,
                                                                     std::uint32_t size_and_alignment) {
    auto* desc0 = desc;
    do {
        if (desc->size_and_alignment == size_and_alignment) { return desc; }
        desc = desc->next_pool;
    } while (desc != desc0);
    return nullptr;
}

template<typename Alloc>
/*static*/ typename pool<Alloc>::pool_desc_t* pool<Alloc>::allocate_new_pool(alloc_type al) {
    auto* desc = al.allocate(1);
    new (desc) alloc_type(std::move(al));
    dllist_make_cycle(&desc->free);
    dllist_make_cycle(&desc->partitions);
    return desc;
}

template<typename Alloc>
/*static*/ typename pool<Alloc>::pool_desc_t* pool<Alloc>::allocate_dummy_pool(const alloc_type& al,
                                                                               std::uint32_t partition_size) {
    auto* desc = allocate_new_pool(al);
    desc->next_pool = desc->root_pool = desc;
    desc->size_and_alignment = 0;
    desc->ref_count = 1;
    desc->partition_size = partition_size;
    return desc;
}

}  // namespace detail
}  // namespace uxs
