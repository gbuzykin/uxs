#pragma once

#include "uxs/common.h"

#include <stdexcept>
#include <string>

namespace uxs {
namespace db {
class UXS_EXPORT database_error : public std::runtime_error {
 public:
    explicit database_error(const char* message);
    explicit database_error(const std::string& message);
    const char* what() const NOEXCEPT override;
};
}  // namespace db
}  // namespace uxs
