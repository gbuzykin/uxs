#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
template UXS_EXPORT void vformat(format_context, format_parse_context);
template UXS_EXPORT void vformat(wformat_context, wformat_parse_context);
}  // namespace sfmt
}  // namespace uxs
