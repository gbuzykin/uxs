#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
#if __cplusplus < 201703L
UXS_EXPORT const meta_tbl_t g_meta_tbl;
#endif  // __cplusplus < 201703L
template UXS_EXPORT void adjust_string(membuffer&, span<const char>, fmt_opts&);
template UXS_EXPORT void adjust_string(wmembuffer&, span<const wchar_t>, fmt_opts&);
template UXS_EXPORT membuffer& vformat(membuffer&, span<const char>, format_args, const std::locale*);
template UXS_EXPORT wmembuffer& vformat(wmembuffer&, span<const wchar_t>, wformat_args, const std::locale*);
}  // namespace sfmt
}  // namespace uxs
