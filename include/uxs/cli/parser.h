#pragma once

#include "uxs/memory.h"
#include "uxs/stringcvt.h"

#include <functional>
#include <map>
#include <vector>

namespace uxs {
namespace cli {

enum class parsing_status {
    ok = 0,
    unspecified_value,
    invalid_value,
    unknown_option,
    unspecified_option,
    conflicting_option,
};

enum class text_briefness { brief = 0, full };

enum class text_coloring { no_color = 0, colored };

enum class node_type { value = 0, option, option_group, command };

template<typename CharT>
using value_handler_fn = std::function<bool(std::basic_string_view<CharT>)>;

template<typename CharT>
class basic_command;

template<typename CharT>
class basic_node {
 public:
    explicit basic_node(node_type type) noexcept : type_(type), is_optional_(false), parent_(nullptr) {}
    basic_node(const basic_node&) = default;
    basic_node& operator=(const basic_node&) = delete;
    virtual ~basic_node() = default;
    virtual std::unique_ptr<basic_node> clone() const = 0;

    node_type get_type() const noexcept { return type_; }
    bool is_optional() const noexcept { return is_optional_; }
    void set_optional(bool v) { is_optional_ = v; }

    const std::basic_string<CharT>& get_doc() const noexcept { return doc_; }
    void add_doc(std::basic_string_view<CharT> text) { doc_ += text; }

    const basic_command<CharT>* get_command() const noexcept;
    basic_command<CharT>* get_command() noexcept;

 protected:
    basic_node* get_parent() const noexcept { return parent_; }
    void make_parent(basic_node& node) { node.parent_ = this; }

 private:
    node_type type_;
    bool is_optional_;
    basic_node* parent_;
    std::basic_string<CharT> doc_;
};

template<typename CharT>
class basic_value : public basic_node<CharT> {
 public:
    basic_value(std::basic_string<CharT> label, value_handler_fn<CharT> fn)
        : basic_node<CharT>(node_type::value), label_(std::move(label)), handler_(std::move(fn)), is_multiple_(false) {}
    std::unique_ptr<basic_node<CharT>> clone() const override { return est::make_unique<basic_value>(*this); }

    const std::basic_string<CharT>& get_label() const noexcept { return label_; }
    const value_handler_fn<CharT>& get_handler() const noexcept { return handler_; }

    bool is_multiple() const noexcept { return is_multiple_; }
    void set_multiple(bool v) { is_multiple_ = v; }

 private:
    std::basic_string<CharT> label_;
    value_handler_fn<CharT> handler_;
    bool is_multiple_;
};

template<typename CharT>
class basic_option_node : public basic_node<CharT> {
 public:
    explicit basic_option_node(node_type type) noexcept : basic_node<CharT>(type) {}

    std::basic_string<CharT> make_text(text_briefness briefness) const;

    template<typename EnumFunc>
    bool traverse_options(const EnumFunc& fn) const;
    template<typename EnumFunc>
    bool traverse_options(const EnumFunc& fn);
};

template<typename CharT>
class basic_option_group : public basic_option_node<CharT> {
 public:
    explicit basic_option_group(bool is_exclusive) noexcept
        : basic_option_node<CharT>(node_type::option_group), is_exclusive_(is_exclusive) {}
    UXS_EXPORT basic_option_group(const basic_option_group&);
    std::unique_ptr<basic_node<CharT>> clone() const override { return est::make_unique<basic_option_group>(*this); }

    const std::vector<std::unique_ptr<basic_option_node<CharT>>>& get_children() const noexcept { return children_; }
    void add_child(std::unique_ptr<basic_option_node<CharT>> child) {
        this->make_parent(*child);
        if (this->is_exclusive() && child->is_optional()) {
            child->set_optional(false);
            this->set_optional(true);
        }
        children_.emplace_back(std::move(child));
    }

    bool is_exclusive() const noexcept { return is_exclusive_; }

