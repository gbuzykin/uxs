#include "uxs/impl/cli/parser_impl.h"

UXS_CLI_INSTANTIATE_IMPLEMENTATION(char);
#if UXS_USE_WCHAR_T != 0
UXS_CLI_INSTANTIATE_IMPLEMENTATION(wchar_t);
#endif  // UXS_USE_WCHAR_T != 0
