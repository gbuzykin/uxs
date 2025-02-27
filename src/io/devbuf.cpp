#include "uxs/impl/io/devbuf_impl.h"

namespace uxs {
template class basic_devbuf<char>;
template class basic_devbuf<wchar_t>;
template class basic_devbuf<std::uint8_t>;
}  // namespace uxs
