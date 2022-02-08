#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <type_traits>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

#ifdef _STANDALONE
#    define CORE_EXPORT
#else
#    define USE_QT
#    ifdef _MSC_VER
#        ifdef vrcCore_EXPORTS
#            define CORE_EXPORT __declspec(dllexport)
#        else
#            define CORE_EXPORT __declspec(dllimport)
#        endif
#    else
#        define CORE_EXPORT
#    endif
#endif

#define assert_release(cond)           (!!(cond) || debug::assert_impl(__FILE__, __LINE__, #cond, true))
#define assert_release2(cond, message) (!!(cond) || debug::assert_impl(__FILE__, __LINE__, message, true))

#ifndef NDEBUG
#    define assert_dev(cond)           (void)(!!(cond) || debug::assert_impl(__FILE__, __LINE__, #    cond))
#    define assert_dev2(cond, message) (void)(!!(cond) || debug::assert_impl(__FILE__, __LINE__, message))
#    if defined(_MSC_VER) && _MSC_VER >= 1800
#        define DEBUG_NEW new (_CLIENT_BLOCK, __FILE__, __LINE__)
#    else  // defined(_MSC_VER) && _MSC_VER >= 1800
#        define DEBUG_NEW new
#    endif  // defined(_MSC_VER) && _MSC_VER >= 1800
#else       // NDEBUG
#    define assert_dev(cond)           ((void)0)
#    define assert_dev2(cond, message) ((void)0)
#endif  // !NDEBUG

#if _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) assert(cond)
#else  // _ITERATOR_DEBUG_LEVEL != 0
#    define iterator_assert(cond) ((void)0)
#endif  // _ITERATOR_DEBUG_LEVEL != 0

#if defined(_MSC_VER)
#    define USE_CHECKED_ITERATORS
#endif  // defined(_MSC_VER)

#if defined(_MSC_VER) && __cplusplus < 201703L
#    define NOEXCEPT throw()
#    define NOEXCEPT_IF(x)
#else  // defined(_MSC_VER) && __cplusplus < 201703L
#    define NOEXCEPT       noexcept
#    define NOEXCEPT_IF(x) noexcept(x)
#endif  // defined(_MSC_VER) && __cplusplus < 201703L

using uint_t = unsigned int;

namespace debug {
CORE_EXPORT bool assert_impl(const char* file, int line, const char* message, bool release = false);
}

namespace util {
template<typename Ty, typename TestTy>
struct type_identity {
    using type = Ty;
};
template<typename Ty, typename TestTy>
using type_identity_t = typename type_identity<Ty, TestTy>::type;
}  // namespace util

#if __cplusplus < 201703L
namespace std {
#    if !defined(_MSC_VER) && __cplusplus < 201402L
template<bool B, typename Ty = void>
using enable_if_t = typename enable_if<B, Ty>::type;
template<typename Ty>
using decay_t = typename decay<Ty>::type;
template<typename Ty>
using remove_reference_t = typename remove_reference<Ty>::type;
template<typename Ty>
using remove_const_t = typename remove_const<Ty>::type;
template<typename Ty>
using remove_cv_t = typename remove_cv<Ty>::type;
template<typename Ty>
using add_const_t = typename add_const<Ty>::type;
template<bool B, typename Ty1, typename Ty2>
using conditional_t = typename conditional<B, Ty1, Ty2>::type;
#    endif  // !defined(_MSC_VER) && __cplusplus < 201402L
template<class Ty>
std::add_const_t<Ty>& as_const(Ty& t) {
    return t;
}
template<class Ty>
void as_const(const Ty&&) = delete;
template<typename TestTy>
using void_t = typename util::type_identity<void, TestTy>::type;
template<bool B>
using bool_constant = integral_constant<bool, B>;
#    if !defined(_MSC_VER)
template<typename Ty>
using is_nothrow_swappable = bool_constant<noexcept(swap(declval<Ty&>(), declval<Ty&>()))>;
#    endif  // !defined(_MSC_VER)
}  // namespace std
#endif  // __cplusplus < 201703L

