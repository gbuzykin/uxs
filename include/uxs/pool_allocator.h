#pragma once

#include "alignment.h"
#include "dllist.h"

#include <cassert>
#include <memory>

namespace uxs {

namespace detail {
template<typename Pool, uint16_t Size, uint16_t Alignment>
struct pool_specializer;

struct pool_part_hdr_t : dllist_node_t {
    uint32_t use_count;
};

template<typename Alloc>
class pool {
 public:
    struct pool_desc_t;
    using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<pool_desc_t>;

    template<typename Ty>
    using pool_specializer_t = pool_specializer<pool, size_of<pool_part_hdr_t, Ty>::value,  //
                                                alignment_of<pool_part_hdr_t, Ty>::value>;

    struct pool_desc_t : alloc_type {
        dllist_node_t free;
        dllist_node_t partitions;
        void* new_node;
        pool_desc_t* root_pool;
        pool_desc_t* next_pool;
        uint32_t size_and_alignment;
        uint32_t ref_count;
        uint32_t node_count_per_partition;
        uint32_t partition_size;

        void (*tidy_pool)(pool_desc_t*);
        void* (*allocate_new)(pool_desc_t*);
        void (*deallocate_partition)(pool_desc_t*, pool_part_hdr_t*);
    };

    pool() : desc_(allocate_dummy_pool(alloc_type(), def_partition_size)) {}
    explicit pool(const Alloc& al) : desc_(allocate_dummy_pool(al, def_partition_size)) {}
    explicit pool(uint32_t partition_size) : desc_(allocate_dummy_pool(alloc_type(), partition_size)) {}
    pool(uint32_t partition_size, const Alloc& al) : desc_(allocate_dummy_pool(al, partition_size)) {}

    pool(const pool& other) : desc_(other.desc_) {
        if (desc_) { ++desc_->root_pool->ref_count; }
    }
    pool& operator=(const pool& other) {
        if (&other != this) { reset(other.desc_); }
        return *this;
    }

    ~pool() { reset(nullptr); }

    template<typename Ty>
    void* allocate() {
        if (desc_->size_and_alignment != pool_specializer_t<Ty>::size_and_alignment) {
            desc_ = pool_specializer_t<Ty>::specialize(desc_);
        }
        return allocate(desc_);
    }

    template<typename Ty>
    void deallocate(void* node) {
        if (desc_->size_and_alignment != pool_specializer_t<Ty>::size_and_alignment) {
            desc_ = pool_specializer_t<Ty>::specialize(desc_);
        }
        deallocate(desc_, node);
    }

    static void* allocate(pool_desc_t* desc) {
        dllist_node_t* node = desc->free.next;
        if (node != &desc->free) {
            inc_use_count(node);
            dllist_remove(node);
            return node;
        }
        return desc->allocate_new(desc);
    }

    static void deallocate(pool_desc_t* desc, void* node) {
        dllist_insert_before(&desc->free, static_cast<dllist_node_t*>(node));
        if (dec_use_count(node) == 0) { desc->deallocate_partition(desc, header(node)); }
    }

    pool_desc_t* desc() { return desc_; }
    void swap(pool& other) { std::swap(desc_, other.desc_); }
    bool is_equal_to(const pool& other) const { return desc_->root_pool == other.desc_->root_pool; }

    void reset(pool_desc_t* desc) {
        if (desc) { ++desc->root_pool->ref_count; }
        if (desc_ && !--desc_->root_pool->ref_count) { tidy(desc_); }
        desc_ = desc;
    }

 protected:
    enum : uint32_t { def_partition_size = 16384 };

    pool_desc_t* desc_;

    template<typename, uint16_t, uint16_t>
    friend struct pool_specializer;

    static pool_part_hdr_t* header(void* node) { return *(static_cast<pool_part_hdr_t**>(node) - 1); }

    static void set_header(void* node, pool_part_hdr_t* hdr) { *(static_cast<pool_part_hdr_t**>(node) - 1) = hdr; }

    static void inc_use_count(void* node) { ++header(node)->use_count; }
    static size_t dec_use_count(void* node) { return --header(node)->use_count; }

    UXS_EXPORT static void tidy(pool_desc_t* desc);
    UXS_EXPORT static pool_desc_t* find_pool(pool_desc_t* desc, uint32_t size_and_alignment);
    UXS_EXPORT static pool_desc_t* allocate_new_pool(alloc_type al);
    UXS_EXPORT static pool_desc_t* allocate_dummy_pool(const alloc_type& al, uint32_t partition_size);
};

extern UXS_EXPORT pool<std::allocator<void>> g_global_pool;

template<typename Pool, uint16_t Size, uint16_t Alignment>
struct pool_specializer {
    struct record_t;
    using pool_desc_t = typename Pool::pool_desc_t;
    using alloc_type = typename std::allocator_traits<typename Pool::alloc_type>::template rebind_alloc<record_t>;

