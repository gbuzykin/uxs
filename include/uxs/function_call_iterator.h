#pragma once

#include "functional.h"

#include <iterator>

namespace uxs {

template<typename Func>
class function_call_iterator : private func_ptr_holder<Func> {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;

    explicit function_call_iterator(const Func& fn) : func_ptr_holder<Func>(fn) {}

    template<typename Ty>
    auto operator=(Ty&& v)
        -> uxs::type_identity_t<function_call_iterator&, decltype(std::declval<Func>()(std::forward<Ty>(v)))> {
        this->get_func()(std::forward<Ty>(v));
        return *this;
    }

    function_call_iterator& operator*() { return *this; }
    function_call_iterator& operator++() { return *this; }
    function_call_iterator operator++(int) { return *this; }
};

template<typename Func>
function_call_iterator<Func> function_caller(const Func& func) {
    return function_call_iterator<Func>(func);
}

#if UXS_USE_CHECKED_ITERATORS != 0
template<typename Func>
struct std::_Is_checked_helper<function_call_iterator<Func>> : std::true_type {};
#endif  // UXS_USE_CHECKED_ITERATORS

}  // namespace uxs
