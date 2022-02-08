#pragma once

#include "iterator.h"

#include <functional>
#include <tuple>

namespace util {

//-----------------------------------------------------------------------------
// Function pointer holder

template<typename Ty>
struct is_function_pointer : std::false_type {};
template<typename Ret, typename... Args>
struct is_function_pointer<Ret (*)(Args...)> : std::true_type {};

template<typename Func, typename = void>
struct func_ptr_holder {
    const Func* func_;
    explicit func_ptr_holder(const Func& func) : func_(&func) {}
    const Func& get_func() const { return *func_; }
};

template<typename Func>
struct func_ptr_holder<Func, std::enable_if_t<is_function_pointer<Func>::value>> {
    Func func_;
    explicit func_ptr_holder(Func func) : func_(func) {}
    Func get_func() const { return func_; }
};

template<typename Func>
struct func_ptr_holder<Func, std::enable_if_t<std::is_empty<Func>::value>> : public Func {
    explicit func_ptr_holder(const Func& func) : Func(func) {}
    ~func_ptr_holder() = default;
    func_ptr_holder(const func_ptr_holder&) = default;
    func_ptr_holder& operator=(const func_ptr_holder&) { return *this; }  // do nothing
    const Func& get_func() const { return *this; }
};

//-----------------------------------------------------------------------------
// Functors

struct deref {
    template<typename Ty>
    auto operator()(Ty&& p) const -> decltype(*p) {
        return *p;
    }
};

struct get {
    template<typename Ty>
    auto operator()(const Ty& p) const -> decltype(p.get()) {
        return p.get();
    }
};

struct lock {
    template<typename Ty>
    auto operator()(const Ty& p) const -> decltype(p.lock()) {
        return p.lock();
    }
};

namespace detail {
template<typename Ty, size_t n, typename Func1, typename Func2>
auto get_n(Ty&& v) -> decltype(Func1{}(std::get<n>(Func2{}(std::forward<Ty>(v))))) {
    return Func1{}(std::get<n>(Func2{}(std::forward<Ty>(v))));
}
template<typename Ty, size_t n, typename Func1, typename Func2, typename... Dummy>
auto get_n(Ty&& v, Dummy&&...) -> decltype(Func1{}(Func2{}(std::forward<Ty>(v)))) {
    static_assert(n == 0, "template parameter `n` should be 0!");
    return Func1{}(Func2{}(std::forward<Ty>(v)));
}
}  // namespace detail

template<size_t n, typename Func1 = nofunc, typename Func2 = nofunc>
struct get_n {
    template<typename Ty>
    auto operator()(Ty&& v) const -> decltype(detail::get_n<Ty, n, Func1, Func2>(std::forward<Ty>(v))) {
        return detail::get_n<Ty, n, Func1, Func2>(std::forward<Ty>(v));
    }
};

namespace detail {
template<typename Ty>
auto get_key(Ty&& v) -> decltype(v.key()) {
    return v.key();
}
template<typename Ty, typename... Dummy>
auto get_key(Ty&& v, Dummy&&...) -> decltype(util::get_n<0>{}(std::forward<Ty>(v))) {
    return util::get_n<0>{}(std::forward<Ty>(v));
}
}  // namespace detail

struct key {
    template<typename Ty>
    auto operator()(Ty&& v) const -> decltype(detail::get_key(std::forward<Ty>(v))) {
        return detail::get_key(std::forward<Ty>(v));
    }
};

template<size_t n, typename Func1 = nofunc>
using deref_get_n = get_n<n, Func1, deref>;

#if __cplusplus < 201402L
template<typename Ty = void>
struct equal_to {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return lhs == rhs; }
};
template<typename Ty = void>
struct less {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return lhs < rhs; }
};
template<>
struct equal_to<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return lhs == rhs;
    }
};
template<>
struct less<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return lhs < rhs;
    }
};
#else   // __cplusplus < 201402L
template<typename Ty = void>
using equal_to = std::equal_to<Ty>;
template<typename Ty = void>
using less = std::less<Ty>;
#endif  // __cplusplus < 201402L

template<typename Val, typename Func, typename Eq>
class is_equal_to_predicate : private func_ptr_holder<Func> {
 public:
    explicit is_equal_to_predicate(const Val& v, const Func& fn) : func_ptr_holder<Func>(fn), v_(&v) {}
    template<typename Ty>
    bool operator()(const Ty& i) const {
        return Eq{}(this->get_func()(i), *v_);
    }

 private:
    const Val* v_;
};

template<typename Val, typename Func = key, typename Eq = equal_to<>>
is_equal_to_predicate<Val, Func, Eq> is_equal_to(const Val& v, const Func& fn = Func{}) {
    return is_equal_to_predicate<Val, Func, Eq>(v, fn);
}

//-----------------------------------------------------------------------------
// Function call iterator

template<typename Func>
class function_call_iterator : private func_ptr_holder<Func> {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using reference = void;
    using pointer = void;

    explicit function_call_iterator(const Func& fn) : func_ptr_holder<Func>(fn) {}

    template<typename Ty>
    function_call_iterator& operator=(const Ty& v) {
        this->get_func()(v);
        return *this;
    }

    function_call_iterator& operator*() { return *this; }
    function_call_iterator& operator++() { return *this; }
    function_call_iterator& operator++(int) { return *this; }
};

template<typename Func>
function_call_iterator<Func> function_caller(const Func& func) {
    return function_call_iterator<Func>(func);
}

#ifdef USE_CHECKED_ITERATORS
template<typename Func>
struct std::_Is_checked_helper<function_call_iterator<Func>> : std::true_type {};
#endif  // USE_CHECKED_ITERATORS

}  // namespace util
