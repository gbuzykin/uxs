#include "uxs/impl/format_impl.h"

uxs::format_error::format_error(const char* message) : std::runtime_error(message) {}
uxs::format_error::format_error(const std::string& message) : std::runtime_error(message) {}
const char* uxs::format_error::what() const noexcept { return std::runtime_error::what(); }

UXS_FMT_INSTANTIATE_IMPLEMENTATION(char);
#if UXS_USE_WCHAR_T != 0
UXS_FMT_INSTANTIATE_IMPLEMENTATION(wchar_t);
#endif  // UXS_USE_WCHAR_T != 0
