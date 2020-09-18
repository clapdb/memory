/*
 * Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
 */
/* +------------------------------------------------------------------+
   |                                                                  |
   |                                                                  |
   |  #####                                                           |
   | #     # #####   ##   #####  #####  # #    # ######  ####   ####  |
   | #         #    #  #  #    # #    # # ##   # #      #      #      |
   |  #####    #   #    # #    # #    # # # #  # #####   ####   ####  |
   |       #   #   ###### #####  #####  # #  # # #           #      # |
   | #     #   #   #    # #   #  #   #  # #   ## #      #    # #    # |
   |  #####    #   #    # #    # #    # # #    # ######  ####   ####  |
   |                                                                  |
   |                                                                  |
   +------------------------------------------------------------------+
*/

#ifndef MEMORY_ARENAHELPER_HPP_
#define MEMORY_ARENAHELPER_HPP_

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>


#define ACstrTag typedef void ArenaConstructable_

#define ADstrSkipTag typedef void DestructionSkippable_


// the macro to declare the inline forcing compiler hint.
#ifdef STAR_ENABLE_FORCE_INLINE
#ifdef __has_attribute
#if __has_attribute(always_inline)
#define ALWAYS_INLINE __attribute__((always_inline))
#endif  // __has_attribute(always_inline)
#if __has_attribute(noinline)
#define ALWAYS_NOINLINE __attribute__((noinline))
#endif  // __has_attribute(noinline)
#endif  // __has_attribute
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE
#endif  // ALWAYS_INLINE

#ifndef ALWAYS_NOINLINE
#define ALWAYS_NOINLINE
#endif  // ALWAYS_NOINLINE

#ifdef _MSC_VER
#error DO NOT SUPPORT Microsoft Visual C++
#endif


#ifdef __UNITTEST
#define FRIEND_TEST(test_case_name, test_name) \
  friend class test_case_name##_##test_name##_Test
#else
#define FRIEND_TEST(test_case_name, test_name)
#endif



namespace starriness {
namespace memory {
namespace align {
// Align to next 8 multiple
template <uint64_t N>
inline ALWAYS_INLINE uint64_t AlignUpTo(uint64_t n) {
  // Align n to next multiple of N
  // (from <Hacker's Delight 2rd edtion>,Chapter 3.)
  // -----------------------------------------------
  // borrow it from protobuf implementation code.
  static_assert((N & (N - 1)) == 0, "AlignUpToN, N is power of 2");
  static_assert(N > 2, "AlignUpToN, N is than 4");
  static_assert(N < 64, "AlignUpToN, N is more than 64");
  return (n + N - 1) & static_cast<uint64_t>(-N);
}

inline ALWAYS_INLINE uint64_t AlignUp(uint64_t n, uint64_t block_size) {
  uint64_t m = n % block_size;
  return n - m + (static_cast<int>(static_cast<bool>(m))) * block_size;
}

}  // namespace align

// class ArenaHelper is for helping the Type stored in the Arena memory.
//
// it use type_traits techneque to indicates supporting for arena for a
// type T at compiler time.
//
//
class Arena;
template <typename T>
class ArenaHelper {
 public:
  template <typename U>
  static char DestructionSkippable(const typename U::DestructionSkippable_*);
  template <typename U>
  static double DestructionSkippable(...);

  typedef std::integral_constant<
      bool, sizeof(DestructionSkippable<T>(static_cast<const T*>(0))) ==
                    sizeof(char) ||
                std::is_trivially_destructible<T>::value>
      is_destructor_skippable;

  template <typename U>
  static char ArenaConstructable(const typename U::ArenaConstructable_*);
  template <typename U>
  static double ArenaConstructable(...);

  typedef std::integral_constant<bool,
                                 sizeof(ArenaConstructable<T>(
                                     static_cast<const T*>(0))) == sizeof(char)>
      is_arena_constructable;

  template <typename... Args>
  static T* Construct(void* ptr, Args&&... args) {
    // placement new make the new Object T is in the ptr-> memory.
    return new (ptr) T(std::forward<Args>(args)...);
  }

  static Arena* GetArena(const T* p) { return p->GetArena(); }

  friend class Arena;
};

// is_arena_constructable<T>::value is true if the message type T has arena
// support enabled, and false otherwise.
//
// is_destructor_skippable<T>::value is true if the message type T has told
// the arena that it is safe to skip the destructor, and false otherwise.
//
// This is inside Arena because only Arena has the friend relationships
// necessary to see the underlying generated code traits.
//
template <typename T>
struct is_arena_constructable : ArenaHelper<T>::is_arena_constructable {};
template <typename T>
struct is_destructor_skippable : ArenaHelper<T>::is_destructor_skippable {};

}  // namespace memory
}  // namespace starriness

#endif  // MEMORY_ARENAHELPER_HPP_
