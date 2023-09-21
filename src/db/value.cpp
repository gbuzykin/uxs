#include "uxs/db/value_impl.h"

namespace uxs {
namespace db {
namespace detail {
template struct flexarray_t<char, std::allocator<char>, false>;
template struct flexarray_t<wchar_t, std::allocator<wchar_t>, false>;
template struct flexarray_t<char, std::allocator<char>, true>;
template struct flexarray_t<wchar_t, std::allocator<wchar_t>, true>;
template struct record_t<char, std::allocator<char>>;
template struct record_t<wchar_t, std::allocator<wchar_t>>;
template struct list_node_type<char, std::allocator<char>>;
template struct list_node_type<wchar_t, std::allocator<wchar_t>>;
}  // namespace detail
template class basic_value<char>;
template class basic_value<wchar_t>;
template UXS_EXPORT bool operator==(const basic_value<char>&, const basic_value<char>&);
template UXS_EXPORT bool operator==(const basic_value<wchar_t>&, const basic_value<wchar_t>&);
}  // namespace db
}  // namespace uxs
