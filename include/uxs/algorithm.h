#pragma once

#include "utility.h"

#include <algorithm>
#include <iterator>
#include <tuple>

namespace uxs {

// ---- find & contains

namespace detail {
template<typename Container, typename Key>
auto find(Container&& c, const Key& k)
    -> std::enable_if_t<std::is_same<decltype(c.find(k)), decltype(std::end(c))>::value,
                        std::pair<decltype(std::end(c)), bool>> {
    const auto it = c.find(k);
    return std::make_pair(it, it != std::end(c));
}
template<typename Range, typename Val, typename... Dummy>
auto find(Range&& r, const Val& v, Dummy&&...) -> std::pair<decltype(std::end(r)), bool> {
    const auto it = std::find(std::begin(r), std::end(r), v);
    return std::make_pair(it, it != std::end(r));
}
}  // namespace detail

template<typename Range, typename Key>
auto find(Range&& r, const Key& k) -> std::pair<decltype(std::end(r)), bool> {
    return detail::find(std::forward<Range>(r), k);
}

template<typename Range, typename Pred>
auto find_if(Range&& r, Pred p) -> std::pair<decltype(std::end(r)), bool> {
    const auto it = std::find_if(std::begin(r), std::end(r), p);
    return std::make_pair(it, it != std::end(r));
}

template<typename Range, typename Key>
bool contains(const Range& r, const Key& k) {
    return detail::find(r, k).second;
}

// ---- erase

namespace detail {
template<typename Container, typename Range, typename Val>
auto erase(Container& c, Range&& r, const Val& v) -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    c.erase(std::remove(std::begin(r), std::end(r), v), std::end(r));
    return prev_sz - c.size();
}
template<typename Container, typename Range, typename Val, typename... Dummy>
auto erase(Container& c, Range&& r, const Val& v, Dummy&&...) -> decltype(std::end(c) == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    auto first = std::begin(r);
    const auto last = std::end(r);
    while (first != last) {
        if (*first == v) {
            first = c.erase(first);
        } else {
            ++first;
        }
    }
    return prev_sz - c.size();
}
template<typename Container, typename Val>
auto erase_test_associative(Container& c, const Val& v)
    -> std::enable_if_t<std::is_same<decltype(c.find(v)), decltype(std::end(c))>::value, decltype(c.size())> {
    static_assert(!est::always_true<Val>::value,
                  "function `uxs::erase` shouldn't be used for associative containers to erase by key!");
    return 0;
}
template<typename Container, typename Val, typename... Dummy>
auto erase_test_associative(Container& c, const Val& v, Dummy&&...) -> decltype(c.size()) {
    return detail::erase(c, c, v);
}
}  // namespace detail

template<typename Container, typename Val>
auto erase(Container& c, const Val& v) -> decltype(c.size()) {
    return detail::erase_test_associative(c, v);
}

template<typename Container, typename Range, typename Val>
auto erase(Container& c, Range&& r, const Val& v) -> decltype(c.size()) {
    return detail::erase(c, std::forward<Range>(r), v);
}

template<typename Container, typename Range>
auto erase_range(Container& c, Range&& r) -> decltype(std::end(c) == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    c.erase(std::begin(r), std::end(r));
    return prev_sz - c.size();
}

// ---- erase_if

namespace detail {
template<typename Container, typename Range, typename Pred>
auto erase_if(Container& c, Range&& r, Pred p) -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    c.erase(std::remove_if(std::begin(r), std::end(r), p), std::end(r));
    return prev_sz - c.size();
}
template<typename Container, typename Range, typename Pred, typename... Dummy>
auto erase_if(Container& c, Range&& r, Pred p, Dummy&&...) -> decltype(std::end(c) == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    auto first = std::begin(r);
    const auto last = std::end(r);
    while (first != last) {
        if (p(*first)) {
            first = c.erase(first);
        } else {
            ++first;
        }
    }
    return prev_sz - c.size();
}
}  // namespace detail

template<typename Container, typename Pred>
auto erase_if(Container& c, Pred p) -> decltype(c.size()) {
    return detail::erase_if(c, c, p);
}

template<typename Container, typename Range, typename Pred>
auto erase_if(Container& c, Range&& r, Pred p) -> decltype(c.size()) {
    return detail::erase_if(c, std::forward<Range>(r), p);
}

// ---- erase_duplicates

namespace detail {
template<typename Container, typename Range, typename Pred>
auto erase_duplicates(Container& c, Range&& r, Pred p) -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    c.erase(std::unique(std::begin(r), std::end(r), p), std::end(r));
    return prev_sz - c.size();
}
template<typename Container, typename Range, typename Pred, typename... Dummy>
auto erase_duplicates(Container& c, Range&& r, Pred p, Dummy&&...) -> decltype(std::end(c) == std::end(r), c.size()) {
    const auto prev_sz = c.size();
    if (prev_sz == 0) { return 0; }
    auto first0 = std::begin(r);
    auto first = std::next(first0);
    const auto last = std::end(r);
    while (first != last) {
        if (p(*first0, *first)) {
            first = c.erase(first);
        } else {
            first0 = first;
            ++first;
        }
    }
    return prev_sz - c.size();
}
}  // namespace detail

