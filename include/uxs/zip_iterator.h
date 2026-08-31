#pragma once

#include "iterator.h"
#include "type_traits.h"

#include <array>
#include <tuple>

namespace uxs {

namespace detail {
template<typename... Iters>
using zip_iterator_tag =
    std::conditional_t<std::conjunction<is_bidirectional_iterator<Iters>...>::value,
                       std::conditional_t<std::conjunction<is_random_access_iterator<Iters>...>::value,
                                          std::random_access_iterator_tag, std::bidirectional_iterator_tag>,
                       std::forward_iterator_tag>;
}  // namespace detail

template<typename... Iters>
class zip_iterator
    : public iterator_facade<zip_iterator<Iters...>, std::tuple<typename std::iterator_traits<Iters>::value_type...>,
                             detail::zip_iterator_tag<Iters...>,
                             std::tuple<typename std::iterator_traits<Iters>::reference...>,
                             std::tuple<typename std::iterator_traits<Iters>::pointer...>> {
    static_assert(sizeof...(Iters) != 0, "base iterator list is empty");
    static_assert(std::conjunction<is_forward_iterator<Iters>...>::value,
                  "all iterators must satisfy at least forward iterator concept");

 private:
    using super = iterator_facade<zip_iterator, std::tuple<typename std::iterator_traits<Iters>::value_type...>,
                                  detail::zip_iterator_tag<Iters...>,
                                  std::tuple<typename std::iterator_traits<Iters>::reference...>,
                                  std::tuple<typename std::iterator_traits<Iters>::pointer...>>;

 public:
    using reference = typename super::reference;
    using difference_type = typename super::difference_type;

    UXS_CONSTEXPR zip_iterator() = default;
    UXS_CONSTEXPR zip_iterator(Iters... its) : curr_(std::make_tuple(its...)) {}

    UXS_CONSTEXPR void increment() { increment_impl(std::index_sequence_for<Iters...>()); }
    UXS_CONSTEXPR void decrement() { decrement_impl(std::index_sequence_for<Iters...>()); }
    UXS_CONSTEXPR void advance(difference_type j) { advance_impl(j, std::index_sequence_for<Iters...>()); }
    UXS_CONSTEXPR reference dereference() const { return dereference_impl(std::index_sequence_for<Iters...>()); }

    UXS_CONSTEXPR bool is_equal_to(const zip_iterator& it) const {
        return is_equal_to_impl(it, std::index_sequence_for<Iters...>());
    }

    UXS_CONSTEXPR bool is_less_than(const zip_iterator& it) const {
        return is_less_than_impl(it, std::index_sequence_for<Iters...>());
    }

    UXS_CONSTEXPR difference_type distance_to(const zip_iterator& it) const {
        return distance_to_impl(it, std::index_sequence_for<Iters...>());
    }

    template<std::size_t I>
    UXS_CONSTEXPR est::type_pack_element_t<I, Iters...> base() const {
        return std::get<I>(curr_);
    }

 private:
    std::tuple<Iters...> curr_;

    template<std::size_t... Indices>
    UXS_CONSTEXPR void increment_impl(std::index_sequence<Indices...>) {
        detail::dummy_variadic(++std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR void decrement_impl(std::index_sequence<Indices...>) {
        detail::dummy_variadic(--std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR void advance_impl(difference_type j, std::index_sequence<Indices...>) {
        detail::dummy_variadic(std::get<Indices>(curr_) += j...);
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR reference dereference_impl(std::index_sequence<Indices...>) const {
        return std::forward_as_tuple(*std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR bool is_equal_to_impl(const zip_iterator& it, std::index_sequence<Indices...>) const {
        return detail::or_variadic(std::get<Indices>(curr_) == std::get<Indices>(it.curr_)...);
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR bool is_less_than_impl(const zip_iterator& it, std::index_sequence<Indices...>) const {
        return detail::and_variadic(std::get<Indices>(curr_) < std::get<Indices>(it.curr_)...);
    }

    template<typename InputIt>
    static UXS_CONSTEXPR InputIt abs_min_element(InputIt first, InputIt last) {
        auto it = first;
        while (++first != last) {
            if (std::abs(*first) < std::abs(*it)) { it = first; }
        }
        return it;
    }

    template<std::size_t... Indices>
    UXS_CONSTEXPR difference_type distance_to_impl(const zip_iterator& it, std::index_sequence<Indices...>) const {
        const std::array<difference_type, sizeof...(Indices)> diffs{
            difference_type(std::get<Indices>(it.curr_) - std::get<Indices>(curr_))...};
        return *abs_min_element(diffs.begin(), diffs.end());
    }
};

template<typename... Iters>
zip_iterator<Iters...> make_zip_iterator(Iters... its) {
    return zip_iterator<Iters...>(its...);
}

template<typename... Range>
auto zip(Range&&... r) -> iterator_range<zip_iterator<decltype(std::end(r))...>> {
    return {make_zip_iterator(std::begin(r)...), make_zip_iterator(std::end(r)...)};
}

}  // namespace uxs
