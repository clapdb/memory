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

#include "memory.hpp"

#include "doctest/doctest.h"

namespace stdb::memory {
using namespace ::stdb::memory::literals;  // NOLINT(google-build-using-namespace)

TEST_CASE("literals::is_digit") {
    constexpr bool five_is_digit = is_digit('5');
    constexpr bool a_is_digit = is_digit('a');
    CHECK_EQ(five_is_digit, true);
    CHECK_EQ(a_is_digit, false);
}

TEST_CASE("Literals::stoi") {
    constexpr uint64_t sixteen = stoi_impl("16", 0);
    CHECK_EQ(sixteen, 16ULL);
}

TEST_CASE("Literals:KB") {
    constexpr auto size = 4_KB;
    CHECK_EQ(size, 4096ULL);
}

TEST_CASE("Literals:MB") {
    constexpr auto size = 4_MB;
    CHECK_EQ(size, 4096ULL * 1024);
}

TEST_CASE("Literals:GB") {
    constexpr auto size = 4_GB;
    CHECK_EQ(size, 4096ULL * 1024 * 1024);
}
}  // namespace stdb::memory
