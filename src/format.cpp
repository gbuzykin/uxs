#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace fmt {
#if __cplusplus < 201703L
const meta_tbl_t g_meta_tbl;
const std::array<const char*, static_cast<unsigned>(arg_type_id::kCustom) + 1> g_allowed_types{
    "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod", "xXbBod",
    "c",      "c",      "",       "fFeEgG", "fFeEgG", "pP",     "s",      ""};
#endif  // __cplusplus < 201703L
template UXS_EXPORT unlimbuf_appender& format_string(unlimbuf_appender&, uxs::span<const char>, fmt_state&);
template UXS_EXPORT limbuf_appender& format_string(limbuf_appender&, uxs::span<const char>, fmt_state&);
template UXS_EXPORT dynbuffer& format_string(dynbuffer&, uxs::span<const char>, fmt_state&);
}  // namespace fmt

template UXS_EXPORT unlimbuf_appender& basic_vformat(unlimbuf_appender&, std::string_view,
                                                     span<const fmt::arg_list_item<unlimbuf_appender>>);
template UXS_EXPORT limbuf_appender& basic_vformat(limbuf_appender&, std::string_view,
                                                   span<const fmt::arg_list_item<limbuf_appender>>);
template UXS_EXPORT dynbuffer& basic_vformat(dynbuffer&, std::string_view, span<const fmt::arg_list_item<dynbuffer>>);

namespace fmt {
template UXS_EXPORT wunlimbuf_appender& format_string(wunlimbuf_appender&, uxs::span<const wchar_t>, fmt_state&);
template UXS_EXPORT wlimbuf_appender& format_string(wlimbuf_appender&, uxs::span<const wchar_t>, fmt_state&);
template UXS_EXPORT wdynbuffer& format_string(wdynbuffer&, uxs::span<const wchar_t>, fmt_state&);
}  // namespace fmt

template UXS_EXPORT wunlimbuf_appender& basic_vformat(wunlimbuf_appender&, std::wstring_view,
                                                      span<const fmt::arg_list_item<wunlimbuf_appender>>);
template UXS_EXPORT wlimbuf_appender& basic_vformat(wlimbuf_appender&, std::wstring_view,
                                                    span<const fmt::arg_list_item<wlimbuf_appender>>);
template UXS_EXPORT wdynbuffer& basic_vformat(wdynbuffer&, std::wstring_view,
                                              span<const fmt::arg_list_item<wdynbuffer>>);
}  // namespace uxs
