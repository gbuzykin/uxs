#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
#if __cplusplus < 201703L
const meta_tbl_t g_meta_tbl;
#endif  // __cplusplus < 201703L
template UXS_EXPORT membuffer& vformat(membuffer&, std::string_view, format_args, const std::locale*);
template UXS_EXPORT wmembuffer& vformat(wmembuffer&, std::wstring_view, wformat_args, const std::locale*);
}  // namespace sfmt
}  // namespace uxs
