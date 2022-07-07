#include "uxs/db/value_impl.h"

namespace uxs {
namespace db {
template bool operator==(const basic_value<char>&, const basic_value<char>&);
template bool operator==(const basic_value<wchar_t>&, const basic_value<wchar_t>&);
template class basic_value<char>;
template class basic_value<wchar_t>;
template struct basic_value<char>::flexarray_t<char>;
template struct basic_value<wchar_t>::flexarray_t<wchar_t>;
template struct basic_value<char>::flexarray_t<basic_value<char>>;
template struct basic_value<wchar_t>::flexarray_t<basic_value<wchar_t>>;
}  // namespace db
}  // namespace uxs