    enum : uint32_t { size_and_alignment = Size | (static_cast<uint32_t>(Alignment) << 16) };

    struct alignas(Alignment) record_t {
        uint8_t x[Size + sizeof(pool_part_hdr_t*)];
    };

    static pool_desc_t* specialize(pool_desc_t* desc);
    static void tidy_pool(pool_desc_t* desc);
    static void* allocate_new(pool_desc_t* desc);
    static void* allocate_new_partition(pool_desc_t* desc);
    static void deallocate_partition(pool_desc_t* desc, pool_part_hdr_t* part_hdr);

    static pool_desc_t* global_pool_desc() {
        static pool_desc_t* desc = nullptr;
        if (!desc) { desc = specialize(g_global_pool.desc()); }
        return desc;
    }
};

template<typename Pool, uint16_t Size, uint16_t Alignment>
/*static*/ typename Pool::pool_desc_t* pool_specializer<Pool, Size, Alignment>::specialize(pool_desc_t* desc) {
    if (pool_desc_t* found = Pool::find_pool(desc, size_and_alignment)) {
        return found;  // pool for desired size and alignment found
    } else if (!dllist_is_empty(&desc->partitions)) {
        // create new pool for desired size and alignment
        pool_desc_t* prev_desc = desc;
        desc = Pool::allocate_new_pool(*desc);
        desc->root_pool = prev_desc->root_pool;
        desc->next_pool = prev_desc->next_pool;
        prev_desc->next_pool = desc;
    }

    // specialize pool for desired size and alignment
    desc->size_and_alignment = size_and_alignment;
    desc->node_count_per_partition = desc->root_pool->partition_size / sizeof(record_t);
    assert(desc->node_count_per_partition > 2);

    desc->tidy_pool = tidy_pool;
    desc->allocate_new = allocate_new_partition;
    desc->deallocate_partition = deallocate_partition;
    return desc;
}

template<typename Pool, uint16_t Size, uint16_t Alignment>
/*static*/ void pool_specializer<Pool, Size, Alignment>::tidy_pool(pool_desc_t* desc) {
    dllist_node_t* part_hdr = desc->partitions.next;
    while (part_hdr != &desc->partitions) {
        dllist_node_t* next_part = part_hdr->next;
        alloc_type(*desc).deallocate(reinterpret_cast<record_t*>(part_hdr), desc->node_count_per_partition);
        part_hdr = next_part;
    }
}

template<typename Pool, uint16_t Size, uint16_t Alignment>
/*static*/ void* pool_specializer<Pool, Size, Alignment>::allocate_new(pool_desc_t* desc) {
    void* node = desc->new_node;
    void* next_node = static_cast<record_t*>(node) - 1;
    if (Pool::header(node) == next_node) {
        desc->allocate_new = allocate_new_partition;
        return node;
    }
    desc->new_node = next_node;
    Pool::set_header(next_node, Pool::header(node));
    return node;
}

template<typename Pool, uint16_t Size, uint16_t Alignment>
/*static*/ void* pool_specializer<Pool, Size, Alignment>::allocate_new_partition(pool_desc_t* desc) {
    record_t* part = alloc_type(*desc).allocate(desc->node_count_per_partition);
    void* node = part + desc->node_count_per_partition - 1;
    pool_part_hdr_t* hdr = reinterpret_cast<pool_part_hdr_t*>(part);
    hdr->use_count = desc->node_count_per_partition - 1;
    dllist_insert_after<dllist_node_t>(&desc->partitions, hdr);
    desc->new_node = part + desc->node_count_per_partition - 2;
    Pool::set_header(node, hdr);
    Pool::set_header(desc->new_node, hdr);
    desc->allocate_new = allocate_new;
    return node;
}

template<typename Pool, uint16_t Size, uint16_t Alignment>
/*static*/ void pool_specializer<Pool, Size, Alignment>::deallocate_partition(pool_desc_t* desc,
                                                                              pool_part_hdr_t* part_hdr) {
    record_t* part = reinterpret_cast<record_t*>(part_hdr);
    for (record_t* record = part; record < part + desc->node_count_per_partition; ++record) {
        dllist_remove(reinterpret_cast<dllist_node_t*>(record));
    }
    if (desc->allocate_new == allocate_new && part_hdr == Pool::header(desc->new_node)) {
        desc->new_node = nullptr;
        desc->allocate_new = allocate_new_partition;
    }
    alloc_type(*desc).deallocate(part, desc->node_count_per_partition);
}

}  // namespace detail

template<typename Ty, typename Alloc = std::allocator<Ty>>
class pool_allocator {
 public:
    using value_type = std::remove_cv_t<Ty>;
    using base_allocator = Alloc;
    using pool_type = detail::pool<typename std::allocator_traits<Alloc>::template rebind_alloc<void>>;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    pool_allocator() = default;
    explicit pool_allocator(const Alloc& al) : pool_(al) {}
    explicit pool_allocator(uint32_t partition_size) : pool_(partition_size) {}
    pool_allocator(uint32_t partition_size, const Alloc& al) : pool_(partition_size, al) {}
    ~pool_allocator() = default;

