#include "uxs/impl/format_impl.h"

using namespace uxs;

UXS_FMT_INSTANTIATE_IMPLEMENTATION(char);
#if UXS_USE_WCHAR_T != 0
UXS_FMT_INSTANTIATE_IMPLEMENTATION(wchar_t);
#endif  // UXS_USE_WCHAR_T != 0
