#include "uxs/db/value_impl.h"

namespace uxs {
namespace db {
template UXS_EXPORT bool operator==(const basic_value<char>&, const basic_value<char>&);
template UXS_EXPORT bool operator==(const basic_value<wchar_t>&, const basic_value<wchar_t>&);
template class UXS_EXPORT basic_value<char>;
template class UXS_EXPORT basic_value<wchar_t>;
template struct UXS_EXPORT basic_value<char>::flexarray_t<char>;
template struct UXS_EXPORT basic_value<wchar_t>::flexarray_t<wchar_t>;
template struct UXS_EXPORT basic_value<char>::flexarray_t<basic_value<char>>;
template struct UXS_EXPORT basic_value<wchar_t>::flexarray_t<basic_value<wchar_t>>;
}  // namespace db
}  // namespace uxs
