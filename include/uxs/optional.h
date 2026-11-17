#pragma once

#include "format_common.h"

#include <exception>

namespace est {

struct nullopt_t {
    explicit constexpr nullopt_t(int) {}
};

constexpr nullopt_t nullopt{0};

class UXS_EXPORT_ALL_STUFF_FOR_GNUC bad_optional_access : public std::exception {
 public:
    UXS_EXPORT const char* what() const noexcept override;
};

namespace detail {

template<typename Ty>
class optional_storage {
 public:
    using value_type = Ty;

    optional_storage() noexcept = default;

    template<typename... Args>
    explicit optional_storage(in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : has_value_(true) {
        ::new (&data_) value_type(std::forward<Args>(args)...);
    }

    template<typename U>
    explicit optional_storage(bool has_value, U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : has_value_(has_value) {
        if (has_value_) { ::new (&data_) value_type(std::forward<U>(v)); }
    }

    bool has_value() const noexcept { return has_value_; }

 protected:
    const value_type& val() const noexcept { return *reinterpret_cast<const value_type*>(&data_); }
    value_type& val() noexcept { return *reinterpret_cast<value_type*>(&data_); }

    template<typename U>
    void construct(U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value) {
        ::new (&data_) value_type(std::forward<U>(v));
        has_value_ = true;
    }

    void clear_has_value() noexcept { has_value_ = false; }

 private:
    bool has_value_ = false;
    alignas(std::alignment_of<value_type>::value) std::uint8_t data_[sizeof(value_type)];
};

template<typename Ty, bool TriviallyDestructible = std::is_trivially_destructible<Ty>::value>
class optional_destructible_storage {};

template<typename Ty>
class optional_destructible_storage<Ty, false> : public optional_storage<Ty> {
 public:
    using value_type = Ty;

    optional_destructible_storage() noexcept = default;

    template<typename... Args>
    explicit optional_destructible_storage(in_place_t,
                                           Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : optional_storage<Ty>(in_place, std::forward<Args>(args)...) {}

    template<typename U>
    explicit optional_destructible_storage(bool has_value, U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : optional_storage<Ty>(has_value, std::forward<U>(v)) {}

    ~optional_destructible_storage() {
        if (this->has_value()) { this->val().~value_type(); }
    }

 protected:
    void tidy() noexcept {
        this->val().~value_type();
        this->clear_has_value();
    }
};

template<typename Ty>
class optional_destructible_storage<Ty, true> : public optional_storage<Ty> {
 public:
    optional_destructible_storage() noexcept = default;

    template<typename... Args>
    explicit optional_destructible_storage(in_place_t,
                                           Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : optional_storage<Ty>(in_place, std::forward<Args>(args)...) {}

    template<typename U>
    explicit optional_destructible_storage(bool has_value, U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : optional_storage<Ty>(has_value, std::forward<U>(v)) {}

 protected:
    void tidy() noexcept { this->clear_has_value(); }
};

template<typename Ty, bool TriviallyCopybleAssignable = std::is_trivially_copy_constructible<Ty>::value &&
                                                        std::is_trivially_copy_assignable<Ty>::value &&
                                                        std::is_trivially_move_constructible<Ty>::value &&
                                                        std::is_trivially_move_assignable<Ty>::value>
class optional_base {};

template<typename Ty>
class optional_base<Ty, false> : public optional_destructible_storage<Ty> {
 public:
    optional_base() noexcept = default;

    template<typename... Args>
    explicit optional_base(in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : optional_destructible_storage<Ty>(in_place, std::forward<Args>(args)...) {}

    template<typename U>
    explicit optional_base(bool has_value, U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : optional_destructible_storage<Ty>(has_value, std::forward<U>(v)) {}

    optional_base(const optional_base& other) noexcept(std::is_nothrow_copy_constructible<Ty>::value)
        : optional_destructible_storage<Ty>(other.has_value(), other.val()) {}

    optional_base(optional_base&& other) noexcept(std::is_nothrow_move_constructible<Ty>::value)
        : optional_destructible_storage<Ty>(other.has_value(), std::move(other.val())) {}

    optional_base& operator=(const optional_base& other) noexcept(std::is_nothrow_copy_constructible<Ty>::value &&
                                                                  std::is_nothrow_copy_assignable<Ty>::value) {
        if (&other == this) { return *this; }
        if (this->has_value()) {
            if (other.has_value()) {
                this->val() = other.val();
            } else {
                this->tidy();
            }
        } else if (other.has_value()) {
            this->construct(other.val());
        }
        return *this;
    }

    optional_base& operator=(optional_base&& other) noexcept(std::is_nothrow_move_constructible<Ty>::value &&
                                                             std::is_nothrow_move_assignable<Ty>::value) {
        if (&other == this) { return *this; }
        if (this->has_value()) {
            if (other.has_value()) {
                this->val() = std::move(other.val());
            } else {
                this->tidy();
            }
        } else if (other.has_value()) {
            this->construct(std::move(other.val()));
        }
        return *this;
    }
};

template<typename Ty>
class optional_base<Ty, true> : public optional_destructible_storage<Ty> {
 public:
    optional_base() noexcept = default;

    template<typename... Args>
    explicit optional_base(in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : optional_destructible_storage<Ty>(in_place, std::forward<Args>(args)...) {}

    template<typename U>
    explicit optional_base(bool has_value, U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : optional_destructible_storage<Ty>(has_value, std::forward<U>(v)) {}
};

template<typename Ty, bool Copy, bool CopyAssignment, bool Move, bool MoveAssignment>
struct enable_copy_move {};
#define UXS_OPTIONAL_ENABLE_COPY_MOVE(copy, copy_assignment, move, move_assignment, copy_enable, \
                                      copy_assignment_enable, move_enable, move_assignment_enable) \
    template<typename Ty> \
    struct enable_copy_move<Ty, copy, copy_assignment, move, move_assignment> { \
        enable_copy_move() noexcept = default; \
        enable_copy_move(const enable_copy_move&) noexcept = copy_enable; \
        enable_copy_move(enable_copy_move&&) noexcept = move_enable; \
        enable_copy_move& operator=(const enable_copy_move&) noexcept = copy_assignment_enable; \
        enable_copy_move& operator=(enable_copy_move&&) noexcept = move_assignment_enable; \
    }
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, true, true, true, default, default, default, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, true, true, false, default, default, default, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, true, false, true, default, default, delete, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, true, false, false, default, default, delete, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, false, true, true, default, delete, default, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, false, true, false, default, delete, default, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, false, false, true, default, delete, delete, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(true, false, false, false, default, delete, delete, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, true, true, true, delete, default, default, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, true, true, false, delete, default, default, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, true, false, true, delete, default, delete, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, true, false, false, delete, default, delete, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, false, true, true, delete, delete, default, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, false, true, false, delete, delete, default, delete);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, false, false, true, delete, delete, delete, default);
UXS_OPTIONAL_ENABLE_COPY_MOVE(false, false, false, false, delete, delete, delete, delete);
#undef UXS_OPTIONAL_ENABLE_COPY_MOVE

}  // namespace detail

template<typename Ty>
class optional
    : public detail::optional_base<Ty>,
      public detail::enable_copy_move<Ty, std::is_copy_constructible<Ty>::value,
                                      std::is_copy_constructible<Ty>::value && std::is_copy_assignable<Ty>::value,
                                      std::is_move_constructible<Ty>::value,
                                      std::is_move_constructible<Ty>::value && std::is_move_assignable<Ty>::value> {
 private:
    template<typename U>
    static std::true_type test_for_optional(const optional<U>&);
    static std::false_type test_for_optional(...);

 public:
    using value_type = Ty;

    optional() noexcept = default;
    optional(nullopt_t) noexcept {}

    template<typename... Args, typename = std::enable_if_t<std::is_constructible<Ty, Args...>::value>>
    explicit optional(in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible<Ty, Args...>::value)
        : detail::optional_base<Ty>(in_place, std::forward<Args>(args)...) {}

    template<typename U,
             typename = std::enable_if_t<
                 std::is_constructible<Ty, U>::value && !std::is_same<std::remove_cvref_t<U>, in_place_t>::value &&
                 !decltype(test_for_optional(std::declval<const std::remove_cvref_t<U>&>()))::value>>
    optional(U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value)
        : detail::optional_base<Ty>(in_place, std::forward<U>(v)) {}

    template<typename U,
             typename = std::enable_if_t<!std::is_same<U, Ty>::value && std::is_constructible<Ty, const U&>::value>>
    optional(const optional<U>& other) noexcept(std::is_nothrow_constructible<Ty, const U&>::value)
        : detail::optional_base<Ty>(other.has_value(), other.val()) {}

    template<typename U,
             typename = std::enable_if_t<!std::is_same<U, Ty>::value && std::is_constructible<Ty, U&&>::value>>
    optional(optional<U>&& other) noexcept(std::is_nothrow_constructible<Ty, U&&>::value)
        : detail::optional_base<Ty>(other.has_value(), std::move(other.val())) {}

    optional& operator=(nullopt_t) noexcept { reset(); }

    template<typename U, typename = std::enable_if_t<
                             std::is_constructible<Ty, U>::value && std::is_assignable<Ty&, U>::value &&
                             !decltype(test_for_optional(std::declval<const std::remove_cvref_t<U>&>()))::value>>
    optional& operator=(U&& v) noexcept(std::is_nothrow_constructible<Ty, U>::value &&
                                        std::is_nothrow_assignable<Ty&, U>::value) {
        if (this->has_value()) {
            this->val() = std::forward<U>(v);
        } else {
            this->construct(std::forward<U>(v));
        }
        return *this;
    }

    template<typename U,
             typename = std::enable_if_t<!std::is_same<U, Ty>::value && std::is_constructible<Ty, const U&>::value &&
                                         std::is_assignable<Ty&, const U&>::value>>
    optional& operator=(const optional<U>& other) noexcept(std::is_nothrow_constructible<Ty, const U&>::value &&
                                                           std::is_nothrow_assignable<Ty&, const U&>::value) {
        if (this->has_value()) {
            if (other.has_value()) {
                this->val() = other.val();
            } else {
                this->tidy();
            }
        } else if (other.has_value()) {
            this->construct(other.val());
        }
        return *this;
    }

    template<typename U, typename = std::enable_if_t<!std::is_same<U, Ty>::value && std::is_constructible<Ty, U&&>::value &&
                                                     std::is_assignable<Ty&, U&&>::value>>
    optional& operator=(optional<U>&& other) noexcept(std::is_nothrow_constructible<Ty, U&&>::value &&
                                                      std::is_nothrow_assignable<Ty&, U&&>::value) {
        if (this->has_value()) {
            if (other.has_value()) {
                this->val() = std::move(other.val());
            } else {
                this->tidy();
            }
        } else if (other.has_value()) {
            this->construct(std::move(other.val()));
        }
        return *this;
    }

    void swap(optional& other) noexcept(std::is_nothrow_move_constructible<Ty>::value &&
                                        std::is_nothrow_swappable<Ty>::value) {
        if (&other == this) { return; }
        if (this->has_value()) {
            if (other.has_value()) {
                std::swap(this->val(), other.val());
            } else {
                other.construct(std::move(this->val()));
                this->tidy();
            }
        } else if (other.has_value()) {
            this->construct(std::move(other.val()));
            other.tidy();
        }
    }

    explicit operator bool() const noexcept { return this->has_value(); }

    void reset() noexcept {
        if (this->has_value()) { this->tidy(); }
    }

    const value_type& value() const {
        if (!this->has_value()) { throw bad_optional_access(); }
        return this->val();
    }

    value_type& value() {
        if (!this->has_value()) { throw bad_optional_access(); }
        return this->val();
    }

    template<typename U>
    value_type value_or(U&& default_value) const {
        return this->has_value() ? this->val() : value_type(std::forward<U>(default_value));
    }

    const Ty& operator*() const noexcept {
        assert(this->has_value());
        return this->val();
    }

    Ty& operator*() noexcept {
        assert(this->has_value());
        return this->val();
    }

    const Ty* operator->() const noexcept { return std::addressof(**this); }
    Ty* operator->() noexcept { return std::addressof(**this); }
};

template<typename U, typename = std::enable_if_t<std::is_constructible<optional<std::decay_t<U>>, U>::value>>
optional<std::decay_t<U>> make_optional(U&& v) {
    return optional<std::decay_t<U>>(std::forward<U>(v));
}

template<typename Ty, typename... Args,
         typename = std::enable_if_t<std::is_constructible<optional<Ty>, in_place_t, Args...>::value>>
optional<Ty> make_optional(Args&&... args) {
    return optional<Ty>(in_place, std::forward<Args>(args)...);
}

}  // namespace est

namespace uxs {

template<typename Ty, typename CharT>
struct formatter<est::optional<Ty>, CharT, std::enable_if_t<is_formattable<Ty, CharT>::value>> {
 private:
    formatter_t<Ty, CharT> underlying_;

 public:
    template<typename Formatter_ = formatter_t<Ty, CharT>>
    UXS_CONSTEXPR std::void_t<decltype(std::declval<Formatter_&>().set_debug_format())> set_debug_format() {
        underlying_.set_debug_format();
    }

    template<typename ParseCtx>
    UXS_CONSTEXPR typename ParseCtx::iterator parse(ParseCtx& ctx) {
        return underlying_.parse(ctx);
    }

    template<typename FmtCtx>
    void format(FmtCtx& ctx, const est::optional<Ty>& val) const {
        if (val) { return underlying_.format(ctx, *val); }
        ctx.out() += string_literal<CharT, 'n', 'u', 'l', 'l', 'o', 'p', 't'>{}();
    }
};

}  // namespace uxs

namespace std {
template<typename U1, typename U2>
void swap(est::optional<U1>& v1, est::optional<U2>& v2) noexcept(noexcept(v1.swap(v2))) {
    v1.swap(v2);
}
}  // namespace std
