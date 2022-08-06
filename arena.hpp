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

#ifndef MEMORY_ARENA_HPP_
#define MEMORY_ARENA_HPP_

#include <glog/logging.h>

#include <cstdlib>
#include <limits>
#include <memory_resource>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <cassert>

#include "arenahelper.hpp"

#define STDB_ASSERT(exp) assert(exp)

#if defined(__GNUC__) && (__GNUC__ >= 11)
#include <source_location>
#elif defined(__clang__)
#include <experimental/source_location>
#endif

namespace stdb::memory {

#if defined(__GNUC__) && (__GNUC__ >= 11)
using source_location = std::source_location;
#elif defined(__clang__)
using source_location = std::experimental::source_location;
#else
#error "no support for other compiler"
#endif

using align::AlignUpTo;
using ::std::size_t;
using ::std::type_info;

// Cleanup Node
struct CleanupNode
{
  void* element;
  void (*cleanup)(void*);
  // Cleanup Node cannot be default constructed or copy/move.
  CleanupNode() = delete;
  CleanupNode(const CleanupNode&) = delete;
  CleanupNode(CleanupNode&&) = delete;
  auto operator=(const CleanupNode&) -> CleanupNode& = delete;
  auto operator=(CleanupNode&&) -> CleanupNode& = delete;
  CleanupNode(void* elem, void (*clean)(void*)) : element(elem), cleanup(clean) {}
  ~CleanupNode() = default;
};

inline constexpr uint64_t kByteSize = 8;
static constexpr uint64_t kCleanupNodeSize = align::AlignUpTo<kByteSize>(sizeof(memory::CleanupNode));

template <typename T>
void arena_destruct_object(void* obj) noexcept {
  reinterpret_cast<T*>(obj)->~T();
}

inline constexpr uint64_t kKiloByte = 1024;
inline constexpr uint64_t kMegaByte = 1024 * 1024;
// inline constexpr uint64_t
class Arena
{
 public:
  // make sure Arena can not be copyable
  Arena(const Arena&) = delete;
  auto operator=(const Arena&) -> Arena& = delete;
  // TODO(hurricane): should make move available, and the move assigment
  Arena(Arena&&) noexcept = delete;
  auto operator=(Arena&&) -> Arena& = delete;

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
    void* (*block_alloc)(uint64_t){nullptr};

    // A Function pointer to a dealloc method for the blocks in the Arena.
    void (*block_dealloc)(void*){nullptr};

    // Arena hooked functions
    // Hooks for adding external functionality.
    // Init hook may return a pointer to a cookie to be stored in the arena.
    // reset and destruction hooks will then be called with the same cookie = delete
    // pointer. This allows us to save an external object per arena instance and
    // use it on the other hooks (Note: It is just as legal for init to return
    // NULL and not use the cookie feature).
    // on_arena_reset and on_arena_destruction also receive the space used in
    // the arena just before the reset.
    void* (*on_arena_init)(Arena* arena, const source_location& loc){nullptr};
    void (*on_arena_reset)(Arena* arena, void* cookie, uint64_t space_used, uint64_t space_wasted){nullptr};
    void (*on_arena_allocation)(const type_info* alloc_type, uint64_t alloc_size, void* cookie){nullptr};
    void (*on_arena_newblock)(uint64_t blk_num, uint64_t blk_size, void* cookie){nullptr};
    void* (*on_arena_destruction)(Arena* arena, void* cookie, uint64_t space_used, uint64_t space_wasted){nullptr};

    [[gnu::always_inline]] inline Options()
        : normal_block_size(4 * kKiloByte),  // 4k is the normal pagesize of modern os
          huge_block_size(2 * kMegaByte),    // TODO(hurricane1026): maybe support 1G
          suggested_initblock_size(4 * kKiloByte) {}

    Options(const Options&) = default;

    auto operator=(const Options&) -> Options& = default;

    Options(Options&&) = default;

    auto operator=(Options&&) -> Options& = default;
    ~Options() = default;

    [[gnu::always_inline]] [[gnu::always_inline]] inline void init() noexcept {
      STDB_ASSERT(normal_block_size > 0);
      if (suggested_initblock_size == 0) {
        suggested_initblock_size = normal_block_size;
      }
      if (huge_block_size == 0) {
        huge_block_size = normal_block_size;
      }
    }
  };  // struct Options

