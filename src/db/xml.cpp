#include "uxs/db/xml_impl.h"
#include "uxs/utf.h"

namespace lex_detail {
#include "xml_lex_defs.h"
}
namespace lex_detail {
#include "xml_lex_analyzer.inl"
}

static uint8_t g_spec_chars[256] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

namespace uxs {
namespace db {

xml::reader::reader(iobuf& input) : input_(input), name_cache_(16) { state_stack_.push_back(lex_detail::sc_initial); }

std::pair<xml::token_t, std::string_view> xml::reader::read_next() {
    if (is_end_element_pending_) {
        is_end_element_pending_ = false;
        return {token_t::end_element, name_cache_.front()};
    }
    if (!input_) { return {token_t::eof, {}}; }
    while (input_.avail() || input_.peek() != iobuf::traits_type::eof()) {
        const char *first = input_.first_avail(), *last = input_.last_avail();
        std::string_view lval;
        if (*first == '<') {  // found '<'
            lex_token_t tt = parse_token(lval);
            auto name_cache_it = name_cache_.begin(), name_cache_prev_it = name_cache_it;
            auto read_attribute = [this, &name_cache_it, &name_cache_prev_it](std::string_view lval) {
                lex_token_t tt = lex_token_t::eof;
                if (name_cache_it != name_cache_.end()) {
                    name_cache_it->assign(lval.data(), lval.size());
                } else {
                    name_cache_it = name_cache_.emplace_after(name_cache_prev_it, lval);
                }
                if ((tt = parse_token(lval)) != lex_token_t::eq) {
                    throw database_error(format("{}: expected `=`", n_ln_));
                }
                if ((tt = parse_token(lval)) != lex_token_t::string) {
                    throw database_error(format("{}: expected valid attribute value", n_ln_));
                }
                name_cache_prev_it = name_cache_it;
                attrs_.emplace(*name_cache_it++, lval);
            };
            switch (tt) {
                case lex_token_t::start_element_open: {  // <name n1=v1 n2=v2...> or <name n1=v1 n2=v2.../>
                    attrs_.clear();
                    name_cache_it->assign(lval.data(), lval.size());
                    ++name_cache_it;
                    while (true) {
                        if ((tt = parse_token(lval)) == lex_token_t::name) {
                            read_attribute(lval);
                        } else if (tt == lex_token_t::close) {
                            return {token_t::start_element, name_cache_.front()};
                        } else if (tt == lex_token_t::end_element_close) {
                            is_end_element_pending_ = true;
                            return {token_t::start_element, name_cache_.front()};
                        } else {
                            throw database_error(format("{}: expected name, `>` or `/>`", n_ln_));
                        }
                    }
                } break;
                case lex_token_t::end_element_open: {  // </name>
                    if ((tt = parse_token(lval)) != lex_token_t::close) {
                        throw database_error(format("{}: expected `>`", n_ln_));
                    }
                    return {token_t::end_element, lval};
                } break;
                case lex_token_t::pi_open: {  // <?xml n1=v1 n2=v2...?>
                    attrs_.clear();
                    if (uxs::compare_strings_nocase(lval, "xml") != 0) {
                        throw database_error(format("{}: invalid document declaration", n_ln_));
                    }
                    name_cache_it->assign(lval.data(), lval.size());
                    ++name_cache_it;
                    while (true) {
                        if ((tt = parse_token(lval)) == lex_token_t::name) {
                            read_attribute(lval);
                        } else if (tt == lex_token_t::pi_close) {
                            return {token_t::preamble, name_cache_.front()};
                        } else {
                            throw database_error(format("{}: expected name or `?>`", n_ln_));
                        }
                    }
                } break;
                case lex_token_t::comment: {  // comment <!--....-->: skip till `-->`
                    int ch = input_.get();
                    do {
                        while (input_ && ch != '-') {
                            if (ch == '\0') { return {token_t::eof, {}}; }
                            if (ch == '\n') { ++n_ln_; }
                            ch = input_.get();
                        }
                        ch = input_.get();
                    } while (input_ && (ch != '-' || input_.peek() != '>'));
                    if (!input_) { break; }
                    input_.advance(1);
                } break;
                default: UNREACHABLE_CODE;
            }
        } else if (*first == '&') {  // parse entity
            if (parse_token(lval) == lex_token_t::predef_entity) { return {token_t::plain_text, lval}; }
            return {token_t::entity, lval};
        } else if (*first != 0) {
            last = std::find_if(first, last, [this](char ch) {
                if (ch != '\n') { return !!g_spec_chars[static_cast<unsigned char>(ch)]; }
                ++n_ln_;
                return false;
            });
            input_.advance(last - first);
            return {token_t::plain_text, std::string_view(first, last - first)};
        } else {
            return {token_t::eof, {}};
        }
    }
    return {token_t::eof, {}};
}

xml::reader::lex_token_t xml::reader::parse_token(std::string_view& lval) {
    while (true) {
        int pat = 0;
        unsigned llen = 0;
        const char* first = input_.first_avail();
        while (true) {
            bool stack_limitation = false;
            const char* last = input_.last_avail();
            if (state_stack_.avail() < static_cast<size_t>(last - first)) {
                last = first + state_stack_.avail();
                stack_limitation = true;
            }
            pat = lex_detail::lex(first, last, state_stack_.p_curr(), &llen, stack_limitation || input_);
            if (pat >= lex_detail::predef_pat_default) {
                break;
            } else if (stack_limitation) {  // enlarge state stack and continue analysis
                state_stack_.reserve(llen);
                first = last;
                continue;
            } else if (!input_) {  // end of sequence, first_ == last_
                return lex_token_t::eof;
            }
            if (input_.avail()) {  // append read buffer to stash
                stash_.append(input_.first_avail(), input_.last_avail());
                input_.advance(input_.avail());
            }
            // read more characters from input
            input_.peek();
            first = input_.first_avail();
        }
        const char* lexeme = input_.first_avail();
        if (stash_.empty()) {  // the stash is empty
            input_.advance(llen);
        } else {
            if (llen >= stash_.size()) {  // all characters in stash buffer are used
                // concatenate full lexeme in stash
                size_t len_rest = llen - stash_.size();
                stash_.append(input_.first_avail(), input_.first_avail() + len_rest);
                input_.advance(len_rest);
            } else {  // at least one character in stash is yet unused
                      // put unused chars back to `iobuf`
                for (const char* p = stash_.last(); p != stash_.curr(); --p) { input_.unget(*(p - 1)); }
            }
            lexeme = stash_.data();
            stash_.clear();  // it resets end pointer, but retains the contents
        }
        switch (pat) {
            // ------ specials
            case lex_detail::pat_amp: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view("&", 1);
                    return lex_token_t::predef_entity;
                }
                str_.push_back('&');
            } break;
            case lex_detail::pat_lt: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view("<", 1);
                    return lex_token_t::predef_entity;
                }
                str_.push_back('<');
            } break;
            case lex_detail::pat_gt: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(">", 1);
                    return lex_token_t::predef_entity;
                }
                str_.push_back('>');
            } break;
            case lex_detail::pat_apos: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view("\'", 1);
                    return lex_token_t::predef_entity;
                }
                str_.push_back('\'');
            } break;
            case lex_detail::pat_quot: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view("\"", 1);
                    return lex_token_t::predef_entity;
                }
                str_.push_back('\"');
            } break;
            case lex_detail::pat_entity: {
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(lexeme + 1, llen - 2);
                    return lex_token_t::entity;
                }
                throw database_error(format("{}: unknown entity name", n_ln_));
            } break;
            case lex_detail::pat_dcode: {
                unsigned unicode = 0;
                for (char ch : std::string_view(lexeme + 2, llen - 3)) { unicode = 10 * unicode + dig_v(ch); }
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(str_.data(), to_utf8(unicode, str_.data()));
                    return lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str_));
            } break;
            case lex_detail::pat_hcode: {
                unsigned unicode = 0;
                for (char ch : std::string_view(lexeme + 3, llen - 4)) { unicode = (unicode << 4) + dig_v(ch); }
                if (state_stack_.back() == lex_detail::sc_initial) {
                    lval = std::string_view(str_.data(), to_utf8(unicode, str_.data()));
                    return lex_token_t::entity;
                }
                to_utf8(unicode, std::back_inserter(str_));
            } break;

            // ------ strings
            case lex_detail::pat_string_nl: {
                ++n_ln_;
                str_.push_back('\n');
            } break;
            case lex_detail::pat_string_other: return lex_token_t::eof;
            case lex_detail::pat_string_seq_quot:
            case lex_detail::pat_string_seq_apos: {
                str_.append(lexeme, lexeme + llen);
            } break;
            case lex_detail::pat_string_close_quot:
            case lex_detail::pat_string_close_apos: {
                if (str_.empty()) {
                    lval = std::string_view(lexeme, llen - 1);
                } else {
                    str_.append(lexeme, lexeme + llen - 1);
                    lval = std::string_view(str_.data(), str_.size());
                    str_.clear();  // it resets end pointer, but retains the contents
                }
                state_stack_.pop_back();
                return lex_token_t::string;
            } break;

            case lex_detail::pat_name: lval = std::string_view(lexeme, llen); return lex_token_t::name;
            case lex_detail::pat_start_element_open: {
                lval = std::string_view(lexeme + 1, llen - 1);
                return lex_token_t::start_element_open;
            } break;
            case lex_detail::pat_end_element_open: {
                lval = std::string_view(lexeme + 2, llen - 2);
                return lex_token_t::end_element_open;
            } break;
            case lex_detail::pat_end_element_close: return lex_token_t::end_element_close;
            case lex_detail::pat_pi_open: lval = std::string_view(lexeme + 2, llen - 2); return lex_token_t::pi_open;
            case lex_detail::pat_pi_close: return lex_token_t::pi_close;
            case lex_detail::pat_comment: return lex_token_t::comment;

            case lex_detail::pat_whitespace: break;
            case lex_detail::predef_pat_default: {
                switch (lexeme[0]) {
                    case '\"': state_stack_.push_back(lex_detail::sc_string_quot); break;
                    case '\'': state_stack_.push_back(lex_detail::sc_string_apos); break;
                    default: return static_cast<lex_token_t>(static_cast<unsigned char>(*first));
                }
            } break;
            default: UNREACHABLE_CODE;
        }
    }
}

