#include "uxs/db/database_error.h"

namespace uxs {
namespace db {
database_error::database_error(const char* message) : std::runtime_error(message) {}
database_error::database_error(const std::string& message) : std::runtime_error(message) {}
const char* database_error::what() const noexcept { return std::runtime_error::what(); }
}  // namespace db
}  // namespace uxs
