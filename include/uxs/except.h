#pragma once

#include "common.h"

#include <stdexcept>

namespace uxs {

[[noreturn]] inline void report_too_much_to_allocate_error() { throw std::length_error("too much to allocate"); }
[[noreturn]] inline void report_index_out_of_range_error() { throw std::out_of_range("index out of range"); }

}  // namespace uxs
