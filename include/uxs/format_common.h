#pragma once

#include "string_conv_common.h"

namespace uxs {

enum class range_format { disabled = 0, sequence, set, map, string };

template<typename Ty, typename CharT = char, typename = void>
struct formatter;

template<typename Range, typename CharT = char>
struct format_kind;

template<typename CharT>
class basic_format_context;

namespace fmt {
template<typename Ty, typename CharT, typename = void>
struct reduce_type {
    using type = Ty;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT,
                   std::enable_if_t<std::is_integral<Ty>::value && std::is_unsigned<Ty>::value &&
                                    !est::is_boolean<Ty>::value && !est::is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::uint32_t)), std::uint32_t, std::uint64_t>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT,
                   std::enable_if_t<std::is_integral<Ty>::value && std::is_signed<Ty>::value &&
                                    !est::is_boolean<Ty>::value && !est::is_character<Ty>::value>> {
    using type = std::conditional_t<(sizeof(Ty) <= sizeof(std::int32_t)), std::int32_t, std::int64_t>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT, std::enable_if_t<est::is_character<Ty>::value>> {
    using type = char32_t;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty, CharT, std::enable_if_t<is_string_like<Ty>::value>> {
    using type = std::basic_string_view<typename string_char_traits_t<Ty>::char_type>;
};
template<typename Ty, typename CharT>
struct reduce_type<Ty*, CharT, std::enable_if_t<!est::is_character<Ty>::value>> {
    using type = const void*;
};
template<typename Ty, typename CharT>
struct reduce_type<
    Ty, CharT,
    std::enable_if_t<std::is_array<Ty>::value && !est::is_character<typename std::remove_extent<Ty>::type>::value>> {
    using type = const void*;
};
template<typename CharT>
struct reduce_type<std::nullptr_t, CharT> {
    using type = const void*;
};
template<typename Ty, typename CharT>
using reduce_type_t = typename reduce_type<std::remove_cv_t<Ty>, CharT>::type;
}  // namespace fmt

// --------------------------

namespace detail {
template<typename Ty, typename FmtCtx>
struct formattable_impl {
    template<typename U>
    static auto test(U* ctx, typename FmtCtx::parse_context* parse_ctx) -> est::always_true<
        decltype(parse_ctx->advance_to(std::declval<formatter<Ty, typename U::char_type>&>().parse(*parse_ctx))),
        decltype(std::declval<const formatter<Ty, typename U::char_type>&>().format(*ctx, std::declval<const Ty&>()))>;
    template<typename U>
    static std::false_type test(...);
    using type = decltype(test<FmtCtx>(nullptr, nullptr));
};
}  // namespace detail

template<typename Ty, typename CharT = char>
struct is_formattable : detail::formattable_impl<fmt::reduce_type_t<Ty, CharT>, basic_format_context<CharT>>::type {};
#if __cplusplus >= 201402L
template<typename Ty, typename CharT = char>
constexpr bool is_formattable_v = is_formattable<Ty, CharT>::value;
#endif  // __cplusplus >= 201402L
#if __cplusplus >= 202002L && defined(__cpp_concepts)
template<typename Ty, typename CharT = char>
concept formattable = is_formattable_v<Ty, CharT>;
#endif  // cpp_concepts

template<typename Ty, typename CharT = char>
using formatter_t = formatter<fmt::reduce_type_t<Ty, CharT>, CharT>;

}  // namespace uxs
