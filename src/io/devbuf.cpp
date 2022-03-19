#include "util/io/devbuf_impl.h"

namespace util {
template class basic_devbuf<char>;
template class basic_devbuf<wchar_t>;
template class basic_devbuf<uint8_t>;
}  // namespace util
