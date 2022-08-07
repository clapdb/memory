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
    return n - reminder + (static_cast<int>(static_cast<bool>(reminder))) * block_size;
}

}  // namespace stdb::memory::align