template<typename Container>
auto erase_duplicates(Container& c) -> decltype(c.size()) {
    return detail::erase_duplicates(c, c, [](decltype(*std::end(c)) x, decltype(*std::end(c)) y) { return x == y; });
}

template<typename Container, typename Pred>
auto erase_duplicates(Container& c, Pred p) -> decltype(c.size()) {
    return detail::erase_duplicates(c, c, p);
}

template<typename Container, typename Range, typename Pred>
auto erase_duplicates(Container& c, Range&& r, Pred p) -> decltype(c.size()) {
    return detail::erase_duplicates(c, std::forward<Range>(r), p);
}

// ---- emplace & erase for random access containers

template<typename Container, typename... Args>
auto emplace_at(Container& c, std::size_t i, Args&&... args) -> std::void_t<decltype(std::begin(c) + i)> {
    c.emplace(std::begin(c) + i, std::forward<Args>(args)...);
}

template<typename Container>
auto erase_at(Container& c, std::size_t i) -> std::void_t<decltype(std::begin(c) + i)> {
    c.erase(std::begin(c) + i);
}

// ---- sorted range lower bound, upper bound & equal range

namespace detail {
template<typename Iter, typename Key, typename KeyFn>
Iter lower_bound(Iter first, std::size_t count, const Key& k, KeyFn fn) {
    while (count > 0) {
        const std::size_t count2 = count / 2;
        const auto mid = std::next(first, count2);
        if (fn(*mid) < k) {
            first = std::next(mid);
            count -= count2 + 1;
        } else {
            count = count2;
        }
    }
    return first;
}
template<typename Iter, typename Key, typename KeyFn>
Iter upper_bound(Iter first, std::size_t count, const Key& k, KeyFn fn) {
    while (count > 0) {
        const std::size_t count2 = count / 2;
        const auto mid = std::next(first, count2);
        if (!(k < fn(*mid))) {
            first = std::next(mid);
            count -= count2 + 1;
        } else {
            count = count2;
        }
    }
    return first;
}
}  // namespace detail

template<typename Range, typename Key, typename KeyFn = identity>
auto lower_bound(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::begin(r) + 1) {
    return detail::lower_bound(std::begin(r), static_cast<std::size_t>(std::end(r) - std::begin(r)), k, fn);
}

template<typename Range, typename Key, typename KeyFn = identity>
auto upper_bound(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::begin(r) + 1) {
    return detail::upper_bound(std::begin(r), static_cast<std::size_t>(std::end(r) - std::begin(r)), k, fn);
}

template<typename Range, typename Key, typename KeyFn = identity>
auto equal_range(Range&& r, const Key& k, KeyFn fn = KeyFn{})
    -> std::pair<decltype(std::begin(r) + 1), decltype(std::end(r))> {
    auto first = std::begin(r);
    std::size_t count = static_cast<std::size_t>(std::end(r) - first);
    while (count > 0) {
        const std::size_t count2 = count / 2;
        const auto mid = std::next(first, count2);
        if (fn(*mid) < k) {
            first = std::next(mid);
            count -= count2 + 1;
        } else if (k < fn(*mid)) {
            count = count2;
        } else {
            return std::make_pair(detail::lower_bound(first, count2, k, fn),
                                  detail::upper_bound(std::next(mid), count - count2 - 1, k, fn));
        }
    }
    return std::make_pair(first, first);
}

// ---- sorted range find

