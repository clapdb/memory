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
    auto parser = OptionParser();
    parser.program("test");
    parser.add_option({"-f", "--file"}).dest("filename").action(Action::Store).help("write report to FILE");
    parser.add_option("-q", "--quiet")
      .action(Action::StoreFalse)
      .type(Type::Bool)
      .dest("quiet")
      .default_value("true")
      .help("don't print status messages to stdout");
    parser.add_option("-v", "--verbose")
      .action(Action::StoreTrue)
      .dest("verbose")
      .type(Type::Bool)
      .default_value("false")
      .help("print status messages to stdout");
    parser.add_option("-c", "--config").dest("config").action(Action::Store).help("config file");
    parser.add_option("-sz", "--size")
      .type(Type::Int)
      .action(Action::Store)
      .dest("size")
      .help("size of the data");

    const char* args[] = {"memory_test", "-f", "test.txt", "-q", "-c", "config.txt", "-sz=100"};
    // auto options = parser.parse_args({&args[1], &args[7]}, args);
    auto options = parser.parse_args(7, args);

    CHECK_EQ(options.get<string>("filename"), "test.txt");
    CHECK_EQ(options.get<bool>("verbose"), false);
    CHECK_EQ(options.get<string>("config"), "config.txt");
    CHECK_EQ(options.get<int>("size"), 100);

    const char* args2[] = {"memory_test", "-f=test.txt", "-q", "-c=config.txt", "--size=100"};
    // auto options2 = parser.parse_args({&args2[1], &args2[5]}, args2);
    auto options2 = parser.parse_args(5, args2);
    CHECK_EQ(options2.get<string>("filename"), "test.txt");
    CHECK_EQ(options2.get<bool>("verbose"), false);
    CHECK_EQ(options2.get<string>("config"), "config.txt");
    CHECK_EQ(options2.get<int>("size"), 100);


    const char* args3[] = {"memory_test", "-f", "test.txt", "-q", "-cconfig.txt", "-sz", "100", "-v"};
    // auto option3 = parser.parse_args({&args3[1], &args3[8]}, args3);
    auto option3 = parser.parse_args(8, args3);
    auto invalid_args = parser.invalid_args();
    CHECK_EQ(invalid_args.size(), 1);
    CHECK_EQ(strcmp(invalid_args[0], "-cconfig.txt"), 0);


    CHECK_EQ(parser.get_raw_argc(), 2);
    CHECK_EQ(strcmp(parser.get_raw_argv()[0], "memory_test"), 0);
    CHECK_EQ(strcmp(parser.get_raw_argv()[1], "-cconfig.txt"), 0);
}

TEST_CASE("optparser::+ prefix") {
  auto parser = OptionParser('+');
  parser.add_option({"+ltc", "++list_testcases"})
        .action(Action::StoreTrue)
        .type(Type::Bool)
        .dest("list_testcases")
        .default_value("false")
        .help("list all testcases");
  
  parser.add_option({"+tc", "++test_case"})
        .action(Action::Store)
        .type(Type::String)
        .dest("test_case")
        .help("run the specified testcase");

  const char* args[] = {"memory_test", "+ltc", "+tc=memory", "-c", "1"};
  auto options = parser.parse_args(5, args);

  CHECK_EQ(options.get<bool>("list_testcases"), true);
  CHECK_EQ(options.get<string>("test_case"), "memory");


  CHECK_EQ(parser.invalid_args().size(), 2);
  CHECK_EQ(strcmp(parser.invalid_args()[0], "-c"), 0);
  CHECK_EQ(strcmp(parser.invalid_args()[1], "1"), 0);

  CHECK_EQ(parser.get_raw_argc(), 3);
  CHECK_EQ(strcmp(parser.get_raw_argv()[0], "memory_test"), 0);
  CHECK_EQ(strcmp(parser.get_raw_argv()[1], "-c"), 0);
  CHECK_EQ(strcmp(parser.get_raw_argv()[2], "1"), 0);
}

