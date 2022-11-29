#include "uxs/cli/parser_impl.h"

namespace uxs {
namespace cli {
template UXS_EXPORT basic_value<char>;
template UXS_EXPORT basic_option_group<char>;
template UXS_EXPORT basic_option<char>;
template UXS_EXPORT basic_command<char>;
template UXS_EXPORT basic_value<wchar_t>;
template UXS_EXPORT basic_option_group<wchar_t>;
template UXS_EXPORT basic_option<wchar_t>;
template UXS_EXPORT basic_command<wchar_t>;
}  // namespace cli
}  // namespace uxs
