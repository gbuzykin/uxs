#include "util/io/zipfilebuf_impl.h"

namespace util {
template class basic_zipfilebuf<char>;
template class basic_zipfilebuf<wchar_t>;
template class basic_zipfilebuf<uint8_t>;
}  // namespace util
