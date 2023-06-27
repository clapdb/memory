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

#pragma once
#include <fmt/format.h>

#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <variant>

#include "container/stdb_vector.hpp"
#include "string/string.hpp"

namespace stdb::optparse {

class Option;

using string = memory::string;
// using string = std::string;

template <typename T>
using vector = stdb::container::stdb_vector<T>;

using Value = std::variant<bool, int32_t, int64_t, float, double, string>;

class ValueStore
{
   private:
    std::map<string, Value> _values;
    std::map<string, vector<Value>> _list_values;
    std::set<string> _usr_set;

   public:
    ValueStore() : _values{}, _list_values{}, _usr_set{} {}
    ValueStore(const ValueStore&) = delete;
    ValueStore(ValueStore&&) noexcept = default;
    ~ValueStore() = default;
    auto operator=(const ValueStore&) -> ValueStore& = delete;
    auto operator=(ValueStore&&) noexcept -> ValueStore& = delete;

    [[nodiscard]] inline auto user_set(const string& key) const -> bool { return _usr_set.find(key) != _usr_set.end(); }

    inline auto set(const string& key, Value val) -> void {
        _values[key] = std::move(val);
        _usr_set.insert(key);
    }

    inline auto append(string key, Value val) -> void {
        auto lit = _list_values.find(key);
        if (lit == _list_values.end()) {
            _list_values.emplace(key, vector<Value>{std::move(val)});
        } else {
            auto& list_values = lit->second;
            list_values.push_back(val);
        }
        _usr_set.insert(key);
    }

    inline auto append(string key, std::initializer_list<Value> values) -> void {
        auto lit = _list_values.find(key);
        if (lit == _list_values.end()) {
            _list_values[key] = vector<Value>{values};
        } else {
            auto& list_values = lit->second;
            for (const auto& val : values) {
                list_values.push_back(val);
            }
        }
        _usr_set.insert(key);
    }

    inline auto increment(string key) -> void {
        auto vit = _values.find(key);
        if (vit == _values.end()) {
            _values[key] = 1;
        } else {
            _values[key] = std::get<int>(vit->second) + 1;
        }
    }

    template <typename T>
    [[nodiscard]] auto get(string key) const noexcept -> std::optional<T> {
        if (auto vit = _values.find(key); vit != _values.end()) {
            return std::get<T>(vit->second);
        }
        return std::nullopt;
    }

    [[nodiscard]] inline auto get_list(string key) const -> std::optional<vector<Value>> {
        if (auto lit = _list_values.find(key); lit != _list_values.end()) [[likely]] {
            return lit->second;
        }
        return std::nullopt;
    }

    template<typename T>
    [[nodiscard]] inline auto get_list(string key) const -> std::optional<vector<T>> {
        if (auto variant_vector = get_list(key)) {
            vector<T> ret;
            for (const auto& val : *variant_vector) {
                ret.push_back(std::get<T>(val));
            }
            return ret;
        }
        return std::nullopt;
    }

};  // class ValueStore
class OptionParser;
enum Action : uint8_t
{
    Null = 0,
    Store = 1,
    StoreTrue,
    StoreFalse,
    Append,
    Count,
    Help,
    Version,
};

enum Type : uint8_t
{
    Bool = 1,
    Int,
    Long,
    Float,
    Double,
    Choice,
    String,
};

class Option
{
   private:
    const OptionParser& _parser;
    vector<string> _names;
    Action _action = Action::Null;
    Type _type = Type::String;
    string _dest = "";
    string _default = "";
    std::set<string> _choices{};
    string _help = "";
    string _env = "";

   public:
    explicit Option(const OptionParser& opp) : _parser(opp) {}
    ~Option() = default;

    Option(const Option&) = default;
    Option(Option&&) noexcept = default;
    auto operator=(const Option&) -> Option& = delete;
    auto operator=(Option&&) noexcept -> Option& = delete;

    Option(const OptionParser& opp, vector<string> names) : _parser(opp), _names(std::move(names)) {}

    auto validate() -> bool;

    [[nodiscard]] inline auto names() const -> const vector<string>& { return _names; }

    inline auto action(Action act) -> Option& {
        _action = act;
        return *this;
    }

    [[nodiscard]] inline auto action() const -> Action { return _action; }

    inline auto type(Type typ) -> Option& {
        _type = typ;
        return *this;
    }

    [[nodiscard]] inline auto type() const -> Type { return _type; }

    inline auto dest(string dst) -> Option& {
        _dest = std::move(dst);
        return *this;
    }

    [[nodiscard]] inline auto dest() const -> string { return _dest; }

    inline auto default_value(string dft) -> Option& {
        _default = std::move(dft);
        return *this;
    }

