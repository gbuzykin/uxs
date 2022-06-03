#pragma once

#include "alignment.h"
#include "allocator.h"
#include "dllist.h"

namespace uxs {

template<uint16_t Size, uint16_t Alignment>
struct pool_specializer;

class UXS_EXPORT pool {
 public:
    struct part_hdr_t : dllist_node_t {
        uint32_t use_count;
    };

    struct pool_desc_t {
        dllist_node_t free;
        dllist_node_t partitions;
        dllist_node_t* new_node;
        pool_desc_t* root_pool;
        pool_desc_t* next_pool;
        uint32_t size_and_alignment;
        uint32_t ref_count;
        uint32_t node_count_per_partition;
        uint32_t partition_size;

        void (*tidy_pool)(pool_desc_t*);
        dllist_node_t* (*allocate_new)(pool_desc_t*);
        void (*deallocate_partition)(pool_desc_t*, part_hdr_t*);
    };

    using alloc_type = std::allocator<pool_desc_t>;

    template<typename Ty>
    using pool_specializer_t = pool_specializer<size_of<part_hdr_t, Ty>::value, alignment_of<part_hdr_t, Ty>::value>;

    pool() : desc_(allocate_dummy_pool(kDefPartitionSize)) {}
    explicit pool(uint32_t partition_size) : desc_(allocate_dummy_pool(partition_size)) {}

    pool(const pool& other) : desc_(other.desc_) {
        if (desc_) { ++desc_->root_pool->ref_count; }
    }
    pool& operator=(const pool& other) {
        if (&other != this) { reset(other.desc_); }
        return *this;
    }

    ~pool() { reset(nullptr); }

    template<typename Ty>
    dllist_node_t* allocate() {
        if (desc_->size_and_alignment != pool_specializer_t<Ty>::kSizeAndAlignment) {
            desc_ = pool_specializer_t<Ty>::specialize(desc_);
        }
        return allocate(desc_);
    }

    template<typename Ty>
    void deallocate(dllist_node_t* node) {
        if (desc_->size_and_alignment != pool_specializer_t<Ty>::kSizeAndAlignment) {
            desc_ = pool_specializer_t<Ty>::specialize(desc_);
        }
        deallocate(desc_, node);
    }

    static dllist_node_t* allocate(pool_desc_t* desc) {
        auto* node = desc->free.next;
        if (node == &desc->free) { return desc->allocate_new(desc); }
        inc_use_count(node);
        dllist_remove(node);
        return node;
    }

    static void deallocate(pool_desc_t* desc, dllist_node_t* node) {
        dllist_insert_before(&desc->free, node);
        if (dec_use_count(node) == 0) { desc->deallocate_partition(desc, header(node)); }
    }

    void swap(pool& other) { std::swap(desc_, other.desc_); }
    bool is_equal_to(const pool& other) const { return desc_->root_pool == other.desc_->root_pool; }

    static void reset_global_pool() { global_pool_.reset(nullptr); }
    static pool_desc_t* global_pool_desc() { return global_pool_.desc_; }

 protected:
    enum : uint32_t { kDefPartitionSize = 16384 };

    pool_desc_t* desc_;

    template<uint16_t, uint16_t>
    friend struct pool_specializer;

    static pool global_pool_;

    static part_hdr_t* header(dllist_node_t* node) { return *(reinterpret_cast<part_hdr_t**>(node) - 1); }

    static void set_header(dllist_node_t* node, part_hdr_t* hdr) { *(reinterpret_cast<part_hdr_t**>(node) - 1) = hdr; }

    static void inc_use_count(dllist_node_t* node) { ++header(node)->use_count; }
    static size_t dec_use_count(dllist_node_t* node) { return --header(node)->use_count; }

    void reset(pool_desc_t* desc) {
        if (desc) { ++desc->root_pool->ref_count; }
        if (desc_ && !--desc_->root_pool->ref_count) { tidy(desc_); }
        desc_ = desc;
    }

    static void tidy(pool_desc_t* desc);
    static pool_desc_t* find_pool(pool_desc_t* desc, uint32_t size_and_alignment);
    static pool_desc_t* allocate_new_pool();
    static pool_desc_t* allocate_dummy_pool(uint32_t partition_size);
};

template<uint16_t Size, uint16_t Alignment>
struct pool_specializer {
    struct record_t {
        typename std::aligned_storage<Size + sizeof(pool::part_hdr_t*), Alignment>::type placeholder;
        dllist_node_t* node() { return reinterpret_cast<dllist_node_t*>(&placeholder); }
        static record_t* from_node(dllist_node_t* node) { return reinterpret_cast<record_t*>(node); }
    };

