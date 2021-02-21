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

#ifndef MEMORY_ARENA_HPP_
#define MEMORY_ARENA_HPP_

#include <cassert>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>
#include <glog/logging.h>

#include "arenahelper.hpp"

namespace stdb {
namespace memory {

using align::AlignUpTo;

class Arena {
 public:
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;
  // Arena Options class for the Arena class 's configiration
  struct Options {
    // following parameters shoule be determined by the OS/CPU Architecture
    // this shoule make cacheline happy and memory locality better.
    // a Block should is a memroy page.

    // normal_block_size shoule match normal page of the OS.
    uint64_t normal_block_size;

    // huge_block_size should match big memory page of the OS.
    uint64_t huge_block_size;

    // suggested block-size
    uint64_t suggested_initblock_size;

    // A Function pointer to an alloc method for the new block in the Arena.
    void* (*block_alloc)(uint64_t);

    // A Function pointer to a dealloc method for the blocks in the Arena.
    void (*block_dealloc)(void*);

    uint64_t default_cleanup_list_size;
    // Arena hooked functions
    // Hooks for adding external functionality.
    // Init hook may return a pointer to a cookie to be stored in the arena.
    // reset and destruction hooks will then be called with the same cookie = delete
    // pointer. This allows us to save an external object per arena instance and
    // use it on the other hooks (Note: It is just as legal for init to return
    // NULL and not use the cookie feature).
    // on_arena_reset and on_arena_destruction also receive the space used in
    // the arena just before the reset.
    void* (*on_arena_init)(Arena* arena);
    void (*on_arena_allocation)(const std::type_info* alloc_type,
                                uint64_t alloc_size, void* cookie);
    void* (*on_arena_destruction)(Arena* arena, void* cookie,
                                  uint64_t space_used);

    [[gnu::always_inline]]
    inline Options()
        : normal_block_size(4096),  // 4k is the normal pagesize of modern os
          huge_block_size(2 * 1024 *
                          1024),  // TODO(hurricane1026): maybe support 1G
          suggested_initblock_size(4096),  // 4k
          block_alloc(nullptr),
          block_dealloc(nullptr),
          default_cleanup_list_size(16),
          on_arena_init(nullptr),
          on_arena_allocation(nullptr),
          on_arena_destruction(nullptr) {
      init();
    }

    [[gnu::always_inline]]
    inline explicit Options(const Options& options)
        : normal_block_size(options.normal_block_size),
          huge_block_size(options.huge_block_size),
          suggested_initblock_size(options.suggested_initblock_size),
          block_alloc(options.block_alloc),
          block_dealloc(options.block_dealloc),
          default_cleanup_list_size(options.default_cleanup_list_size),
          on_arena_init(options.on_arena_init),
          on_arena_allocation(options.on_arena_allocation),
          on_arena_destruction(options.on_arena_destruction) {
      init();
    }

    [[gnu::always_inline]]
    inline void init() {
      assert(normal_block_size > 0);
      if (suggested_initblock_size == 0)
        suggested_initblock_size = normal_block_size;
      if (huge_block_size == 0)
        huge_block_size = normal_block_size;
    }
  };  // struct Options

  class Block {
   public:

    Block(uint64_t size, Block* prev);


    [[gnu::always_inline]]
    inline char* Pointer() {
      return reinterpret_cast<char*>(this) + pos_;
    }

    [[gnu::always_inline]]
    inline char* alloc(uint64_t size) {
      assert(size <= (size_ - pos_));
      //char* p = Pointer();
      return Pointer() + size;
    }

    [[gnu::always_inline]]
    inline Block* prev() const { return prev_; }

    [[gnu::always_inline]]
    inline uint64_t size() const { return size_; }

    [[gnu::always_inline]]
    inline uint64_t remain() {
      assert(size_ >= pos_);
      return size_ - pos_;
    }

   private:
    Block* prev_;
    uint64_t pos_;
    uint64_t size_;

  };

  // Arena constructor
  explicit Arena(const Options& op)
      : options_(op),
        last_block_(nullptr),
        cookie_(nullptr),
        space_allocated_(0) {
    cleanups_ = new std::vector<std::function<void()>>();
    if (options_.default_cleanup_list_size > 0) {
      cleanups_->reserve(options_.default_cleanup_list_size);
    }
  }

  // Arena desctructor
  ~Arena() {
    // execute all cleanup functions.
    DoCleanup();
    // free blocks
    FreeBlocks();
    delete cleanups_;
    // make sure the on_arena_destruction was set.
    if (options_.on_arena_destruction != nullptr) {
      options_.on_arena_destruction(this, cookie_, space_allocated_);
    }
  }

  template <typename T>
  [[gnu::noinline]]
  void Own(T* obj) {
    static_assert(!is_arena_constructable<T>::value,
                  "Own requires a type can not create in arean");
    std::function<void()> cleaner = [obj] { delete obj; };
    AddCleanup(cleaner);
    return;
  }

  uint64_t Reset();

  [[gnu::always_inline]]
  inline uint64_t SpaceAllocated() const {
    return space_allocated_;
  }

