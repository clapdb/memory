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

#include <fmt/core.h>

#include <cassert>
#include <concepts>
#include <cstdlib>
#include <limits>
#include <memory_resource>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "arenahelper.hpp"

#if defined(__GNUC__) && (__GNUC__ >= 11)
#include <source_location>
#elif defined(__clang__)
#include <experimental/source_location>
#endif

#define STDB_ASSERT(exp) assert(exp)

#include <boost/core/demangle.hpp>
#define TYPENAME(type) boost::core::demangle(typeid(type).name())  // NOLINT

namespace stdb::memory {
using STring = std::string;
using ::stdb::memory::align::AlignUpTo;

#if defined(__GNUC__) && (__GNUC__ >= 11)
using source_location = std::source_location;
#elif defined(__clang__)
using source_location = std::experimental::source_location;
#else
#error "no support for other compiler"
#endif

using ::std::size_t;
using ::std::type_info;

// Cleanup Node
struct CleanupNode
{
    void* element;
    void (*cleanup)(void*);
};

inline constexpr uint64_t kByteSize = 8;
static constexpr uint64_t kCleanupNodeSize = AlignUpTo<kByteSize>(sizeof(memory::CleanupNode));

template <typename T>
void arena_destruct_object(void* obj) noexcept {
    reinterpret_cast<T*>(obj)->~T();
}

template <typename T>
void arena_delete_object(void* obj) noexcept {
    delete reinterpret_cast<T*>(obj);
}

inline constexpr uint64_t kKiloByte = 1024;
inline constexpr uint64_t kMegaByte = 1024 * 1024;

template <typename T>
concept Creatable = Constructable<T> ||(std::is_standard_layout<T>::value&& std::is_trivial<T>::value);

template <typename T>
concept TriviallyDestructible = std::is_trivially_destructible<T>::value;

enum class ArenaContainStatus : uint8_t
{
    NotContain = 0,
    BlockHeader,
    BlockCleanup,
    BlockUsed,
    BlockUnUsed,
};

class Arena
{
   public:
    // make sure Arena can not be copyable
    Arena(const Arena&) = delete;
    auto operator=(const Arena&) -> Arena& = delete;

    [[gnu::always_inline]] inline Arena(Arena&& other) noexcept
        : _options(other._options),
          _last_block(std::exchange(other._last_block, nullptr)),
          _cookie(std::exchange(other._cookie, nullptr)),
          _space_allocated(std::exchange(other._space_allocated, 0)) {}
    [[gnu::always_inline]] auto operator=(Arena&& other) noexcept -> Arena& {
        _options = other._options;
        _last_block = std::exchange(other._last_block, nullptr);
        _cookie = std::exchange(other._cookie, nullptr);
        _space_allocated = std::exchange(other._space_allocated, 0);
        return *this;
    }

    // Arena Options class for the Arena class 's configuration
    struct Options
    {
        // following parameters should be determined by the OS/CPU Architecture
        // this should make cache-line happy and memory locality better.
        // a Block should is a memory page.

        // normal_block_size should match normal page of the OS.
        uint64_t normal_block_size;

        // huge_block_size should match big memory page of the OS.
        uint64_t huge_block_size;

        // suggested block-size
        uint64_t suggested_init_block_size;

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

        inline static auto GetDefaultOptions() -> Options {
            return {.normal_block_size = 4 * kKiloByte,
                    .huge_block_size = 2 * kMegaByte,
                    .suggested_init_block_size = 4 * kKiloByte,
                    .block_alloc = &std::malloc,
                    .block_dealloc = &std::free};
        }

        /*
        Options()
            : normal_block_size(4 * kKiloByte),  // 4k is the normal pagesize of modern os
              huge_block_size(2 * kMegaByte),    // TODO(hurricane1026): maybe support 1G
              suggested_init_block_size(4 * kKiloByte) {}
         */

        // Options(const Options&) = default;

        // auto operator=(const Options&) -> Options& = default;

        // Options(Options&&) = default;

        // auto operator=(Options&&) -> Options& = default;
        // ~Options() = default;

