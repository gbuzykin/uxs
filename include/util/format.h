#pragma once

#include "string_cvt.h"

#include <vector>

namespace util {

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
        auto s = arg_cvt(std::forward<Ty>(v), std::forward<Args>(args)...);
        auto width = static_cast<size_t>(field.width);
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
