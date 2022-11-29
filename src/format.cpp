#include "uxs/format_impl.h"

using namespace uxs;

namespace uxs {
namespace detail {
template UXS_EXPORT unlimbuf_appender& fmt_append_string(unlimbuf_appender&, std::string_view, fmt_state&);
template UXS_EXPORT limbuf_appender& fmt_append_string(limbuf_appender&, std::string_view, fmt_state&);
template UXS_EXPORT dynbuffer& fmt_append_string(dynbuffer&, std::string_view, fmt_state&);
}  // namespace detail

template UXS_EXPORT unlimbuf_appender& basic_vformat(unlimbuf_appender&, std::string_view,
                                                     span<const detail::fmt_arg_list_item<unlimbuf_appender>>);
template UXS_EXPORT limbuf_appender& basic_vformat(limbuf_appender&, std::string_view,
                                                   span<const detail::fmt_arg_list_item<limbuf_appender>>);
template UXS_EXPORT dynbuffer& basic_vformat(dynbuffer&, std::string_view,
                                             span<const detail::fmt_arg_list_item<dynbuffer>>);

namespace detail {
template UXS_EXPORT wunlimbuf_appender& fmt_append_string(wunlimbuf_appender&, std::wstring_view, fmt_state&);
template UXS_EXPORT wlimbuf_appender& fmt_append_string(wlimbuf_appender&, std::wstring_view, fmt_state&);
template UXS_EXPORT wdynbuffer& fmt_append_string(wdynbuffer&, std::wstring_view, fmt_state&);
}  // namespace detail

template UXS_EXPORT wunlimbuf_appender& basic_vformat(wunlimbuf_appender&, std::wstring_view,
                                                      span<const detail::fmt_arg_list_item<wunlimbuf_appender>>);
template UXS_EXPORT wlimbuf_appender& basic_vformat(wlimbuf_appender&, std::wstring_view,
                                                    span<const detail::fmt_arg_list_item<wlimbuf_appender>>);
template UXS_EXPORT wdynbuffer& basic_vformat(wdynbuffer&, std::wstring_view,
                                              span<const detail::fmt_arg_list_item<wdynbuffer>>);
}  // namespace uxs