 private:
    std::vector<std::unique_ptr<basic_option_node<CharT>>> children_;
    const bool is_exclusive_;
};

template<typename CharT>
class basic_option : public basic_option_node<CharT> {
 public:
    explicit basic_option(std::initializer_list<std::basic_string_view<CharT>> keys)
        : basic_option_node<CharT>(node_type::option), keys_(keys.begin(), keys.end()) {}
    UXS_EXPORT basic_option(const basic_option&);
    std::unique_ptr<basic_node<CharT>> clone() const override { return est::make_unique<basic_option>(*this); }

    const std::vector<std::basic_string<CharT>>& get_keys() const noexcept { return keys_; }

    const std::vector<std::unique_ptr<basic_value<CharT>>>& get_values() const noexcept { return values_; }
    void add_value(std::unique_ptr<basic_value<CharT>> val) {
        this->make_parent(*val);
        values_.emplace_back(std::move(val));
    }

    const std::function<void()>& get_handler() const noexcept { return handler_; }
    void set_handler(std::function<void()> fn) { handler_ = std::move(fn); }

 private:
    const std::vector<std::basic_string<CharT>> keys_;
    std::vector<std::unique_ptr<basic_value<CharT>>> values_;
    std::function<void()> handler_;
};

template<typename CharT>
template<typename EnumFunc>
bool basic_option_node<CharT>::traverse_options(const EnumFunc& fn) const {
    if (this->get_type() == node_type::option_group) {
        for (const auto& opt : static_cast<const basic_option_group<CharT>&>(*this).get_children()) {
            if (!std::as_const(*opt).traverse_options(fn)) { return false; }
        }
    }
    return fn(*this);
}

template<typename CharT>
template<typename EnumFunc>
bool basic_option_node<CharT>::traverse_options(const EnumFunc& fn) {
    return std::as_const(*this).traverse_options(
        [&fn](const basic_option_node<CharT>& opt) { return fn(const_cast<basic_option_node<CharT>&>(opt)); });
}

template<typename CharT>
struct parsing_result {
#if __cplusplus < 201703L
    parsing_result(parsing_status s, int c, const basic_node<CharT>* n) : status(s), argc_parsed(c), node(n) {}
#endif  // __cplusplus < 201703L
    operator bool() const { return status == parsing_status::ok; }
    parsing_status status = parsing_status::ok;
    int argc_parsed = 0;
    const basic_node<CharT>* node = nullptr;
};

template<typename CharT>
class basic_command : public basic_node<CharT> {
 public:
    explicit basic_command(std::basic_string<CharT> name)
        : basic_node<CharT>(node_type::command), name_(std::move(name)),
          opts_(est::make_unique<basic_option_group<CharT>>(false)) {
        this->make_parent(*opts_);
    }
    UXS_EXPORT basic_command(const basic_command&);
    std::unique_ptr<basic_node<CharT>> clone() const override { return est::make_unique<basic_command>(*this); }

    const std::basic_string<CharT>& get_name() const noexcept { return name_; }
    const std::basic_string<CharT>& get_overview() const noexcept { return overview_; }
    const std::vector<std::unique_ptr<basic_value<CharT>>>& get_values() const noexcept { return values_; }
    basic_option_group<CharT>& get_options() const noexcept { return *opts_; }
    const std::map<std::basic_string_view<CharT>, std::unique_ptr<basic_command>>& get_subcommands() const noexcept {
        return subcommands_;
    }
    const basic_command* get_parent_command() const noexcept {
        return static_cast<const basic_command*>(this->get_parent());
    }
    basic_command* get_parent_command() noexcept { return static_cast<basic_command*>(this->get_parent()); }

    void add_overview(std::basic_string_view<CharT> text) { overview_ += text; }
    void add_value(std::unique_ptr<basic_value<CharT>> val) {
        this->make_parent(*val);
        values_.emplace_back(std::move(val));
    }
    UXS_EXPORT void add_option(std::unique_ptr<basic_option_node<CharT>> opt);
    UXS_EXPORT void add_subcommand(std::unique_ptr<basic_command> cmd);

    const std::function<void()>& get_handler() const noexcept { return handler_; }
    void set_handler(std::function<void()> fn) { handler_ = std::move(fn); }