  class Block
  {
   public:
    Block(uint64_t size, Block* prev);

    void Reset() noexcept;

    // NOLINTNEXTLINE
    [[gnu::always_inline]] inline auto Pos() noexcept -> char* { return reinterpret_cast<char*>(this) + pos_; }

    // NOLINTNEXTLINE
    [[gnu::always_inline]] inline auto CleanupPos() noexcept -> char* { return reinterpret_cast<char*>(this) + limit_; }

    [[gnu::always_inline]] inline auto alloc(uint64_t size) noexcept -> char* {
      STDB_ASSERT(size <= (limit_ - pos_));
      char* p = Pos();
      pos_ += size;
      return p;
    }

    [[gnu::always_inline]] inline auto alloc_cleanup() noexcept -> char* {
      STDB_ASSERT(pos_ + kCleanupNodeSize <= limit_);
      limit_ -= kCleanupNodeSize;
      return CleanupPos();
    }

    [[gnu::always_inline]] inline void register_cleanup(void* obj, void (*cleanup)(void*)) noexcept {
      auto* ptr = alloc_cleanup();
      new (ptr) CleanupNode(obj, cleanup);
    }

    [[gnu::always_inline]] [[nodiscard]] inline auto prev() const noexcept -> Block* { return prev_; }

    [[gnu::always_inline]] [[nodiscard]] inline auto size() const noexcept -> uint64_t { return size_; }

    [[nodiscard, gnu::always_inline]] inline auto remain() const noexcept -> uint64_t {
      STDB_ASSERT(limit_ >= pos_);
      return limit_ - pos_;
    }

    inline void run_cleanups() noexcept {
      // NOLINTNEXTLINE
      auto* node = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + limit_);
      // NOLINTNEXTLINE
      auto* last = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + size_);

