#pragma once

#include "functional.h"

#include <iterator>

namespace uxs {

template<typename Func>
class function_output_iterator : private detail::func_ptr_holder<Func> {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using reference = void;
    using pointer = void;

    explicit function_output_iterator(const Func& fn) : detail::func_ptr_holder<Func>(fn) {}

    template<typename Ty>
    auto operator=(Ty&& v)
        -> est::type_identity_t<function_output_iterator&, decltype(std::declval<Func>()(std::forward<Ty>(v)))> {
        this->get_func()(std::forward<Ty>(v));
        return *this;
    }

    function_output_iterator& operator*() { return *this; }
    function_output_iterator& operator++() { return *this; }
    function_output_iterator operator++(int) { return *this; }
};

template<typename Func>
function_output_iterator<Func> make_function_iterator(const Func& func) {
    return function_output_iterator<Func>(func);
}

}  // namespace uxs
