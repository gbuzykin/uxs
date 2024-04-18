#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
template UXS_EXPORT void vformat(format_context, std::string_view);
template UXS_EXPORT void vformat(wformat_context, std::wstring_view);
}  // namespace sfmt
}  // namespace uxs
