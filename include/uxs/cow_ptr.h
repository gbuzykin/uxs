#pragma once

#include "common.h"

#include <atomic>
#include <cassert>
#include <memory>
#include <utility>

namespace uxs {

template<typename Ty>
class cow_ptr {
 private:
    struct object_body_t {
        template<typename... Args>
        object_body_t(Args&&... args) : obj(std::forward<Args>(args)...) {}
        std::atomic<uint32_t> ref_count{1};
        Ty obj;
    };

 public:
    cow_ptr() : ptr_(new object_body_t) {}
    ~cow_ptr() { reset(nullptr); }
    cow_ptr(const cow_ptr& other) NOEXCEPT : ptr_(other.ref()) {}
    cow_ptr& operator=(const cow_ptr& other) NOEXCEPT {
        if (&other != this) { reset(other.ref()); }
        return *this;
    }
    cow_ptr(cow_ptr&& other) NOEXCEPT : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    cow_ptr& operator=(cow_ptr&& other) NOEXCEPT {
        if (&other != this) {
            reset(other.ptr_);
            other.ptr_ = nullptr;
        }
        return *this;
    }

    operator bool() const NOEXCEPT { return ptr_ != nullptr; }

    const Ty& operator*() const NOEXCEPT {
        assert(ptr_);
        return ptr_->obj;
    }

    Ty& operator*() {
        assert(ptr_);
        if (ptr_->ref_count > 1) { reset(new object_body_t(ptr_->obj)); }
        return ptr_->obj;
    }

    const Ty* operator->() const NOEXCEPT { return std::addressof(**this); }
    Ty* operator->() { return std::addressof(**this); }

 private:
    object_body_t* ptr_;
    explicit cow_ptr(object_body_t* ptr) : ptr_(ptr) {}
    object_body_t* ref() const {
        if (ptr_) { ++ptr_->ref_count; }
        return ptr_;
    }
    void reset(object_body_t* ptr) {
        if (ptr_ && --ptr_->ref_count == 0) { delete ptr_; }
        ptr_ = ptr;
    }

    template<typename Ty_, typename... Args>
    friend cow_ptr<Ty_> make_cow(Args&&...);
};

template<typename Ty, typename... Args>
cow_ptr<Ty> make_cow(Args&&... args) {
    return cow_ptr<Ty>(new typename cow_ptr<Ty>::object_body_t(std::forward<Args>(args)...));
}

}  // namespace uxs
