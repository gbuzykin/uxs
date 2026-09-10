#include "uxs/impl/format_impl.h"

using namespace uxs;

namespace uxs {
namespace fmt {
template UXS_EXPORT void format_impl(format_context, format_parse_context);
template UXS_EXPORT void format_impl(wformat_context, wformat_parse_context);
}  // namespace fmt
}  // namespace uxs
