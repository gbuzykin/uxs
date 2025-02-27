#include "uxs/impl/io/iflatbuf_impl.h"

namespace uxs {
template class basic_iflatbuf<char>;
template class basic_iflatbuf<wchar_t>;
template class basic_iflatbuf<std::uint8_t>;
}  // namespace uxs
