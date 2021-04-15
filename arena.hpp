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

#include <glog/logging.h>

#include <cassert>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "arenahelper.hpp"

namespace stdb {
namespace memory {

using align::AlignUpTo;

struct CleanupNode
{
  void* element;
  void (*cleanup)(void*);
  CleanupNode() = delete;
  CleanupNode(void* elem, void (*clean)(void*)) : element(elem), cleanup(clean) {}
};

static constexpr uint64_t kCleanupNodeSize = align::AlignUpTo<8>(sizeof(memory::CleanupNode));

template <typename T>
void arena_destruct_object(void* obj) {
  reinterpret_cast<T*>(obj)->~T();
}

class Arena
{
 public:
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;
  // Arena Options class for the Arena class 's configiration
  struct Options
  {
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
    void (*on_arena_reset)(Arena* arena, void* cookie, uint64_t space_used);
    void (*on_arena_allocation)(const std::type_info* alloc_type, uint64_t alloc_size, void* cookie);
    void* (*on_arena_destruction)(Arena* arena, void* cookie, uint64_t space_used);

    [[gnu::always_inline]] inline Options()
        : normal_block_size(4096),           // 4k is the normal pagesize of modern os
          huge_block_size(2 * 1024 * 1024),  // TODO(hurricane1026): maybe support 1G
          suggested_initblock_size(4096),    // 4k
          block_alloc(nullptr),
          block_dealloc(nullptr),
          on_arena_init(nullptr),
          on_arena_reset(nullptr),
          on_arena_allocation(nullptr),
          on_arena_destruction(nullptr) {
      init();
    }

    [[gnu::always_inline]] inline explicit Options(const Options& options)
        : normal_block_size(options.normal_block_size),
          huge_block_size(options.huge_block_size),
          suggested_initblock_size(options.suggested_initblock_size),
          block_alloc(options.block_alloc),
          block_dealloc(options.block_dealloc),
          on_arena_init(options.on_arena_init),
          on_arena_reset(options.on_arena_reset),
          on_arena_allocation(options.on_arena_allocation),
          on_arena_destruction(options.on_arena_destruction) {
      init();
    }

    [[gnu::always_inline]] inline void init() noexcept {
      assert(normal_block_size > 0);
      if (suggested_initblock_size == 0) suggested_initblock_size = normal_block_size;
      if (huge_block_size == 0) huge_block_size = normal_block_size;
    }
  };  // struct Options

  class Block
  {
   public:
    Block(uint64_t size, Block* prev);

    void Reset() noexcept;

    [[nodiscard, gnu::always_inline]] inline char* Pos() noexcept { return reinterpret_cast<char*>(this) + pos_; }

    [[nodiscard, gnu::always_inline]] inline char* CleanupPos() noexcept {
      return reinterpret_cast<char*>(this) + limit_;
    }

    [[nodiscard, gnu::always_inline]] inline char* alloc(uint64_t size) noexcept {
      assert(size <= (limit_ - pos_));
      char* p = Pos();
      pos_ += size;
      return p;
    }

    [[nodiscard, gnu::always_inline]] inline char* alloc_cleanup() noexcept {
      assert(pos_ + kCleanupNodeSize <= limit_);
      limit_ -= kCleanupNodeSize;
      return CleanupPos();
    }

    [[gnu::always_inline]] inline void register_cleanup(void* obj, void (*cleanup)(void*)) noexcept {
      auto ptr = alloc_cleanup();
      new (ptr) CleanupNode(obj, cleanup);
      return;
    }

    [[nodiscard, gnu::always_inline]] inline Block* prev() const noexcept { return prev_; }

    [[gnu::always_inline]] inline uint64_t size() const noexcept { return size_; }

    [[gnu::always_inline]] inline uint64_t remain() noexcept {
      assert(limit_ >= pos_);
      return limit_ - pos_;
    }

    inline void run_cleanups() noexcept {
      CleanupNode* node = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + limit_);
      CleanupNode* last = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + size_);
      for (; node < last; ++node) {
        node->cleanup(node->element);
      }
    }

   private:
    Block* prev_;
    uint64_t pos_;
    uint64_t size_;   // the size of the block
    uint64_t limit_;  // the limit can be use for Create
  };

