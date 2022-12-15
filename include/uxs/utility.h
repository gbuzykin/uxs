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
#    if __cplusplus < 201402L && (!defined(_MSC_VER) || _MSC_VER > 1800)
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
#    endif  // __cplusplus < 201402L && (!defined(_MSC_VER) || _MSC_VER > 1800)
#    if !defined(_MSC_VER) || _MSC_VER <= 1800
template<class Ty>
add_const_t<Ty>& as_const(Ty& t) {
    return t;
}
template<class Ty>
void as_const(const Ty&&) = delete;
template<typename TestTy>
using void_t = typename uxs::type_identity<void, TestTy>::type;
#    endif  // !defined(_MSC_VER) || _MSC_VER <= 1800
template<bool B>
using bool_constant = integral_constant<bool, B>;
#    if !defined(_MSC_VER) || _MSC_VER > 1800
template<typename Ty>
using is_nothrow_swappable = bool_constant<noexcept(swap(declval<Ty&>(), declval<Ty&>()))>;
#    endif  // !defined(_MSC_VER) || _MSC_VER > 1800
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

template<typename Ty>
struct remove_cv : std::remove_cv<Ty> {};
template<typename Ty>
using remove_cv_t = typename remove_cv<Ty>::type;
template<typename Ty1, typename Ty2>
struct remove_cv<std::pair<Ty1, Ty2>> {
    using type = std::pair<std::remove_cv_t<Ty1>, std::remove_cv_t<Ty2>>;
};

namespace detail {
template<typename... Ts>
void dummy_variadic(Ts&&...) {}
#if __cplusplus < 201703L
template<typename Ty>
bool and_variadic(const Ty& v1) {
    return !!v1;
}
template<typename Ty, typename... Ts>
bool and_variadic(const Ty& v1, const Ts&... vn) {
    return (!!v1 && and_variadic(vn...));
}
template<typename Ty>
bool or_variadic(const Ty& v1) {
    return !!v1;
}
template<typename Ty, typename... Ts>
bool or_variadic(const Ty& v1, const Ts&... vn) {
    return (!!v1 || or_variadic(vn...));
}
#else   // __cplusplus < 201703L
template<typename Ty, typename... Ts>
bool and_variadic(const Ty& v1, const Ts&... vn) {
    return (!!v1 && ... && !!vn);
}
template<typename Ty, typename... Ts>
bool or_variadic(const Ty& v1, const Ts&... vn) {
    return (!!v1 || ... || !!vn);
}
#endif  // __cplusplus < 201703L
}  // namespace detail

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
