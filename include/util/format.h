#pragma once

#include "string_cvt.h"

#include <vector>

namespace util {

namespace detail {

UTIL_EXPORT const char* format(const char* p, const char* last, std::string& str, std::string_view arg);

template<typename Ty, typename = std::void_t<typename string_converter<Ty>::is_string_converter>>
const char* format(const char* p, const char* last, std::string& str, const Ty& arg) {
    return detail::format(p, last, str, to_string(arg));
}

template<typename Ty1, typename Ty2, typename... Ts>
const char* format(const char* p, const char* last, std::string& str, Ty1&& arg1, Ty2&& arg2, Ts&&... other) {
    p = detail::format(p, last, str, std::forward<Ty1>(arg1));
    return detail::format(p, last, str, std::forward<Ty2>(arg2), std::forward<Ts>(other)...);
}

}  // namespace detail

inline std::string format(std::string_view fmt) { return std::string(fmt); }

template<typename... Ts>
std::string format(std::string_view fmt, Ts&&... args) {
    std::string str;
    str.reserve(256);
    const char* p = detail::format(fmt.data(), fmt.data() + fmt.size(), str, std::forward<Ts>(args)...);
    str.append(p, fmt.size() - static_cast<size_t>(p - fmt.data()));
    return str;
}

struct sfield {
    explicit sfield(int in_width, char in_fill = ' ') : width(in_width), fill(in_fill) {}
    int width;
    char fill;
};

class UTIL_EXPORT sformat {
 public:
    explicit sformat(std::string_view fmt) : fmt_(fmt) {}
    std::string str() const;
    operator std::string() const { return str(); }

    template<typename Ty, typename... Args>
    sformat& arg(Ty&& v, Args&&... args) {
        args_.emplace_back(arg_cvt(std::forward<Ty>(v), std::forward<Args>(args)...));
        return *this;
    }

    template<typename Ty, typename... Args>
    sformat& arg(Ty&& v, sfield field, Args&&... args) {
        std::string s = arg_cvt(std::forward<Ty>(v), std::forward<Args>(args)...);
        size_t width = static_cast<size_t>(field.width);
        if (width > s.size()) { s.insert(0, width - s.size(), field.fill); }
        args_.emplace_back(std::move(s));
        return *this;
    }

 private:
    std::string arg_cvt(std::string_view s) { return static_cast<std::string>(s); }
    std::string arg_cvt(std::string s) { return std::move(s); }
    std::string arg_cvt(const char* s) { return std::string(s); }
    std::string arg_cvt(const sformat& fmt) { return fmt.str(); }
    std::string arg_cvt(void* p) { return arg_cvt(reinterpret_cast<uintptr_t>(p), scvt_base::kBase16); }

    template<typename Ty, typename = std::void_t<typename string_converter<Ty>::is_string_converter>, typename... Args>
    std::string arg_cvt(const Ty& v, Args&&... args) {
        return to_string(v, std::forward<Args>(args)...);
    }

    std::string_view fmt_;
    std::vector<std::string> args_;
};

}  // namespace util
