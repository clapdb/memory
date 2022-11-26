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

// NOLINTBEGIN
#include "align/align.hpp"
#include "assert/assert_config.hpp"
#define ArenaFullManagedTag using ArenaManaged_ = void;                    // NOLINT
#define ArenaManagedCreateOnlyTag using ArenaManagedSkipDestruct_ = void;  // NOLINT
#if defined(__linux__)
#include <memory_resource>  // for memory_resource
namespace pmr = ::std::pmr;
#elif defined(__APPLE__)
#include <experimental/memory_resource>  // for memory_resource
#include <experimental/string>           // for memory_resource
#include <experimental/vector>           // for memory_resource
namespace pmr = ::std::experimental::pmr;
#else
#error "no support for other compiler"
#endif

// NOLINTEND

namespace stdb::memory {
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
concept NonConstructable = !
is_arena_full_managable<T>::value;

template <typename T>
concept DestructorSkippable = is_destructor_skippable<T>::value;

}  // namespace stdb::memory
