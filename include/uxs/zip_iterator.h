#pragma once

#include "iterator.h"
#include "metaprog_alg.h"

#include <tuple>

namespace uxs {

namespace detail {
template<typename... Iter>
struct zip_iterator_category {
    using tag = std::conditional_t<std::conjunction<is_bidirectional_iterator<Iter>...>::value,
                                   std::conditional_t<std::conjunction<is_random_access_iterator<Iter>...>::value,
                                                      std::random_access_iterator_tag, std::bidirectional_iterator_tag>,
                                   std::forward_iterator_tag>;
};
}  // namespace detail

template<typename... Iter>
class zip_iterator
    : public iterator_facade<zip_iterator<Iter...>, std::tuple<typename std::iterator_traits<Iter>::value_type...>,
                             typename detail::zip_iterator_category<Iter...>::tag,
                             std::tuple<typename std::iterator_traits<Iter>::reference...>,
                             std::tuple<typename std::iterator_traits<Iter>::pointer...>> {
    static_assert(sizeof...(Iter) != 0, "base iterator list is empty");
    static_assert(std::conjunction<is_forward_iterator<Iter>...>::value,
                  "all iterators must satisfy at least forward iterator concept");

 private:
    using super = iterator_facade<zip_iterator, std::tuple<typename std::iterator_traits<Iter>::value_type...>,
                                  typename detail::zip_iterator_category<Iter...>::tag,
                                  std::tuple<typename std::iterator_traits<Iter>::reference...>,
                                  std::tuple<typename std::iterator_traits<Iter>::pointer...>>;

 public:
    using reference = typename super::reference;
    using difference_type = typename super::difference_type;

    zip_iterator() noexcept = default;
    zip_iterator(Iter... curr) noexcept : curr_(std::make_tuple(curr...)) {}

    void increment() noexcept(noexcept(this->increment_impl(std::index_sequence_for<Iter...>{}))) {
        increment_impl(std::index_sequence_for<Iter...>{});
    }

    void decrement() noexcept(noexcept(this->decrement_impl(std::index_sequence_for<Iter...>{}))) {
        decrement_impl(std::index_sequence_for<Iter...>{});
    }

    void advance(difference_type j) noexcept(noexcept(this->advance_impl(j, std::index_sequence_for<Iter...>{}))) {
        advance_impl(j, std::index_sequence_for<Iter...>{});
    }

    reference dereference() const noexcept { return dereference_impl(std::index_sequence_for<Iter...>{}); }

    bool is_equal_to(const zip_iterator& it) const noexcept {
        return is_equal_to_impl(it, std::index_sequence_for<Iter...>{});
    }

    bool is_less_than(const zip_iterator& it) const noexcept {
        return is_less_than_impl(it, std::index_sequence_for<Iter...>{});
    }

    difference_type distance_to(const zip_iterator& it) const noexcept {
        return distance_to_impl(it, std::index_sequence_for<Iter...>{});
    }

    template<std::size_t Index>
    typename std::tuple_element<Index, std::tuple<Iter...>>::type base() const {
        return std::get<Index>(curr_);
    }

 private:
    std::tuple<Iter...> curr_;

    template<std::size_t... Indices>
    void increment_impl(std::index_sequence<Indices...>) noexcept(
        std::conjunction<std::bool_constant<noexcept(++std::get<Indices>(curr_))>...>::value) {
        detail::dummy_variadic(++std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    void decrement_impl(std::index_sequence<Indices...>) noexcept(
        std::conjunction<std::bool_constant<noexcept(--std::get<Indices>(curr_))>...>::value) {
        detail::dummy_variadic(--std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    void advance_impl(difference_type j, std::index_sequence<Indices...>) noexcept(
        std::conjunction<std::bool_constant<noexcept(std::get<Indices>(curr_) += j)>...>::value) {
        detail::dummy_variadic(std::get<Indices>(curr_) += j...);
    }

    template<std::size_t... Indices>
    reference dereference_impl(std::index_sequence<Indices...>) const noexcept {
        return std::forward_as_tuple(*std::get<Indices>(curr_)...);
    }

    template<std::size_t... Indices>
    bool is_equal_to_impl(const zip_iterator& it, std::index_sequence<Indices...>) const noexcept {
        return detail::or_variadic(std::get<Indices>(curr_) == std::get<Indices>(it.curr_)...);
    }

    template<std::size_t... Indices>
    bool is_less_than_impl(const zip_iterator& it, std::index_sequence<Indices...>) const noexcept {
        return detail::and_variadic(std::get<Indices>(curr_) < std::get<Indices>(it.curr_)...);
    }

    template<std::size_t... Indices>
    difference_type distance_to_impl(const zip_iterator& it, std::index_sequence<Indices...>) const noexcept {
        return std::min({std::distance(std::get<Indices>(curr_), std::get<Indices>(it.curr_))...});
    }
};

template<typename... Iter>
zip_iterator<Iter...> make_zip_iterator(Iter... its) noexcept {
    return zip_iterator<Iter...>(its...);
}

template<typename... Range>
auto zip(Range&&... r) noexcept -> iterator_range<zip_iterator<decltype(std::end(r))...>> {
    return {zip_iterator<decltype(std::end(r))...>(std::begin(r)...),
            zip_iterator<decltype(std::end(r))...>(std::end(r)...)};
}

}  // namespace uxs
