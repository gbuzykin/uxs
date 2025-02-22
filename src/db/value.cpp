#include "uxs/db/value_impl.h"

namespace uxs {
namespace db {
namespace detail {
template struct flexarray_t<char, std::allocator<char>>;
template struct flexarray_t<wchar_t, std::allocator<wchar_t>>;
#if defined(__GNUC__)
template struct UXS_EXPORT flexarray_t<basic_value<char>, std::allocator<char>>;
template struct UXS_EXPORT flexarray_t<basic_value<wchar_t>, std::allocator<wchar_t>>;
#else
template struct flexarray_t<basic_value<char>, std::allocator<char>>;
template struct flexarray_t<basic_value<wchar_t>, std::allocator<wchar_t>>;
#endif
template struct record_t<char, std::allocator<char>>;
template struct record_t<wchar_t, std::allocator<wchar_t>>;
template class record_value<char, std::allocator<char>>;
template class record_value<wchar_t, std::allocator<wchar_t>>;
}  // namespace detail
template class basic_value<char>;
template class basic_value<wchar_t>;
template UXS_EXPORT bool operator==(const basic_value<char>&, const basic_value<char>&) noexcept;
template UXS_EXPORT bool operator==(const basic_value<wchar_t>&, const basic_value<wchar_t>&) noexcept;
}  // namespace db
}  // namespace uxs
