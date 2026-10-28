#include "uxs/io/iostate.h"

uxs::iobuf_error::iobuf_error(const char* message) : std::runtime_error(message) {}
uxs::iobuf_error::iobuf_error(const std::string& message) : std::runtime_error(message) {}
const char* uxs::iobuf_error::what() const noexcept { return std::runtime_error::what(); }