        void init() noexcept {
            STDB_ASSERT(normal_block_size > 0);
            if (suggested_init_block_size == 0) {
                suggested_init_block_size = normal_block_size;
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
        [[gnu::always_inline]] inline auto Pos() noexcept -> char* { return reinterpret_cast<char*>(this) + _pos; }

        // NOLINTNEXTLINE
        [[gnu::always_inline]] inline auto CleanupPos() noexcept -> char* {
            return reinterpret_cast<char*>(this) + _limit;  // NOLINT
        }

        auto alloc(uint64_t size) noexcept -> char* {
            STDB_ASSERT(size <= (_limit - _pos));
            char* ptr = Pos();
            _pos += size;
            return ptr;
        }

        [[gnu::always_inline]] inline auto alloc_cleanup() noexcept -> char* {
            STDB_ASSERT(_pos + kCleanupNodeSize <= _limit);
            _limit -= kCleanupNodeSize;
            return CleanupPos();
        }

        [[gnu::always_inline]] inline void register_cleanup(void* obj, void (*cleanup)(void*)) noexcept {
            auto* ptr = alloc_cleanup();
            new (ptr) CleanupNode{obj, cleanup};
        }

        [[gnu::always_inline, nodiscard]] inline auto prev() const noexcept -> Block* { return _prev; }

        [[gnu::always_inline, nodiscard]] inline auto size() const noexcept -> uint64_t { return _size; }

        [[gnu::always_inline, nodiscard]] inline auto limit() const noexcept -> uint64_t { return _limit; }

        [[gnu::always_inline, nodiscard]] inline auto pos() const noexcept -> uint64_t { return _pos; }

        [[nodiscard, gnu::always_inline]] inline auto remain() const noexcept -> uint64_t {
            STDB_ASSERT(_limit >= _pos);
            return _limit - _pos;
        }

        void run_cleanups() noexcept {
            // NOLINTNEXTLINE
            auto* node = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + _limit);
            // NOLINTNEXTLINE
            auto* last = reinterpret_cast<CleanupNode*>(reinterpret_cast<char*>(this) + _size);

            // NOLINTNEXTLINE
            for (; node < last; ++node) {
                node->cleanup(node->element);
            }
        }

        [[nodiscard, gnu::always_inline]] inline auto cleanups() const noexcept -> uint64_t {
            uint64_t space = _size - _limit;
            STDB_ASSERT(space % kCleanupNodeSize == 0);
            return space / kCleanupNodeSize;
        }

       private:
        Block* _prev;
        uint64_t _pos;
        uint64_t _size;   // the size of the block
        uint64_t _limit;  // the limit can be use for Create
    };

    class memory_resource : public ::std::pmr::memory_resource
    {
       public:
        explicit memory_resource(Arena* arena) : _arena(arena) { STDB_ASSERT(arena != nullptr); };
        [[nodiscard]] auto get_arena() const -> Arena* { return _arena; }

       protected:
        auto do_allocate(size_t bytes, size_t /* alignment */) noexcept -> void* override {
            return reinterpret_cast<char*>(_arena->allocateAligned(bytes));
        }

        void do_deallocate([[maybe_unused]] void* /*unused*/, [[maybe_unused]] size_t /*unused*/,
                           [[maybe_unused]] size_t /*unused*/) noexcept override{};

        [[nodiscard]] auto do_is_equal(const ::std::pmr::memory_resource& _other) const noexcept -> bool override {
            try {
                auto other = dynamic_cast<const memory_resource&>(_other);
                return _arena == other._arena;
            } catch (std::bad_cast&) {
                return false;
            }
        }

       private:
        Arena* _arena;
    };

    // Arena constructor
    explicit Arena(const Options& ops) : _options(ops), _last_block(nullptr), _cookie(nullptr), _space_allocated(0ULL) {
        _options.init();
        init();
    }

    explicit Arena(Options&& ops) noexcept
        : _options(ops), _last_block(nullptr), _cookie(nullptr), _space_allocated(0ULL) {
        _options.init();
        init();
    }

    ~Arena() {
        // free memory_resource first
        delete _resource;
        // free blocks
        uint64_t all_waste_space = free_all_blocks();
        // make sure the on_arena_destruction was not free.
        if (_options.on_arena_destruction != nullptr) [[likely]] {
            _options.on_arena_destruction(this, _cookie, _space_allocated, all_waste_space);
        }
    }

