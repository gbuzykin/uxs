#include "uxs/impl/cli/parser_impl.h"

namespace uxs {
namespace cli {
template class basic_option_group<char>;
template class basic_option<char>;
template class basic_command<char>;
template class basic_option_group<wchar_t>;
template class basic_option<wchar_t>;
template class basic_command<wchar_t>;
}  // namespace cli
}  // namespace uxs