    template <typename T>
    inline auto default_value(T dft) -> Option& {
        _default = std::to_string(dft);
        return *this;
    }

    [[nodiscard]] inline auto default_value() const -> string { return _default; }

    template <typename InputIterator>
    inline auto choices(InputIterator begin, InputIterator end) -> Option& {
        _choices.insert(begin, end);
        type(Type::Choice);
        return *this;
    }

    inline auto choices(::std::initializer_list<string> choices) -> Option& {
        _choices.insert(choices.begin(), choices.end());
        type(Type::Choice);
        return *this;
    }

    [[nodiscard]] inline auto choices() const -> const std::set<string>& { return _choices; }

    inline auto help(string msg) -> Option& {
        _help = std::move(msg);
        return *this;
    }

    inline auto help() -> string { return _help; }

    inline auto env(string var_name) -> Option& {
        _env = std::move(var_name);
        return *this;
    }

    [[nodiscard]] inline auto env() const -> string { return _env; }

};  // class Option

enum ConflictHandler : uint8_t
{
    Error = 0,
    Replace,
};

enum OptionType : uint8_t
{
    ShortOpt = 0,
    LongOpt,
    InvalidOpt,
};

class ArgList
{
   private:
    vector<string> _args;
    vector<string>::iterator _it;

   public:
    explicit ArgList(vector<string> args) : _args(std::move(args)), _it(_args.begin()) {}
    ~ArgList() = default;
    ArgList(const ArgList&) = delete;
    ArgList(ArgList&&) noexcept = delete;
    auto operator=(const ArgList&) -> ArgList& = delete;
    auto operator=(ArgList&&) noexcept -> ArgList& = delete;

    [[nodiscard]] inline auto empty() const -> bool { return _it == _args.end(); }
    inline auto pop() -> string { return *_it++; }
    [[nodiscard]] inline auto peek() const -> string { return *_it; }

};  // class ArgList

class OptionParser
{
   private:
    const char _prefix = '-';
    string _long_prefix = "--";
    string _program;
    string _usage;
    string _version;
    vector<Option> _options;
    std::map<string, size_t> _long_option_map;
    std::map<string, size_t> _short_option_map;
    vector<string> _invalid_args;
    ConflictHandler _conflict_handler = ConflictHandler::Error;

    [[nodiscard]] auto extract_arg_type(const string& arg) const -> OptionType;

    auto add_option(Option option) -> Option&;

    auto find_opt(string opt_name) -> Option*;

   public:
    explicit OptionParser(char prefix);
    OptionParser(char prefix, ConflictHandler handler);
    explicit OptionParser(ConflictHandler handler);
    OptionParser() = default;
    ~OptionParser() = default;

    OptionParser(const OptionParser&) = default;
    auto operator=(const OptionParser&) -> OptionParser& = delete;
    OptionParser(OptionParser&&) noexcept = default;
    auto operator=(OptionParser&&) noexcept -> OptionParser& = delete;

    [[nodiscard]] auto extract_option_type(const string& opt) const -> OptionType;
    inline auto program(string prog) -> OptionParser& {
        _program = std::move(prog);
        return *this;
    }

    [[nodiscard]] inline auto program() const -> string { return _program; }

    inline auto usage(string msg) -> OptionParser& {
        _usage = std::move(msg);
        return *this;
    }

    [[nodiscard]] inline auto usage() const -> string { return _usage; }

    inline auto version(string ver) -> OptionParser& {
        _version = std::move(ver);
        return *this;
    }

    [[nodiscard]] inline auto version() const -> string { return _version; }

    auto add_option(std::initializer_list<string> names) -> Option&;

    auto add_option(string short_name, string long_name) -> Option&;

    auto add_option(string name) -> Option&;

    auto add_usage_option(string usage_msg) -> void;
    auto add_help_option(string help_msg) -> void;
    auto add_version_option(string version_msg) -> void;

    auto handle_opt(ValueStore&, ArgList& args) -> bool;
//    auto handle_long_opt(ValueStore&, ArgList& args) -> bool;

    static auto process_opt(const Option&, ValueStore&, string) -> bool;

    auto parse_args(vector<string> args) -> ValueStore;

    auto parse_args(int argc, char** argv) -> ValueStore;

    auto format_usage() -> string;
    auto format_help() -> string;
    auto print_help() -> void;
    auto print_usage() -> void;
    auto format_verison() -> string;
    auto print_version() -> void;

    /**
     * @brief move the invalid args vector out of the Object.
     *
     * @return the moved invalid args vector.
     */
    auto invalid_args() -> vector<string>;

    [[nodiscard]] auto invalid_args_to_str() -> string;
    [[nodiscard]] auto has_value_to_process(string current_arg, const ArgList& args) const -> bool;
};

}  // namespace stdb::optparse
