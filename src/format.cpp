#include "uxs/impl/format_impl.h"

using namespace uxs;

namespace uxs {
namespace fmt {
template UXS_EXPORT void vformat(format_context, format_parse_context);
template UXS_EXPORT void vformat(wformat_context, wformat_parse_context);
}  // namespace fmt
}  // namespace uxs
