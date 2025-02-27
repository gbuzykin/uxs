#include "uxs/impl/io/filebuf_impl.h"

namespace uxs {
template class basic_filebuf<char>;
template class basic_filebuf<wchar_t>;
template class basic_filebuf<std::uint8_t>;
}  // namespace uxs
