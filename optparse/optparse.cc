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
#include <optional>
#include <ranges>

namespace stdb::optparse {

auto trim_string(string input) -> string {
    size_t end_pos = input.find_last_not_of(" \t\n\r\f\v");
    size_t start_pos = input.find_first_not_of(" \t\n\r\f\v");
    if (end_pos == string::npos or start_pos == string::npos) {
        return "";
    }
    return input.substr(start_pos, end_pos - start_pos + 1);
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
          fmt::format("Invalid prefix: {}, the prefix has to be one of -, +, #, $, &, %", _prefix));
    }
    _long_prefix = fmt::format("{}{}", _prefix, _prefix);
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
                throw std::logic_error(fmt::format("Short option {} is already registered", name));
            }
            // if the ConflictHandler is Replace, we will replace the old option with the new one.
            _short_option_map[name] = _options.size() - 1;
        } else if (type == OptionType::LongOpt) {
            if (_conflict_handler == ConflictHandler::Error and _long_option_map.contains(name)) {
                throw std::logic_error(fmt::format("Long option {} is already registered", name));
            }
            // if the ConflictHandler is Replace, we will replace the old option with the new one.
            _long_option_map[name] = _options.size() - 1;
        } else {
            throw std::logic_error(fmt::format("Invalid option name: {}", name));
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
    auto short_help_name = fmt::format("{}h", _prefix);
    auto long_help_name = fmt::format("{}help", _long_prefix);
    if (not _short_option_map.contains(short_help_name) and not _long_option_map.contains(long_help_name)) {
        add_option(short_help_name, long_help_name)
          .dest("help")
          .action(Action::StoreTrue)
          .nargs(0)
          .type(Type::Bool)
          .help(local_msg.empty() ? fmt::format("show the help of the {}", _program) : std::move(local_msg));
    }
}

auto OptionParser::add_usage_option(string usage_msg) -> void {
    auto local_msg = std::move(usage_msg);
    auto short_usage_name = fmt::format("{}u", _prefix);
    auto long_usage_name = fmt::format("{}usage", _long_prefix);
    if (not _short_option_map.contains(short_usage_name) and not _long_option_map.contains(long_usage_name)) {
        add_option(short_usage_name, long_usage_name)
          .dest("usage")
          .action(Action::StoreTrue)
          .nargs(0)
          .type(Type::Bool)
          .help(local_msg.empty() ? fmt::format("show usage of the {}", _program) : std::move(local_msg));
    }
}

auto OptionParser::add_version_option(stdb::optparse::string version_msg) -> void {
    auto local_msg = std::move(version_msg);
    auto short_version_name = fmt::format("{}v", _prefix);
    auto long_version_name = fmt::format("{}version", _long_prefix);
    if (not _short_option_map.contains(short_version_name) and not _long_option_map.contains(long_version_name)) {
        add_option(short_version_name, long_version_name)
          .dest("version")
          .action(Action::StoreTrue)
          .nargs(0)
          .type(Type::Bool)
          .help(local_msg.empty() ? fmt::format("show version of the {}", _program) : std::move(local_msg));
    }
}

auto OptionParser::add_option(string name) -> Option& {
    Option option{*this, {std::move(name)}};
    return add_option(std::move(option));
}

