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

#pragma once
#include <cstdint>  // for uint64_t

namespace stdb::memory::literals {
constexpr auto is_digit(char chr) noexcept -> bool { return chr <= '9' && chr >= '0'; }

constexpr auto stoi_impl(const char* str, uint64_t value = 0) -> uint64_t {
    // NOLINTNEXTLINE
    return (*str != '\0') ? is_digit(*str)
                              ? stoi_impl(str + 1, static_cast<uint64_t>(*str - '0') + value * 10)
                              : throw "compile-time-error: not a digit"  // NOLINT: can not be throw in real world.
                          : value;
}

constexpr auto stoi(const char* str) -> uint64_t { return stoi_impl(str); }
constexpr uint64_t kilo = (1ULL << 10ULL);
// NOLINTNEXTLINE(bugprone-exception-escape)
inline constexpr auto operator""_KB(const char* uint_str) noexcept -> uint64_t { return stoi(uint_str) * kilo; }
// NOLINTNEXTLINE(bugprone-exception-escape)
inline constexpr auto operator""_MB(const char* uint_str) noexcept -> uint64_t { return stoi(uint_str) * kilo * kilo; }
// NOLINTNEXTLINE(bugprone-exception-escape)
inline constexpr auto operator""_GB(const char* uint_str) noexcept -> uint64_t {
    return stoi(uint_str) * kilo * kilo * kilo;
}
}  // namespace stdb::memory::literals
