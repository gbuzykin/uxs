#pragma once

#include "alignment.h"
#include "allocator.h"
#include "dllist.h"

namespace util {

template<uint16_t Size, uint16_t Alignment>
class sized_pool;

class UTIL_EXPORT pool_base {
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

    struct global_pool_list_item_t {
        global_pool_list_item_t* next;
        pool_base* pool;
        pool_desc_t* pool_desc() { return pool->desc_; }
        void reset() { return pool->reset(nullptr); }
    };

    using alloc_type = std::allocator<pool_desc_t>;

    template<typename Ty>
    using sized_pool_type = sized_pool<size_of<part_hdr_t, Ty>::value, alignment_of<part_hdr_t, Ty>::value>;

    pool_base() : desc_(allocate_dummy_pool(kDefPartitionSize)) {}
    explicit pool_base(uint32_t partition_size) : desc_(allocate_dummy_pool(partition_size)) {}

    pool_base(const pool_base& other) NOEXCEPT : desc_(other.desc_) {
        assert(desc_);
        ++desc_->root_pool->ref_count;
    }
    pool_base& operator=(const pool_base& other) NOEXCEPT {
        if (&other != this) { reset(other.desc_); }
        return *this;
    }

    ~pool_base() { reset(nullptr); }

    void swap(pool_base& other) NOEXCEPT { std::swap(desc_, other.desc_); }

    bool is_equal_to(const pool_base& other) const NOEXCEPT { return desc_->root_pool == other.desc_->root_pool; }

    static global_pool_list_item_t* global_pool_list() { return global_pool_list_; }

 protected:
    enum : uint32_t { kDefPartitionSize = 16384 };

    pool_desc_t* desc_;

    static global_pool_list_item_t* global_pool_list_;
    static pool_base global_pool_;

    static part_hdr_t* header(dllist_node_t* node) { return *(reinterpret_cast<part_hdr_t**>(node) - 1); }

    static void set_header(dllist_node_t* node, part_hdr_t* hdr) { *(reinterpret_cast<part_hdr_t**>(node) - 1) = hdr; }

    static void inc_use_count(dllist_node_t* node) { ++header(node)->use_count; }
    static size_t dec_use_count(dllist_node_t* node) { return --header(node)->use_count; }

    void reset(pool_desc_t* desc) {
        assert(desc_);
        if (desc) { ++desc->ref_count; }
        if (!--desc_->ref_count) { tidy(); }
        desc_ = desc;
    }

    dllist_node_t* allocate_impl() {
        auto* node = desc_->free.next;
        if (node == &desc_->free) { return desc_->allocate_new(desc_); }
        inc_use_count(node);
        dllist_remove(node);
        return node;
    }

    void deallocate_impl(dllist_node_t* node) {
        dllist_insert_before(&desc_->free, node);
        if (dec_use_count(node) == 0) { desc_->deallocate_partition(desc_, header(node)); }
    }

    void tidy();
    static pool_desc_t* find_pool(pool_desc_t* desc, uint32_t size_and_alignment);
    static pool_desc_t* allocate_new_pool();
    static pool_desc_t* allocate_dummy_pool(uint32_t partition_size);
};

template<uint16_t Size, uint16_t Alignment>
class sized_pool : public pool_base {
 public:
    struct record_t {
        typename std::aligned_storage<Size + sizeof(part_hdr_t*), Alignment>::type placeholder;
        dllist_node_t* node() { return (dllist_node_t*)&placeholder; }
        static record_t* from_node(dllist_node_t* node) { return reinterpret_cast<record_t*>(node); }
    };

    using alloc_traits = std::allocator_traits<typename pool_base::alloc_type>;
    using alloc_type = typename alloc_traits::template rebind_alloc<record_t>;

    sized_pool() = default;
    sized_pool(const pool_base& other) NOEXCEPT : pool_base(other) {}
    sized_pool& operator=(const pool_base& other) NOEXCEPT {
        pool_base::operator=(other);
        return *this;
    }

