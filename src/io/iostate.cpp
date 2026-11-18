#include "uxs/io/iostate.h"

#include <stdexcept>

namespace uxs {

namespace detail {
class iobuf_error_category final : public std::error_category {
 public:
    const char* name() const noexcept override { return "iobuf"; }
    std::string message(int code) const override {
        if (code == static_cast<int>(iobuf_errc::failure)) { return "iobuf failure"; }
        return "unspecified iobuf_category failure";
    }
};
}  // namespace detail

const std::error_category& iobuf_category() noexcept {
    static detail::iobuf_error_category category;
    return category;
}

iobuf_error::iobuf_error(const char* message) : std::system_error(detail::make_iobuf_error_code(), message) {}
iobuf_error::iobuf_error(const std::string& message) : std::system_error(detail::make_iobuf_error_code(), message) {}
const char* iobuf_error::what() const noexcept { return std::runtime_error::what(); }
}  // namespace uxs