auto OptionParser::parse_args(int argc, char** argv) -> ValueStore {
    if (argc == 0) {
        // do nothing if argc is 0
        return {};
    }
    if (_program.empty()) {
        _program.assign(argv[0]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return parse_args(
      vector<string>(&argv[1], &argv[argc]));  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

auto OptionParser::parse_args(vector<stdb::optparse::string> args) -> ValueStore {
    for (auto& opt : _options) {
        if (not opt.validate()) {
            throw std::logic_error(fmt::format("incomplete option: {}", opt.names().front()));
        }
    }
    add_usage_option({});
    add_help_option({});
    add_version_option({});
    ArgList args_list{std::move(args)};
    ValueStore store;
    while (not args_list.empty()) {
        auto front = args_list.peek();
        auto arg_type = extract_arg_type(front);
        if (arg_type == OptionType::InvalidOpt) {
            // pop the first argument is not an option, and drag-and-drop to the invalid args list.
            auto the_invalid_arg = args_list.pop();
            _invalid_args.emplace_back(std::move(the_invalid_arg));
        } else if (arg_type == OptionType::ShortOpt) {
            handle_short_opt(store, args_list);
        } else {
            // arg_type == OptionType::LongOpt
            handle_long_opt(store, args_list);
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
            auto parsed_val = parse_value(str_val, type, &opt);
            if (parsed_val) {
                store.append(dest, *parsed_val);
                return true;
            }
            return false;
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

auto extract_long_opt_name(string opt) -> string {
    auto delim = opt.find('=');
    if (delim == string::npos) {
        return opt;
    }
    return opt.substr(0, delim);
}

auto extract_short_opt_name(string opt) -> string { /*return opt.substr(0, 2);*/
    return extract_long_opt_name(opt);
}

auto extract_long_opt_value(string opt) -> string {
    auto delim = opt.find('=');
    if (delim == string::npos) {
        return {};
    }
    return opt.substr(delim + 1);
}

auto extract_short_opt_value(string opt) -> string {
    /*
    if (opt.size() == 2) return {};
    return opt.find('=') == string::npos ? opt.substr(2) : opt.substr(3);
     */
    return extract_long_opt_value(opt);
}


auto OptionParser::handle_short_opt(ValueStore& values, ArgList& args) -> bool {
    if (not args.empty()) {
        // get front of args
        auto front = args.pop();
        // we do not use other prefix except '-' for short options.
        if (front[0] == _prefix and front[1] == 'h') {
            // if the front is -h, then print help message and exit.
            print_help();
            exit(0);
        }
        auto short_name = extract_short_opt_name(front);
        short_name = trim_string(short_name);

        // lookup the option of short options.
        auto opt_it = _short_option_map.find(short_name);
        if (opt_it == _short_option_map.end()) {
            // if the option is not found, then it is an invalid option.
            _invalid_args.emplace_back(std::move(front));
            // TODO(hurricane):maybe use enum to represent the return value.
            // for telling the caller the next arg is not valid for any option.
            return true;
        }
        size_t opt_offset = opt_it->second;
        auto& opt = _options[opt_offset];
        if (opt.nargs() == 0) {
            process_opt(opt, values, {});
            return true;
        }
        // if nargs is 1, then process the option and pop the next 1 argument.
        if (opt.nargs() == 1) {
            if (auto val = extract_short_opt_value(front); val.empty()) {
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
        auto nargs = opt.nargs();
        if (auto val = extract_short_opt_value(front); not val.empty()) {
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

auto OptionParser::handle_long_opt(stdb::optparse::ValueStore& values, ArgList& args) -> bool {
    if (not args.empty()) {
        auto front = args.pop();
        if (front.ends_with("help") and front.starts_with(_long_prefix)) {
            print_help();
            exit(0);
        }
        auto long_name = extract_long_opt_name(front);

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
        if (opt.nargs() == 0) {
            process_opt(opt, values, {});
            return true;
        }
        // if nargs is 2, then process the option and pop the next 2 arguments.
        if (opt.nargs() == 1) {
            if (auto val = extract_long_opt_value(front); val.empty()) {
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
        if (auto val = extract_long_opt_value(front); not val.empty()) {
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

auto to_upper(string& input) -> void {
    std::transform(input.begin(), input.end(), input.begin(),
                   [](unsigned char c) { return std::toupper(c); });  // NOLINT(readability-identifier-length)
}

auto OptionParser::format_usage() -> string {
    if (_usage.empty()) {
        // enrich the _usage message.
        _usage = fmt::format("usage: {}", _program);
        for (auto& opt : _options) {
            if (opt.nargs() > 0) {
                auto upper_dest = opt.dest();
                to_upper(upper_dest);
                auto opt_msg = fmt::format(" [{} {}]", opt.names().front(), upper_dest);
                _usage += std::string_view{opt_msg};
            } else {
                auto opt_msg = fmt::format(" [{}]", opt.names().front());
                _usage += std::string_view{opt_msg};
            }
        }
        _usage += "\n";
    }
    return _usage;
}

auto OptionParser::format_verison() -> string {
    if (_version.empty()) {
        _version = fmt::format("{} : {}", _program, _version);
    }
    return _version;
}

auto format_opt_names(const vector<string>& names, const string& dest) -> string {
    assert(not names.empty());
    auto opt_msg = fmt::format("{} {}", names.front(), dest);
    if (names.size() == 1) {
        return opt_msg;
    }
    // for-loop the rest names, and append to the opt_msg
    for (size_t off = 1; off < names.size(); ++off) {
        opt_msg += fmt::format(", {} {}", names[off], dest);
    }
    return opt_msg;
}

auto OptionParser::format_help() -> string {
    constexpr int column_width = 80 - 1;
    auto content = fmt::format("{} \n options: \n", format_usage());
    for (auto& opt : _options) {
        string line;
        if (opt.nargs() > 0) {
            auto upper_dest = opt.dest();
            to_upper(upper_dest);
            line = fmt::format("  {}", format_opt_names(opt.names(), upper_dest));
        } else {
            line = fmt::format("  {}", format_opt_names(opt.names(), {}));
        }
        auto help = opt.help();
        int space_width = column_width - static_cast<int>(line.size()) - static_cast<int>(help.size());
        if (space_width > 0) {
            // the line + help is shorter than 79
            auto spaces = string(static_cast<size_t>(space_width), ' ');
            content += fmt::format("{}{}{}\n", line, spaces, help);
        } else {
            // newline and add help msg
            content += fmt::format("{}\n  {}\n", line, help);
        }
    }
    return content;
}

auto OptionParser::print_help() -> void { fmt::print("{}", format_help()); }
auto OptionParser::print_usage() -> void { fmt::print("{}", format_usage()); }

auto OptionParser::print_version() -> void { fmt::print("{}", format_verison()); }

auto OptionParser::invalid_args() -> vector<string> {
    return std::move(_invalid_args);
}

}  // namespace stdb::optparse
