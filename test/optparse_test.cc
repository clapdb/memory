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
#include "doctest/doctest.h"

namespace stdb::optparse {
TEST_CASE("optparser::smoke") {
    OptionParser parser = OptionParser();
    parser.add_option({"-f", "--file"}).dest("filename").action(Action::Store).nargs(2).help("write report to FILE");
    parser.add_option("-q", "--quiet")
      .action(Action::StoreFalse)
      .type(Type::Bool)
      .nargs(1)
      .dest("quiet")
      .default_value("true")
      .help("don't print status messages to stdout");
    parser.add_option("-v", "--verbose")
      .action(Action::StoreTrue)
      .dest("verbose")
      .type(Type::Bool)
      .nargs(1)
      .default_value("false")
      .help("print status messages to stdout");
    parser.add_option("-c", "--config").dest("config").action(Action::Store).nargs(2).help("config file");
    parser.add_option("-s", "--size").type(Type::Int).action(Action::Store).dest("size").nargs(2).help("size of the data");

    auto options = parser.parse_args({"-f", "test.txt", "-q", "-c", "config.txt", "-s", "100"});

    CHECK_EQ(options.get<string>("filename"), "test.txt");
    CHECK_EQ(options.get<bool>("verbose"), false);
    CHECK_EQ(options.get<string>("config"), "config.txt");
    CHECK_EQ(options.get<int>("size"), 100);

}

}