    parsing_result<CharT> parse(int argc, const CharT* const* argv) const { return parse(this, argc, argv); }

    UXS_EXPORT std::basic_string<CharT> make_man_page(text_coloring coloring) const;

 private:
    std::basic_string<CharT> name_, overview_;
    std::vector<std::unique_ptr<basic_value<CharT>>> values_;
    std::unique_ptr<basic_option_group<CharT>> opts_;
    std::map<std::basic_string_view<CharT>, const basic_option<CharT>*> opt_map_;
    std::map<std::basic_string_view<CharT>, std::unique_ptr<basic_command>> subcommands_;
    std::function<void()> handler_;

    UXS_EXPORT static parsing_result<CharT> parse(const basic_command* cmd, int argc, const CharT* const* argv);
};

template<typename CharT>
const basic_command<CharT>* basic_node<CharT>::get_command() const noexcept {
    const auto* node = this;
    do {
        if (node->get_type() == node_type::command) { return static_cast<const basic_command<CharT>*>(node); }
        node = node->parent_;
    } while (node);
    return nullptr;
}

template<typename CharT>
basic_command<CharT>* basic_node<CharT>::get_command() noexcept {
    return const_cast<basic_command<CharT>*>(std::as_const(*this).get_command());
}

template<typename CharT>
class basic_overview_wrapper {
 public:
    explicit basic_overview_wrapper(std::basic_string_view<CharT> text) : text_(text) {}

    const std::basic_string_view<CharT>& get_text() const { return text_; }

 private:
    std::basic_string_view<CharT> text_;
};

template<typename CharT>
class basic_value_wrapper {
 public:
    explicit basic_value_wrapper(std::unique_ptr<basic_value<CharT>> ptr) noexcept : ptr_(std::move(ptr)) {}
    basic_value_wrapper(std::basic_string<CharT> label, value_handler_fn<CharT> fn)
        : ptr_(est::make_unique<basic_value<CharT>>(std::move(label), std::move(fn))) {}
#if __cplusplus < 201703L
    ~basic_value_wrapper() = default;
    basic_value_wrapper(basic_value_wrapper&& other) noexcept : ptr_(std::move(other.ptr_)) {}
    basic_value_wrapper& operator=(basic_value_wrapper&& other) noexcept {
        ptr_ = std::move(other.ptr_);
        return *this;
    }
#endif  // __cplusplus < 201703L

    basic_value_wrapper clone() const {
        return basic_value_wrapper(static_pointer_cast<basic_value<CharT>>(ptr_->clone()));
    }
    basic_value<CharT>& operator*() const noexcept { return *ptr_; }
    basic_value<CharT>* operator->() const noexcept { return ptr_.get(); }
    basic_value<CharT>* get() const noexcept { return ptr_.get(); }
    std::unique_ptr<basic_value<CharT>> release() noexcept { return std::move(ptr_); }

    basic_value_wrapper optional(bool v = true) {
        ptr_->set_optional(v);
        return std::move(*this);
    }

    basic_value_wrapper& operator%=(std::basic_string_view<CharT> doc) {
        ptr_->add_doc(doc);
        return *this;
    }

    friend basic_value_wrapper operator%(basic_value_wrapper val, std::basic_string_view<CharT> doc) {
        return std::move(val %= doc);
    }

    basic_value_wrapper multiple(bool v = true) {
        ptr_->set_multiple(v);
        return std::move(*this);
    }

 private:
    std::unique_ptr<basic_value<CharT>> ptr_;
};

template<typename CharT>
class basic_option_node_wrapper {
 public:
    explicit basic_option_node_wrapper(std::unique_ptr<basic_option_node<CharT>> ptr) noexcept : ptr_(std::move(ptr)) {}
#if __cplusplus < 201703L
    ~basic_option_node_wrapper() = default;
    basic_option_node_wrapper(basic_option_node_wrapper&& other) noexcept : ptr_(std::move(other.ptr_)) {}
    basic_option_node_wrapper& operator=(basic_option_node_wrapper&& other) noexcept {
        ptr_ = std::move(other.ptr_);
        return *this;
    }
#endif  // __cplusplus < 201703L

