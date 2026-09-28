#include "uxs/impl/io/oflatbuf_impl.h"

UXS_IO_OFLATBUF_INSTANTIATE_IMPLEMENTATION(char, std::allocator<char>);
#if UXS_USE_WCHAR_T != 0
UXS_IO_OFLATBUF_INSTANTIATE_IMPLEMENTATION(wchar_t, std::allocator<wchar_t>);
#endif  // UXS_USE_WCHAR_T != 0
UXS_IO_OFLATBUF_INSTANTIATE_IMPLEMENTATION(std::uint8_t, std::allocator<std::uint8_t>);
