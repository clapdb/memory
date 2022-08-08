/*
 * Copyright (C) 2022 hurricane <l@stdb.io>. All rights reserved.
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
