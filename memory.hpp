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
#include <cstdint>  // for uint64_t

namespace stdb::memory::literals {
constexpr auto is_digit(char chr) noexcept -> bool { return chr <= '9' && chr >= '0'; }

constexpr auto stoi_impl(const char* str, uint64_t value = 0) -> uint64_t {
    // NOLINTNEXTLINE
    return (*str != '\0') ? is_digit(*str)
                              ? stoi_impl(str + 1, static_cast<uint64_t>(*str - '0') + value * 10)  // NOLINT
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