    basic_option_node_wrapper clone() const {
        return basic_option_node_wrapper(static_pointer_cast<basic_option_node<CharT>>(ptr_->clone()));
    }
    basic_option_node<CharT>& operator*() const noexcept { return *ptr_; }
    basic_option_node<CharT>* operator->() const noexcept { return ptr_.get(); }
    basic_option_node<CharT>* get() const noexcept { return ptr_.get(); }
    std::unique_ptr<basic_option_node<CharT>> release() noexcept { return std::move(ptr_); }

    basic_option_node_wrapper optional(bool v = true) {
        ptr_->set_optional(v);
        return std::move(*this);
    }

    basic_option_node_wrapper& operator%=(std::basic_string_view<CharT> doc) {
        ptr_->add_doc(doc);
        return *this;
    }

    friend basic_option_node_wrapper operator%(basic_option_node_wrapper opt, std::basic_string_view<CharT> doc) {
        return std::move(opt %= doc);
    }

    basic_option_node_wrapper& operator&=(basic_option_node_wrapper opt) {
        if (&opt == this) { return *this; }
        if (ptr_->get_type() == node_type::option_group && !ptr_->is_optional() &&
            !static_cast<basic_option_group<CharT>&>(*ptr_).is_exclusive()) {
            static_cast<basic_option_group<CharT>&>(*ptr_).add_child(opt.release());
        } else {
            auto group = est::make_unique<basic_option_group<CharT>>(false);
            group->add_child(std::move(ptr_));
            group->add_child(opt.release());
            ptr_ = std::move(group);
        }
        return *this;
    }

    basic_option_node_wrapper& operator|=(basic_option_node_wrapper opt) {
        if (&opt == this) { return *this; }
        if (ptr_->get_type() == node_type::option_group &&
            static_cast<basic_option_group<CharT>&>(*ptr_).is_exclusive()) {
            static_cast<basic_option_group<CharT>&>(*ptr_).add_child(opt.release());
        } else {
            auto group = est::make_unique<basic_option_group<CharT>>(true);
            group->add_child(std::move(ptr_));
            group->add_child(opt.release());
            ptr_ = std::move(group);
        }
        return *this;
    }

    friend basic_option_node_wrapper operator&(basic_option_node_wrapper lhs, basic_option_node_wrapper rhs) {
        return std::move(lhs &= std::move(rhs));
    }

    friend basic_option_node_wrapper operator|(basic_option_node_wrapper lhs, basic_option_node_wrapper rhs) {
        return std::move(lhs |= std::move(rhs));
    }

 private:
    std::unique_ptr<basic_option_node<CharT>> ptr_;
};

template<typename CharT>
class basic_option_wrapper : public basic_option_node_wrapper<CharT> {
 public:
    explicit basic_option_wrapper(std::initializer_list<std::basic_string_view<CharT>> keys)
        : basic_option_node_wrapper<CharT>(est::make_unique<basic_option<CharT>>(keys)) {}
#if __cplusplus < 201703L
    ~basic_option_wrapper() = default;
    basic_option_wrapper(basic_option_wrapper&& other) noexcept : basic_option_node_wrapper<CharT>(std::move(other)) {}
    basic_option_wrapper& operator=(basic_option_wrapper&& other) noexcept {
        basic_option_node_wrapper<CharT>::operator=(std::move(other));
        return *this;
    }
#endif  // __cplusplus < 201703L

    basic_option_wrapper optional(bool v = true) {
        (*this)->set_optional(v);
        return std::move(*this);
    }

    basic_option_wrapper& operator%=(std::basic_string_view<CharT> doc) {
        (*this)->add_doc(doc);
        return *this;
    }

    friend basic_option_wrapper operator%(basic_option_wrapper opt, std::basic_string_view<CharT> doc) {
        return std::move(opt %= doc);
    }