  // Arena constructor
  explicit Arena(const Options& op) : options_(op), last_block_(nullptr), cookie_(nullptr), space_allocated_(0ULL) {
    // Init();
  }

  // Arena desctructor
  ~Arena() {
    // free blocks
    free_all_blocks();
    // make sure the on_arena_destruction was set.
    if (options_.on_arena_destruction != nullptr) [[likely]] {
      options_.on_arena_destruction(this, cookie_, space_allocated_);
    }
  }

  template <typename T>
  [[nodiscard, gnu::noinline]] bool Own(T* obj) noexcept {
    static_assert(!is_arena_constructable<T>::value, "Own requires a type can not create in arean");
    // std::function<void()> cleaner = [obj] { delete obj; };
    return addCleanup(obj, &arena_destruct_object<T>);
  }

  inline uint64_t Reset() noexcept {
    // free all blocks except the first block
    free_blocks_except_head();
    if (options_.on_arena_reset != nullptr) [[likely]] {
      options_.on_arena_reset(this, cookie_, space_allocated_);
    }
    // reset all internal status.
    uint64_t reset_size = space_allocated_;
    space_allocated_ = last_block_->size();
    last_block_->Reset();
    return reset_size;
  }

  [[gnu::always_inline]] inline uint64_t SpaceAllocated() const noexcept { return space_allocated_; }

  // new from arena, and register cleanup function if need
  // always allocating in the arena memory
  // the type T should have the tag:
  template <typename T, typename... Args>
  [[nodiscard]] T* Create(Args&&... args) noexcept {
    static_assert(is_arena_constructable<T>::value || (std::is_standard_layout<T>::value && std::is_trivial<T>::value),
                  "New requires a constructible type");
    char* ptr = allocateAligned(sizeof(T));
    if (ptr != nullptr) [[likely]] {
      ArenaHelper<T>::Construct(ptr, std::forward<Args>(args)...);
      T* result = reinterpret_cast<T*>(ptr);
      if (!RegisterDestructor<T>(result)) [[unlikely]] {
        return nullptr;
      }
      if (options_.on_arena_allocation != nullptr) [[likely]]
        options_.on_arena_allocation(&typeid(T), sizeof(T), cookie_);
      return result;
    } else {
      return nullptr;
    }
  }

  // new array from arena, and register cleanup function if need
  template <typename T>
  [[nodiscard]] T* CreateArray(uint64_t num) noexcept {
    static_assert(std::is_standard_layout<T>::value && std::is_trivial<T>::value,
                  "NewArray requires a trivially constructible type");
    static_assert(std::is_trivially_destructible<T>::value, "NewArray requires a trivially destructible type");
    CHECK_LE(num, std::numeric_limits<uint64_t>::max() / sizeof(T));
    const uint64_t n = sizeof(T) * num;
    char* p = allocateAligned(n);
    if (p != nullptr) [[likely]] {
      T* curr = reinterpret_cast<T*>(p);
      for (uint64_t i = 0; i < num; ++i) {
        ArenaHelper<T>::Construct(curr++);
      }
      if (options_.on_arena_allocation != nullptr) [[likely]] {
        options_.on_arena_allocation(&typeid(T), n, cookie_);
      }
      return reinterpret_cast<T*>(p);
    }
    return nullptr;
  }

  // if return nullptr means failure
  [[nodiscard, gnu::always_inline]] inline char* AllocateAligned(uint64_t bytes) noexcept {
    if (char* ptr = allocateAligned(bytes); ptr != nullptr) [[likely]] {
      if (options_.on_arena_allocation != nullptr) [[likely]] {
        options_.on_arena_allocation(nullptr, bytes, cookie_);
      }
      return ptr;
    }
    return nullptr;
  }

  // if return nullptr means failure
  [[nodiscard, gnu::always_inline]] inline char* AllocateAlignedAndAddCleanup(uint64_t bytes, void* element,
                                                                              void (*cleanup)(void*)) noexcept {
    if (char* ptr = allocateAligned(bytes); ptr != nullptr) [[likely]] {
      if (addCleanup(element, cleanup)) [[likely]] {
        if (options_.on_arena_allocation != nullptr) [[likely]] {
          options_.on_arena_allocation(nullptr, bytes, cookie_);
        }
        return ptr;
      }
    }
    return nullptr;
  }

