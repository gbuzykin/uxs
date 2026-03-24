#pragma once

#include "iobuf.h"

#include "uxs/membuffer.h"

namespace uxs {

template<typename Ty>
class basic_iomembuffer final : public basic_membuffer<Ty> {
 public:
    using size_type = typename basic_membuffer<Ty>::size_type;

    explicit basic_iomembuffer(basic_iobuf<Ty>& out) noexcept
        : basic_membuffer<Ty>(out.first(), out.pos(), out.capacity()), out_(out) {}
    ~basic_iomembuffer() override { flush(); }
    void flush() noexcept { out_.setpos(this->size()); }

 private:
    basic_iobuf<Ty>& out_;

    size_type try_grow(size_type /*extra*/) override {
        flush();
        if (!out_.reserve().good()) { return 0; }
        this->reset(out_.first(), out_.pos(), out_.capacity());
        return this->avail();
    }
};

using iomembuffer = basic_iomembuffer<char>;
using wiomembuffer = basic_iomembuffer<wchar_t>;

}  // namespace uxs
