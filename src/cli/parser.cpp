#include "uxs/cli/parser_impl.h"

namespace uxs {
namespace cli {
template class UXS_EXPORT basic_value<char>;
template class UXS_EXPORT basic_option_group<char>;
template class UXS_EXPORT basic_option<char>;
template class UXS_EXPORT basic_command<char>;
template class UXS_EXPORT basic_value<wchar_t>;
template class UXS_EXPORT basic_option_group<wchar_t>;
template class UXS_EXPORT basic_option<wchar_t>;
template class UXS_EXPORT basic_command<wchar_t>;
}  // namespace cli
}  // namespace uxs
