#pragma once

#include "string_view.h"
#include "utility.h"

#include <cctype>

namespace util {

template<unsigned base>
CONSTEXPR int dig(char ch) {
    return static_cast<int>(ch - '0');
}

template<>
CONSTEXPR int dig<16>(char ch) {
    if (ch >= 'a' && ch <= 'f') { return static_cast<int>(ch - 'a') + 10; }
    if (ch >= 'A' && ch <= 'F') { return static_cast<int>(ch - 'A') + 10; }
    return static_cast<int>(ch - '0');
}

template<typename InputIt, typename InputFn = nofunc>
unsigned from_hex(InputIt in, int digs, InputFn fn = InputFn{}, bool* ok = nullptr) {
    unsigned val = 0;
    while (digs > 0) {
        char ch = fn(*in++);
        val <<= 4;
        --digs;
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            val |= dig<16>(ch);
        } else {
            if (ok) { *ok = false; }
            return val;
        }
    }
    if (ok) { *ok = true; }
    return val;
}

template<typename OutputIt, typename OutputFn = nofunc>
void to_hex(unsigned val, OutputIt out, int digs, OutputFn fn = OutputFn{}) {
    int shift = digs << 2;
    while (shift > 0) {
        shift -= 4;
        *out++ = fn("0123456789ABCDEF"[(val >> shift) & 0xf]);
    }
}

enum class scvt_fp { kScientific = 0, kFixed, kGeneral };
enum class scvt_base { kBase8 = 0, kBase10, kBase16 };

template<typename Ty>
struct string_converter_base {
    using is_string_converter = int;
    static Ty default_value() { return {}; }
};

template<typename Ty>
struct string_converter;

template<>
struct UTIL_EXPORT string_converter<int16_t> : string_converter_base<int16_t> {
    static const char* from_string(std::string_view s, int16_t& val, bool* ok = nullptr);
    static std::string to_string(int16_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<int32_t> : string_converter_base<int32_t> {
    static const char* from_string(std::string_view s, int32_t& val, bool* ok = nullptr);
    static std::string to_string(int32_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<int64_t> : string_converter_base<int64_t> {
    static const char* from_string(std::string_view s, int64_t& val, bool* ok = nullptr);
    static std::string to_string(int64_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<uint16_t> : string_converter_base<uint16_t> {
    static const char* from_string(std::string_view s, uint16_t& val, bool* ok = nullptr);
    static std::string to_string(uint16_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<uint32_t> : string_converter_base<uint32_t> {
    static const char* from_string(std::string_view s, uint32_t& val, bool* ok = nullptr);
    static std::string to_string(uint32_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<uint64_t> : string_converter_base<uint64_t> {
    static const char* from_string(std::string_view s, uint64_t& val, bool* ok = nullptr);
    static std::string to_string(uint64_t val, scvt_base base = scvt_base::kBase10);
};

template<>
struct UTIL_EXPORT string_converter<float> : string_converter_base<float> {
    static const char* from_string(std::string_view s, float& val, bool* ok = nullptr);
    static std::string to_string(float val, scvt_fp fmt = scvt_fp::kGeneral, int prec = -1);
};

template<>
struct UTIL_EXPORT string_converter<double> : string_converter_base<double> {
    static const char* from_string(std::string_view s, double& val, bool* ok = nullptr);
    static std::string to_string(double val, scvt_fp fmt = scvt_fp::kGeneral, int prec = -1);
};

template<>
struct UTIL_EXPORT string_converter<bool> : string_converter_base<bool> {
    static const char* from_string(std::string_view s, bool& val, bool* ok = nullptr);
    static std::string to_string(bool val) { return val ? "true" : "false"; }
};

template<typename Ty, typename Def, typename... Args>
Ty from_string(std::string_view s, Def&& def, Args&&... args) {
    Ty result(std::forward<Def>(def));
    string_converter<Ty>::from_string(s, result, std::forward<Args>(args)...);
    return result;
}

template<typename Ty>
Ty from_string(std::string_view s) {
    Ty result(string_converter<Ty>::default_value());
    string_converter<Ty>::from_string(s, result);
    return result;
}

template<typename Ty, typename... Args>
std::string to_string(const Ty& val, Args&&... args) {
    return string_converter<Ty>::to_string(val, std::forward<Args>(args)...);
}

}  // namespace util
