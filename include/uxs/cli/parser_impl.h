#pragma once

#include "parser.h"

#include "uxs/algorithm.h"
#include "uxs/io/oflatbuf.h"

#include <numeric>
#include <unordered_set>

namespace uxs {
namespace cli {

// --------------------------

template<typename CharT>
basic_option_group<CharT>::basic_option_group(const basic_option_group& group)
    : basic_option_node<CharT>(group), is_exclusive_(group.is_exclusive_) {
    children_.reserve(group.children_.size());
    for (const auto& child : group.children_) {
        add_child(uxs::static_pointer_cast<basic_option_node<CharT>>(child->clone()));
    }
}

template<typename CharT>
basic_option<CharT>::basic_option(const basic_option& opt)
    : basic_option_node<CharT>(opt), keys_(opt.keys_), handler_(opt.handler_) {
    values_.reserve(opt.values_.size());
    for (const auto& val : opt.values_) { add_value(uxs::make_unique<basic_value<CharT>>(*val)); }
}

template<typename CharT>
basic_command<CharT>::basic_command(const basic_command& cmd)
    : basic_node<CharT>(cmd), name_(cmd.name_), overview_(cmd.overview_),
      opts_(uxs::make_unique<basic_option_group<CharT>>(*cmd.opts_)) {
    values_.reserve(cmd.values_.size());
    for (const auto& val : cmd.values_) { add_value(uxs::make_unique<basic_value<CharT>>(*val)); }
    for (const auto& item : cmd.subcommands_) { add_subcommand(uxs::make_unique<basic_command<CharT>>(*item.second)); }
}

template<typename CharT>
void basic_command<CharT>::add_option(std::unique_ptr<basic_option_node<CharT>> opt) {
    opt->traverse_options([this](const basic_option_node<CharT>& node) {
        if (node.get_type() != node_type::option) { return true; }
        const auto& opt = static_cast<const basic_option<CharT>&>(node);
        for (const auto& k : opt.get_keys()) { opt_map_.emplace(k, &opt); }
        return true;
    });
    opts_->add_child(std::move(opt));
}

template<typename CharT>
void basic_command<CharT>::add_subcommand(std::unique_ptr<basic_command<CharT>> cmd) {
    auto result = subcommands_.emplace(cmd->name_, std::move(cmd));
    if (result.second) { this->make_parent(*result.first->second); }
}

// --------------------------

template<typename CharT>
/*static*/ parsing_result<CharT> basic_command<CharT>::parse(const basic_command* cmd, int argc,
                                                             const CharT* const* argv) {
    const int argc0 = argc;
    --argc, ++argv;
    if (cmd->get_handler()) { cmd->get_handler()(); }
    for (; argc > 0; --argc, ++argv) {
        std::basic_string_view<CharT> arg(*argv);
        auto subcmd_it = cmd->subcommands_.find(arg);
        if (subcmd_it == cmd->subcommands_.end()) { break; }
        cmd = &*subcmd_it->second;
        if (cmd->get_handler()) { cmd->get_handler()(); }
    }

    auto find_option_by_key = [cmd](std::basic_string_view<CharT> arg) {
        auto opt_it = cmd->opt_map_.lower_bound(arg);
        if (opt_it != cmd->opt_map_.end() && !(arg < opt_it->first)) { return opt_it; }
        while (opt_it != cmd->opt_map_.begin() && !arg.empty()) {
            auto it1 = arg.begin();
            const auto& key = (--opt_it)->first;
            for (auto it2 = key.begin(); it1 != arg.end() && it2 != key.end(); ++it1, ++it2) {
                if (*it1 != *it2) { break; }
            }
            arg = arg.substr(0, it1 - arg.begin());  // common prefix
            if (key == arg                           // `key` is a prefix of `arg`
                && !opt_it->second->get_values().empty()) {
                return opt_it;
            }
        }
        return cmd->opt_map_.end();
    };

    auto parse_value = [cmd, &find_option_by_key, &argc, &argv](const basic_value<CharT>& val, std::size_t n_prefix) {
        const int argc0 = argc;
        if (argc > 0) {
            if (!val.is_multiple() && !val.is_optional()) {
                std::basic_string_view<CharT> arg(*argv + n_prefix);
                if (val.get_handler()(arg)) { --argc, ++argv; }
            } else {
                for (const int argc1 = val.is_multiple() ? 0 : argc - 1; argc > argc1; --argc, ++argv) {
                    std::basic_string_view<CharT> arg(*argv + n_prefix);
                    if ((!n_prefix && find_option_by_key(arg) != cmd->opt_map_.end()) || !val.get_handler()(arg)) {
                        break;
                    }
                    n_prefix = 0;
                }
            }
        }
        return argc0 != argc;
    };

    std::unordered_set<const basic_option_node<CharT>*> specified, optional;

    auto val_it = cmd->values_.begin();
    std::size_t count_multiple = 0;
    while (argc > 0) {
        std::basic_string_view<CharT> arg(*argv);
        auto opt_it = find_option_by_key(arg);
        if (opt_it != cmd->opt_map_.end()) {
            std::size_t n_prefix = opt_it->first.size();
            if (arg.size() == n_prefix) { n_prefix = 0, --argc, ++argv; }
            const auto& opt = *opt_it->second;
            for (const auto& val : opt.get_values()) {
                if (parse_value(*val, n_prefix)) {
                    n_prefix = 0;
                } else if (n_prefix || !val->is_optional()) {
                    return parsing_result<CharT>{parsing_status::invalid_value, argc0 - argc, &*val};
                }
            }
            if (opt.get_handler()) { opt.get_handler()(); }
            specified.emplace(&opt);
        } else if (val_it != cmd->values_.end()) {
            do {
                const auto& val = *val_it;
                if (val->get_handler()(arg)) {
                    --argc, ++argv;
                    if (!val->is_multiple()) {
                        ++val_it;
                    } else {
                        ++count_multiple;
                    }
                    break;
                } else if (val->is_optional() || count_multiple) {
                    ++val_it, count_multiple = 0;
                } else {
                    return parsing_result<CharT>{parsing_status::invalid_value, argc0 - argc, &*val};
                }
            } while (val_it != cmd->values_.end());
        } else {
            return parsing_result<CharT>{parsing_status::unknown_option, argc0 - argc, cmd};
        }
    }

    if (count_multiple) { ++val_it; }

    while (val_it != cmd->values_.end()) {
        const auto& val = *val_it++;
        if (!val->is_optional()) {
            return parsing_result<CharT>{parsing_status::unspecified_value, argc0 - argc, &*val};
        }
    }

    parsing_result<CharT> result{parsing_status::ok, argc0 - argc, cmd};

    cmd->opts_->traverse_options([&specified, &optional, &result](const basic_option_node<CharT>& node) {
        if (node.get_type() != node_type::option_group) {
            if (node.is_optional()) { optional.emplace(&node); }
            return true;
        }

        const auto& group = static_cast<const basic_option_group<CharT>&>(node);
        bool is_specified = false, is_optional = false;
        if (group.is_exclusive()) {
            for (const auto& opt : group.get_children()) {
                if (uxs::contains(optional, &*opt)) { is_optional = true; }
                if (uxs::contains(specified, &*opt)) {
                    if (is_specified) {
                        result.status = parsing_status::conflicting_option;
                        result.node = &*opt;
                        return false;
                    }
                    is_specified = true;
                }
            }
        } else {
            const basic_node<CharT>* first_unspecified = nullptr;
            is_optional = true;
            for (const auto& opt : group.get_children()) {
                bool is_child_optional = uxs::contains(optional, &*opt);
                if (!is_child_optional) { is_optional = false; }
                if (uxs::contains(specified, &*opt)) {
                    is_specified = true;
                } else if (!is_child_optional) {
                    if (!first_unspecified) { first_unspecified = &*opt; }
                    if (is_specified) { break; }
                }
            }
            if (is_specified && first_unspecified) {
                result.status = parsing_status::unspecified_option;
                result.node = first_unspecified;
                return false;
            }
        }

        is_optional = is_optional || group.is_optional();

        if (!is_specified && !is_optional) {
            result.status = parsing_status::unspecified_option;
            result.node = &group;
            return false;
        }

        if (is_specified) { specified.emplace(&group); }
        if (is_optional) { optional.emplace(&group); }
        return true;
    });

    return result;
}

// --------------------------

template<typename CharT>
std::basic_string<CharT> basic_option_node<CharT>::make_text(text_briefness briefness) const {
    if (this->get_type() == node_type::option) {
        const auto& opt = static_cast<const basic_option<CharT>&>(*this);
        const auto& keys = opt.get_keys();
        if (keys.empty()) { return {}; }
        std::basic_string<CharT> s(keys.front());
        bool no_space = !s.empty() && s.back() == '=';
        if (briefness == text_briefness::full) {
            for (const auto& key : uxs::make_subrange(keys, 1)) { s += ',', s += ' ', s += key; }
            no_space = !keys.back().empty() && keys.back().back() == '=';
        }
        for (const auto& val : opt.get_values()) {
            if (!no_space) { s += ' '; }
            if (val->is_optional()) { s += '['; }
            s += val->get_label();
            if (val->is_optional()) { s += ']'; }
        }
        return s;
    }
    assert(this->get_type() == node_type::option_group);
    const auto& group = static_cast<const basic_option_group<CharT>&>(*this);
    auto make_child_string = [&group, briefness](const basic_option_node<CharT>& opt) {
        if (opt.is_optional()) { return static_cast<CharT>('[') + opt.make_text(briefness) + static_cast<CharT>(']'); }
        if (!group.is_exclusive() && opt.get_type() == node_type::option_group &&
            static_cast<const basic_option_group<CharT>&>(opt).is_exclusive()) {
            return static_cast<CharT>('(') + opt.make_text(briefness) + static_cast<CharT>(')');
        }
        return opt.make_text(briefness);
    };
    if (group.get_children().empty()) { return {}; }
    std::basic_string<CharT> s(make_child_string(*group.get_children().front()));
    for (const auto& opt : uxs::make_subrange(group.get_children(), 1)) {
        s += group.is_exclusive() ? '|' : ' ';
        s += make_child_string(*opt);
    }
    return s;
}

namespace detail {
template<typename CharT>
void print_text_with_margin(uxs::basic_iobuf<CharT>& out, std::basic_string_view<CharT> text, std::size_t left_margin) {
    auto it = text.begin();
    while (it != text.end()) {
        auto end_it = std::find(it, text.end(), '\n');
        if (end_it != text.end()) { ++end_it; }
        if (it != text.begin()) { out.fill_n(left_margin, ' '); }
        out.write(uxs::as_span(&*it, end_it - it));
        it = end_it;
    }
}
}  // namespace detail

template<typename CharT>
std::basic_string<CharT> basic_command<CharT>::make_man_page(text_coloring coloring) const {
    static const auto color_br_white = string_literal<CharT, '\033', '[', '1', ';', '3', '7', 'm'>{}();
    static const auto color_green = string_literal<CharT, '\033', '[', '0', ';', '3', '2', 'm'>{}();
    static const auto color_normal = string_literal<CharT, '\033', '[', '0', 'm'>{}();
    static const unsigned tab_size = 4, max_width = 100, max_margin = 24, gap = 2;

    basic_oflatbuf<CharT> osb;
    const bool start_with_nl = !overview_.empty() && overview_.front() == '\n';
    const bool end_width_nl = !overview_.empty() && overview_.back() == '\n';

    auto print_usage = [this, &osb, start_with_nl, end_width_nl, coloring]() {
        if (coloring == text_coloring::colored) { osb.write(color_br_white); }
        const auto label_usage = string_literal<CharT, 'U', 'S', 'A', 'G', 'E', ':', ' '>{}();
        osb.write(label_usage);
        if (coloring == text_coloring::colored) { osb.write(color_normal); }
        if (start_with_nl) { osb.put('\n').fill_n(tab_size, ' '); }

        const std::size_t left_margin = start_with_nl ? tab_size : label_usage.size();
        std::size_t width = left_margin + name_.size();
        std::vector<std::basic_string<CharT>> cmd_names;
        cmd_names.reserve(4);
        for (const auto* p = this->get_parent_command(); p; p = p->get_parent_command()) {
            cmd_names.emplace_back(p->name_);
        }

        if (coloring == text_coloring::colored) { osb.write(color_green); }
        for (const auto& name : uxs::make_reverse_range(cmd_names)) {
            width += 1 + name.size();
            osb.write(name).put(' ');
        }
        osb.write(name_);
        for (const auto& val : values_) {
            const auto& label = val->get_label();
            width += 1 + label.size();
            osb.put(' ').write(label);
        }

        const auto& opts = opts_->get_children();
        if (!opts.empty()) {
            std::vector<std::basic_string<CharT>> opts_str;
            opts_str.reserve(opts.size());
            uxs::transform(opts, std::back_inserter(opts_str), [](decltype(*opts.cbegin()) opt) {
                if (opt->is_optional()) {
                    return static_cast<CharT>('[') + opt->make_text(text_briefness::brief) + static_cast<CharT>(']');
                }
                return opt->make_text(text_briefness::brief);
            });
            for (const auto& s : opts_str) {
                if (width + s.size() + 1 > max_width) {
                    width = left_margin + tab_size - 1;
                    osb.put('\n').fill_n(width, ' ');
                }
                width += s.size() + 1;
                osb.put(' ').write(s);
            }
        }

        if (!subcommands_.empty()) {
            const auto label_subcommand = string_literal<CharT, '{', 'S', 'U', 'B', 'C', 'O', 'M', 'M', 'A', 'N', 'D',
                                                         '}', ' ', '.', '.', '.'>{}();
            osb.put('\n').fill_n(left_margin, ' ');
            for (const auto& name : uxs::make_reverse_range(cmd_names)) { osb.write(name).put(' '); }
            osb.write(name_).put(' ').write(label_subcommand);
        }
        if (coloring == text_coloring::colored) { osb.write(color_normal); }
        osb.put('\n');
        if (end_width_nl) { osb.put('\n'); }
    };

    auto print_parameters = [this, &osb, end_width_nl, coloring]() {
        if (uxs::all_of(values_, [](decltype(*values_.cbegin()) val) { return val->get_doc().empty(); })) { return; }

        if (coloring == text_coloring::colored) { osb.write(color_br_white); }
        const auto label_parameters =
            string_literal<CharT, 'P', 'A', 'R', 'A', 'M', 'E', 'T', 'E', 'R', 'S', ':', ' '>{}();
        osb.write(label_parameters);
        if (coloring == text_coloring::colored) { osb.write(color_normal); }

        const std::size_t width = std::min<std::size_t>(
            max_margin, std::accumulate(values_.begin(), values_.end(), static_cast<std::size_t>(0),
                                        [](std::size_t w, decltype(*values_.cbegin()) val) {
                                            return std::max(w, val->get_label().size());
                                        }));
        for (const auto& val : values_) {
            if (val->get_doc().empty()) { continue; }
            const bool start_from_new_line = val->get_label().size() > max_margin;
            if (coloring == text_coloring::colored) { osb.write(color_green); }
            osb.put('\n').fill_n(tab_size, ' ').write(val->get_label());
            if (coloring == text_coloring::colored) { osb.write(color_normal); }
            if (start_from_new_line) {
                osb.put('\n').fill_n(tab_size + gap + max_margin, ' ');
            } else {
                osb.fill_n(width + gap - val->get_label().size(), ' ');
            }
            detail::print_text_with_margin<CharT>(osb, val->get_doc(),
                                                  tab_size + gap + (start_from_new_line ? max_margin : width));
        }

        osb.put('\n');
        if (end_width_nl) { osb.put('\n'); }
    };

    auto print_options = [this, &osb, end_width_nl, coloring]() {
        std::vector<std::basic_string<CharT>> opts_str;
        opts_str.reserve(32);
        opts_->traverse_options([&opts_str](const basic_option_node<CharT>& node) {
            if (!node.get_doc().empty()) { opts_str.emplace_back(node.make_text(text_briefness::full)); }
            return true;
        });

        if (opts_str.empty()) { return; }

        if (coloring == text_coloring::colored) { osb.write(color_br_white); }
        const auto label_options = string_literal<CharT, 'O', 'P', 'T', 'I', 'O', 'N', 'S', ':', ' '>{}();
        osb.write(label_options);
        if (coloring == text_coloring::colored) { osb.write(color_normal); }

        const std::size_t width = std::min<std::size_t>(
            max_margin,
            std::accumulate(opts_str.begin(), opts_str.end(), static_cast<std::size_t>(0),
                            [](std::size_t w, decltype(*opts_str.cbegin()) s) { return std::max(w, s.size()); }));
        auto opts_str_it = opts_str.begin();
        opts_->traverse_options([&osb, &opts_str_it, width, coloring](const basic_option_node<CharT>& node) {
            if (node.get_doc().empty()) { return true; }
            const auto& s = *opts_str_it++;
            const bool start_from_new_line = s.size() > max_margin;
            if (coloring == text_coloring::colored) { osb.write(color_green); }
            osb.put('\n').fill_n(tab_size, ' ').write(s);
            if (coloring == text_coloring::colored) { osb.write(color_normal); }
            if (start_from_new_line) {
                osb.put('\n').fill_n(tab_size + gap + max_margin, ' ');
            } else {
                osb.fill_n(width + gap - s.size(), ' ');
            }
            detail::print_text_with_margin<CharT>(osb, node.get_doc(),
                                                  tab_size + gap + (start_from_new_line ? max_margin : width));
            return true;
        });

        osb.put('\n');
        if (end_width_nl) { osb.put('\n'); }
    };

    auto print_subcommands = [this, &osb, end_width_nl, coloring]() {
        if (uxs::all_of(subcommands_,
                        [](decltype(*subcommands_.cbegin()) item) { return item.second->get_doc().empty(); })) {
            return;
        }

        if (coloring == text_coloring::colored) { osb.write(color_br_white); }
        const auto label_subcommands =
            string_literal<CharT, 'S', 'U', 'B', 'C', 'O', 'M', 'M', 'A', 'N', 'D', 'S', ':', ' '>{}();
        osb.write(label_subcommands);
        if (coloring == text_coloring::colored) { osb.write(color_normal); }

        const std::size_t width = std::min<std::size_t>(
            max_margin, std::accumulate(subcommands_.begin(), subcommands_.end(), static_cast<std::size_t>(0),
                                        [](std::size_t w, decltype(*subcommands_.cbegin()) item) {
                                            return std::max(w, item.first.size());
                                        }));
        for (const auto& subcmd : subcommands_) {
            if (subcmd.second->get_doc().empty()) { continue; }
            const bool start_from_new_line = subcmd.first.size() > max_margin;
            if (coloring == text_coloring::colored) { osb.write(color_green); }
            osb.put('\n').fill_n(tab_size, ' ').write(subcmd.first);
            if (coloring == text_coloring::colored) { osb.write(color_normal); }
            if (start_from_new_line) {
                osb.put('\n').fill_n(tab_size + gap + max_margin, ' ');
            } else {
                osb.fill_n(width + gap - subcmd.first.size(), ' ');
            }
            detail::print_text_with_margin<CharT>(osb, subcmd.second->get_doc(),
                                                  tab_size + gap + (start_from_new_line ? max_margin : width));
        }

        osb.put('\n');
        if (end_width_nl) { osb.put('\n'); }
    };

    if (!overview_.empty()) {
        if (coloring == text_coloring::colored) { osb.write(color_br_white); }
        const auto label_overview = string_literal<CharT, 'O', 'V', 'E', 'R', 'V', 'I', 'E', 'W', ':', ' '>{}();
        osb.write(label_overview);
        if (coloring == text_coloring::colored) { osb.write(color_normal); }
        detail::print_text_with_margin<CharT>(osb, overview_, start_with_nl ? tab_size : label_overview.size());
        osb.put('\n');
    }

    print_usage();
    print_parameters();
    print_options();
    print_subcommands();
    return std::basic_string<CharT>(osb.data(), osb.size());
}

}  // namespace cli
}  // namespace uxs