    template<typename Fn>
    basic_option_wrapper call(const Fn& fn) {
        static_cast<basic_option<CharT>&>(**this).set_handler(fn);
        return std::move(*this);
    }
    template<typename Ty>
    basic_option_wrapper set(Ty& t, Ty v) {
        static_cast<basic_option<CharT>&>(**this).set_handler([&t, v]() { t = v; });
        return std::move(*this);
    }
    basic_option_wrapper set(bool& flag, bool v = true) {
        static_cast<basic_option<CharT>&>(**this).set_handler([&flag, v]() { flag = v; });
        return std::move(*this);
    }

    basic_option_wrapper& operator&=(basic_value_wrapper<CharT> val) {
        static_cast<basic_option<CharT>&>(**this).add_value(val.release());
        return *this;
    }

    friend basic_option_wrapper operator&(basic_option_wrapper opt, basic_value_wrapper<CharT> val) {
        return std::move(opt &= std::move(val));
    }
};

template<typename CharT>
class basic_command_wrapper {
 public:
    explicit basic_command_wrapper(std::unique_ptr<basic_command<CharT>> ptr) noexcept : ptr_(std::move(ptr)) {}
    explicit basic_command_wrapper(std::basic_string<CharT> name)
        : ptr_(est::make_unique<basic_command<CharT>>(std::move(name))) {}
#if __cplusplus < 201703L
    ~basic_command_wrapper() = default;
    basic_command_wrapper(basic_command_wrapper&& other) noexcept : ptr_(std::move(other.ptr_)) {}
    basic_command_wrapper& operator=(basic_command_wrapper&& other) noexcept {
        ptr_ = std::move(other.ptr_);
        return *this;
    }
#endif  // __cplusplus < 201703L

    basic_command_wrapper clone() const {
        return basic_command_wrapper(static_pointer_cast<basic_command<CharT>>(ptr_->clone()));
    }
    basic_command<CharT>& operator*() const noexcept { return *ptr_; }
    basic_command<CharT>* operator->() const noexcept { return ptr_.get(); }
    basic_command<CharT>* get() const noexcept { return ptr_.get(); }
    std::unique_ptr<basic_command<CharT>> release() noexcept { return std::move(ptr_); }

    basic_command_wrapper& operator%=(std::basic_string_view<CharT> doc) {
        ptr_->add_doc(doc);
        return *this;
    }

    friend basic_command_wrapper operator%(basic_command_wrapper cmd, std::basic_string_view<CharT> doc) {
        return std::move(cmd %= doc);
    }

    template<typename Fn>
    basic_command_wrapper call(const Fn& fn) {
        ptr_->set_handler(fn);
        return std::move(*this);
    }
    template<typename Ty>
    basic_command_wrapper set(Ty& t, Ty v) {
        ptr_->set_handler([&t, v]() { t = v; });
        return std::move(*this);
    }
    basic_command_wrapper set(bool& flag, bool v = true) {
        ptr_->set_handler([&flag, v]() { flag = v; });
        return std::move(*this);
    }

    basic_command_wrapper& operator<<=(basic_overview_wrapper<CharT> overview) {
        ptr_->add_overview(overview.get_text());
        return *this;
    }

    basic_command_wrapper& operator<<=(basic_value_wrapper<CharT> val) {
        ptr_->add_value(val.release());
        return *this;
    }

    basic_command_wrapper& operator<<=(basic_option_node_wrapper<CharT> opt) {
        ptr_->add_option(opt.release());
        return *this;
    }

    basic_command_wrapper& operator<<=(basic_command_wrapper subcmd) {
        if (&subcmd == this) { return *this; }
        ptr_->add_subcommand(subcmd.release());
        return *this;
    }

    friend basic_command_wrapper operator<<(basic_command_wrapper cmd, basic_overview_wrapper<CharT> overview) {
        return std::move(cmd <<= overview);
    }

    friend basic_command_wrapper operator<<(basic_command_wrapper cmd, basic_value_wrapper<CharT> val) {
        return std::move(cmd <<= std::move(val));
    }

