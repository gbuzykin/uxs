#include "util/io/iobuf_impl.h"

namespace util {
template class basic_iobuf<char>;
template class basic_iobuf<wchar_t>;
template class basic_iobuf<uint8_t>;
}  // namespace util
