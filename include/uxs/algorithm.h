#pragma once

#include "functional.h"

#include <algorithm>

namespace uxs {

// ---- find & contains

namespace detail {
template<typename Container, typename Key>
auto find(Container&& c, const Key& k)
    -> std::enable_if_t<std::is_same<decltype(c.find(k)), decltype(std::end(c))>::value,
                        std::pair<decltype(std::end(c)), bool>> {
    auto it = c.find(k);
    return std::make_pair(it, it != std::end(c));
}
template<typename Range, typename Val, typename... Dummy>
auto find(Range&& r, const Val& v, Dummy&&...) -> std::pair<decltype(std::end(r)), bool> {
    auto it = std::find(std::begin(r), std::end(r), v);
    return std::make_pair(it, it != std::end(r));
}
}  // namespace detail

template<typename Range, typename Key>
auto find(Range&& r, const Key& k) -> std::pair<decltype(std::end(r)), bool> {
    return detail::find(std::forward<Range>(r), k);
}

template<typename Range, typename Pred>
auto find_if(Range&& r, Pred p) -> std::pair<decltype(std::end(r)), bool> {
    auto it = std::find_if(std::begin(r), std::end(r), p);
    return std::make_pair(it, it != std::end(r));
}

template<typename Range, typename Key>
bool contains(const Range& r, const Key& k) {
    return detail::find(r, k).second;
}

template<typename Range, typename Pred>
bool contains_if(const Range& r, Pred p) {
    return std::find_if(std::begin(r), std::end(r), p) != std::end(r);
}

// ---- erase

namespace detail {
template<typename Container, typename Range, typename Val>
auto erase(Container& c, Range&& r, const Val& v) -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    auto old_sz = c.size();
    c.erase(std::remove(std::begin(r), std::end(r), v), std::end(r));
    return old_sz - c.size();
}
template<typename Container, typename Range, typename Val, typename... Dummy>
auto erase(Container& c, Range&& r, const Val& v, Dummy&&...) -> decltype(std::end(c) == std::end(r), c.size()) {
    auto old_sz = c.size();
    for (auto first = std::begin(r), last = std::end(r); first != last;) {
        if (*first == v) {
            first = c.erase(first);
        } else {
            ++first;
        }
    }
    return old_sz - c.size();
}

template<typename Container, typename Val>
auto erase_val(Container& c, const Val& v)
    -> std::enable_if_t<std::is_same<decltype(c.find(v)), decltype(std::end(c))>::value, decltype(c.size())> {
    static_assert(!std::is_same<Val, Val>::value,
                  "function `uxs::erase_one` should be used for associative containers to erase by key!");
    return 0;
}
template<typename Container, typename Val, typename... Dummy>
auto erase_val(Container& c, const Val& v, Dummy&&...) -> decltype(c.size()) {
    return detail::erase(c, c, v);
}
}  // namespace detail

template<typename Container, typename Range, typename Val>
auto erase(Container& c, Range&& r, const Val& v) -> decltype(c.size()) {
    return detail::erase(c, std::forward<Range>(r), v);
}

template<typename Container, typename Val>
auto erase(Container& c, const Val& v) -> decltype(c.size()) {
    return detail::erase_val(c, v);
}

template<typename Container, typename Key>
auto erase_one(Container& c, const Key& k) -> decltype(std::end(c)) {
    auto result = detail::find(c, k);
    if (result.second) { return c.erase(result.first); }
    return result.first;
}

namespace detail {
template<typename Container, typename Range, typename Pred>
auto erase_if(Container& c, Range&& r, Pred p)  //
    -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    auto old_sz = c.size();
    c.erase(std::remove_if(std::begin(r), std::end(r), p), std::end(r));
    return old_sz - c.size();
}
template<typename Container, typename Range, typename Pred, typename... Dummy>
auto erase_if(Container& c, Range&& r, Pred p, Dummy&&...)  //
    -> decltype(std::end(c) == std::end(r), c.size()) {
    auto old_sz = c.size();
    for (auto first = std::begin(r), last = std::end(r); first != last;) {
        if (p(*first)) {
            first = c.erase(first);
        } else {
            ++first;
        }
    }
    return old_sz - c.size();
}
}  // namespace detail

template<typename Container, typename Range, typename Pred>
auto erase_if(Container& c, Range&& r, Pred p) -> decltype(c.size()) {
    return detail::erase_if(c, std::forward<Range>(r), p);
}

