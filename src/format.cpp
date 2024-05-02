#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
template UXS_EXPORT membuffer& vformat(membuffer&, std::string_view, format_args, const std::locale*);
template UXS_EXPORT wmembuffer& vformat(wmembuffer&, std::wstring_view, wformat_args, const std::locale*);
}  // namespace sfmt
}  // namespace uxs