    friend basic_command_wrapper operator<<(basic_command_wrapper cmd, basic_option_node_wrapper<CharT> opt) {
        return std::move(cmd <<= std::move(opt));
    }

    friend basic_command_wrapper operator<<(basic_command_wrapper cmd, basic_command_wrapper subcmd) {
        return std::move(cmd <<= std::move(subcmd));
    }

 private:
    std::unique_ptr<basic_command<CharT>> ptr_;
};

inline basic_overview_wrapper<char> overview(std::string_view text) { return basic_overview_wrapper<char>(text); }
inline basic_command_wrapper<char> command(std::string name) { return basic_command_wrapper<char>(std::move(name)); }
inline basic_option_wrapper<char> option(std::initializer_list<std::string_view> keys) {
    return basic_option_wrapper<char>(keys).optional();
}
inline basic_option_wrapper<char> required(std::initializer_list<std::string_view> keys) {
    return basic_option_wrapper<char>(keys);
}

inline basic_value_wrapper<char> value(std::string name, std::string& s) {
    return basic_value_wrapper<char>(std::move(name), [&s](std::string_view arg) {
        s = std::string(arg);
        return true;
    });
}

template<typename Ty>
basic_value_wrapper<char> value(std::string name, Ty& v) {
    return basic_value_wrapper<char>(std::move(name), [&v](std::string_view arg) { return stoval(arg, v) != 0; });
}

inline basic_value_wrapper<char> values(std::string name, std::vector<std::string>& vec) {
    return basic_value_wrapper<char>(std::move(name),
                                     [&vec](std::string_view arg) {
                                         vec.emplace_back(arg);
                                         return true;
                                     })
        .multiple();
}

template<typename Ty>
basic_value_wrapper<char> values(std::string name, std::vector<Ty>& vec) {
    return basic_value_wrapper<char>(std::move(name),
                                     [&vec](std::string_view arg) {
                                         Ty v;
                                         if (stoval(arg, v) != 0) {
                                             vec.emplace_back(v);
                                             return true;
                                         }
                                         return false;
                                     })
        .multiple();
}

inline basic_overview_wrapper<wchar_t> overview(std::wstring_view text) {
    return basic_overview_wrapper<wchar_t>(text);
}
inline basic_command_wrapper<wchar_t> command(std::wstring name) {
    return basic_command_wrapper<wchar_t>(std::move(name));
}
inline basic_option_wrapper<wchar_t> option(std::initializer_list<std::wstring_view> keys) {
    return basic_option_wrapper<wchar_t>(keys).optional();
}
inline basic_option_wrapper<wchar_t> required(std::initializer_list<std::wstring_view> keys) {
    return basic_option_wrapper<wchar_t>(keys);
}

inline basic_value_wrapper<wchar_t> value(std::wstring name, std::wstring& s) {
    return basic_value_wrapper<wchar_t>(std::move(name), [&s](std::wstring_view arg) {
        s = std::wstring(arg);
        return true;
    });
}

template<typename Ty>
basic_value_wrapper<wchar_t> value(std::wstring name, Ty& v) {
    return basic_value_wrapper<wchar_t>(std::move(name), [&v](std::wstring_view arg) { return wstoval(arg, v) != 0; });
}

inline basic_value_wrapper<wchar_t> values(std::wstring name, std::vector<std::wstring>& vec) {
    return basic_value_wrapper<wchar_t>(std::move(name),
                                        [&vec](std::wstring_view arg) {
                                            vec.emplace_back(arg);
                                            return true;
                                        })
        .multiple();
}

template<typename Ty>
basic_value_wrapper<wchar_t> values(std::wstring name, std::vector<Ty>& vec) {
    return basic_value_wrapper<wchar_t>(std::move(name),
                                        [&vec](std::wstring_view arg) {
                                            Ty v;
                                            if (wstoval(arg, v) != 0) {
                                                vec.emplace_back(v);
                                                return true;
                                            }
                                            return false;
                                        })
        .multiple();
}

}  // namespace cli
}  // namespace uxs
