#include "uxs/io/iostate.h"

using namespace uxs;

iobuf_error::iobuf_error(const char* message) : std::runtime_error(message) {}
iobuf_error::iobuf_error(const std::string& message) : std::runtime_error(message) {}
const char* iobuf_error::what() const noexcept { return std::runtime_error::what(); }
