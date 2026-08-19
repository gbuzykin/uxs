#pragma once

#include "common.h"

#include <iterator>

namespace uxs {

template<typename Func>
class function_output_iterator_facade {
 public:
    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = void;
    using reference = void;
    using pointer = void;

    template<typename Ty>
    function_output_iterator_facade& operator=(Ty&& v) {
        static_cast<Func&>(*this)(std::forward<Ty>(v));
        return *this;
    }

    function_output_iterator_facade& operator*() { return *this; }
    function_output_iterator_facade& operator++() { return *this; }
    function_output_iterator_facade operator++(int) { return *this; }
};

}  // namespace uxs