    template<typename Ty2>
    pool_allocator(const pool_allocator<Ty2, Alloc>& other) noexcept : pool_(other.pool_) {}
    template<typename Ty2>
    pool_allocator& operator=(const pool_allocator<Ty2, Alloc>& other) noexcept {
        if (&other != this) { pool_ = other.pool_; }
        return *this;
    }

    pool_allocator select_on_container_copy_construction() const noexcept { return *this; }
    base_allocator get_base_allocator() const noexcept { return base_allocator(*pool_.desc()); }

    void swap(pool_allocator& other) noexcept { pool_.swap(other.pool_); }

    Ty* allocate(size_t sz) {
        if (sz == 1) { return static_cast<Ty*>(pool_.template allocate<Ty>()); }
        using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
        return alloc_type(*pool_.desc()).allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool_.template deallocate<Ty>(p);
        } else {
            using alloc_type = typename std::allocator_traits<Alloc>::template rebind_alloc<Ty>;
            alloc_type(*pool_.desc()).deallocate(p, sz);
        }
    }

    template<typename Ty2>
    bool is_equal_to(const pool_allocator<Ty2, Alloc>& other) const noexcept {
        return pool_.is_equal_to(other.pool_);
    }

 private:
    template<typename, typename>
    friend class pool_allocator;
    pool_type pool_;
};

template<typename TyL, typename TyR, typename Alloc>
bool operator==(const pool_allocator<TyL, Alloc>& lhs, const pool_allocator<TyR, Alloc>& rhs) noexcept {
    return lhs.is_equal_to(rhs);
}
template<typename TyL, typename TyR, typename Alloc>
bool operator!=(const pool_allocator<TyL, Alloc>& lhs, const pool_allocator<TyR, Alloc>& rhs) noexcept {
    return !(lhs == rhs);
}

template<typename Ty>
class global_pool_allocator {
 public:
    using value_type = std::remove_cv_t<Ty>;
    using base_allocator = std::allocator<Ty>;
    using pool_type = detail::pool<std::allocator<void>>;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    global_pool_allocator() noexcept {}
    ~global_pool_allocator() = default;

    template<typename Ty2>
    global_pool_allocator(const global_pool_allocator<Ty2>&) noexcept {}
    template<typename Ty2>
    global_pool_allocator& operator=(const global_pool_allocator<Ty2>&) noexcept {
        return *this;
    }

    global_pool_allocator select_on_container_copy_construction() const noexcept { return *this; }
    base_allocator get_base_allocator() const noexcept { return {}; }

    Ty* allocate(size_t sz) {
        if (sz == 1) {
            return static_cast<Ty*>(pool_type::allocate(pool_type::pool_specializer_t<Ty>::global_pool_desc()));
        }
        return base_allocator().allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool_type::deallocate(pool_type::pool_specializer_t<Ty>::global_pool_desc(), p);
        } else {
            base_allocator().deallocate(p, sz);
        }
    }
};

template<typename TyL, typename TyR>
bool operator==(const global_pool_allocator<TyL>& lhs, const global_pool_allocator<TyR>& rhs) noexcept {
    return true;
}
template<typename TyL, typename TyR>
bool operator!=(const global_pool_allocator<TyL>& lhs, const global_pool_allocator<TyR>& rhs) noexcept {
    return !(lhs == rhs);
}

}  // namespace uxs

namespace std {
template<typename Ty, typename Alloc>
void swap(uxs::pool_allocator<Ty, Alloc>& a1, uxs::pool_allocator<Ty, Alloc>& a2) noexcept(noexcept(a1.swap(a2))) {
    a1.swap(a2);
}
template<typename Ty>
void swap(uxs::global_pool_allocator<Ty>& a1, uxs::global_pool_allocator<Ty>& a2) noexcept {}
}  // namespace std