  // new from arena, and register cleanup function if need
  // always allocating in the arena memory
  // the type T should have the tag:
  template <typename T, typename... Args>
  T* Create(Args&&... args) {
    static_assert(is_arena_constructable<T>::value ||
        (std::is_standard_layout<T>::value && std::is_trivial<T>::value),
                  "New requires a constructible type");
    char* ptr = allocateAligned(sizeof(T));
    if (ptr != nullptr) {
      ArenaHelper<T>::Construct(ptr, std::forward<Args>(args)...);
      T* result = reinterpret_cast<T*>(ptr);
      RegisterDestructor<T>(result);
      if (options_.on_arena_allocation != nullptr)
        options_.on_arena_allocation(&typeid(T), sizeof(T), cookie_);
      return result;
    }
    return nullptr;
  }

  // new array from arena, and register cleanup function if need
  template <typename T>
  T* CreateArray(uint64_t num) {
    static_assert(std::is_standard_layout<T>::value && std::is_trivial<T>::value,
                  "NewArray requires a trivially constructible type");
    static_assert(std::is_trivially_destructible<T>::value,
                  "NewArray requires a trivially destructible type");
    CHECK_LE(num, std::numeric_limits<uint64_t>::max() / sizeof(T));
    const uint64_t n = sizeof(T) * num;
    char* p = allocateAligned(n);
    if (p != nullptr) {
      T* curr = reinterpret_cast<T*>(p);
      for (uint64_t i = 0; i < num; ++i) {
        ArenaHelper<T>::Construct(curr++);
      }
      if (options_.on_arena_allocation != nullptr)
        options_.on_arena_allocation(&typeid(T), n, cookie_);
      return reinterpret_cast<T*>(p);
    }
    return nullptr;
  }

  // if return nullptr means failure
  [[gnu::always_inline]]
  inline char* AllocateAligned(uint64_t bytes) {
    char* ptr = allocateAligned(bytes);
    if (ptr != nullptr) {
      if (options_.on_arena_allocation != nullptr)
        options_.on_arena_allocation(nullptr, bytes, cookie_);
    } else {
      return nullptr;
    }
    return ptr;
  }

  // if return nullptr means failure
  [[gnu::always_inline]]
  inline char* AllocateAlignedAndAddCleanup(
      uint64_t bytes, const std::function<void()> c) {
    char* ptr = allocateAligned(bytes);
    if (ptr != nullptr) {
      AddCleanup(c);
      if (options_.on_arena_allocation)
        options_.on_arena_allocation(nullptr, bytes, cookie_);
      return ptr;
    }
    return nullptr;
  }

  [[gnu::always_inline]]
  inline void Init() {
    if (options_.on_arena_init != nullptr)
      cookie_ = options_.on_arena_init(this);
  }

 private:
  // New Block while current Block has not enough memory.
  Block* NewBlock(uint64_t min_bytes, Block* prev_block);

  char* allocateAligned(uint64_t);

  // AddCleanup will never check the exist memory
  [[gnu::always_inline]]
  inline void AddCleanup(const std::function<void()>& c) {
    cleanups_->push_back(c);
    return;
  }

  [[gnu::always_inline]]
  static inline uint64_t align_size(uint64_t n) {
    return align::AlignUpTo<8>(n);
  }

  template <typename T>
  [[gnu::always_inline]]
  inline void RegisterDestructor(T* ptr) {
    RegisterDestructorInternal(
        ptr, typename ArenaHelper<T>::is_destructor_skippable::type());
  }

  template <typename T>
  [[gnu::always_inline]]
  inline void RegisterDestructorInternal(T* /*ptr*/,
                                                       std::true_type) {}

  template <typename T>
  [[gnu::always_inline]]
  inline void RegisterDestructorInternal(T* ptr,
                                                       std::false_type) {
    std::function<void()> cleaner = [ptr] { ptr->~T(); };
    AddCleanup(cleaner);
  }

  [[gnu::always_inline]]
  inline void DoCleanup() {
    for (auto f : *cleanups_) {
      f();
    }
  }

  [[gnu::always_inline]]
  inline void FreeBlocks() {
    Block* curr = last_block_;
    Block* prev;
    while (curr != nullptr) {
      prev = curr->prev();
      options_.block_dealloc(curr);
      curr = prev;
    }
  }

  Options options_;
  Block* last_block_;
  void* cookie_;

  std::vector<std::function<void()>>* cleanups_;

  uint64_t space_allocated_;

  const uint64_t kThresholdHuge = 4;

  // FRIEND_TEST will not generate code in release binary.
  FRIEND_TEST(ArenaTest, CtorTest);
  FRIEND_TEST(ArenaTest, NewBlockTest);
  FRIEND_TEST(ArenaTest, AddCleanupTest);
  FRIEND_TEST(ArenaTest, FreeBlocksTest);
  FRIEND_TEST(ArenaTest, DoCleanupTest);
  FRIEND_TEST(ArenaTest, OwnTest);
  FRIEND_TEST(ArenaTest, SpaceTest);
  FRIEND_TEST(ArenaTest, RegisterDestructorTest);
  FRIEND_TEST(ArenaTest, CreateArrayTest);
  FRIEND_TEST(ArenaTest, CreateTest);
  FRIEND_TEST(ArenaTest, DstrTest);
  FRIEND_TEST(ArenaTest, NullTest);
  FRIEND_TEST(ArenaTest, HookTest);
};  // class Arena

static const uint64_t kBlockHeaderSize =
    align::AlignUpTo<8>(sizeof(memory::Arena::Block));

static const uint64_t kCleanupFuncSize =
    align::AlignUpTo<8>(sizeof(std::function<void()>));

}  // namespace memory
}  // namespace stdb

#endif  // MEMORY_ARENA_HPP_
