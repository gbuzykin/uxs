#pragma once

#include <stdexcept>
#include <string>

namespace uxs {
namespace db {
class exception : public std::runtime_error {
 public:
    explicit exception(const char* message) : std::runtime_error(message) {}
    explicit exception(const std::string& message) : std::runtime_error(message) {}
};
}  // namespace db
}  // namespace uxs
