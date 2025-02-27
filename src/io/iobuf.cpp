#include "uxs/impl/io/iobuf_impl.h"

namespace uxs {
template class basic_iobuf<char>;
template class basic_iobuf<wchar_t>;
template class basic_iobuf<std::uint8_t>;
}  // namespace uxs