namespace util {

#if __cplusplus < 201402L
template<typename... Ts>
struct maximum;
template<typename... Ts>
struct minimum;
template<typename Ty>
struct maximum<Ty> : Ty {};
template<typename Ty>
struct minimum<Ty> : Ty {};
template<typename Ty1, typename Ty2, typename... Rest>
struct maximum<Ty1, Ty2, Rest...> : maximum<std::conditional_t<(Ty1::value < Ty2::value), Ty2, Ty1>, Rest...> {};
template<typename Ty1, typename Ty2, typename... Rest>
struct minimum<Ty1, Ty2, Rest...> : minimum<std::conditional_t<(Ty1::value < Ty2::value), Ty1, Ty2>, Rest...> {};
#else   // __cplusplus < 201402L
template<typename Ty1, typename... Ts>
struct maximum : std::integral_constant<typename Ty1::value_type, std::max({Ty1::value, Ts::value...})> {};
template<typename Ty1, typename... Ts>
struct minimum : std::integral_constant<typename Ty1::value_type, std::min({Ty1::value, Ts::value...})> {};
#endif  // __cplusplus < 201402L

template<typename... Ts>
struct size_of : maximum<size_of<Ts>...> {};
template<typename... Ts>
struct alignment_of : maximum<std::alignment_of<Ts>...> {};
template<typename Ty>
struct size_of<Ty> : std::integral_constant<size_t, sizeof(Ty)> {};
template<typename Ty>
struct alignment_of<Ty> : std::alignment_of<Ty> {};

template<size_t Alignment>
struct aligned {
    template<size_t Size>
    struct type : std::integral_constant<size_t, (Size + Alignment - 1) & ~(Alignment - 1)> {};
    static size_t size(size_t sz) { return (sz + Alignment - 1) & ~(Alignment - 1); }
};

template<typename Ty>
struct remove_const : std::remove_const<Ty> {};
template<typename Ty>
using remove_const_t = typename remove_const<Ty>::type;
template<typename Ty1, typename Ty2>
struct remove_const<std::pair<Ty1, Ty2>> {
    using type = std::pair<std::remove_const_t<Ty1>, std::remove_const_t<Ty2>>;
};

struct nocopy_t {
    nocopy_t() = default;
    nocopy_t(const nocopy_t&) = delete;
    nocopy_t& operator=(const nocopy_t&) = delete;
};

template<typename Ty>
Ty get_and_set(Ty& v, std::remove_reference_t<Ty> v_new) {
    std::swap(v, v_new);
    return v_new;
}

template<typename QtTy>
struct qt_type_converter;

template<typename QtTy, typename = std::void_t<typename qt_type_converter<QtTy>::is_qt_type_converter>, typename... Args>
QtTy to_qt(Args&&... args) {
    return qt_type_converter<QtTy>().to_qt(std::forward<Args>(args)...);
}

template<typename Ty, typename QtTy, typename = std::void_t<typename qt_type_converter<QtTy>::is_qt_type_converter>>
Ty from_qt(const QtTy& val) {
    return qt_type_converter<QtTy>().template from_qt<Ty>(val);
}

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
// Allocator support

#if __cplusplus < 201703L
template<typename Alloc, typename = void>
struct is_alloc_always_equal : std::is_empty<Alloc> {};
template<typename Alloc>
struct is_alloc_always_equal<Alloc, std::void_t<typename Alloc::is_always_equal>> : Alloc::is_always_equal {};
#else   // __cplusplus < 201703L
template<typename Alloc>
using is_alloc_always_equal = typename std::allocator_traits<Alloc>::is_always_equal;
#endif  // __cplusplus < 201703L

//-----------------------------------------------------------------------------
// Functors

struct nofunc {
    template<typename Ty>
    auto operator()(Ty&& v) const -> decltype(std::forward<Ty>(v)) {
        return std::forward<Ty>(v);
    }
};

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
auto get_n(Ty&& v, Dummy&&... dummy) -> decltype(Func1{}(Func2{}(std::forward<Ty>(v)))) {
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
auto get_key(Ty&& v, Dummy&&... dummy) -> decltype(util::get_n<0>{}(std::forward<Ty>(v))) {
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

}  // namespace util
