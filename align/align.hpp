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
#include <cstdint>

namespace stdb::memory::align {

inline constexpr uint64_t kMaxAlignSize = 64;
// Align to next 8 multiple
template <uint64_t N>
[[gnu::always_inline]] constexpr auto AlignUpTo(uint64_t n) noexcept -> uint64_t {
    // Align n to next multiple of N
    // (from <Hacker's Delight 2rd edition>,Chapter 3.)
    static_assert((N & (N - 1)) == 0, "AlignUpToN, N is power of 2");
    static_assert(N > 2, "AlignUpToN, N is than 4");
    static_assert(N < kMaxAlignSize, "AlignUpToN, N is more than 64");
    return (n + N - 1) & static_cast<uint64_t>(-N);
}

[[gnu::always_inline]] inline auto AlignUp(uint64_t n, uint64_t block_size) noexcept -> uint64_t {
    uint64_t reminder = n % block_size;
    return n - reminder + (static_cast<uint64_t>(static_cast<bool>(reminder))) * block_size;
}

}  // namespace stdb::memory::align
