#pragma once

#include "../span.h"
#include "../string_view.h"
#include "iostate.h"

namespace util {

template<typename CharT>
class UTIL_EXPORT basic_iobuf : public iostate {
 public:
    using char_type = CharT;
    using traits_type = std::char_traits<CharT>;
    using size_type = size_t;
    using int_type = typename traits_type::int_type;
    using pos_type = typename traits_type::pos_type;
    using off_type = typename traits_type::off_type;

    basic_iobuf() = default;
    explicit basic_iobuf(iomode mode) : iostate(mode) {}
    basic_iobuf(iomode mode, iostate_bits state) : iostate(mode, state) {}
    virtual ~basic_iobuf() = default;
    basic_iobuf(const basic_iobuf&) = delete;
    basic_iobuf& operator=(const basic_iobuf&) = delete;
    basic_iobuf(basic_iobuf&& other) NOEXCEPT;
    basic_iobuf& operator=(basic_iobuf&& other) NOEXCEPT;

    size_type in_avail() const { return last_ - curr_; }
    span<const char_type> in_avail_view() const { return as_span(curr_, in_avail()); }

    int_type peek() {
        if (curr_ == last_ && (!this->good() || underflow() < 0)) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return traits_type::eof();
        }
        return traits_type::to_int_type(*curr_);
    }

    int_type get() {
        if (curr_ == last_ && (!this->good() || underflow() < 0)) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
            return traits_type::eof();
        }
        return traits_type::to_int_type(*curr_++);
    }

    void unget() {
        this->setstate(this->rdstate() & ~iostate_bits::kEof);
        if (curr_ == first_ && (!this->good() || ungetfail() < 0)) {
            this->setstate(iostate_bits::kEof | iostate_bits::kFail);
        } else {
            --curr_;
        }
    }

    basic_iobuf& put(char_type ch) {
        if (curr_ != last_) {
            *curr_++ = ch;
        } else if (!this->good() || overflow(ch) < 0) {
            this->setstate(iostate_bits::kBad);
        }
        return *this;
    }

    size_type read(span<char_type> s);
    basic_iobuf& write(span<const char_type> s);
    basic_iobuf& write(const char_type* cstr) { return write(std::basic_string_view<char_type>(cstr)); }
    basic_iobuf& fill_n(size_type n, char_type ch);
    basic_iobuf& flush();
    basic_iobuf& endl() { return put('\n').flush(); }

 protected:
    virtual int underflow();
    virtual int ungetfail();
    virtual int overflow(char_type ch);
    virtual int sync();

    char_type* first() const { return first_; }
    char_type* curr() const { return curr_; }
    char_type* last() const { return last_; }
    void bump(int count) { curr_ += count; }
    void setcurr(char_type* curr) { curr_ = curr; }
    void setview(char_type* first, char_type* curr, char_type* last) {
        first_ = first;
        curr_ = curr;
        last_ = last;
    }

 private:
    char_type* first_ = nullptr;
    char_type* curr_ = nullptr;
    char_type* last_ = nullptr;
};

using iobuf = basic_iobuf<char>;
using wiobuf = basic_iobuf<wchar_t>;
using u8iobuf = basic_iobuf<uint8_t>;

namespace stdbuf {
extern UTIL_EXPORT iobuf& out;
extern UTIL_EXPORT iobuf& in;
extern UTIL_EXPORT iobuf& log;
extern UTIL_EXPORT iobuf& err;
}  // namespace stdbuf

}  // namespace util
