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
#include <list>
#include <set>
#include <variant>

#include <iterator>
#include "container/stdb_vector.hpp"

namespace stdb::optparse {

class Option;

//using string = memory::string;
using string = std::string;

template<typename T>
using vector = stdb::container::stdb_vector<T>;

using Value = std::variant<bool, int, long, float, double, string>;

class ValueStore {
   private:
    std::map<string, Value> _values;
    std::map<string, vector<Value>> _list_values;
    std::set<string> _usr_set;
   public:
    ValueStore(): _values{}, _list_values{}, _usr_set{} {}
    ValueStore(const ValueStore&) = delete;
    ValueStore(ValueStore&&) noexcept = default;
    ValueStore& operator=(const ValueStore&) = delete;
    ValueStore& operator=(ValueStore&&) noexcept = default;

    inline auto has(string key) const -> bool {
        return _values.find(key) != _values.end();
    }

    inline auto user_set(string key) const -> bool {
        return _usr_set.find(key) != _usr_set.end();
    }

    inline auto set(string key, Value val) -> void {
        _values[key] = val;
        _usr_set.insert(key);
    }

    inline auto append(string key, Value val) -> void {
        auto it = _list_values.find(key);
        if (it == _list_values.end()) {
            _list_values.emplace(key, vector<Value>{val});
        } else {
            auto& list_values = it->second;
            list_values.push_back(val);
        }
        _usr_set.insert(key);
    }

    inline auto append(string key, std::initializer_list<Value> vals) -> void {
        auto it = _list_values.find(key);
        if (it == _list_values.end()) {
            _list_values[key] = vector<Value>{vals};
        } else {
            auto& list_values = it->second;
            for (auto& val : vals) {
                list_values.push_back(val);
            }
        }
        _usr_set.insert(key);
    }

    inline auto increment(string key) -> void {
        auto it = _values.find(key);
        if (it == _values.end()) {
            _values[key] = 1;
        } else {
            _values[key] = std::get<int>(it->second) + 1;
        }
    }

    template<typename T>
    inline auto get(string key) const -> T {
        auto it = _values.find(key);
        if (it == _values.end()) [[unlikely]] {
            throw std::out_of_range{key.data()};
        }
        auto val = it->second;
        return std::get<T>(val);
    }

    inline auto get_list(string key) const -> vector<Value> {
        auto it = _list_values.find(key);
        if (it == _list_values.end()) [[unlikely]] {
            throw std::out_of_range{key.data()};
        }
        return it->second;
    }

}; // class ValueStore

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

class Option {
 private:
  const OptionParser& _parser;
  vector<string> _names;
  Action _action = Action::Null;
  Type _type = Type::String;
  string _dest = "";
  string _default = "";
  size_t _nargs = 0;
  std::set<string> _choices{};
  string _help = "";
  string _env = "";

 public:
  Option(const OptionParser& p): _parser(p) {}
  ~Option() = default;

  Option(const Option&) = default;
  Option(Option&&) noexcept = default;

  Option(const OptionParser& p, vector<string> names):
    _parser(p), _names(std::move(names)) {}

  auto validate() -> bool;

  inline auto names() const -> const vector<string>& {
      return _names;
  }

  inline auto action(Action a) -> Option& {
      _action = a;
      return *this;
  }

  inline auto action() const -> Action {
      return _action;
  }

  inline auto type(Type t) -> Option& {
      _type = t;
      return *this;
  }

  inline auto type() const -> Type {
      return _type;
  }

  inline auto dest(string d) -> Option& {
      _dest = std::move(d);
      return *this;
  }

  inline auto dest() const -> string {
      return _dest;
  }

  inline auto default_value(string d) -> Option& {
      _default = std::move(d);
      return *this;
  }

  template<typename T>
  inline auto default_value(T t) -> Option& {
      _default = std::to_string(t);
      return *this;
  };

  inline auto default_value() const -> string {
      return _default;
  }

  inline auto nargs(size_t n) -> Option& {
      _nargs = n;
      return *this;
  }

  inline auto nargs() -> size_t {
      return _nargs;
  }

  template<typename InputIterator>
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

  inline auto choices() const -> const std::set<string>& {
      return _choices;
  }

  inline auto help(string h) -> Option& {
      _help = std::move(h);
      return *this;
  }

  inline auto help() -> string {
      return _help;
  }

  inline auto env(string e) -> Option& {
      _env = std::move(e);
      return *this;
  }

  inline auto env() const -> string {
      return _env;
  }

}; // class Option

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

class ArgList {
 private:
  vector<string> _args;
  vector<string>::iterator _it;
 public:
  ArgList(vector<string> args): _args(std::move(args)), _it(_args.begin()) {}
  ~ArgList() = default;
  ArgList(const ArgList&) = delete;
  ArgList(ArgList&&) noexcept = delete;
  ArgList& operator=(const ArgList&) = delete;
  ArgList& operator=(ArgList&&) noexcept = delete;

  inline auto empty() const -> bool {
    return _it == _args.end();
  }
  inline auto pop() -> string {
    return *_it++;
  }
  inline auto peek() const -> string {
    return *_it;
  }

}; // class ArgList

class OptionParser {
  private:
    const char _prefix = '-';
    string  _long_prefix = "--";
    string _program;
    string _usage;
    string _version;
    vector<Option> _options;
    std::map<string, size_t> _long_option_map;
    std::map<string, size_t> _short_option_map;
    vector<string> _invalid_args;
    ConflictHandler _conflict_handler = ConflictHandler::Error;

    auto extract_arg_type(string arg) const -> OptionType;

    auto add_option(Option option) -> Option&;

 public:
    auto extract_option_type(string opt) const -> OptionType;
    explicit OptionParser(char prefix);
    OptionParser(char prefix, ConflictHandler handler);
    explicit OptionParser(ConflictHandler handler);
    OptionParser() = default;

    ~OptionParser() = default;

    inline auto program(string p) -> OptionParser& {
        _program = std::move(p);
        return *this;
    }

    inline auto program() const -> string {
        return _program;
    }

    inline auto usage(string u) -> OptionParser& {
        _usage = std::move(u);
        return *this;
    }

    inline auto usage() const -> string {
        return _usage;
    }

    inline auto version(string v) -> OptionParser& {
      _version = std::move(v);
        return *this;
    }

    inline auto version() const -> string {
        return _version;
    }

    auto add_option(std::initializer_list<string> names) -> Option&;

    auto add_option(string short_name, string long_name) -> Option&;

    auto add_option(string name) -> Option&;

    auto add_usage_option(string usage_msg) -> void;
    auto add_help_option(string help_msg) -> void;

    auto handle_short_opt(ValueStore&, ArgList& args) -> bool;
    auto handle_long_opt(ValueStore&, ArgList& args) -> bool;

    auto process_opt(const Option&, ValueStore&, string) -> bool;

    auto parse_args(vector<string> args) -> ValueStore;

    auto parse_args(int argc, char** argv) -> ValueStore;

    auto format_usage() -> string;
    auto format_help() -> string;
    auto print_help() -> void;
};

} // namespace stdb::optparse



