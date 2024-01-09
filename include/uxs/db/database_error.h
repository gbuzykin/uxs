#pragma once

#include "uxs/common.h"

#include <stdexcept>
#include <string>

namespace uxs {
namespace db {
class UXS_EXPORT_ALL_STUFF_FOR_GNUC database_error : public std::runtime_error {
 public:
    UXS_EXPORT explicit database_error(const char* message);
    UXS_EXPORT explicit database_error(const std::string& message);
    UXS_EXPORT const char* what() const noexcept override;
};
}  // namespace db
}  // namespace uxs