template<typename Container, typename Pred>
auto erase_if(Container& c, Pred p) -> decltype(c.size()) {
    return detail::erase_if(c, c, p);
}

template<typename Container, typename Range>
auto erase_range(Container& c, Range&& r) -> decltype(std::end(c) == std::end(r), c.size()) {
    auto old_sz = c.size();
    c.erase(std::begin(r), std::end(r));
    return old_sz - c.size();
}

// ---- unique

namespace detail {
template<typename Container, typename Range, typename Pred>
auto unique(Container& c, Range&& r, Pred p)  //
    -> decltype(std::begin(c) + 1 == std::end(r), c.size()) {
    auto old_sz = c.size();
    c.erase(std::unique(std::begin(r), std::end(r), p), std::end(r));
    return old_sz - c.size();
}
template<typename Container, typename Range, typename Pred, typename... Dummy>
auto unique(Container& c, Range&& r, Pred p, Dummy&&...)  //
    -> decltype(std::end(c) == std::end(r), c.size()) {
    auto old_sz = c.size();
    if (old_sz == 0) { return 0; }
    for (auto it0 = std::begin(r), first = std::next(it0), last = std::end(r); first != last;) {
        if (p(*it0, *first)) {
            first = c.erase(first);
        } else {
            it0 = first++;
        }
    }
    return old_sz - c.size();
}
}  // namespace detail

template<typename Container, typename Range, typename Pred>
auto unique(Container& c, Range&& r, Pred p) -> decltype(c.size()) {
    return detail::unique(c, std::forward<Range>(r), p);
}

template<typename Container, typename Pred = equal_to<>>
auto unique(Container& c, Pred p = Pred{}) -> decltype(c.size()) {
    return detail::unique(c, c, p);
}

// ---- emplace & erase for random access containers

template<typename Container, typename... Args>
auto emplace_at(Container& c, size_t i, Args&&... args) -> std::void_t<decltype(std::begin(c) + i)> {
    c.emplace(std::begin(c) + i, std::forward<Args>(args)...);
}

template<typename Container>
auto erase_at(Container& c, size_t i) -> std::void_t<decltype(std::begin(c) + i)> {
    c.erase(std::begin(c) + i);
}

// ---- sorted range lower bound, upper bound & equal range

namespace detail {
template<typename Iter, typename Key, typename KeyFn = key>
Iter lower_bound(Iter first, size_t count, const Key& k, KeyFn fn = KeyFn{}) {
    while (count > 0) {
        size_t count2 = count / 2;
        auto mid = std::next(first, count2);
        if (fn(*mid) < k) {
            first = ++mid;
            count -= count2 + 1;
        } else {
            count = count2;
        }
    }
    return first;
}
template<typename Iter, typename Key, typename KeyFn = key>
Iter upper_bound(Iter first, size_t count, const Key& k, KeyFn fn = KeyFn{}) {
    while (count > 0) {
        size_t count2 = count / 2;
        auto mid = std::next(first, count2);
        if (!(k < fn(*mid))) {
            first = ++mid;
            count -= count2 + 1;
        } else {
            count = count2;
        }
    }
    return first;
}
}  // namespace detail

template<typename Range, typename Key, typename KeyFn = key>
auto lower_bound(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::begin(r) + 1) {
    return detail::lower_bound(std::begin(r), static_cast<size_t>(std::end(r) - std::begin(r)), k, fn);
}

template<typename Range, typename Key, typename KeyFn = key>
auto upper_bound(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::begin(r) + 1) {
    return detail::upper_bound(std::begin(r), static_cast<size_t>(std::end(r) - std::begin(r)), k, fn);
}

