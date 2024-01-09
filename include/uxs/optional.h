#pragma once

#include "utility.h"

#if __cplusplus >= 201703L && UXS_HAS_INCLUDE(<optional>)

#    include <optional>

namespace uxs {
using nullopt_t = std::nullopt_t;
constexpr nullopt_t nullopt() { return std::nullopt; }
template<typename Ty>
using optional = std::optional<Ty>;
using bad_optional_access = std::bad_optional_access;
}  // namespace uxs

#else  // optional

#    include <cassert>
#    include <stdexcept>

namespace uxs {

struct nullopt_t {};
inline nullopt_t nullopt() { return nullopt_t{}; }

class bad_optional_access : public std::exception {
 public:
    const char* what() const noexcept override { return "bad optional access"; }
};

template<typename Ty>
class optional {
 public:
    using value_type = Ty;

    optional() noexcept {}
    optional(nullopt_t) noexcept {}

    optional(const optional& other) : valid_(other.valid_) {
        if (valid_) { new (&data_) value_type(other.val()); }
    }

    optional(optional&& other) noexcept : valid_(other.valid_) {
        if (valid_) { new (&data_) value_type(std::move(other.val())); }
    }

    ~optional() {
        if (valid_) { val().~value_type(); }
    }

    template<typename... Args>
    explicit optional(in_place_t, Args&&... args) : valid_(true) {
        new (&data_) value_type(std::forward<Args>(args)...);
    }

    template<typename U, typename = std::enable_if_t<std::is_constructible<Ty, U&&>::value &&
                                                     !std::is_same<std::decay_t<U>, in_place_t>::value &&
                                                     !std::is_same<std::decay_t<U>, optional<Ty>>::value>>
    optional(U&& v) : valid_(true) {
        new (&data_) value_type(std::forward<U>(v));
    }

    template<typename U>
    optional(const optional<U>& other) : valid_(other.valid_) {
        if (valid_) { new (&data_) value_type(other.val()); }
    }

    template<typename U>
    optional(optional<U>&& other) : valid_(other.valid_) {
        if (valid_) { new (&data_) value_type(std::move(other.val())); }
    }

    optional& operator=(const optional&) = delete;
    optional& operator=(optional&&) = delete;

    bool has_value() const noexcept { return valid_; }
    explicit operator bool() const noexcept { return valid_; }

    void reset() noexcept {
        if (valid_) { val().~value_type(); }
        valid_ = false;
    }

    const value_type& value() const {
        if (valid_) { return val(); }
        throw bad_optional_access();
    }

    value_type& value() {
        if (valid_) { return val(); }
        throw bad_optional_access();
    }

    template<typename U>
    value_type value_or(U&& def) const {
        return valid_ ? val() : value_type(std::forward<U>(def));
    }

    const Ty& operator*() const noexcept {
        assert(valid_);
        return val();
    }

    Ty& operator*() noexcept {
        assert(valid_);
        return val();
    }

    const Ty* operator->() const noexcept { return std::addressof(**this); }
    Ty* operator->() noexcept { return std::addressof(**this); }

 private:
    bool valid_ = false;
    typename std::aligned_storage<sizeof(value_type), std::alignment_of<value_type>::value>::type data_;

    const value_type& val() const { return *reinterpret_cast<const value_type*>(&data_); }
    value_type& val() { return *reinterpret_cast<value_type*>(&data_); }
};

}  // namespace uxs

#endif  // optional

namespace uxs {

template<typename Ty>
optional<std::decay_t<Ty>> make_optional(Ty&& value) {
    return optional<std::decay_t<Ty>>(std::forward<Ty>(value));
}

template<typename Ty, typename... Args>
optional<Ty> make_optional(Args&&... args) {
    return optional<Ty>(uxs::in_place(), std::forward<Args>(args)...);
}

}  // namespace uxs