    template <NonConstructable T>
    [[gnu::noinline]] auto Own(T* obj) noexcept -> bool {
        return addCleanup(obj, &arena_delete_object<T>);
    }

    inline auto Reset() noexcept -> uint64_t {
        // free all blocks except the first block
        uint64_t all_waste_space = free_blocks_except_head();
        if (_options.on_arena_reset != nullptr) [[likely]] {
            _options.on_arena_reset(this, _cookie, _space_allocated, all_waste_space);
        }
        // reset all internal status.
        uint64_t reset_size = _space_allocated;
        _space_allocated = _last_block->size();
        _last_block->Reset();
        return reset_size;
    }

    [[nodiscard, gnu::always_inline]] inline auto SpaceAllocated() const noexcept -> uint64_t {
        return _space_allocated;
    }
    [[nodiscard]] auto SpaceRemains() const noexcept -> uint64_t {
        uint64_t remains = 0;
        for (Block* curr = _last_block; curr != nullptr; curr = curr->prev()) {
            remains += curr->remain();
        }
        return remains;
    }

    // new from arena, and register cleanup function if needed
    // always allocating in the arena memory
    // the type T should have the tag:
    template <Creatable T, typename... Args>
    [[nodiscard]] auto Create(Args&&... args) noexcept -> T* {
        char* ptr = allocateAligned(sizeof(T));
        if (ptr != nullptr) [[likely]] {
            ArenaHelper<T>::Construct(ptr, *this, std::forward<Args>(args)...);
            T* result = reinterpret_cast<T*>(ptr);
            if (!RegisterDestructor<T>(result)) [[unlikely]] {
                return nullptr;
            }
            if (_options.on_arena_allocation != nullptr) [[likely]] {
                _options.on_arena_allocation(&typeid(T), sizeof(T), _cookie);
            }
            return result;
        }
        return nullptr;
    }

    // new array from arena, and register cleanup function if needed
    template <Creatable T>
    [[nodiscard]] auto CreateArray(uint64_t num) noexcept -> T* requires TriviallyDestructible<T> {
        if (num > std::numeric_limits<uint64_t>::max() / sizeof(T)) {
            fmt::print(
              stderr,
              "CreateArray need too many memory, that more than max of uint64_t, the num of array is {}, and the Type "
              "is {}, sizeof T is {}",
              num, TYPENAME(T), sizeof(T));
        }
        const uint64_t size = sizeof(T) * num;
        char* p = allocateAligned(size);
        if (p != nullptr) [[likely]] {
            T* curr = reinterpret_cast<T*>(p);
            for (uint64_t i = 0; i < num; ++i) {
                ArenaHelper<T>::Construct(curr++, *this);
            }
            if (_options.on_arena_allocation != nullptr) [[likely]] {
                _options.on_arena_allocation(&typeid(T), size, _cookie);
            }
            return reinterpret_cast<T*>(p);
        }
        return nullptr;
    }

    /*
     * Allocate a piece of aligned memory in the arena.
     * return nullptr means failure
     */
    [[nodiscard]] auto AllocateAligned(uint64_t bytes) noexcept -> char* {
        if (char* ptr = allocateAligned(bytes); ptr != nullptr) [[likely]] {
            if (_options.on_arena_allocation != nullptr) [[likely]] {
                _options.on_arena_allocation(nullptr, bytes, _cookie);
            }
            return ptr;
        }
        return nullptr;
    }

    /*
     * Allocate a piece of aligned memory, and place a cleanup node in end of block.
     * return nullptr means failure
     */
    [[nodiscard]] auto AllocateAlignedAndAddCleanup(uint64_t bytes, void* element, void (*cleanup)(void*)) noexcept
      -> char* {
        if (char* ptr = allocateAligned(bytes); ptr != nullptr) [[likely]] {
            if (addCleanup(element, cleanup)) [[likely]] {
                if (_options.on_arena_allocation != nullptr) [[likely]] {
                    { _options.on_arena_allocation(nullptr, bytes, _cookie); }
                }
                return ptr;
            }
        }
        return nullptr;
    }