template<typename Range, typename Key, typename KeyFn = key>
auto equal_range(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> iterator_range<decltype(std::begin(r) + 1)> {
    auto first = std::begin(r);
    size_t count = static_cast<size_t>(std::end(r) - first);
    while (count > 0) {
        size_t count2 = count / 2;
        auto mid = std::next(first, count2);
        if (fn(*mid) < k) {
            first = ++mid;
            count -= count2 + 1;
        } else if (k < fn(*mid)) {
            count = count2;
        } else {
            return make_range(detail::lower_bound(first, count2, k, fn),
                              detail::upper_bound(++mid, count - count2 - 1, k, fn));
        }
    }
    return make_range(first, first);
}

// ---- sorted range find

template<typename Range, typename Key, typename KeyFn = key>
auto binary_find(Range&& r, const Key& k, KeyFn fn = KeyFn{}) -> std::pair<decltype(std::end(r)), bool> {
    auto it = uxs::lower_bound(r, k, fn);
    return std::make_pair(it, (it != std::end(r)) && !(k < fn(*it)));
}

template<typename Range, typename Key, typename KeyFn = key>
bool binary_contains(const Range& r, const Key& k, KeyFn fn = KeyFn{}) {
    return uxs::binary_find(r, k, fn).second;
}

// ---- sorted container insert & remove

namespace detail {
template<typename Container, typename Key, typename... Args, size_t... Indices, typename KeyFn>
auto binary_emplace_unique(Container& c, const Key& k, const std::tuple<Args...>& args, std::index_sequence<Indices...>,
                           KeyFn fn) -> std::pair<decltype(std::end(c)), bool> {
    auto result = uxs::binary_find(c, k, fn);
    if (result.second) { return std::make_pair(result.first, false); }
    return std::make_pair(c.emplace(result.first, std::forward<Args>(std::get<Indices>(args))...), true);
}
}  // namespace detail

template<typename Container, typename Key, typename... Args, typename KeyFn = key>
auto binary_emplace_unique(Container& c, const Key& k, const std::tuple<Args...>& args = {}, KeyFn fn = KeyFn{})
    -> std::pair<decltype(std::end(c)), bool> {
    return detail::binary_emplace_unique(c, k, args, std::index_sequence_for<Args...>{}, fn);
}

template<typename Container, typename Val, typename KeyFn = key>
auto binary_insert_unique(Container& c, Val&& v, KeyFn fn = KeyFn{}) -> std::pair<decltype(std::end(c)), bool> {
    return uxs::binary_emplace_unique(c, fn(v), std::forward_as_tuple(std::forward<Val>(v)), fn);
}

template<typename Container, typename Key, typename KeyFn = key>
auto binary_access_unique(Container& c, Key&& k, KeyFn fn = KeyFn{}) -> decltype(*std::begin(c)) {
    auto result = uxs::binary_find(c, k, fn);
    if (result.second) { return *result.first; }
    result.first = c.emplace(result.first);
    fn(*result.first) = std::forward<Key>(k);
    return *result.first;
}

namespace detail {
template<typename Container, typename Key, typename... Args, size_t... Indices, typename KeyFn>
auto binary_emplace_new(Container& c, const Key& k, const std::tuple<Args...>& args, std::index_sequence<Indices...>,
                        KeyFn fn) -> decltype(std::end(c)) {
    return c.emplace(uxs::lower_bound(c, k, fn), std::forward<Args>(std::get<Indices>(args))...);
}
}  // namespace detail

template<typename Container, typename Key, typename... Args, typename KeyFn = key>
auto binary_emplace_new(Container& c, const Key& k, const std::tuple<Args...>& args = {}, KeyFn fn = KeyFn{})
    -> decltype(std::end(c)) {
    return detail::binary_emplace_new(c, k, args, std::index_sequence_for<Args...>{}, fn);
}

template<typename Container, typename Val, typename KeyFn = key>
auto binary_insert_new(Container& c, Val&& v, KeyFn fn = KeyFn{}) -> decltype(std::end(c)) {
    return uxs::binary_emplace_new(c, fn(v), std::forward_as_tuple(std::forward<Val>(v)), fn);
}

template<typename Container, typename Key, typename KeyFn = key>
auto binary_access_new(Container& c, Key&& k, KeyFn fn = KeyFn{}) -> decltype(*std::begin(c)) {
    auto it = c.emplace(uxs::lower_bound(c, k, fn));
    fn(*it) = std::forward<Key>(k);
    return *it;
}

template<typename Container, typename Key, typename KeyFn = key>
auto binary_erase_one(Container& c, const Key& k, KeyFn fn = KeyFn{}) -> decltype(std::end(c)) {
    auto result = uxs::binary_find(c, k, fn);
    if (result.second) { return c.erase(result.first); }
    return result.first;
}

// ---- other algorithms

template<typename Range, typename OutputIt>
OutputIt copy(const Range& r, OutputIt out) {
    return std::copy(std::begin(r), std::end(r), out);
}

template<typename Range, typename OutputIt>
OutputIt uninitialized_copy(const Range& r, OutputIt out) {
    return std::uninitialized_copy(std::begin(r), std::end(r), out);
}
template<typename Range, typename OutputIt, typename Pred>
OutputIt copy_if(const Range& r, OutputIt out, Pred p) {
    return std::copy_if(std::begin(r), std::end(r), out, p);
}

template<typename Range, typename OutputIt, typename TransfFunc>
OutputIt transform(const Range& r, OutputIt out, TransfFunc func) {
    return std::transform(std::begin(r), std::end(r), out, func);
}

template<typename Range, typename OutputIt, typename TransfFunc, typename Pred>
OutputIt transform_if(const Range& r, OutputIt out, TransfFunc func, Pred p) {
    for (auto first = std::begin(r), last = std::end(r); first != last; ++first) {
        if (p(*first)) { *out++ = func(*first); }
    }
    return out;
}

template<typename Range, typename Comp = less<>>
void sort(Range& r, Comp comp = Comp{}) {
    std::sort(std::begin(r), std::end(r), comp);
}

template<typename Range, typename Val>
auto count(const Range& r, const Val& v) -> decltype(std::distance(std::begin(r), end(r))) {
    return std::count(std::begin(r), std::end(r), v);
}

template<typename Range, typename Pred>
auto count_if(const Range& r, Pred p) -> decltype(std::distance(std::begin(r), end(r))) {
    return std::count_if(std::begin(r), std::end(r), p);
}

template<typename Range, typename Pred>
bool any_of(const Range& r, Pred p) {
    return std::any_of(std::begin(r), std::end(r), p);
}

template<typename Range, typename Pred>
bool all_of(const Range& r, Pred p) {
    return std::all_of(std::begin(r), std::end(r), p);
}

template<typename Range, typename Pred>
bool none_of(const Range& r, Pred p) {
    return std::none_of(std::begin(r), std::end(r), p);
}

template<typename Range, typename InputIt, typename Pred = equal_to<>>
bool equal(const Range& r, InputIt in, Pred p = Pred{}) {
    return std::equal(std::begin(r), std::end(r), in, p);
}

template<typename Range, typename Comp = less<>>
auto min_element(Range&& r, Comp comp = Comp{}) -> decltype(std::end(r)) {
    return std::min_element(std::begin(r), std::end(r), comp);
}

template<typename Range, typename Comp = less<>>
auto max_element(Range&& r, Comp comp = Comp{}) -> decltype(std::end(r)) {
    return std::max_element(std::begin(r), std::end(r), comp);
}

template<typename Range, typename UnaryFunc>
UnaryFunc for_each(Range&& r, UnaryFunc func) {
    return std::for_each(std::begin(r), std::end(r), func);
}

#if __cplusplus < 201703L
namespace detail {
template<typename Func, typename>
struct for_loop_helper;
template<typename Func>
struct for_loop_helper<Func, std::false_type> {
    template<typename Range, typename... InputIts>
    auto operator()(Range&& r, Func func, InputIts... its) -> decltype(std::end(r)) {
        auto it = std::begin(r), end_it = std::end(r);
        for (; it != end_it; ++it, detail::dummy_variadic(++its...)) {
            if (!func(*it, *its...)) { break; }
        }
        return it;
    }
};
template<typename Func>
struct for_loop_helper<Func, std::true_type> {
    template<typename Range, typename... InputIts>
    auto operator()(Range&& r, Func func, InputIts... its) -> decltype(std::end(r)) {
        auto it = std::begin(r), end_it = std::end(r);
        for (; it != end_it; ++it, detail::dummy_variadic(++its...)) { func(*it, *its...); }
        return it;
    }
};
}  // namespace detail
template<typename Range, typename Func, typename... InputIts>
auto for_loop(Range&& r, Func func, InputIts... its) -> decltype(std::end(r)) {
    return detail::for_loop_helper<Func, typename std::is_same<decltype(func(*std::begin(r), *its...)), void>::type>()(
        std::forward<Range>(r), func, its...);
}
#else   // __cplusplus < 201703L
template<typename Range, typename Func, typename... InputIts>
auto for_loop(Range&& r, Func func, InputIts... its) -> decltype(std::end(r)) {
    auto it = std::begin(r), end_it = std::end(r);
    for (; it != end_it; (++it, ..., ++its)) {
        if constexpr (std::is_same_v<decltype(func(*std::begin(r), *its...)), void>) {
            func(*it, *its...);
        } else {
            if (!func(*it, *its...)) { break; }
        }
    }
    return it;
}
#endif  // __cplusplus < 201703L

}  // namespace uxs
