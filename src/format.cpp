#include "util/format_impl.h"

using namespace util;

namespace util {
namespace detail {
template unlimbuf_appender& fmt_append_string(unlimbuf_appender&, std::string_view, fmt_state&);
template limbuf_appender& fmt_append_string(limbuf_appender&, std::string_view, fmt_state&);
template dynbuf_appender& fmt_append_string(dynbuf_appender&, std::string_view, fmt_state&);
}  // namespace detail

template unlimbuf_appender& format_append_v(unlimbuf_appender&, std::string_view,
                                            span<const detail::fmt_arg_list_item<unlimbuf_appender>>);
template limbuf_appender& format_append_v(limbuf_appender&, std::string_view,
                                          span<const detail::fmt_arg_list_item<limbuf_appender>>);
template dynbuf_appender& format_append_v(dynbuf_appender&, std::string_view,
                                          span<const detail::fmt_arg_list_item<dynbuf_appender>>);

namespace detail {
template wunlimbuf_appender& fmt_append_string(wunlimbuf_appender&, std::wstring_view, fmt_state&);
template wlimbuf_appender& fmt_append_string(wlimbuf_appender&, std::wstring_view, fmt_state&);
template wdynbuf_appender& fmt_append_string(wdynbuf_appender&, std::wstring_view, fmt_state&);
}  // namespace detail

template wunlimbuf_appender& format_append_v(wunlimbuf_appender&, std::wstring_view,
                                             span<const detail::fmt_arg_list_item<wunlimbuf_appender>>);
template wlimbuf_appender& format_append_v(wlimbuf_appender&, std::wstring_view,
                                           span<const detail::fmt_arg_list_item<wlimbuf_appender>>);
template wdynbuf_appender& format_append_v(wdynbuf_appender&, std::wstring_view,
                                           span<const detail::fmt_arg_list_item<wdynbuf_appender>>);
}  // namespace util