TEST_CASE("optparser::comma_split") {
    auto parser = OptionParser('-');
    parser.add_option({"-f", "--file"}).dest("files").action(Action::Append).help("input files");
    parser.add_option("-q", "--quiet")
      .action(Action::StoreFalse)
      .type(Type::Bool)
      .dest("quiet")
      .default_value("true")
      .help("don't print status messages to stdout");

    const char* args[] = {"memory_test", "-f=test.txt,  test2.txt", "-q"};
    // auto options = parser.parse_args({&args[1], &args[3]}, args);
    auto options = parser.parse_args(3, args);
    auto file_list = options.get_list<string>("files");
    CHECK_EQ(file_list->size(), 2);
    CHECK_EQ(file_list->at(0), "test.txt");
    CHECK_EQ(file_list->at(1), "test2.txt");
}

TEST_CASE("optparser::choice") {
    auto parser = OptionParser();
    parser.add_option("-m", "--mode")
      .dest("mode")
      .action(Action::Store)
      .type(Type::Choice)
      .choices({"work", "wait", "silent"})
      .help("show modes");

    parser.program("test");
    const char* choice_args[] = {"memory_test", "-m", "work"};
    // auto options = parser.parse_args({&choice_args[1], &choice_args[3]}, choice_args);
    auto options = parser.parse_args(3, choice_args);
    CHECK_EQ(options.get<string>("mode"), "work");
}

TEST_CASE("optparser::complex") {
    auto parser = OptionParser();
    parser.program("test");
    parser.add_option({"-f", "--file"}).dest("filename").action(Action::Store).help("write report to FILE");
    parser.add_option("-q", "--quiet")
      .action(Action::StoreFalse)
      .type(Type::Bool)
      .dest("quiet")
      .default_value("true")
      .help("don't print status messages to stdout");
    parser.add_option("-v", "--verbose")
      .action(Action::StoreTrue)
      .dest("verbose")
      .type(Type::Bool)
      .default_value(0)
      .help("print status messages to stdout");
    parser.add_option("-c", "--config").dest("config").action(Action::Store).help("config file");
    parser.add_option("-r", "--ratio").type(Type::Int).action(Action::Append).help("ratios");
    parser.add_option("--duration")
      .type(Type::Double)
      .action(Action::Store)
      .help(
        "print duration time for the loooooooooooooooooooooong running!! lasting lasting lasting for testing testing");
    parser.add_option("-t", "--test").type(Type::Bool).action(Action::Store).help("test");

    const char* complex_args[] = {"memory_test", "-f", "test.txt", "-q", "-c", "config.txt", "--duration=2.0", "-r=1", "100"};
    auto options = parser.parse_args(9, complex_args);

    CHECK_EQ(options.get<string>("filename"), "test.txt");
    CHECK_EQ(options.get<bool>("verbose"), false);
    CHECK_EQ(options.get<string>("config"), "config.txt");
    CHECK_EQ(options.get_list("ratio")->size(), 2);
    CHECK_EQ(options.get<double>("duration"), 2.0);
    CHECK_EQ(options.get<bool>("help"), std::nullopt);
    CHECK_EQ(options.get<bool>("test"), std::nullopt);

    auto help_msg = parser.format_help();
    fmt::print("{}", help_msg);
    CHECK_EQ(help_msg.empty(), false);

    const char* usage_args[] = {"memory_test", "-u"};
    auto usage_options = parser.parse_args(2, usage_args);
    CHECK_EQ(usage_options.get<bool>("usage"), true);
    fmt::print("============\n");
    fmt::print("{}", parser.format_usage());
    fmt::print("------------\n");
    fmt::print("{}", parser.format_help());

    const char* args2[] = {"memory_test", "-f=test.txt", "--duration=2.0", "-r =1", "100"};
    auto options2 = parser.parse_args(5, args2);
    // the bool type always has a default value
    CHECK_EQ(options2.get<bool>("quiet"), false);
    CHECK_EQ(options2.get<bool>("help"), std::nullopt);
    // the string type need to check if it has a value
    CHECK_EQ(options2.get<string>("config").has_value(), false);
    auto ratios = options2.get_list("ratio");

    CHECK_EQ(ratios->size(), 2);
    auto ratio1 = std::get<int>(ratios->at(0));
    auto ratio2 = std::get<int>(ratios->at(1));

    CHECK_EQ(ratio1, 1);
    CHECK_EQ(ratio2, 100);

    auto int_ratios = options2.get_list<int>("ratio");
    CHECK_EQ(int_ratios->size(), 2);
    CHECK_EQ(int_ratios->at(0), 1);
    CHECK_EQ(int_ratios->at(1), 100);
}


}  // namespace stdb::optparse
