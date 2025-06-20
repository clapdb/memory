/*
 * Copyright (C) 2020 Beijing Jinyi Data Technology Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 +------------------------------------------------------------------------------+
|                                                                              |
|                                                                              |
|                    ..######..########.########..########.                    |
|                    .##....##....##....##.....##.##.....##                    |
|                    .##..........##....##.....##.##.....##                    |
|                    ..######.....##....##.....##.########.                    |
|                    .......##....##....##.....##.##.....##                    |
|                    .##....##....##....##.....##.##.....##                    |
|                    ..######.....##....########..########.                    |
|                                                                              |
|                                                                              |
|                                                                              |
+------------------------------------------------------------------------------+
*/

#include "optparse/optparse.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>

namespace stdb::optparse {

auto trim_string(std::string_view input) -> string {
    size_t end_pos = input.find_last_not_of(" \t\n\r\f\v");
    size_t start_pos = input.find_first_not_of(" \t\n\r\f\v");
    if (end_pos == string::npos or start_pos == string::npos) {
        return "";
    }
    return string{input.substr(start_pos, end_pos - start_pos + 1)};
}

inline auto split(std::string_view str, std::string_view delimiter, bool skip_empty) -> vector<string> {
    vector<string> tokens;

    size_t pos_current = 0;
    size_t pos_last = 0;
    size_t length = 0;

    while (true) {
        pos_current = str.find(delimiter, pos_last);
        if (pos_current == string::npos) pos_current = str.size();

        length = pos_current - pos_last;
        if (!skip_empty || (length != 0)) tokens.emplace_back(trim_string(str.substr(pos_last, length)));

        if (pos_current == str.size()) {
            break;
        }
        pos_last = pos_current + delimiter.size();
    }
    return tokens;
}

auto Option::validate() -> bool {
    // if _dest is empty, we will use the first long option name as the _dest.
    if (_dest.empty()) {
        for (const auto& nnn : _names) {
            if (_parser.extract_option_type(nnn) == OptionType::LongOpt) {
                _dest = nnn.substr(2);
                break;
            }
        }
    }
    if (_names.empty() or _action == Action::Null or _dest.empty() or (_type != Type::Choice and not _choices.empty()))
      [[unlikely]]
        return false;
    return true;
}

OptionParser::OptionParser(char prefix) : _prefix(prefix) {
    if (_prefix != '-' and _prefix != '+' and _prefix != '#' and _prefix != '$' and _prefix != '&' and _prefix != '%') {
        throw std::logic_error(
          std::format("Invalid prefix: {}, the prefix has to be one of -, +, #, $, &, %", _prefix));
    }
    _long_prefix = std::format("{}{}", _prefix, _prefix);
}

OptionParser::OptionParser(stdb::optparse::ConflictHandler handler) : _conflict_handler(handler) {}

OptionParser::OptionParser(char prefix, stdb::optparse::ConflictHandler handler)
    : _prefix{prefix}, _conflict_handler{handler} {}

auto OptionParser::extract_option_type(const string& opt) const -> OptionType {
    if (opt.size() > 2 and opt.substr(0, 2) == _long_prefix) {
        return OptionType::LongOpt;
    }

    if (opt[0] == _prefix) {
        return OptionType::ShortOpt;
    }

    return OptionType::InvalidOpt;
}

auto OptionParser::extract_arg_type(const string& arg) const -> OptionType {
    if (not arg.starts_with(_prefix)) {
        return OptionType::InvalidOpt;
    }
    if (arg.starts_with(_long_prefix)) {
        return OptionType::LongOpt;
    }
    return OptionType::ShortOpt;
}

auto OptionParser::add_option(stdb::optparse::Option option) -> Option& {
    _options.emplace_back(std::move(option));
    // register all names for the option.
    for (const auto& name : _options.back().names()) {
        auto type = extract_option_type(name);
        if (type == OptionType::ShortOpt) {
            if (_conflict_handler == ConflictHandler::Error and _short_option_map.contains(name)) {
                throw std::logic_error(std::format("Short option {} is already registered", name));
            }
            // if the ConflictHandler is Replace, we will replace the old option with the new one.
            _short_option_map[name] = _options.size() - 1;
        } else if (type == OptionType::LongOpt) {
            if (_conflict_handler == ConflictHandler::Error and _long_option_map.contains(name)) {
                throw std::logic_error(std::format("Long option {} is already registered", name));
            }
            // if the ConflictHandler is Replace, we will replace the old option with the new one.
            _long_option_map[name] = _options.size() - 1;
        } else {
            throw std::logic_error(std::format("Invalid option name: {}", name));
        }
    }

    return _options.back();
}

auto OptionParser::add_option(std::initializer_list<string> names) -> Option& {
    Option option{*this, {names}};
    return add_option(std::move(option));
}

auto OptionParser::add_option(string short_name, string long_name) -> Option& {
    Option option{*this, {std::move(short_name), std::move(long_name)}};
    return add_option(std::move(option));
}

auto OptionParser::add_help_option(string help_msg) -> void {
    auto local_msg = std::move(help_msg);
    auto short_help_name = std::format("{}h", _prefix);
    auto long_help_name = std::format("{}help", _long_prefix);
    if (not _short_option_map.contains(short_help_name) and not _long_option_map.contains(long_help_name)) {
        add_option(short_help_name, long_help_name)
          .dest("help")
          .action(Action::StoreTrue)
          .type(Type::Bool)
          .help(local_msg.empty() ? std::format("show the help of the {}", _program) : std::move(local_msg));
    }
}

auto OptionParser::add_usage_option(string usage_msg) -> void {
    auto local_msg = std::move(usage_msg);
    auto short_usage_name = std::format("{}u", _prefix);
    auto long_usage_name = std::format("{}usage", _long_prefix);
    if (not _short_option_map.contains(short_usage_name) and not _long_option_map.contains(long_usage_name)) {
        add_option(short_usage_name, long_usage_name)
          .dest("usage")
          .action(Action::StoreTrue)
          .type(Type::Bool)
          .help(local_msg.empty() ? std::format("show usage of the {}", _program) : std::move(local_msg));
    }
}

auto OptionParser::add_version_option(stdb::optparse::string version_msg) -> void {
    _version = std::move(version_msg);
    auto short_version_name = std::format("{}v", _prefix);
    auto long_version_name = std::format("{}version", _long_prefix);
    if (not _short_option_map.contains(short_version_name) and not _long_option_map.contains(long_version_name)) {
        add_option(short_version_name, long_version_name)
          .dest("version")
          .action(Action::StoreTrue)
          .type(Type::Bool)
          .help(std::format("show version of the {}", _program));
    }
}

auto OptionParser::add_option(string name) -> Option& {
    Option option{*this, {std::move(name)}};
    return add_option(std::move(option));
}

auto OptionParser::parse_args(int argc, const char** argv) -> ValueStore {
    if (argc == 0) {
        // do nothing if argc is 0
        throw std::logic_error("argc is 0");
    }
    if (_program.empty()) {
        _program.assign(argv[0]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    _argc = argc;
    _argv = argv;
    return parse_args({&argv[1], &argv[argc]}, argv);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

auto OptionParser::parse_args(vector<stdb::optparse::string> args, const char** argv) -> ValueStore {
    // once parse args, clear all invalid_args
    _invalid_args.clear();
    for (auto& opt : _options) {
        if (not opt.validate()) {
            throw std::logic_error(std::format("incomplete option: {}", opt.names().front()));
        }
    }
    add_usage_option({});
    add_help_option({});
    add_version_option({});
    ArgList args_list{std::move(args)};
    ValueStore store;
    auto argv_span = std::span(argv, static_cast<uint32_t>(_argc));
    while (not args_list.empty()) {
        auto front = args_list.peek();
        auto arg_type = extract_arg_type(front);
        if (arg_type == OptionType::InvalidOpt) {
            // pop the first argument is not an option, and drag-and-drop to the invalid args list.
            auto the_invalid_arg = args_list.pop();
            // _invalid_args.emplace_back(std::move(the_invalid_arg));
            _invalid_args.push_back(argv_span[args_list.pos()]);
        } else {
            // arg_type == OptionType::LongOpt or OptionType::ShortOpt
            if (not handle_opt(store, args_list, argv_span)) {
                _invalid_args.push_back(argv_span[args_list.pos()]);
            }
        }
    }
    // check defaults
    for (auto& opt : _options) {
        if (not opt.default_value().empty()) {
            auto dest = opt.dest();
            if (not store.user_set(dest)) {
                process_opt(opt, store, opt.default_value());
            }
        }
    }
    // check environment variables
    for (auto& opt : _options) {
        if (not opt.env().empty()) {
            auto dest = opt.dest();
            if (not store.user_set(dest)) {
                // get the environment variable
                auto* env_val = std::getenv(opt.env().data());
                if (env_val != nullptr) {
                    process_opt(opt, store, env_val);
                }
            }
        }
    }

    return store;
}
template <typename T>
concept OptValue = std::is_same_v<T, bool> or std::is_same_v<T, int32_t> or std::is_same_v<T, int64_t> or
                   std::is_same_v<T, float> or std::is_same_v<T, double>;

template <OptValue T>
auto parse_string(const string& str) -> T {
    T val;
    std::stringstream sst(str.data());
    sst >> val;
    return val;
}

auto is_mutli_value(const string& val_str) -> bool { return val_str.find(',') != string::npos; }

auto parse_value(string val, Type typ, const Option* option = nullptr) -> std::optional<Value> {
    if (typ == Type::Bool) {
        return parse_string<bool>(val);
    }
    if (typ == Type::Int) {
        return parse_string<int>(val);
    }
    if (typ == Type::Long) {
        return parse_string<int64_t>(val);
    }
    if (typ == Type::Float) {
        return parse_string<float>(val);
    }
    if (typ == Type::Double) {
        return parse_string<double>(val);
    }
    if (typ == Type::Choice) {
        auto choice = val;
        if (option != nullptr) {
            const auto& choices = option->choices();
            if (choices.contains(choice)) {
                return choice;
            }
        }
        return std::nullopt;
    }
    return val;
}

auto OptionParser::process_opt(const stdb::optparse::Option& opt, stdb::optparse::ValueStore& store, string str_val)
  -> bool {
    auto dest = opt.dest();
    auto type = opt.type();
    switch (opt.action()) {
        case Store: {
            auto parsed_val = parse_value(str_val, type, &opt);
            if (parsed_val) {
                store.set(dest, *parsed_val);
                return true;
            }
            return false;
        }

        case StoreTrue: {
            if (str_val.empty()) {
                store.set(dest, true);
            } else {
                auto parsed_val = parse_value(str_val, type, &opt);
                if (parsed_val) {
                    store.set(dest, *parsed_val);
                    return true;
                }
                return false;
            }
            return true;
        }
        case StoreFalse: {
            if (str_val.empty()) {
                store.set(dest, false);
            } else {
                auto parsed_val = parse_value(str_val, type, &opt);
                if (parsed_val) {
                    store.set(dest, *parsed_val);
                    return true;
                }
                return false;
            }
            return true;
        }
        case Append: {
            if (not is_mutli_value(str_val)) {
                auto parsed_val = parse_value(str_val, type, &opt);
                if (parsed_val) {
                    store.append(dest, *parsed_val);
                    return true;
                }
                return false;
            }
            // split the string by ',' to get the arg values
            auto values = split(str_val, ",", true);
            for (auto& val : values) {
                auto parsed_val = parse_value(val, type, &opt);
                if (parsed_val) {
                    store.append(dest, *parsed_val);
                } else {
                    return false;
                }
            }
            return true;
        }
        case Count: {
            store.increment(dest);
            return true;
        }
        case Help: {
            throw std::logic_error("Help action is not supported yet.");
            __builtin_unreachable();
        }
        case Version: {
            throw std::logic_error("Version action is not supported yet.");
            __builtin_unreachable();
        }
        case Null: {
            throw std::logic_error("Null action is init value of Action, that was incorrect.");
            __builtin_unreachable();
        }
    }
    __builtin_unreachable();
    return false;
}

auto extract_opt_name(string opt) -> string {
    auto delim = opt.find('=');
    if (delim == string::npos) {
        return opt;
    }
    return opt.substr(0, delim);
}

auto extract_opt_value(string opt) -> string {
    auto delim = opt.find('=');
    if (delim == string::npos) {
        return {};
    }
    return opt.substr(delim + 1);
}

auto OptionParser::has_value_to_process(string current_arg, const ArgList& args) const -> bool {
    return current_arg.find('=') != string::npos or (not args.empty() and not args.peek().starts_with(_prefix));
}

auto OptionParser::find_opt(string opt_name) -> Option* {
    if (opt_name.starts_with(_long_prefix)) {
        if (auto opt_it = _long_option_map.find(opt_name); opt_it != _long_option_map.end()) [[likely]] {
            return &_options[opt_it->second];
        }
    } else if (opt_name.starts_with(_prefix)) {
        if (auto opt_it = _short_option_map.find(opt_name); opt_it != _short_option_map.end()) [[likely]] {
            return &_options[opt_it->second];
        }
    }
    return nullptr;
}

auto OptionParser::handle_opt(ValueStore& values, ArgList& args, std::span<const char*> argv_span) -> bool {
    if (not args.empty()) {
        // get front of args
        auto front = args.pop();

        auto opt_name = extract_opt_name(front);
        opt_name = trim_string(opt_name);

        auto* opt_ptr = find_opt(opt_name);
        if (opt_ptr == nullptr) {
            // if the opt is not found, then it is an invalid option.
            return false;
        }
        auto& opt = *opt_ptr;
        if (not has_value_to_process(front, args)) {
            if (opt.type() == Type::Bool) {
                process_opt(opt, values, {});
                return true;
            }
            throw std::logic_error(std::format("option {} is not Bool, so requires an value-argument", opt.dest()));
        }
        // has_value_to_process in the front
        if (auto val = extract_opt_value(front); val.empty()) {
            // if the value is empty, then pop the next argument.
            if (not args.empty()) {
                auto next_val = args.pop();
                process_opt(opt, values, next_val);
            } else {
                return false;
            }
        } else {
            process_opt(opt, values, val);
        }
        // continue process following values for Append opt
        while (not args.empty()) {
            auto next_arg = args.peek();
            // if the next arg is not start with prefix, then it is a value
            if (next_arg.starts_with(_prefix)) {
                break;
            }
            // make sure the opt is Append type
            if (opt.action() != Append) {
                // pop the first argument is not an option, and drag-and-drop to the invalid args list.
                auto the_invalid_arg = args.pop();
                // _invalid_args.emplace_back(std::move(the_invalid_arg));
                _invalid_args.push_back(argv_span[args.pos()]);
            } else {
                auto next_val = args.pop();
                process_opt(opt, values, next_val);
            }
        }
        return true;
    }
    return false;
}

/*
auto OptionParser::handle_long_opt(stdb::optparse::ValueStore& values, ArgList& args) -> bool {
    if (not args.empty()) [[likely]] {
        auto front = args.pop();

        auto long_name = extract_opt_name(front);

        long_name = trim_string(long_name);

        auto opt_it = _long_option_map.find(long_name);
        if (opt_it == _long_option_map.end()) {
            _invalid_args.emplace_back(std::move(front));
            // TODO(hurricane): maybe use enum to represent the return value.
            // for telling the caller the next arg is not valid for any option.
            return true;
        }
        size_t opt_offset = opt_it->second;
        auto& opt = _options[opt_offset];
        if (has_value_to_process(front, args)) {
            if (opt.type() == Type::Bool) {
                process_opt(opt, values, {});
                return true;
            }
            // raise error if the option is not a bool option.
            throw std::logic_error(std::format("option {} is not Bool, and it requires an argument", front));
        }
        // if nargs is 2, then process the option and pop the next 2 arguments.
        if (opt.nargs() == 1) {
            if (auto val = extract_opt_value(front); val.empty()) {
                // if the value is empty, then pop the next argument.
                if (not args.empty()) {
                    auto next_val = args.pop();
                    process_opt(opt, values, next_val);
                } else {
                    return false;
                }
            } else {
                process_opt(opt, values, val);
            }
            return true;
        }
        // if nargs is greater than 2, then it is an invalid option.

        auto nargs = opt.nargs();
        if (auto val = extract_opt_value(front); not val.empty()) {
            process_opt(opt, values, val);
            --nargs;
        }
        // if nargs is greater than 2, then it is an append option to a list.
        for (size_t i = 0; i < nargs; ++i) {
            if (not args.empty()) {
                auto next_val = args.pop();
                // should make sure the action is Append
                assert(opt.action() == Append);
                process_opt(opt, values, next_val);
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}
 */

auto to_upper(string& input) -> void {
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c) { return std::toupper(c); });  // NOLINT(readability-identifier-length)
}

auto OptionParser::format_usage() -> string {
    if (_usage.empty()) {
        // enrich the _usage message.
        _usage = std::format("usage: {}", _program);
        for (auto& opt : _options) {
            if (opt.type() == Type::Bool) {
                auto upper_dest = opt.dest();
                to_upper(upper_dest);
                auto opt_msg = std::format(" [{} {}]", opt.names().front(), upper_dest);
                _usage += std::string_view{opt_msg};
            } else {
                auto opt_msg = std::format(" [{}]", opt.names().front());
                _usage += std::string_view{opt_msg};
            }
        }
        _usage += "\n";
    }
    return _usage;
}

auto OptionParser::format_verison() -> string {
    if (_version.empty()) {
        _version = std::format("{} : {}", _program, "0.0.0");
    }
    return _version;
}

auto format_opt_names(const vector<string>& names) -> string {
    if (names.empty()) {
        throw std::logic_error("names should not be empty vector");
    }
    auto opt_msg = std::format("{}", names.front());
    if (names.size() == 1) {
        return opt_msg;
    }
    // for-loop the rest names, and append to the opt_msg
    for (size_t off = 1; off < names.size(); ++off) {
        opt_msg += std::format(", {}", names[off]);
    }
    return opt_msg;
}

auto OptionParser::format_help() -> string {
    vector<std::pair<string, string>> line_and_helps;

    for (auto& opt : _options) {
        string line = std::format("{}=<{}>", format_opt_names(opt.names()), opt.type());
        auto help = opt.help();
        line_and_helps.emplace_back(line, help);
    }

    const auto max_line =
      std::max_element(line_and_helps.begin(), line_and_helps.end(),
                       [](auto& lhs, auto& rhs) -> bool { return rhs.first.size() > lhs.first.size(); });

    const auto align_size = max_line->first.size() + 1;
    auto content = std::format("{} \n options: \n", format_usage());
    for (const auto& [line, help] : line_and_helps) {
        const int space_width = align_size - line.size();
        auto spaces = string(static_cast<size_t>(space_width), ' ');
        content += std::format("  {}{}{}\n", line, spaces, help);
    }
    return content;
}

auto OptionParser::print_help() -> void { std::print("{}", format_help()); }
auto OptionParser::print_usage() -> void { std::print("{}", format_usage()); }

auto OptionParser::print_version() -> void { std::print("{}", format_verison()); }

auto OptionParser::invalid_args() -> vector<const char*> { return _invalid_args; }

auto OptionParser::get_raw_argc() const -> int {
    // the first argument is the program name.
    return _invalid_args.size() + 1;
}

auto OptionParser::get_raw_argv() const -> std::unique_ptr<const char*[]> {  // NOLINT
    auto new_argv_size = _invalid_args.size() + 1;
    auto rst = std::unique_ptr<const char*[]>(new const char*[new_argv_size]);  // NOLINT
    rst[0] = _argv[0];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (size_t i = 1; i < new_argv_size; ++i) {
        rst[i] = _invalid_args[i - 1];
    }
    return rst;
}
}  // namespace stdb::optparse

namespace std {

auto formatter<stdb::optparse::OptionType>::format(stdb::optparse::OptionType opt_type,
                                                   std::format_context& ctx) const noexcept -> decltype(ctx.out()) {
    std::string str;
    switch (opt_type) {
        // generate all the option type names.
        case stdb::optparse::OptionType::ShortOpt:
            str = "short option";
            break;
        case stdb::optparse::OptionType::LongOpt:
            str = "long option";
            break;
        case stdb::optparse::OptionType::InvalidOpt:
            str = "invalid option";
            break;
    }
    return formatter<string>::format(str, ctx);
}

auto formatter<stdb::optparse::Type>::format(stdb::optparse::Type opt_type, std::format_context& ctx) const noexcept
  -> decltype(ctx.out()) {
    std::string str;
    switch (opt_type) {
        case stdb::optparse::Type::Bool:
            str = "bool";
            break;
        case stdb::optparse::Type::Int:
            str = "int";
            break;
        case stdb::optparse::Type::Long:
            str = "long";
            break;
        case stdb::optparse::Type::Float:
            str = "float";
            break;
        case stdb::optparse::Type::Double:
            str = "double";
            break;
        case stdb::optparse::Type::Choice:
            str = "choice";
            break;
        case stdb::optparse::Type::String:
            str = "string";
            break;
    }
    return formatter<string_view>::format(str, ctx);
}
}  // namespace std