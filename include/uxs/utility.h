#pragma once

#include "common.h"

#include <initializer_list>  // NOLINT
#include <type_traits>
#include <utility>

#define UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM_(ty, modifiers) \
    modifiers ty operator|(ty lhs, ty rhs) { \
        return static_cast<ty>(static_cast<typename std::underlying_type<ty>::type>(lhs) | \
                               static_cast<typename std::underlying_type<ty>::type>(rhs)); \
    } \
    modifiers ty operator&(ty lhs, ty rhs) { \
        return static_cast<ty>(static_cast<typename std::underlying_type<ty>::type>(lhs) & \
                               static_cast<typename std::underlying_type<ty>::type>(rhs)); \
    } \
    modifiers ty operator^(ty lhs, ty rhs) { \
        return static_cast<ty>(static_cast<typename std::underlying_type<ty>::type>(lhs) ^ \
                               static_cast<typename std::underlying_type<ty>::type>(rhs)); \
    } \
    modifiers ty operator~(ty flags) { \
        return static_cast<ty>(~static_cast<typename std::underlying_type<ty>::type>(flags)); \
    } \
    modifiers bool operator!(ty flags) { return static_cast<typename std::underlying_type<ty>::type>(flags) == 0; } \
    modifiers ty& operator|=(ty& lhs, ty rhs) { return lhs = lhs | rhs; } \
    modifiers ty& operator&=(ty& lhs, ty rhs) { return lhs = lhs & rhs; } \
    modifiers ty& operator^=(ty& lhs, ty rhs) { return lhs = lhs ^ rhs; } \
    static_assert(true, "")
#define UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM(ty) UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM_(ty, inline UXS_CONSTEXPR)
#define UXS_IMPLEMENT_FRIEND_BITWISE_OPS_FOR_ENUM(ty) \
    UXS_IMPLEMENT_BITWISE_OPS_FOR_ENUM_(ty, friend inline UXS_CONSTEXPR)

namespace est {
template<typename... Ts>
struct always_true : std::true_type {};
template<typename Ty, typename... Ts>
struct type_identity {
    using type = Ty;
};
template<typename Ty, typename... Ts>
using type_identity_t = typename type_identity<Ty, Ts...>::type;
}  // namespace est

#if __cplusplus < 201703L
namespace est {
namespace detail {
template<std::size_t... Indices>
struct index_sequence {};
template<std::size_t N, std::size_t... Next>
struct make_index_sequence : make_index_sequence<N - 1U, N - 1U, Next...> {};
template<std::size_t... Next>
struct make_index_sequence<0U, Next...> {
    using type = index_sequence<Next...>;
};
}  // namespace detail
}  // namespace est
namespace std {
#    if __cplusplus < 201402L
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
#    endif  // type traits
#    if !defined(__cpp_lib_bool_constant)
template<bool B>
using bool_constant = integral_constant<bool, B>;
#    endif  // bool constant
#    if !defined(__cpp_lib_as_const)
template<typename Ty>
add_const_t<Ty>& as_const(Ty& t) {
    return t;
}
template<typename Ty>
void as_const(const Ty&&) = delete;
#    endif  // as const
#    if !defined(__cpp_lib_void_t)
template<typename Ty, typename... Ts>
using void_t = typename est::type_identity<void, Ty, Ts...>::type;
#    endif  // void_t
#    if !defined(__cpp_lib_is_swappable)
template<typename Ty>
using is_nothrow_swappable = bool_constant<noexcept(swap(declval<Ty&>(), declval<Ty&>()))>;
#    endif  // is swappable
#    if __cplusplus < 201402L && !defined(__cpp_lib_integer_sequence)
template<std::size_t... Indices>
using index_sequence = est::detail::index_sequence<Indices...>;
template<std::size_t N>
using make_index_sequence = typename est::detail::make_index_sequence<N>::type;
template<typename... Ts>
using index_sequence_for = make_index_sequence<sizeof...(Ts)>;
#    endif  // integer sequence
}  // namespace std
#endif  // __cplusplus < 201703L

#if !defined(__cpp_lib_remove_cvref)
namespace std {
template<typename Ty>
using remove_cvref = remove_cv<remove_reference_t<Ty>>;
template<typename Ty>
using remove_cvref_t = typename remove_cvref<Ty>::type;
}  // namespace std
#endif  // remove_cvref

namespace est {

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

#if __cplusplus < 201703L
struct in_place_t {};
inline in_place_t in_place() { return in_place_t{}; }
template<typename Ty>
struct in_place_type_t {};
template<typename Ty>
inline in_place_type_t<Ty> in_place_type() {
    return in_place_type_t<Ty>{};
}
#else   //__cplusplus < 201703L
using in_place_t = std::in_place_t;
constexpr in_place_t in_place() { return std::in_place; }
template<typename Ty>
using in_place_type_t = std::in_place_type_t<Ty>;
template<typename Ty>
constexpr in_place_type_t<Ty> in_place_type() {
    return std::in_place_type<Ty>;
}
#endif  //__cplusplus < 201703L

}  // namespace est

namespace uxs {

template<typename Ty, typename = void>
struct is_boolean : std::false_type {};
template<typename Ty>
struct is_boolean<Ty, std::enable_if_t<std::is_same<std::remove_cv_t<Ty>, bool>::value>> : std::true_type {};

template<typename CharT, typename = void>
struct is_character : std::false_type {};
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, char>::value ||
                                            std::is_same<std::remove_cv_t<CharT>, wchar_t>::value ||
                                            std::is_same<std::remove_cv_t<CharT>, char16_t>::value ||
                                            std::is_same<std::remove_cv_t<CharT>, char32_t>::value>> : std::true_type {
};
#if __cplusplus >= 202002L
template<typename CharT>
struct is_character<CharT, std::enable_if_t<std::is_same<std::remove_cv_t<CharT>, char8_t>::value>> : std::true_type {};
#endif  // __cplusplus >= 202002L

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
Ty get_and_set(Ty& v, Ty v_new) {
    std::swap(v, v_new);
    return v_new;
}

template<typename Ty, std::size_t Offset, typename MemberTy>
Ty* get_containing_record(MemberTy* member_ptr) {
    return reinterpret_cast<Ty*>(reinterpret_cast<std::uint8_t*>(member_ptr) - Offset);
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