    dllist_node_t* allocate() {
        if (desc_->size_and_alignment != kSizeAndAlignment) { init(); }
        return allocate_impl();
    }

    void deallocate(dllist_node_t* node) {
        if (desc_->size_and_alignment != kSizeAndAlignment) { init(); }
        deallocate_impl(node);
    }

    static sized_pool& instance() {
        static struct item_t : global_pool_list_item_t {
            sized_pool global_pool_inst;
            explicit item_t() : global_pool_inst(global_pool_) {
                next = global_pool_list_;
                pool = &global_pool_inst;
                global_pool_list_ = this;
            }
        } item;
        return item.global_pool_inst;
    }

 private:
    enum : uint32_t { kSizeAndAlignment = Size | (static_cast<uint32_t>(Alignment) << 16) };

    void init();
    static void tidy_pool(pool_desc_t* desc);
    static dllist_node_t* allocate_new(pool_desc_t* desc);
    static dllist_node_t* allocate_new_partition(pool_desc_t* desc);
    static void deallocate_partition(pool_desc_t* desc, part_hdr_t* part_hdr);
};

template<uint16_t Size, uint16_t Alignment>
void sized_pool<Size, Alignment>::init() {
    auto* desc = desc_;
    if (auto* found = find_pool(desc, kSizeAndAlignment)) {
        desc_ = found;
        return;  // pool for size found
    } else if (!dllist_is_empty(&desc->partitions)) {
        // specialize pool for size
        desc = allocate_new_pool();
        desc->root_pool = desc_->root_pool;
        desc->next_pool = desc_->next_pool;
        desc_->next_pool = desc;
        desc_ = desc;
    }

    desc->size_and_alignment = kSizeAndAlignment;
    desc->node_count_per_partition = desc->root_pool->partition_size / sizeof(record_t);
    assert(desc->node_count_per_partition > 2);

    desc->tidy_pool = tidy_pool;
    desc->allocate_new = allocate_new_partition;
    desc->deallocate_partition = deallocate_partition;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ void sized_pool<Size, Alignment>::tidy_pool(pool_desc_t* desc) {
    auto* part_hdr = desc->partitions.next;
    while (part_hdr != &desc->partitions) {
        auto* next_part = part_hdr->next;
        alloc_type().deallocate(record_t::from_node(part_hdr), desc->node_count_per_partition);
        part_hdr = next_part;
    }
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ dllist_node_t* sized_pool<Size, Alignment>::allocate_new(pool_desc_t* desc) {
    auto* node = desc->new_node;
    auto* next_node = (record_t::from_node(node) - 1)->node();
    if (header(node) == next_node) {
        desc->allocate_new = allocate_new_partition;
        return node;
    }
    desc->new_node = next_node;
    set_header(next_node, header(node));
    return node;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ dllist_node_t* sized_pool<Size, Alignment>::allocate_new_partition(pool_desc_t* desc) {
    auto* part = alloc_type().allocate(desc->node_count_per_partition);
    auto* node = (part + desc->node_count_per_partition - 1)->node();
    auto* hdr = static_cast<part_hdr_t*>(part->node());
    hdr->use_count = desc->node_count_per_partition - 1;
    dllist_insert_after(&desc->partitions, part->node());
    desc->new_node = (part + desc->node_count_per_partition - 2)->node();
    set_header(node, hdr);
    set_header(desc->new_node, hdr);
    desc->allocate_new = allocate_new;
    return node;
}

template<uint16_t Size, uint16_t Alignment>
/*static*/ void sized_pool<Size, Alignment>::deallocate_partition(pool_desc_t* desc, part_hdr_t* part_hdr) {
    auto* part = record_t::from_node(part_hdr);
    for (auto* record = part; record < part + desc->node_count_per_partition; ++record) {
        dllist_remove(record->node());
    }
    if (desc->allocate_new == allocate_new && part_hdr == header(desc->new_node)) {
        desc->new_node = nullptr;
        desc->allocate_new = allocate_new_partition;
    }
    alloc_type().deallocate(part, desc->node_count_per_partition);
}

template<typename Ty>
class pool_allocator {
 private:
    using alloc_traits = std::allocator_traits<typename pool_base::alloc_type>;
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
        if (sz == 1) { return (Ty*)pool_.allocate(); }
        return alloc_type().allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool_.deallocate((dllist_node_t*)p);
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
    pool_base::sized_pool_type<Ty> pool_;
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
    pool_base pool_;
};

template<typename Ty1, typename Ty2>
bool operator==(const pool_allocator<Ty1>& lhs, const pool_allocator<Ty2>& rhs) NOEXCEPT {
    return lhs.is_equal_to(rhs);
}
template<typename Ty>
bool operator==(const pool_allocator<Ty>& lhs, const pool_allocator<Ty>& rhs) NOEXCEPT {
    return lhs.is_equal_to(rhs);
}

template<typename Ty1, typename Ty2>
bool operator!=(const pool_allocator<Ty1>& lhs, const pool_allocator<Ty2>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}
template<typename Ty>
bool operator!=(const pool_allocator<Ty>& lhs, const pool_allocator<Ty>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}

template<typename Ty>
class global_pool_allocator {
 private:
    using alloc_traits = std::allocator_traits<typename pool_base::alloc_type>;
    using alloc_type = typename alloc_traits::template rebind_alloc<Ty>;

 public:
    using value_type = typename std::remove_cv<Ty>::type;
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    global_pool_allocator() NOEXCEPT = default;
    ~global_pool_allocator() = default;

    template<typename Ty2>
    global_pool_allocator(const global_pool_allocator<Ty2>&) NOEXCEPT {}
    template<typename Ty2>
    global_pool_allocator& operator=(const global_pool_allocator<Ty2>&) NOEXCEPT {
        return *this;
    }

    global_pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }

    pool_base::sized_pool_type<Ty>& pool() { return pool_base::sized_pool_type<Ty>::instance(); }

    Ty* allocate(size_t sz) {
        if (sz == 1) { return (Ty*)pool().allocate(); }
        return alloc_type().allocate(sz);
    }

    void deallocate(Ty* p, size_t sz) {
        if (sz == 1) {
            pool().deallocate((dllist_node_t*)p);
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

    global_pool_allocator() NOEXCEPT = default;
    ~global_pool_allocator() = default;

    template<typename Ty2>
    global_pool_allocator(const global_pool_allocator<Ty2>&) NOEXCEPT {}
    template<typename Ty2>
    global_pool_allocator& operator=(const global_pool_allocator<Ty2>&) NOEXCEPT {
        return *this;
    }

    global_pool_allocator select_on_container_copy_construction() const NOEXCEPT { return *this; }
};

template<typename Ty1, typename Ty2>
bool operator==(const global_pool_allocator<Ty1>& lhs, const global_pool_allocator<Ty2>& rhs) NOEXCEPT {
    return true;
}
template<typename Ty>
bool operator==(const global_pool_allocator<Ty>& lhs, const global_pool_allocator<Ty>& rhs) NOEXCEPT {
    return true;
}

template<typename Ty1, typename Ty2>
bool operator!=(const global_pool_allocator<Ty1>& lhs, const global_pool_allocator<Ty2>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}
template<typename Ty>
bool operator!=(const global_pool_allocator<Ty>& lhs, const global_pool_allocator<Ty>& rhs) NOEXCEPT {
    return !(lhs == rhs);
}

}  // namespace util

namespace std {
template<typename Ty>
void swap(util::pool_allocator<Ty>& a1, util::pool_allocator<Ty>& a2) NOEXCEPT_IF(NOEXCEPT_IF(a1.swap(a2))) {
    a1.swap(a2);
}
template<typename Ty>
void swap(util::global_pool_allocator<Ty>& a1, util::global_pool_allocator<Ty>& a2) NOEXCEPT {}
}  // namespace std