  [[gnu::always_inline]] inline void Init() {
    if (options_.on_arena_init != nullptr) [[likely]] {
      cookie_ = options_.on_arena_init(this);
    }
  }

 private:
  // New Block while current Block has not enough memory.
  [[nodiscard]] Block* newBlock(uint64_t min_bytes, Block* prev_block) noexcept;

  [[nodiscard]] char* allocateAligned(uint64_t) noexcept;

  [[nodiscard, gnu::always_inline]] inline bool need_create_new_block(uint64_t need_bytes) noexcept {
    return (last_block_ == nullptr) || (need_bytes > last_block_->remain());
  }

  [[nodiscard, gnu::always_inline]] inline bool addCleanup(void* o, void (*cleanup)(void*)) noexcept {
    if (need_create_new_block(kCleanupNodeSize)) [[unlikely]] {
      Block* curr = newBlock(kCleanupNodeSize, last_block_);
      if (curr != nullptr) [[likely]]
        last_block_ = curr;
      else
        return false;
    }
    last_block_->register_cleanup(o, cleanup);
    return true;
  }

  [[gnu::always_inline]] static inline uint64_t align_size(uint64_t n) noexcept { return align::AlignUpTo<8>(n); }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline bool RegisterDestructor(T* ptr) noexcept {
    return RegisterDestructorInternal(ptr, typename ArenaHelper<T>::is_destructor_skippable::type());
  }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline bool RegisterDestructorInternal(T*, std::true_type) noexcept {
    return true;
  }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline bool RegisterDestructorInternal(T* ptr, std::false_type) noexcept {
    return addCleanup(ptr, &arena_destruct_object<T>);
  }

  [[gnu::always_inline]] inline void free_all_blocks() noexcept {
    Block* curr = last_block_;
    Block* prev;

    while (curr != nullptr) {
      prev = curr->prev();
      // run all cleanups first
      curr->run_cleanups();
      options_.block_dealloc(curr);
      curr = prev;
    }
    return;
  }

  [[gnu::always_inline]] inline void free_blocks_except_head() noexcept {
    Block* curr = last_block_;
    Block* prev;

    while (curr != nullptr && curr->prev() != nullptr) {
      prev = curr->prev();
      // run all cleanups first
      curr->run_cleanups();
      options_.block_dealloc(curr);
      curr = prev;
    }
    // reset the last_block_ to the first block
    last_block_ = curr;
    return;
  }

 private:
  Options options_;
  Block* last_block_;

  // should be initialized by on_arena_init
  // and should be destroy by on_arena_destruction
  void* cookie_;

  uint64_t space_allocated_;

  static constexpr uint64_t kThresholdHuge = 4;

  // FRIEND_TEST will not generate code in release binary.
  FRIEND_TEST(ArenaTest, CtorTest);
  FRIEND_TEST(ArenaTest, NewBlockTest);
  FRIEND_TEST(ArenaTest, addCleanupTest);
  FRIEND_TEST(ArenaTest, addCleanup_Fail_Test);
  FRIEND_TEST(ArenaTest, free_blocks_Test);
  FRIEND_TEST(ArenaTest, free_blocks_except_first_Test);
  FRIEND_TEST(ArenaTest, DoCleanupTest);
  FRIEND_TEST(ArenaTest, OwnTest);
  FRIEND_TEST(ArenaTest, SpaceTest);
  FRIEND_TEST(ArenaTest, RegisterDestructorTest);
  FRIEND_TEST(ArenaTest, CreateArrayTest);
  FRIEND_TEST(ArenaTest, CreateTest);
  FRIEND_TEST(ArenaTest, DstrTest);
  FRIEND_TEST(ArenaTest, NullTest);
  FRIEND_TEST(ArenaTest, HookTest);
  FRIEND_TEST(ArenaTest, ResetTest);
  FRIEND_TEST(ArenaTest, Reset_with_cleanup_Test);
};  // class Arena

static constexpr uint64_t kBlockHeaderSize = align::AlignUpTo<8>(sizeof(memory::Arena::Block));

}  // namespace memory
}  // namespace stdb

#endif  // MEMORY_ARENA_HPP_
