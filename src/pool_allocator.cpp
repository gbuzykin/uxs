#include "util/pool_allocator.h"

using namespace util;

/*static*/ pool pool::global_pool_;

/*static*/ void pool::tidy(pool_desc_t* desc) {
    auto* desc0 = desc;
    do {
        auto* next = desc->next_pool;
        if (desc->size_and_alignment) { desc->tidy_pool(desc); }
        alloc_type().deallocate(desc, 1);
        desc = next;
    } while (desc != desc0);
}

/*static*/ pool::pool_desc_t* pool::find_pool(pool_desc_t* desc, uint32_t size_and_alignment) {
    auto* desc0 = desc;
    do {
        if (desc->size_and_alignment == size_and_alignment) { return desc; }
        desc = desc->next_pool;
    } while (desc != desc0);
    return nullptr;
}

/*static*/ pool::pool_desc_t* pool::allocate_new_pool() {
    auto* desc = alloc_type().allocate(1);
    dllist_make_cycle(&desc->free);
    dllist_make_cycle(&desc->partitions);
    return desc;
}

/*static*/ pool::pool_desc_t* pool::allocate_dummy_pool(uint32_t partition_size) {
    auto* desc = allocate_new_pool();
    desc->next_pool = desc->root_pool = desc;
    desc->size_and_alignment = 0;
    desc->ref_count = 1;
    desc->partition_size = partition_size;
    return desc;
}