    enum : uint32_t { kSizeAndAlignment = Size | (static_cast<uint32_t>(Alignment) << 16) };

    using alloc_traits = std::allocator_traits<typename pool::alloc_type>;
    using alloc_type = typename alloc_traits::template rebind_alloc<record_t>;

    static pool::pool_desc_t* global_pool_desc() {
        static pool::pool_desc_t* desc = nullptr;
        if (!desc) { desc = specialize(pool::global_pool_desc()); }
        return desc;
    }

    static pool::pool_desc_t* specialize(pool::pool_desc_t* desc);
    static void tidy_pool(pool::pool_desc_t* desc);
    static dllist_node_t* allocate_new(pool::pool_desc_t* desc);
    static dllist_node_t* allocate_new_partition(pool::pool_desc_t* desc);
    static void deallocate_partition(pool::pool_desc_t* desc, pool::part_hdr_t* part_hdr);
};

template<uint16_t Size, uint16_t Alignment>
/*static*/ pool::pool_desc_t* pool_specializer<Size, Alignment>::specialize(pool::pool_desc_t* desc) {
    if (auto* found = pool::find_pool(desc, kSizeAndAlignment)) {
        return found;  // pool for desired size and alignment found
    } else if (!dllist_is_empty(&desc->partitions)) {
        // create new pool for desired size and alignment
        auto* prev_desc = desc;
        desc = pool::allocate_new_pool();
        desc->root_pool = prev_desc->root_pool;
        desc->next_pool = prev_desc->next_pool;
        prev_desc->next_pool = desc;
    }

    // specialize pool for desired size and alignment
    desc->size_and_alignment = kSizeAndAlignment;
    desc->node_count_per_partition = desc->root_pool->partition_size / sizeof(record_t);
    assert(desc->node_count_per_partition > 2);

    desc->tidy_pool = tidy_pool;
    desc->allocate_new = allocate_new_partition;
    desc->deallocate_partition = deallocate_partition;
    return desc;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ void pool_specializer<Size, Alignment>::tidy_pool(pool::pool_desc_t* desc) {
    auto* part_hdr = desc->partitions.next;
    while (part_hdr != &desc->partitions) {
        auto* next_part = part_hdr->next;
        alloc_type().deallocate(record_t::from_node(part_hdr), desc->node_count_per_partition);
        part_hdr = next_part;
    }
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ dllist_node_t* pool_specializer<Size, Alignment>::allocate_new(pool::pool_desc_t* desc) {
    auto* node = desc->new_node;
    auto* next_node = (record_t::from_node(node) - 1)->node();
    if (pool::header(node) == next_node) {
        desc->allocate_new = allocate_new_partition;
        return node;
    }
    desc->new_node = next_node;
    pool::set_header(next_node, pool::header(node));
    return node;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ dllist_node_t* pool_specializer<Size, Alignment>::allocate_new_partition(pool::pool_desc_t* desc) {
    auto* part = alloc_type().allocate(desc->node_count_per_partition);
    auto* node = (part + desc->node_count_per_partition - 1)->node();
    auto* hdr = static_cast<pool::part_hdr_t*>(part->node());
    hdr->use_count = desc->node_count_per_partition - 1;
    dllist_insert_after(&desc->partitions, part->node());
    desc->new_node = (part + desc->node_count_per_partition - 2)->node();
    pool::set_header(node, hdr);
    pool::set_header(desc->new_node, hdr);
    desc->allocate_new = allocate_new;
    return node;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ void pool_specializer<Size, Alignment>::deallocate_partition(pool::pool_desc_t* desc,
                                                                        pool::part_hdr_t* part_hdr) {
    auto* part = record_t::from_node(part_hdr);
    for (auto* record = part; record < part + desc->node_count_per_partition; ++record) {
        dllist_remove(record->node());
    }
    if (desc->allocate_new == allocate_new && part_hdr == pool::header(desc->new_node)) {
        desc->new_node = nullptr;
        desc->allocate_new = allocate_new_partition;
    }
    alloc_type().deallocate(part, desc->node_count_per_partition);
}

template<typename Ty>
class pool_allocator {
 private:
    using alloc_traits = std::allocator_traits<typename pool::alloc_type>;
    using alloc_type = typename alloc_traits::template rebind_alloc<Ty>;

 public:
    using value_type = typename std::remove_cv<Ty>::type;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    pool_allocator() = default;
    explicit pool_allocator(uint32_t partition_size) : pool_(partition_size) {}
    ~pool_allocator() = default;

    template<typename Ty2>
    pool_allocator(const pool_allocator<Ty2>& other) NOEXCEPT : pool_(other.pool_) {}
    template<typename Ty2>
    pool_allocator& operator=(const pool_allocator<Ty2>& other) NOEXCEPT {
        if (&other != this) { pool_ = other.pool_; }
        return *this;
    }

    pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }

    void swap(pool_allocator& other) NOEXCEPT { pool_.swap(other.pool_); }

    Ty* allocate(size_t sz) {
        if (sz == 1) { return reinterpret_cast<Ty*>(pool_.allocate<Ty>()); }
        return alloc_type().allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool_.deallocate<Ty>(reinterpret_cast<dllist_node_t*>(p));
        } else {
            alloc_type().deallocate(p, sz);
        }
    }

    template<typename Ty2>
    bool is_equal_to(const pool_allocator<Ty2>& other) const NOEXCEPT {
        return pool_.is_equal_to(other.pool_);
    }

 private:
    template<typename>
    friend class pool_allocator;
    pool pool_;
};

template<>
class pool_allocator<void> {
 public:
    using value_type = void;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    pool_allocator() = default;
    explicit pool_allocator(uint32_t partition_size) : pool_(partition_size) {}
    ~pool_allocator() = default;

    template<typename Ty2>
    pool_allocator(const pool_allocator<Ty2>& other) NOEXCEPT : pool_(other.pool_) {}
    template<typename Ty2>
    pool_allocator& operator=(const pool_allocator<Ty2>& other) NOEXCEPT {
        if (&other != this) { pool_ = other.pool_; }
        return *this;
    }

    pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }

    void swap(pool_allocator& other) NOEXCEPT { pool_.swap(other.pool_); }

    template<typename Ty2>
    bool is_equal_to(const pool_allocator<Ty2>& other) const NOEXCEPT {
        return pool_.is_equal_to(other.pool_);
    }

 private:
    template<typename>
    friend class pool_allocator;
    pool pool_;
};

template<typename TyL, typename TyR>
bool operator==(const pool_allocator<TyL>& lhs, const pool_allocator<TyR>& rhs) NOEXCEPT {
    return lhs.is_equal_to(rhs);
}
template<typename TyL, typename TyR>
bool operator!=(const pool_allocator<TyL>& lhs, const pool_allocator<TyR>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}

template<typename Ty>
class global_pool_allocator {
 private:
    using alloc_traits = std::allocator_traits<typename pool::alloc_type>;
    using alloc_type = typename alloc_traits::template rebind_alloc<Ty>;

 public:
    using value_type = typename std::remove_cv<Ty>::type;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    global_pool_allocator() NOEXCEPT {}
    ~global_pool_allocator() = default;

    template<typename Ty2>
    global_pool_allocator(const global_pool_allocator<Ty2>&) NOEXCEPT {}
    template<typename Ty2>
    global_pool_allocator& operator=(const global_pool_allocator<Ty2>&) NOEXCEPT {
        return *this;
    }

    global_pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }

    Ty* allocate(size_t sz) {
        if (sz == 1) { return reinterpret_cast<Ty*>(pool::allocate(pool::pool_specializer_t<Ty>::global_pool_desc())); }
        return alloc_type().allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool::deallocate(pool::pool_specializer_t<Ty>::global_pool_desc(), reinterpret_cast<dllist_node_t*>(p));
        } else {
            alloc_type().deallocate(p, sz);
        }
    }
};

template<>
class global_pool_allocator<void> {
 public:
    using value_type = void;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    global_pool_allocator() NOEXCEPT {}
    ~global_pool_allocator() = default;

    template<typename Ty2>
    global_pool_allocator(const global_pool_allocator<Ty2>&) NOEXCEPT {}
    template<typename Ty2>
    global_pool_allocator& operator=(const global_pool_allocator<Ty2>&) NOEXCEPT {
        return *this;
    }

    global_pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }
};

template<typename TyL, typename TyR>
bool operator==(const global_pool_allocator<TyL>& lhs, const global_pool_allocator<TyR>& rhs) NOEXCEPT {
    return true;
}
template<typename TyL, typename TyR>
bool operator!=(const global_pool_allocator<TyL>& lhs, const global_pool_allocator<TyR>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}

}  // namespace uxs

namespace std {
template<typename Ty>
void swap(uxs::pool_allocator<Ty>& a1, uxs::pool_allocator<Ty>& a2) NOEXCEPT_IF(NOEXCEPT_IF(a1.swap(a2))) {
    a1.swap(a2);
}
template<typename Ty>
void swap(uxs::global_pool_allocator<Ty>& a1, uxs::global_pool_allocator<Ty>& a2) NOEXCEPT {}
}  // namespace std
