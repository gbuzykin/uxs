#include "uxs/io/oflatbuf_impl.h"

namespace uxs {
template class basic_oflatbuf<char>;
template class basic_oflatbuf<wchar_t>;
template class basic_oflatbuf<uint8_t>;
}  // namespace uxs