    [[gnu::always_inline]] inline auto get_memory_resource() noexcept -> memory_resource* {
        STDB_ASSERT(_resource != nullptr);
        return _resource;
    };

    auto check(const char* ptr) -> ArenaContainStatus;

    /*
     * get all cleanup nodes, just for testing.
     */
    [[maybe_unused]] auto cleanups() -> uint64_t {
        uint64_t total = 0;
        Block* curr = _last_block;
        while (curr != nullptr) {
            total += curr->cleanups();
            curr = curr->prev();
        }
        return total;
    }

   private:
    /*
     * init the arena
     * call the callback to monitor and metrics: this arena was inited.
     */
    [[gnu::always_inline]] inline void init(const source_location& loc = source_location::current()) noexcept {
        try {
            _resource = new memory_resource{this};
        } catch (std::bad_alloc& ex) {
            _resource = nullptr;
            fmt::print(stderr, "new memory resource failed while Arena::init");
        }
        if (_options.on_arena_init != nullptr) [[likely]] {
            _cookie = _options.on_arena_init(this, loc);
        }
    }

    /*
     * new a block within the arena.
     * New Block while current Block has not enough memory.
     */
    auto newBlock(uint64_t min_bytes, Block* prev_block) noexcept -> Block*;

    /*
     * internal allocate aligned impl.
     */
    auto allocateAligned(uint64_t) noexcept -> char*;

    /*
     * check if needed a new block
     */
    [[nodiscard, gnu::always_inline]] inline auto need_create_new_block(uint64_t need_bytes) noexcept -> bool {
        return (_last_block == nullptr) || (need_bytes > _last_block->remain());
    }

    /*
     * add A Cleanup node to current block.
     */
    [[nodiscard]] auto addCleanup(void* obj, void (*cleanup)(void*)) noexcept -> bool {
        if (need_create_new_block(kCleanupNodeSize)) [[unlikely]] {
            Block* curr = newBlock(kCleanupNodeSize, _last_block);
            if (curr != nullptr) {
                _last_block = curr;
            } else {
                return false;
            }
        }
        _last_block->register_cleanup(obj, cleanup);
        return true;
    }

    /*
     * a thin wrapper for AlignUpTo
     */
    [[nodiscard, gnu::always_inline]] static inline auto align_size(uint64_t n) noexcept -> uint64_t {
        return AlignUpTo<kByteSize>(n);
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

    /*
     * free all blocks and return all remains size of all blocks that was freed.
     */
    auto free_all_blocks() noexcept -> uint64_t {
        Block* curr = _last_block;
        Block* prev = nullptr;
        uint64_t remain_size = 0;

        while (curr != nullptr) {
            prev = curr->prev();
            // add the size of curr blk.
            remain_size += curr->remain();
            // run all cleanups first
            curr->run_cleanups();
            _options.block_dealloc(curr);
            curr = prev;
        }
        _last_block = nullptr;
        return remain_size;
    }

    /*
     * free all blocks except the first block.
     */
    auto free_blocks_except_head() noexcept -> uint64_t {
        Block* curr = _last_block;
        Block* prev = nullptr;
        uint64_t remain_size = 0;

        while (curr != nullptr && curr->prev() != nullptr) {
            prev = curr->prev();
            // add the size of curr blk.
            remain_size += curr->remain();
            // run all cleanups first
            curr->run_cleanups();
            _options.block_dealloc(curr);
            curr = prev;
        }
        // add the curr blk remain to result
        remain_size += curr->remain();
        // reset the last_block_ to the first block
        _last_block = curr;
        return remain_size;
    }

    Options _options;
    Block* _last_block;
    memory_resource* _resource{nullptr};

    // should be initialized by on_arena_init
    // and should be destroyed by on_arena_destruction
    void* _cookie;

    uint64_t _space_allocated;

    static constexpr uint64_t kThresholdHuge = 4;

    friend class ArenaTestHelper;
};  // class Arena

static constexpr uint64_t kBlockHeaderSize = AlignUpTo<kByteSize>(sizeof(memory::Arena::Block));

}  // namespace stdb::memory
