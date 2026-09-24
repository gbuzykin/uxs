#include "uxs/impl/db/value_impl.h"

namespace uxs {
namespace db {
namespace detail {
template class flexarray_t<char, std::allocator<char>>;
template class flexarray_t<wchar_t, std::allocator<wchar_t>>;
template class UXS_EXPORT_ALL_STUFF_FOR_GNUC flexarray_t<value, std::allocator<char>>;
template class UXS_EXPORT_ALL_STUFF_FOR_GNUC flexarray_t<wvalue, std::allocator<wchar_t>>;
template class object_t<char, std::allocator<char>>;
template class object_t<wchar_t, std::allocator<wchar_t>>;
template class object_item<char, std::allocator<char>>;
template class object_item<wchar_t, std::allocator<wchar_t>>;
}  // namespace detail
template class basic_value<char>;
template class basic_value<wchar_t>;
template UXS_EXPORT bool operator==(const value&, const value&) noexcept;
template UXS_EXPORT bool operator==(const wvalue&, const wvalue&) noexcept;
}  // namespace db
}  // namespace uxs
