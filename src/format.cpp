#include "util/format_impl.h"

using namespace util;

namespace util {
namespace detail {
template unlimbuf_appender& fmt_append_string(unlimbuf_appender&, std::string_view, fmt_state&);
template limbuf_appender& fmt_append_string(limbuf_appender&, std::string_view, fmt_state&);
template dynbuf_appender& fmt_append_string(dynbuf_appender&, std::string_view, fmt_state&);
}  // namespace detail

template unlimbuf_appender& basic_format(unlimbuf_appender&, std::string_view,
                                         span<const detail::fmt_arg_list_item<unlimbuf_appender>>);
template limbuf_appender& basic_format(limbuf_appender&, std::string_view,
                                       span<const detail::fmt_arg_list_item<limbuf_appender>>);
template dynbuf_appender& basic_format(dynbuf_appender&, std::string_view,
                                       span<const detail::fmt_arg_list_item<dynbuf_appender>>);

namespace detail {
template wunlimbuf_appender& fmt_append_string(wunlimbuf_appender&, std::wstring_view, fmt_state&);
template wlimbuf_appender& fmt_append_string(wlimbuf_appender&, std::wstring_view, fmt_state&);
template wdynbuf_appender& fmt_append_string(wdynbuf_appender&, std::wstring_view, fmt_state&);
}  // namespace detail

template wunlimbuf_appender& basic_format(wunlimbuf_appender&, std::wstring_view,
                                          span<const detail::fmt_arg_list_item<wunlimbuf_appender>>);
template wlimbuf_appender& basic_format(wlimbuf_appender&, std::wstring_view,
                                        span<const detail::fmt_arg_list_item<wlimbuf_appender>>);
template wdynbuf_appender& basic_format(wdynbuf_appender&, std::wstring_view,
                                        span<const detail::fmt_arg_list_item<wdynbuf_appender>>);
}  // namespace util
