#pragma once

#include "utility.h"

#include <functional>

namespace uxs {

//-----------------------------------------------------------------------------
// Function pointer holder

namespace detail {
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
}  // namespace detail

//-----------------------------------------------------------------------------
// Functors

#if __cplusplus < 201402L
template<typename Ty = void>
struct equal_to {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return lhs == rhs; }
};
template<typename Ty = void>
struct not_equal_to {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return !(lhs == rhs); }
};
template<typename Ty = void>
struct less {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return lhs < rhs; }
};
template<typename Ty = void>
struct greater {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return rhs < lhs; }
};
template<typename Ty = void>
struct less_equal {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return !(rhs < lhs); }
};
template<typename Ty = void>
struct greater_equal {
    bool operator()(const Ty& lhs, const Ty& rhs) const { return !(lhs < rhs); }
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
struct not_equal_to<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return !(lhs == rhs);
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
template<>
struct greater<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return rhs < lhs;
    }
};
template<>
struct less_equal<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return !(rhs < lhs);
    }
};
template<>
struct greater_equal<void> {
    using is_transparent = int;
    template<typename Ty1, typename Ty2>
    bool operator()(const Ty1& lhs, const Ty2& rhs) const {
        return !(lhs < rhs);
    }
};
#else   // __cplusplus < 201402L
template<typename Ty = void>
using equal_to = std::equal_to<Ty>;
template<typename Ty = void>
using not_equal_to = std::not_equal_to<Ty>;
template<typename Ty = void>
using less = std::less<Ty>;
template<typename Ty = void>
using greater = std::greater<Ty>;
template<typename Ty = void>
using less_equal = std::less_equal<Ty>;
template<typename Ty = void>
using greater_equal = std::greater_equal<Ty>;
#endif  // __cplusplus < 201402L

}  // namespace uxs
