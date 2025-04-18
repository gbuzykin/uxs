#include "uxs/impl/db/value_impl.h"

namespace uxs {
namespace db {
namespace detail {
template class flexarray_t<char, std::allocator<char>>;
template class flexarray_t<wchar_t, std::allocator<wchar_t>>;
template class UXS_EXPORT_ALL_STUFF_FOR_GNUC flexarray_t<basic_value<char>, std::allocator<char>>;
template class UXS_EXPORT_ALL_STUFF_FOR_GNUC flexarray_t<basic_value<wchar_t>, std::allocator<wchar_t>>;
template class record_t<char, std::allocator<char>>;
template class record_t<wchar_t, std::allocator<wchar_t>>;
template class record_value<char, std::allocator<char>>;
template class record_value<wchar_t, std::allocator<wchar_t>>;
}  // namespace detail
template class basic_value<char>;
template class basic_value<wchar_t>;
template UXS_EXPORT bool operator==(const basic_value<char>&, const basic_value<char>&) noexcept;
template UXS_EXPORT bool operator==(const basic_value<wchar_t>&, const basic_value<wchar_t>&) noexcept;
}  // namespace db
}  // namespace uxs
