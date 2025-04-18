#pragma once

#include "iobuf.h"

#include "uxs/string_cvt.h"

namespace uxs {

template<typename Ty>
class basic_iomembuffer final : public basic_membuffer<Ty> {
 public:
    explicit basic_iomembuffer(basic_iobuf<Ty>& out) noexcept
        : basic_membuffer<Ty>(out.first_avail(), out.last_avail()), out_(out) {}
    ~basic_iomembuffer() override { flush(); }
    void flush() noexcept { out_.advance(this->curr() - out_.first_avail()); }

 private:
    basic_iobuf<Ty>& out_;

    std::size_t try_grow(std::size_t /*extra*/) override {
        flush();
        if (!out_.reserve().good()) { return 0; }
        this->set(out_.first_avail(), out_.last_avail());
        return this->avail();
    }
};

using iomembuffer = basic_iomembuffer<char>;
using wiomembuffer = basic_iomembuffer<wchar_t>;

}  // namespace uxs