      // NOLINTNEXTLINE
      for (; node < last; ++node) {
        node->cleanup(node->element);
      }
    }

    [[nodiscard, gnu::always_inline]] inline auto cleanups() const noexcept -> uint64_t {
      uint64_t space = size_ - limit_;
      STDB_ASSERT(space % kCleanupNodeSize == 0);

      return space / kCleanupNodeSize;
    }

   private:
    Block* prev_;
    uint64_t pos_;
    uint64_t size_;   // the size of the block
    uint64_t limit_;  // the limit can be use for Create
  };

  class memory_resource : public ::std::pmr::memory_resource
  {
   public:
    explicit memory_resource(Arena* arena) : arena_(arena) { STDB_ASSERT(arena != nullptr); };
    [[nodiscard]] auto get_arena() const -> Arena* { return arena_; }

   protected:
    auto do_allocate(size_t bytes, size_t /* alignment */) noexcept -> void* override {
      return reinterpret_cast<char*>(arena_->allocateAligned(bytes));
    }

    void do_deallocate([[maybe_unused]] void* p, [[maybe_unused]] size_t bytes,
                       [[maybe_unused]] size_t /* alignment*/) noexcept override{};

    [[nodiscard]] auto do_is_equal(const ::std::pmr::memory_resource& _other) const noexcept -> bool override {
      try {
        auto other = dynamic_cast<const memory_resource&>(_other);
        return arena_ == other.arena_;
      } catch (std::bad_cast&) {
        return false;
      }
    }

   private:
    Arena* arena_;
  };

  // Arena constructor
  explicit Arena(const Options& op) : options_(op), last_block_(nullptr), cookie_(nullptr), space_allocated_(0ULL) {
    // init();
    options_.init();
  }

  // Arena desctructor
  ~Arena() {
    // free blocks
    uint64_t all_waste_space = free_all_blocks();
    // make sure the on_arena_destruction was set.
    if (options_.on_arena_destruction != nullptr) {
      [[likely]] options_.on_arena_destruction(this, cookie_, space_allocated_, all_waste_space);
    }
  }

  template <typename T>
  [[gnu::noinline]] auto Own(T* obj) noexcept -> bool {
    static_assert(!is_arena_constructable<T>::value, "Own requires a type can not create in arean");
    // std::function<void()> cleaner = [obj] { delete obj; };
    return addCleanup(obj, &arena_destruct_object<T>);
  }

  inline auto Reset() noexcept -> uint64_t {
    // free all blocks except the first block
    uint64_t all_waste_space = free_blocks_except_head();
    if (options_.on_arena_reset != nullptr) {
      [[likely]] options_.on_arena_reset(this, cookie_, space_allocated_, all_waste_space);
    }
    // reset all internal status.
    uint64_t reset_size = space_allocated_;
    space_allocated_ = last_block_->size();
    last_block_->Reset();
    return reset_size;
  }

  [[nodiscard, gnu::always_inline]] inline auto SpaceAllocated() const noexcept -> uint64_t { return space_allocated_; }
  [[nodiscard, gnu::always_inline]] inline auto SpaceRemains() const noexcept -> uint64_t {
    uint64_t remains = 0;
    for (Block* curr = last_block_; curr != nullptr; curr = curr->prev()) {
      remains += curr->remain();
    }
    return remains;
  }

  // new from arena, and register cleanup function if need
  // always allocating in the arena memory
  // the type T should have the tag:
  template <typename T, typename... Args>
  [[nodiscard]] auto Create(Args&&... args) noexcept -> T* {
    static_assert(is_arena_constructable<T>::value || (std::is_standard_layout<T>::value && std::is_trivial<T>::value),
                  "New requires a constructible type");
    char* ptr = allocateAligned(sizeof(T));
    if (ptr != nullptr) {
      [[likely]] ArenaHelper<T>::Construct(ptr, this, std::forward<Args>(args)...);
      T* result = reinterpret_cast<T*>(ptr);
      if (!RegisterDestructor<T>(result)) {
        [[unlikely]] return nullptr;
      }
      if (options_.on_arena_allocation != nullptr) {
        [[likely]] options_.on_arena_allocation(&typeid(T), sizeof(T), cookie_);
      }
      return result;
    }
    return nullptr;
  }

  // new array from arena, and register cleanup function if need
  template <typename T>
  [[nodiscard]] auto CreateArray(uint64_t num) noexcept -> T* {
    static_assert(
      std::is_standard_layout<T>::value && (std::is_trivial<T>::value || std::is_constructible<T, Arena*>::value),
      "NewArray requires a trivially constructible type or can be constructed with a Arena*");
    static_assert(std::is_trivially_destructible<T>::value, "NewArray requires a trivially destructible type");
    CHECK_LE(num, std::numeric_limits<uint64_t>::max() / sizeof(T));
    const uint64_t n = sizeof(T) * num;
    char* p = allocateAligned(n);
    if (p != nullptr) {
      [[likely]] {
        T* curr = reinterpret_cast<T*>(p);
        for (uint64_t i = 0; i < num; ++i) {
          ArenaHelper<T>::Construct(curr++, this);
        }
        if (options_.on_arena_allocation != nullptr) {
          [[likely]] { options_.on_arena_allocation(&typeid(T), n, cookie_); }
        }
        return reinterpret_cast<T*>(p);
      }
    }
    return nullptr;
  }

  // if return nullptr means failure
  [[nodiscard, gnu::always_inline]] inline auto AllocateAligned(uint64_t bytes) noexcept -> char* {
    if (char* ptr = allocateAligned(bytes); ptr != nullptr) {
      [[likely]] if (options_.on_arena_allocation != nullptr) {
        [[likely]] options_.on_arena_allocation(nullptr, bytes, cookie_);
      }
      return ptr;
    }
    return nullptr;
  }

  // if return nullptr means failure
  [[nodiscard, gnu::always_inline]] inline auto AllocateAlignedAndAddCleanup(uint64_t bytes, void* element,
                                                                             void (*cleanup)(void*)) noexcept -> char* {
    if (char* ptr = allocateAligned(bytes); ptr != nullptr) {
      [[likely]] {
        if (addCleanup(element, cleanup)) {
          [[likely]] {
            if (options_.on_arena_allocation != nullptr) {
              [[likely]] { options_.on_arena_allocation(nullptr, bytes, cookie_); }
            }
            return ptr;
          }
        }
      }
    }
    return nullptr;
  }

  [[gnu::always_inline]] inline void init(const source_location& loc = source_location::current()) noexcept {
    if (options_.on_arena_init != nullptr) {
      [[likely]] { cookie_ = options_.on_arena_init(this, loc); }
    }
  }

  [[gnu::always_inline]] inline auto get_memory_resource() noexcept -> memory_resource {
    return memory_resource{this};
  };

  // for test
  [[maybe_unused]] auto cleanups() -> uint64_t {
    uint64_t total = 0;
    Block* curr = last_block_;
    while (curr != nullptr) {
      total += curr->cleanups();
      curr = curr->prev();
    }
    return total;
  }

 private:
  // New Block while current Block has not enough memory.
  auto newBlock(uint64_t min_bytes, Block* prev_block) noexcept -> Block*;

  auto allocateAligned(uint64_t) noexcept -> char*;

  [[nodiscard, gnu::always_inline]] inline auto need_create_new_block(uint64_t need_bytes) noexcept -> bool {
    return (last_block_ == nullptr) || (need_bytes > last_block_->remain());
  }

  [[nodiscard, gnu::always_inline]] inline auto addCleanup(void* o, void (*cleanup)(void*)) noexcept -> bool {
    if (need_create_new_block(kCleanupNodeSize)) {
      [[unlikely]] {
        Block* curr = newBlock(kCleanupNodeSize, last_block_);
        if (curr != nullptr) {
          [[likely]] last_block_ = curr;
        } else {
          return false;
        }
      }
    }
    last_block_->register_cleanup(o, cleanup);
    return true;
  }

  [[nodiscard, gnu::always_inline]] static inline auto align_size(uint64_t n) noexcept -> uint64_t {
    return align::AlignUpTo<kByteSize>(n);
  }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline auto RegisterDestructor(T* ptr) noexcept -> bool {
    return RegisterDestructorInternal(ptr, typename ArenaHelper<T>::is_destructor_skippable::type());
  }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline auto RegisterDestructorInternal(T* /*unused*/,
                                                                           ::std::true_type /*unused*/) noexcept
    -> bool {
    return true;
  }

  template <typename T>
  [[nodiscard, gnu::always_inline]] inline auto RegisterDestructorInternal(T* ptr,
                                                                           ::std::false_type /*unused*/) noexcept
    -> bool {
    return addCleanup(ptr, &arena_destruct_object<T>);
  }

  // return all remains size of all blocks that was freed.
  [[gnu::always_inline]] inline auto free_all_blocks() noexcept -> uint64_t {
    Block* curr = last_block_;
    Block* prev = nullptr;
    uint64_t remain_size = 0;

    while (curr != nullptr) {
      prev = curr->prev();
      // add the size of curr blk.
      remain_size += curr->remain();
      // run all cleanups first
      curr->run_cleanups();
      options_.block_dealloc(curr);
      curr = prev;
    }
    return remain_size;
  }

  [[gnu::always_inline]] inline auto free_blocks_except_head() noexcept -> uint64_t {
    Block* curr = last_block_;
    Block* prev = nullptr;
    uint64_t remain_size = 0;

    while (curr != nullptr && curr->prev() != nullptr) {
      prev = curr->prev();
      // add the size of curr blk.
      remain_size += curr->remain();
      // run all cleanups first
      curr->run_cleanups();
      options_.block_dealloc(curr);
      curr = prev;
    }
    // add the curr blk remain to result
    remain_size += curr->remain();
    // reset the last_block_ to the first block
    last_block_ = curr;
    return remain_size;
  }

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
  FRIEND_TEST(ArenaTest, AddCleanupTest);
  FRIEND_TEST(ArenaTest, AddCleanupFailTest);
  FRIEND_TEST(ArenaTest, FreeBlocksTest);
  FRIEND_TEST(ArenaTest, FreeBlocksExceptFirstTest);
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
  FRIEND_TEST(ArenaTest, ResetWithCleanupTest);
  FRIEND_TEST(ArenaTest, AllocatorAwareTest);
};  // class Arena

static constexpr uint64_t kBlockHeaderSize = align::AlignUpTo<kByteSize>(sizeof(memory::Arena::Block));

}  // namespace stdb::memory

#endif  // MEMORY_ARENA_HPP_
