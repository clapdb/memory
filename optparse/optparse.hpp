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
#include "string/string.hpp"
#include <sstream>
#include <map>
//#include <unordered_map>
#include <list>
#include <set>
#include <variant>

#include <iterator>
#include "container/stdb_vector.hpp"

namespace stdb::optparse {

class Option;

using string = memory::string;

template<typename T>
using vector = stdb::container::stdb_vector<T>;

using choice = std::set<string>;
using Value = std::variant<int, long, float, double, string, choice>;


class ValueStore {
   private:
    std::map<string, Value> _values;
    std::map<string, std::list<Value>> _list_values;
    std::set<string> _usr_set;
   public:
    ValueStore(): _values(), _list_values(), _usr_set() {}
    inline auto has(string key) const -> bool {
        return _values.find(key) != _values.end();
    }

    template<typename T>
    inline auto get(string key) const -> T {
        auto it = _values.find(key);
        if (it == _values.end()) [[unlikely]] return {};
        return std::get<T>(it->second);
    }

}; // class ValueStore

class OptionParser;
enum Action : uint8_t
{
    Store = 0,
    StoreConst,
    StoreTrue,
    StoreFalse,
    Append,
    AppendConst,
    Count,
    Help,
    Version,
    Callback,
};

enum Type : uint8_t
{
    Int = 0,
    Long,
    Float,
    Double,
    Choice,
    String,
};

enum Const : uint8_t
{
    True = 0,
    False,
};

class Option {
 private:
  const OptionParser& _parser;
  vector<string> _names;
  Action _action;
  Type _type;
  string _dest;
  string _default;
  size_t _nargs;
  Const _const;
  vector<string> _choices;
  string _help;
  string _env;

 public:
  Option(const OptionParser& p): _parser(p) {}
  ~Option() = default;

  Option(const Option&) = default;
  Option(Option&&) noexcept = default;

  inline auto names(std::initializer_list<string> names) -> Option& {
    _names = names;
    return *this;
  }

  inline auto names(vector<string> names) -> Option& {
    _names = std::move(names);
    return *this;
  }

  inline auto names() const -> const vector<string>& {
    return _names;
  }


  inline auto action(Action a) -> Option& {
    _action = a;
    return *this;
  }

  inline auto type(Type t) -> Option& {
    _type = t;
    return *this;
  }

  inline auto dest(string d) -> Option& {
    _dest = std::move(d);
    return *this;
  }

  inline auto set_default(string d) -> Option& {
    _default = std::move(d);
    return *this;
  }

  inline auto get_default() const -> string {
    return _default;
  }

  inline auto nargs(size_t n) -> Option& {
    _nargs = n;
    return *this;
  }

  inline auto set_const(Const c) -> Option& {
    _const = c;
    return *this;
  }
  inline auto get_const() const -> Const {
    return _const;
  }

  template<typename InputIterator>
  inline auto choices(InputIterator begin, InputIterator end) -> Option& {
    _choices.assign(begin, end);
    type(Type::Choice);
    return *this;
  }

  inline auto choices(::std::initializer_list<string> choices) -> Option& {
    _choices.assign(choices.begin(), choices.end());
    type(Type::Choice);
    return *this;
  }

  inline auto help(string h) -> Option& {
    _help = std::move(h);
    return *this;
  }

  inline auto env(string e) -> Option& {
    _env = std::move(e);
    return *this;
  }

}; // class Option

enum ConflictHandler : uint8_t
{
    Error = 0,
    Resolve,
};

enum OptionType : uint8_t
{
    ShortOpt = 0,
    LongOpt,
    InvalidOpt,
};

class OptionParser {
 private:
  const char _prefix = '-';
  string  _long_prefix = "--";
  string _program;
  string _usage;
  string _version;
  vector<Option> _options;
  std::map<string, Option*> _long_option_map;
  std::map<string, Option*> _short_option_map;
  vector<string> _invalid_args;

  auto extract_option_type(string opt) const -> OptionType {
    if (opt.size() == 2 and opt[0] == _prefix) {
        return OptionType::ShortOpt;
    } else if (opt.size() > 2 and opt.substr(0, 2) == _long_prefix) {
        return OptionType::LongOpt;
    } else {
        return OptionType::InvalidOpt;
    }
  }


  auto extract_arg_type(string arg) const -> OptionType {
    if (not arg.starts_with(_prefix)) {
        return OptionType::InvalidOpt;
    }
    if (arg.starts_with(_long_prefix)) {
        return OptionType::LongOpt;
    }
    return OptionType::ShortOpt;
  }

 public:
  explicit OptionParser(char prefix) : _prefix(prefix) {
    if (not(_prefix == '-' or _prefix == '+' or _prefix == '#' or _prefix == '$' or _prefix == '&' or _prefix == '%')) {
        throw std::logic_error(fmt::format("Invalid prefix: {}, the prefix has to be one of -, +, #, $, &, %", _prefix));
    }
    _long_prefix = fmt::format("{}{}", _prefix, _prefix);
  }
  ~OptionParser() = default;

  auto program(string p) -> OptionParser& {
    _program = std::move(p);
    return *this;
  }
  auto program() const -> string {
    return _program;
  }
  auto usage(string u) -> OptionParser& {
    _usage = std::move(u);
    return *this;
  }
  auto usage() const -> string {
    return _usage;
  }
  auto version(string v) -> OptionParser& {
    _version = std::move(v);
    return *this;
  }
  auto version() const -> string {
    return _version;
  }

  auto add_option(Option option) -> void {
    _options.emplace_back(std::move(option));
    // register all names for the option.
    for (const auto& name : _options.back().names()) {
        auto type = extract_option_type(name);
        if (type == OptionType::ShortOpt) {
            _short_option_map[name] = &_options.back();
        } else if (type == OptionType::LongOpt) {
            _long_option_map[name] = &_options.back();
        } else {
            throw std::logic_error(fmt::format("Invalid option name: {}", name));
        }
    }
    return;
  }

  auto handle_short_opt() -> void {

      return;
  }


  auto handle_long_opt() -> void {

      return;
  }


  auto parse_args(vector<string> args) -> ValueStore {
      auto local_args = std::move(args);
      ValueStore store;
      auto front = local_args.front();
      auto arg_type = extract_arg_type(front);
      while (not local_args.empty()) {
        if (arg_type == OptionType::InvalidOpt) {
            // do nothing if the first argument is not an option.
            _invalid_args.emplace_back(std::move(front));
            continue;
        }
        if (arg_type == OptionType::ShortOpt) {
            handle_short_opt();
        } else if (arg_type == OptionType::LongOpt) {
            handle_long_opt();
        }
      }
  }


  auto parse_args(int argc, char** argv) -> ValueStore {
    if (argc == 0) {
        // do nothing if argc is 0
        return {};
    }
    if (_program.empty()) {
        _program.assign(argv[0]);
    }
    return parse_args(vector<string>(&argv[1], &argv[argc]));
  }

};

} // namespace stdb::optparse



