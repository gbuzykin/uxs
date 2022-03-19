#include "util/io/filebuf_impl.h"

namespace util {
template class basic_filebuf<char>;
template class basic_filebuf<wchar_t>;
template class basic_filebuf<uint8_t>;
}  // namespace util
