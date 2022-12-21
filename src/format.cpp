#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace sfmt {
#if __cplusplus < 201703L
UXS_EXPORT const meta_tbl_t g_meta_tbl;
#endif  // __cplusplus < 201703L
template UXS_EXPORT unlimbuf_appender& adjust_string(unlimbuf_appender&, uxs::span<const char>, fmt_opts&);
template UXS_EXPORT limbuf_appender& adjust_string(limbuf_appender&, uxs::span<const char>, fmt_opts&);
template UXS_EXPORT dynbuffer& adjust_string(dynbuffer&, uxs::span<const char>, fmt_opts&);
}  // namespace sfmt

template UXS_EXPORT unlimbuf_appender& basic_vformat(unlimbuf_appender&, std::string_view,
                                                     span<const sfmt::arg_list_item<unlimbuf_appender>>);
template UXS_EXPORT limbuf_appender& basic_vformat(limbuf_appender&, std::string_view,
                                                   span<const sfmt::arg_list_item<limbuf_appender>>);
template UXS_EXPORT dynbuffer& basic_vformat(dynbuffer&, std::string_view, span<const sfmt::arg_list_item<dynbuffer>>);

namespace sfmt {
template UXS_EXPORT wunlimbuf_appender& adjust_string(wunlimbuf_appender&, uxs::span<const wchar_t>, fmt_opts&);
template UXS_EXPORT wlimbuf_appender& adjust_string(wlimbuf_appender&, uxs::span<const wchar_t>, fmt_opts&);
template UXS_EXPORT wdynbuffer& adjust_string(wdynbuffer&, uxs::span<const wchar_t>, fmt_opts&);
}  // namespace sfmt

template UXS_EXPORT wunlimbuf_appender& basic_vformat(wunlimbuf_appender&, std::wstring_view,
                                                      span<const sfmt::arg_list_item<wunlimbuf_appender>>);
template UXS_EXPORT wlimbuf_appender& basic_vformat(wlimbuf_appender&, std::wstring_view,
                                                    span<const sfmt::arg_list_item<wlimbuf_appender>>);
template UXS_EXPORT wdynbuffer& basic_vformat(wdynbuffer&, std::wstring_view,
                                              span<const sfmt::arg_list_item<wdynbuffer>>);
}  // namespace uxs
