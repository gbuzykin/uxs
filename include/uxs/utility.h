#pragma once

#include "common.h"

#include <type_traits>
#include <utility>

#define UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(ty, enum_base) \
    CONSTEXPR ty operator&(ty lhs, ty rhs) { \
        return static_cast<ty>(static_cast<enum_base>(lhs) & static_cast<enum_base>(rhs)); \
    } \
    CONSTEXPR ty operator|(ty lhs, ty rhs) { \
        return static_cast<ty>(static_cast<enum_base>(lhs) | static_cast<enum_base>(rhs)); \
    } \
    CONSTEXPR ty operator~(ty flags) { return static_cast<ty>(~static_cast<enum_base>(flags)); } \
    CONSTEXPR bool operator!(ty flags) { return static_cast<enum_base>(flags) == 0; } \
    CONSTEXPR ty& operator&=(ty& lhs, ty rhs) { return lhs = lhs & rhs; } \
    CONSTEXPR ty& operator|=(ty& lhs, ty rhs) { return lhs = lhs | rhs; } \
    static_assert(true, "")

#if defined(_MSC_VER)
#    define UNREACHABLE_CODE __assume(false)
#elif defined(__GNUC__)
#    define UNREACHABLE_CODE __builtin_unreachable()
#else
#    define UNREACHABLE_CODE
#endif

namespace uxs {
template<typename Ty, typename TestTy = void>
struct type_identity {
    using type = Ty;
};
template<typename Ty, typename TestTy = void>
using type_identity_t = typename type_identity<Ty, TestTy>::type;
}  // namespace uxs

#if __cplusplus < 201703L
namespace uxs {
namespace detail {
template<size_t... Indices>
struct index_sequence {};
template<size_t N, size_t... Next>
struct make_index_sequence : make_index_sequence<N - 1U, N - 1U, Next...> {};
template<size_t... Next>
struct make_index_sequence<0U, Next...> {
    using type = index_sequence<Next...>;
};
}  // namespace detail
}  // namespace uxs
namespace std {
#    if !defined(_MSC_VER) && __cplusplus < 201402L
template<bool B, typename Ty = void>
using enable_if_t = typename enable_if<B, Ty>::type;
template<typename Ty>
using decay_t = typename decay<Ty>::type;
template<typename Ty>
using remove_reference_t = typename remove_reference<Ty>::type;
template<typename Ty>
using remove_pointer_t = typename remove_pointer<Ty>::type;
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
add_const_t<Ty>& as_const(Ty& t) {
    return t;
}
template<class Ty>
void as_const(const Ty&&) = delete;
template<typename TestTy>
using void_t = typename uxs::type_identity<void, TestTy>::type;
template<bool B>
using bool_constant = integral_constant<bool, B>;
#    if !defined(_MSC_VER)
template<typename Ty>
using is_nothrow_swappable = bool_constant<noexcept(swap(declval<Ty&>(), declval<Ty&>()))>;
#    endif  // !defined(_MSC_VER)
#    if __cplusplus < 201402L
template<size_t... Indices>
using index_sequence = uxs::detail::index_sequence<Indices...>;
template<size_t N>
using make_index_sequence = typename uxs::detail::make_index_sequence<N>::type;
template<typename... Ts>
using index_sequence_for = make_index_sequence<sizeof...(Ts)>;
#    endif  // __cplusplus < 201402L
}  // namespace std
#endif  // __cplusplus < 201703L

namespace uxs {

template<typename Ty>
struct remove_const : std::remove_const<Ty> {};
template<typename Ty>
using remove_const_t = typename remove_const<Ty>::type;
template<typename Ty1, typename Ty2>
struct remove_const<std::pair<Ty1, Ty2>> {
    using type = std::pair<std::remove_const_t<Ty1>, std::remove_const_t<Ty2>>;
};

namespace detail {
template<typename Range, typename Ty>
struct is_contiguous_range {
    template<typename Range_>
    static auto test(Range_* r) -> std::is_convertible<decltype(r->data() + r->size()), Ty*>;
    template<typename Range_>
    static std::false_type test(...);
    using type = decltype(test<Range>(nullptr));
};
}  // namespace detail

template<typename Range, typename Ty>
struct is_contiguous_range : detail::is_contiguous_range<Range, Ty>::type {};

template<typename Ty>
Ty get_and_set(Ty& v, std::remove_reference_t<Ty> v_new) {
    std::swap(v, v_new);
    return v_new;
}

struct nofunc {
    template<typename Ty>
    auto operator()(Ty&& v) const -> decltype(std::forward<Ty>(v)) {
        return std::forward<Ty>(v);
    }
};

struct grow {
    template<typename TyL, typename TyR>
    TyL& operator()(TyL& lhs, const TyR& rhs) const {
        return lhs += rhs;
    }
};

}  // namespace uxs
