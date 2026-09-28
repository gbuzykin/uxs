#include "uxs/impl/db/value_impl.h"

UXS_DB_VALUE_INSTANTIATE_IMPLEMENTATION(char, std::allocator<char>);
#if UXS_USE_WCHAR_T != 0
UXS_DB_VALUE_INSTANTIATE_IMPLEMENTATION(wchar_t, std::allocator<wchar_t>);
#endif  // UXS_USE_WCHAR_T != 0
