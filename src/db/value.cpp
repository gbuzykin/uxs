#include "uxs/impl/db/value_impl.h"

uxs::db::bad_value_access::bad_value_access(const char* message) : std::runtime_error(message) {}
uxs::db::bad_value_access::bad_value_access(const std::string& message) : std::runtime_error(message) {}
const char* uxs::db::bad_value_access::what() const noexcept { return std::runtime_error::what(); }

UXS_DB_VALUE_INSTANTIATE_IMPLEMENTATION(char, std::allocator<char>);
#if UXS_USE_WCHAR_T != 0
UXS_DB_VALUE_INSTANTIATE_IMPLEMENTATION(wchar_t, std::allocator<wchar_t>);
#endif  // UXS_USE_WCHAR_T != 0
