#include "util/format_impl.h"

using namespace util;

namespace util {
namespace detail {
template unlimbuf_appender& fmt_append_string(std::string_view, unlimbuf_appender&, fmt_state&);
template limbuf_appender& fmt_append_string(std::string_view, limbuf_appender&, fmt_state&);
template dynbuf_appender& fmt_append_string(std::string_view, dynbuf_appender&, fmt_state&);
}  // namespace detail

template unlimbuf_appender& format_append_v(std::string_view, unlimbuf_appender&,
                                            span<const detail::fmt_arg_list_item<unlimbuf_appender>>);
template limbuf_appender& format_append_v(std::string_view, limbuf_appender&,
                                          span<const detail::fmt_arg_list_item<limbuf_appender>>);
template dynbuf_appender& format_append_v(std::string_view, dynbuf_appender&,
                                          span<const detail::fmt_arg_list_item<dynbuf_appender>>);

namespace detail {
template wunlimbuf_appender& fmt_append_string(std::wstring_view, wunlimbuf_appender&, fmt_state&);
template wlimbuf_appender& fmt_append_string(std::wstring_view, wlimbuf_appender&, fmt_state&);
template wdynbuf_appender& fmt_append_string(std::wstring_view, wdynbuf_appender&, fmt_state&);
}  // namespace detail

template wunlimbuf_appender& format_append_v(std::wstring_view, wunlimbuf_appender&,
                                             span<const detail::fmt_arg_list_item<wunlimbuf_appender>>);
template wlimbuf_appender& format_append_v(std::wstring_view, wlimbuf_appender&,
                                           span<const detail::fmt_arg_list_item<wlimbuf_appender>>);
template wdynbuf_appender& format_append_v(std::wstring_view, wdynbuf_appender&,
                                           span<const detail::fmt_arg_list_item<wdynbuf_appender>>);
}  // namespace util
