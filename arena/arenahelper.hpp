/*
 * Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
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
#include <cstdlib>
#include <functional>
#include <type_traits>
#include <utility>

#define ArenaFullManagedTag using ArenaManaged_ = void;                    // NOLINT
#define ArenaManagedCreateOnlyTag using ArenaManagedSkipDestruct_ = void;  // NOLINT

namespace stdb::memory {

namespace align {

inline constexpr uint64_t kMaxAlignSize = 64;
// Align to next 8 multiple
template <uint64_t N>
[[gnu::always_inline]] constexpr auto AlignUpTo(uint64_t n) noexcept -> uint64_t {
    // Align n to next multiple of N
    // (from <Hacker's Delight 2rd edtion>,Chapter 3.)
    // -----------------------------------------------
    // borrow it from protobuf implementation code.
    static_assert((N & (N - 1)) == 0, "AlignUpToN, N is power of 2");
    static_assert(N > 2, "AlignUpToN, N is than 4");
    static_assert(N < kMaxAlignSize, "AlignUpToN, N is more than 64");
    return (n + N - 1) & static_cast<uint64_t>(-N);
}

[[gnu::always_inline]] inline auto AlignUp(uint64_t n, uint64_t block_size) noexcept -> uint64_t {
    uint64_t reminder = n % block_size;
    return n - reminder + (static_cast<uint64_t>(static_cast<bool>(reminder))) * block_size;
}



}  // namespace align

/*
 * class ArenaHelper is for helping the Type stored in the Arena memory.
 *
 * it uses type_traits and constexpr techniques to indicates supporting for arena for a type T at compiler time.
 */
class Arena;

template <typename T>
class ArenaHelper
{
   public:
    template <typename U>
    static auto DestructionSkippable(const typename U::ArenaManagedSkipDestruct_*) -> char;

    template <typename U>
    static auto DestructionSkippable(...) -> double;

    using is_destructor_skippable =
      std::integral_constant<bool, sizeof(DestructionSkippable<T>(static_cast<const T*>(0))) == sizeof(char) ||
                                     std::is_trivially_destructible<T>::value>;

    template <typename U>
    static auto ArenaConstructable(const typename U::ArenaManaged_*) -> char;
    template <typename U>
    static auto ArenaConstructable(...) -> double;

    using is_arena_constructable =
      std::integral_constant<bool, sizeof(ArenaConstructable<T>(static_cast<const T*>(0))) == sizeof(char)>;

    friend class Arena;
};

/*
 * is_arena_full_managable<T>::value is true if the message type T has arena
 * support enabled, and false otherwise.
 *
 * is_destructor_skippable<T>::value is true if the message type T has told
 * the arena that it is safe to skip the destructor, and false otherwise.
 *
 * This is inside Arena because only Arena has the friend relationships
 * necessary to see the underlying generated code traits.
 */
template <typename T>
struct is_arena_full_managable : ArenaHelper<T>::is_arena_constructable
{};
template <typename T>
struct is_destructor_skippable : ArenaHelper<T>::is_destructor_skippable
{};

template <typename T>
concept Constructable = is_arena_full_managable<T>::value || is_destructor_skippable<T>::value;

template <typename T>
concept NonConstructable = !is_arena_full_managable<T>::value;

template <typename T>
concept DestructorSkippable = is_destructor_skippable<T>::value;

}  // namespace stdb::memory