/*static*/ xml::reader::string_class xml::reader::classify_string(const std::string_view& sval) {
    int state = lex_detail::sc_value;
    for (unsigned char ch : sval) {
        state = lex_detail::Dtran[lex_detail::dtran_width * state + lex_detail::symb2meta[ch]];
    }
    switch (lex_detail::accept[state]) {
        case lex_detail::pat_null: return string_class::null;
        case lex_detail::pat_true: return string_class::true_value;
        case lex_detail::pat_false: return string_class::false_value;
        case lex_detail::pat_decimal: return string_class::integer_number;
        case lex_detail::pat_neg_decimal: return string_class::negative_integer_number;
        case lex_detail::pat_real: return string_class::real_number;
        case lex_detail::pat_ws_with_nl: return string_class::ws_with_nl;
        case lex_detail::pat_other_value: return string_class::other;
        default: UNREACHABLE_CODE;
    }
}

namespace xml {
template UXS_EXPORT basic_value<char> reader::read(std::string_view, const std::allocator<char>&);
template UXS_EXPORT basic_value<wchar_t> reader::read(std::string_view, const std::allocator<wchar_t>&);
template UXS_EXPORT void writer::write(const basic_value<char>&, std::string_view, unsigned);
template UXS_EXPORT void writer::write(const basic_value<wchar_t>&, std::string_view, unsigned);
}  // namespace xml
}  // namespace db
}  // namespace uxs