template<typename Range, typename Key, typename KeyFn = identity>
auto binary_find(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> std::pair<decltype(std::end(r)), bool> {
    const auto it = lower_bound(r, k, fn);
    return std::make_pair(it, (it != std::end(r)) && !(k < fn(*it)));
}

template<typename Range, typename Key, typename KeyFn = identity>
bool binary_contains(const Range& r, const Key& k, KeyFn fn = KeyFn{}) {
    return binary_find(r, k, fn).second;
}

// ---- sorted container insert & remove

namespace detail {
template<typename Container, typename Key, typename... Args, std::size_t... Indices, typename KeyFn>
auto binary_emplace_unique(Container& c, const Key& k, std::tuple<Args...>& args, std::index_sequence<Indices...>,
                           KeyFn fn) -> std::pair<decltype(std::end(c)), bool> {
    const auto result = uxs::binary_find(c, k, fn);
    if (result.second) { return std::make_pair(result.first, false); }
    return std::make_pair(c.emplace(result.first, std::forward<Args>(std::get<Indices>(args))...), true);
}
template<typename Container, typename Key, typename... Args, std::size_t... Indices, typename KeyFn>
auto binary_emplace_new(Container& c, const Key& k, std::tuple<Args...>& args, std::index_sequence<Indices...>,
                        KeyFn fn) -> decltype(std::end(c)) {
    return c.emplace(uxs::lower_bound(c, k, fn), std::forward<Args>(std::get<Indices>(args))...);
}
}  // namespace detail

template<typename Container, typename Key, typename... Args, typename KeyFn = identity>
auto binary_emplace_unique(Container& c, const Key& k, std::tuple<Args...> args, KeyFn fn = KeyFn{})
    -> std::pair<decltype(std::end(c)), bool> {
    return detail::binary_emplace_unique(c, k, args, std::index_sequence_for<Args...>(), fn);
}

template<typename Container, typename Val, typename KeyFn = identity>
auto binary_insert_unique(Container& c, Val&& v, KeyFn fn = KeyFn{}) -> std::pair<decltype(std::end(c)), bool> {
    return binary_emplace_unique(c, fn(v), std::forward_as_tuple(std::forward<Val>(v)), fn);
}

template<typename Container, typename Key, typename KeyFn = identity>
auto binary_access_unique(Container& c, Key&& k, KeyFn fn = KeyFn{}) -> decltype(*std::begin(c)) {
    auto result = binary_find(c, k, fn);
    if (result.second) { return *result.first; }
    result.first = c.emplace(result.first);
    fn(*result.first) = std::forward<Key>(k);
    return *result.first;
}

template<typename Container, typename Key, typename... Args, typename KeyFn = identity>
auto binary_emplace_new(Container& c, const Key& k, std::tuple<Args...> args, KeyFn fn = KeyFn{})
    -> decltype(std::end(c)) {
    return detail::binary_emplace_new(c, k, args, std::index_sequence_for<Args...>(), fn);
}

template<typename Container, typename Val, typename KeyFn = identity>
auto binary_insert_new(Container& c, Val&& v, KeyFn fn = KeyFn{}) -> decltype(std::end(c)) {
    return binary_emplace_new(c, fn(v), std::forward_as_tuple(std::forward<Val>(v)), fn);
}

template<typename Container, typename Key, typename KeyFn = identity>
auto binary_access_new(Container& c, Key&& k, KeyFn fn = KeyFn{}) -> decltype(*std::begin(c)) {
    const auto it = c.emplace(lower_bound(c, k, fn));
    fn(*it) = std::forward<Key>(k);
    return *it;
}

template<typename Container, typename Key, typename KeyFn = identity>
auto binary_erase_one(Container& c, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::end(c)) {
    const auto result = binary_find(c, k, fn);
    if (result.second) { return c.erase(result.first); }
    return result.first;
}

template<typename Container, typename Key, typename KeyFn = identity>
auto binary_erase_all(Container& c, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::end(c)) {
    const auto prev_sz = c.size();
    const auto result = equal_range(c, k, fn);
    c.erase(result.first, result.second);
    return prev_sz - c.size();
}

// ---- loop

#if __cplusplus < 201703L
namespace detail {
template<typename Func, typename>
struct loop_helper;
template<typename Func>
struct loop_helper<Func, std::false_type> {
    template<typename Range, typename... InputIts>
    auto operator()(Range&& r, Func fn, InputIts... its) -> decltype(std::end(r)) {
        auto first = std::begin(r);
        for (const auto last = std::end(r); first != last; ++first, detail::dummy_variadic(++its...)) {
            if (!fn(*first, *its...)) { break; }
        }
        return first;
    }
};
template<typename Func>
struct loop_helper<Func, std::true_type> {
    template<typename Range, typename... InputIts>
    auto operator()(Range&& r, Func fn, InputIts... its) -> decltype(std::end(r)) {
        auto first = std::begin(r);
        for (const auto last = std::end(r); first != last; ++first, detail::dummy_variadic(++its...)) {
            fn(*first, *its...);
        }
        return first;
    }
};
}  // namespace detail
template<typename Range, typename Func, typename... InputIts>
auto loop_for(Range&& r, Func fn, InputIts... its) -> decltype(std::end(r)) {
    return detail::loop_helper<Func, typename std::is_same<decltype(fn(*std::begin(r), *its...)), void>::type>()(
        std::forward<Range>(r), fn, its...);
}
#else   // __cplusplus < 201703L
template<typename Range, typename Func, typename... InputIts>
auto loop_for(Range&& r, Func fn, InputIts... its) -> decltype(std::end(r)) {
    auto first = std::begin(r);
    for (const auto last = std::end(r); first != last; (++first, ..., ++its)) {
        if constexpr (std::is_same_v<decltype(fn(*std::begin(r), *its...)), void>) {
            fn(*first, *its...);
        } else {
            if (!fn(*first, *its...)) { break; }
        }
    }
    return first;
}
#endif  // __cplusplus < 201703L

}  // namespace uxs
