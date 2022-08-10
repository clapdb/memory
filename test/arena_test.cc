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

#include "arena/arena.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
#ifndef _MULTI_THREAD_TEST_
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#endif
#include "doctest/doctest.h"

using stdb::memory::align::AlignUp;
using stdb::memory::align::AlignUpTo;
namespace stdb::memory {
class alloc_class
{
   public:
    alloc_class() : init(static_cast<char*>(malloc(4 * 1024 * 1024))), curr(init) {}
    virtual ~alloc_class() { free(init); }

    virtual auto alloc(uint64_t size) -> void* {
        alloc_sizes.push_back(size);
        void* ret = static_cast<void*>(curr);
        ptrs.push_back(ret);
        curr += size;
        return ret;
    }

    void dealloc(void* ptr) { free_ptrs.push_back(ptr); }

    void reset() {
        alloc_sizes.clear();
        ptrs.clear();
        free_ptrs.clear();
    }

    // private:
    std::vector<uint64_t> alloc_sizes;
    std::vector<void*> ptrs;
    std::vector<void*> free_ptrs;
    char* init{nullptr};
    char* curr{nullptr};
};

class alloc_fail_class : public alloc_class
{
   public:
    auto alloc(uint64_t) -> void* override { return nullptr; }
    ~alloc_fail_class() override = default;
};

class BlockTest
{
   public:
    BlockTest() : ptr(malloc(1024)) {}
    ~BlockTest() { free(ptr); }

   protected:
    auto Pointer() -> void* { return ptr; }

   private:
    void* ptr{nullptr};
};

class cleanup_mock
{
   public:
    void cleanup1(void*) { clean1 = true; }
    void cleanup2(void*) { clean2 = true; }
    void cleanup3(void*) { clean3 = true; }
    bool clean1{false};
    bool clean2{false};
    bool clean3{false};
};

void cleanup_mock_fn1(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup1(p); }

void cleanup_mock_fn2(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup2(p); }

void cleanup_mock_fn3(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup3(p); }

thread_local cleanup_mock* mock_cleaners;

TEST_CASE_FIXTURE(BlockTest, "BlockTest.CotrTest1") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    CHECK_EQ(b->Pos(), reinterpret_cast<char*>(Pointer()) + kBlockHeaderSize);
    CHECK_EQ(b->size(), 1024ULL);
    CHECK_EQ(b->pos(), kBlockHeaderSize);
    CHECK_EQ(b->limit(), 1024ULL);
    CHECK_EQ(b->prev(), nullptr);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.CotrTest2") {
    // NOLINTNEXTLINE
    void* last = malloc(100);
    auto* last_b = reinterpret_cast<Arena::Block*>(last);
    auto* b = new (Pointer()) Arena::Block(1024, last_b);
    CHECK_EQ(b->prev(), last_b);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    // NOLINTNEXTLINE
    free(last);
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.AllocTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc(200);
    CHECK_NE(x, nullptr);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize - 200ULL);
    CHECK_EQ(b->Pos() - x, 200LL);
    CHECK_EQ(b->pos(), kBlockHeaderSize + 200LL);
    CHECK_EQ(b->size(), 1024ULL);
    CHECK_EQ(b->limit(), 1024ULL);
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.AllocCleanupTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc_cleanup();
    CHECK_NE(x, nullptr);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize);
    CHECK_EQ(b->limit(), b->size() - kCleanupNodeSize);
    char* x1 = b->alloc_cleanup();
    CHECK_EQ(x - x1, kCleanupNodeSize);
    CHECK_EQ(b->limit(), b->size() - 2 * kCleanupNodeSize);
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.RegCleanupTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize);
    CHECK_EQ(b->limit(), b->size() - kCleanupNodeSize);
    b->run_cleanups();
    CHECK(mock_cleaners->clean1);
    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.RunCleanupTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn2);
    b->run_cleanups();
    CHECK(mock_cleaners->clean1);
    CHECK(mock_cleaners->clean2);
    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.ResetTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc(200);
    CHECK_NE(x, nullptr);
    CHECK_EQ(b->size(), 1024ULL);
    CHECK_EQ(b->limit(), 1024ULL);
    CHECK_EQ(b->remain(), 1024ULL - 200ULL - kBlockHeaderSize);
    CHECK_EQ(b->Pos() - x, 200LL);
    b->Reset();
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    CHECK_EQ(b->limit(), 1024ULL);
    CHECK_EQ(b->Pos() - x, 0LL);
}

TEST_CASE_FIXTURE(BlockTest, "BlockTest.ResetwithCleanupTest") {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    char* x = b->alloc(200);
    CHECK_NE(x, nullptr);
    CHECK_EQ(b->remain(), 1024ULL - 200ULL - kBlockHeaderSize);
    CHECK_EQ(b->Pos() - x, 200LL);
    CHECK_EQ(b->limit(), 1024ULL);

    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize - 200);
    CHECK_EQ(b->limit(), 1024ULL - kCleanupNodeSize);

    b->Reset();
    CHECK_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    CHECK_EQ(b->Pos() - x, 0LL);
    CHECK_EQ(b->limit(), 1024ULL);

    CHECK(mock_cleaners->clean1);
    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST_CASE("AlignTest") {
    SUBCASE("AlignUpToTest8") {
        CHECK_EQ(AlignUpTo<8>(15), 16ULL);
        CHECK_EQ(AlignUpTo<8>(1), 8ULL);
        CHECK_EQ(AlignUpTo<8>(32), 32ULL);
        CHECK_EQ(AlignUpTo<8>(255), 256ULL);
        CHECK_EQ(AlignUpTo<8>(1024), 1024ULL);
    }

    SUBCASE("AlignUpToTest16") {
        CHECK_EQ(AlignUpTo<16>(15), 16ULL);
        CHECK_EQ(AlignUpTo<16>(1), 16ULL);
        CHECK_EQ(AlignUpTo<16>(32), 32ULL);
        CHECK_EQ(AlignUpTo<16>(255), 256ULL);
        CHECK_EQ(AlignUpTo<16>(1024), 1024ULL);
    }

    SUBCASE("AlignUpToTest4") {
        CHECK_EQ(AlignUpTo<4>(15), 16ULL);
        CHECK_EQ(AlignUpTo<4>(1), 4ULL);
        CHECK_EQ(AlignUpTo<4>(32), 32ULL);
        CHECK_EQ(AlignUpTo<4>(255), 256ULL);
        CHECK_EQ(AlignUpTo<4>(1024), 1024ULL);
    }

    // test without on_arena_* trigger
    SUBCASE("AlignUpTest") {
        CHECK_EQ(AlignUp(63 + 2048, 1024), 3072ULL);
        CHECK_EQ(AlignUp(2048, 1024), 2048ULL);
    }
}

class ArenaTest
{
   public:
    Arena::Options ops_complex;
    Arena::Options ops_simple;

    thread_local static alloc_class* mock;

    static auto mock_alloc(uint64_t size) -> void* { return mock->alloc(size); }

    static void mock_dealloc(void* ptr) { return mock->dealloc(ptr); }

    ArenaTest() {
        // initialize the ops_complex
        ops_complex.block_alloc = &mock_alloc;
        ops_complex.block_dealloc = &mock_dealloc;
        ops_complex.normal_block_size = 1024ULL;
        ops_complex.suggested_init_block_size = 4096ULL;
        ops_complex.huge_block_size = 1024ULL * 1024ULL;
        ops_complex.logger_func = &default_logger_func;

        // initialize the ops_simple
        ops_simple.block_alloc = &mock_alloc;
        ops_simple.block_dealloc = &mock_dealloc;
        ops_simple.normal_block_size = 1024ULL;
        ops_simple.suggested_init_block_size = 1024ULL;
        ops_simple.huge_block_size = 1024ULL;
        ops_simple.logger_func = &default_logger_func;
    }
};

class ArenaTestHelper
{
   public:
    explicit ArenaTestHelper(Arena& a) : _arena(a) {}

    const Arena::Options& options() { return _arena._options; }
    [[nodiscard]] uint64_t space_allocated() const { return _arena._space_allocated; }
    [[nodiscard]] Arena::Block*& last_block() const { return _arena._last_block; }

    auto newBlock(uint64_t m, Arena::Block* prev_b) noexcept -> Arena::Block* { return _arena.newBlock(m, prev_b); }
    auto addCleanup(void* a, void (*cleanup)(void*)) noexcept -> bool { return _arena.addCleanup(a, cleanup); }
    auto free_blocks_except_head() noexcept -> uint64_t { return _arena.free_blocks_except_head(); }
    auto free_all_blocks() noexcept -> uint64_t { return _arena.free_all_blocks(); }
    auto cookie() -> void* { return _arena._cookie; }

   private:
    Arena& _arena;
};

thread_local alloc_class* ArenaTest::mock = nullptr;

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.CtorTest") {
    Arena a(ops_complex);
    ArenaTestHelper ah(a);
    CHECK_EQ(ah.last_block(), nullptr);
    CHECK_EQ(ah.options().normal_block_size, 1024ULL);
    CHECK_EQ(ah.options().block_alloc, &mock_alloc);
    CHECK_EQ(ah.options().block_dealloc, &mock_dealloc);
    Arena b(ops_simple);
    ArenaTestHelper bh(b);
    CHECK_EQ(bh.options().normal_block_size, 1024ULL);
    CHECK_EQ(bh.options().suggested_init_block_size, 1024ULL);
    CHECK_EQ(bh.options().huge_block_size, 1024ULL);
    CHECK_EQ(bh.last_block(), nullptr);
    CHECK_EQ(bh.options().block_alloc, &mock_alloc);
    CHECK_EQ(bh.options().block_dealloc, &mock_dealloc);
    Arena::Options o = Arena::Options::GetDefaultOptions();
    o.block_alloc = mock_alloc;
    o.block_dealloc = mock_dealloc;
    Arena c(o);
    ArenaTestHelper ch(c);
    CHECK_EQ(ch.options().normal_block_size, 4096ULL);
    CHECK_EQ(ch.options().suggested_init_block_size, 4096ULL);
    CHECK_EQ(ch.options().huge_block_size, 2ULL * 1024ULL * 1024ULL);
    CHECK_EQ(ch.options().block_alloc, &mock_alloc);
    CHECK_EQ(ch.options().block_dealloc, &mock_dealloc);
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.NewBlockTest") {
    // the case bearing the a and b is leaking.
    // because I just want to test the NewBlock
    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    ArenaTestHelper ah(*a);
    Arena::Block* bb = ah.newBlock(100, nullptr);
    CHECK_EQ(bb, mock->ptrs.front());
    CHECK_EQ(ah.space_allocated(), 1024ULL);
    CHECK_EQ(bb->remain(), 1024 - kBlockHeaderSize);

    auto* b = new Arena(ops_complex);
    ArenaTestHelper bh(*b);
    // will ignore the init block from building cleanups
    // this situation will never occurs in the real world.
    auto* xx = bh.newBlock(200, nullptr);
    CHECK_EQ(xx, mock->ptrs.back());

    bb = bh.newBlock(2500, bb);

    bb = bh.newBlock(300 * 1024 + 100, bb);

    bb = bh.newBlock(2 * 1024 * 1024, bb);
    auto* bb1 = bh.newBlock(2 * 1024 * 1024, bb);
    CHECK_EQ(bb1, mock->ptrs.back());
    CHECK_EQ(bb1->prev(), bb);
    // cleanup
    delete mock;
    mock = nullptr;
    delete a;
    delete b;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.AllocateTest") {
    mock = new alloc_class;
    auto* x = new Arena(ops_complex);
    auto* new_ptr = x->AllocateAligned(3500);
    CHECK_EQ(new_ptr, static_cast<char*>(mock->init) + sizeof(Arena::Block));

    auto* next_ptr = x->AllocateAligned(755);

    CHECK_EQ(next_ptr, static_cast<char*>(mock->ptrs.back()) + sizeof(Arena::Block));
    delete x;
    CHECK_EQ(mock->free_ptrs.size(), 2);
    CHECK_EQ(mock->free_ptrs.front(), mock->ptrs.back());
    CHECK_EQ(mock->free_ptrs.back(), mock->ptrs.front());
    delete mock;
    mock = nullptr;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.AddCleanupTest") {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    auto* a = new Arena(ops_complex);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    bool ok = ah.addCleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK(ok);
    CHECK_EQ(1, a->cleanups());

    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* obj = malloc(120);  // will be free automatically
    bool ok2 = ah.addCleanup(obj, &std::free);
    CHECK(ok2);
    CHECK_EQ(2, a->cleanups());
    CHECK_EQ(ah.last_block()->remain(), ah.last_block()->size() - kBlockHeaderSize - kCleanupNodeSize * 2);

    CHECK_EQ(mock->alloc_sizes.front(), 4096);
    delete a;
    delete mock;
    delete mock_cleaners;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.AddCleanupFailTest") {
    auto* a = new Arena(ops_complex);
    mock = new alloc_fail_class;
    ArenaTestHelper ah(*a);
    // this line means malloc failed, so no space for cleanup Node.

    mock_cleaners = new cleanup_mock;
    bool ok = ah.addCleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK_FALSE(ok);

    CHECK_EQ(mock->alloc_sizes.size(), 0);
    delete a;
    delete mock;
    delete mock_cleaners;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.FreeBlocksExceptFirstTest") {
    auto* a = new Arena(ops_simple);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;

    ah.last_block() = ah.newBlock(1024 - kBlockHeaderSize, nullptr);
    CHECK_EQ(ah.last_block(), mock->ptrs.back());
    CHECK_EQ(mock->alloc_sizes.back(), 1024);
    ah.last_block() = ah.newBlock(2048 - kBlockHeaderSize, ah.last_block());
    CHECK_EQ(ah.last_block(), mock->ptrs.back());
    CHECK_EQ(mock->alloc_sizes.back(), 2048);
    ah.last_block() = ah.newBlock(4096 - kBlockHeaderSize, ah.last_block());
    CHECK_EQ(ah.last_block(), mock->ptrs.back());
    CHECK_EQ(mock->alloc_sizes.back(), 4096);

    // FreeBlocks should not be call out of the class, just use ~Arena
    auto wasted = ah.free_blocks_except_head();
    CHECK_EQ(wasted, 1024 * 7 - 3 * kBlockHeaderSize);
    CHECK_EQ(mock->free_ptrs.size(), 2);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    ah.last_block() = nullptr;
    delete a;
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.ResetTest") {
    auto* a = new Arena(ops_simple);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;

    ah.last_block() = ah.newBlock(1024 - kBlockHeaderSize, nullptr);
    ah.last_block() = ah.newBlock(2048 - kBlockHeaderSize, ah.last_block());
    ah.last_block() = ah.newBlock(4096 - kBlockHeaderSize, ah.last_block());

    a->Reset();
    CHECK_EQ(mock->free_ptrs.size(), 2);

    CHECK_EQ(ah.last_block(), mock->ptrs.front());
    CHECK_EQ(ah.space_allocated(), 1024);
    CHECK_EQ(ah.last_block()->remain(), 1024 - kBlockHeaderSize);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    ah.last_block() = nullptr;
    delete a;
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.ResetWithCleanupTest") {
    auto* a = new Arena(ops_simple);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;

    ah.last_block() = ah.newBlock(1024 - kBlockHeaderSize, nullptr);
    ah.last_block() = ah.newBlock(2048 - kBlockHeaderSize, ah.last_block());
    ah.last_block() = ah.newBlock(4096 - kBlockHeaderSize, ah.last_block());

    bool ok = ah.addCleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK(ok);

    a->Reset();

    CHECK(mock_cleaners->clean1);
    CHECK_EQ(ah.last_block(), mock->ptrs.front());
    CHECK_EQ(ah.space_allocated(), 1024);
    CHECK_EQ(ah.last_block()->remain(), 1024 - kBlockHeaderSize);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    ah.last_block() = nullptr;
    delete a;
    delete mock_cleaners;
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.FreeBlocksTest") {
    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    ArenaTestHelper ah(*a);

    ah.last_block() = ah.newBlock(1024 - kBlockHeaderSize, nullptr);
    ah.last_block() = ah.newBlock(2048 - kBlockHeaderSize, ah.last_block());
    ah.last_block() = ah.newBlock(4096 - kBlockHeaderSize, ah.last_block());

    auto wasted = ah.free_all_blocks();
    CHECK_EQ(wasted, 7 * 1024 - 3 * kBlockHeaderSize);
    CHECK_EQ(mock->free_ptrs.size(), 3);

    delete a;
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.DoCleanupTest") {
    auto* a = new Arena(ops_complex);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    bool ok = ah.addCleanup(mock_cleaners, &cleanup_mock_fn1);
    CHECK(ok);

    delete a;
    CHECK(mock_cleaners->clean1);
    delete mock_cleaners;
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.to_string<std::pmr::string>") {
    auto* a = new Arena(ops_simple);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;
    std::string str = "this is a very very long string that need to be allocated on arena";
    std::string s;

    {
        std::pmr::string pmr_str(str, a->get_memory_resource());
        CHECK_EQ(ah.space_allocated(), 1024ULL);
        CHECK_EQ(strcmp(pmr_str.c_str(), str.c_str()), 0);
    }

    delete a;
    CHECK_EQ(mock->ptrs.size(), 1);
    CHECK_EQ(mock->free_ptrs.size(), 1);
    CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
    CHECK_EQ(mock->alloc_sizes.front(), 1024);
    delete mock;
}

class mock_own
{
   public:
    mock_own() { count++; }
    ~mock_own() { count--; }
    inline static int count = 0;
};

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.OwnTest") {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    auto* m = new mock_own();
    auto* a = new Arena(ops_complex);
    mock = new alloc_class;
    bool ok = a->Own<mock_own>(m);
    CHECK(ok);
    delete a;
    CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
    CHECK_EQ(mock_own::count, 0);
    delete mock;
    mock = nullptr;
}

struct mock_struct
{
    int i;
    char c;
    double rate;
    void* p;
};

class cstr_class
{
   public:
    void construct(const std::string& input) {
        name = input;
        count++;
    }
    void destruct(const std::string& input) {
        name = input;
        count--;
    }
    std::string name;
    inline static int count = 0;
};

thread_local cstr_class* cstr;

class mock_class_need_dstr
{
   public:
    ArenaFullManagedTag;
    mock_class_need_dstr(int i, std::string name) : index_(i), n_(std::move(name)) { cstr->construct(n_); }
    ~mock_class_need_dstr() { cstr->destruct(n_); }
    auto verify(int i, const std::string& name) -> bool { return (i == index_) && (name == n_); }

   private:
    int index_;
    const std::string n_;
};

class mock_class_without_dstr
{
   public:
    ArenaManagedCreateOnlyTag;
    static void construct() { count++; }
    static void destruct() { count--; }
    // NOLINTNEXTLINE
    explicit mock_class_without_dstr(std::string name) : n_(std::move(name)) { cstr->construct(n_); }
    ~mock_class_without_dstr() { cstr->destruct(n_); }
    auto verify(const std::string& n) -> bool { return n_ == n; }

   private:
    const std::string n_;
    inline static int count = 0;
};

class mock_class_with_arena
{
   public:
    ArenaFullManagedTag;
    // explicit mock_class_with_arena(Arena* arena) : arena_(arena) {}
    mock_class_with_arena() = default;

   private:
    // Arena* arena_{nullptr};
};

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.CreateTest") {
    mock = new alloc_class;
    cstr = new cstr_class;
    auto* a = new Arena(ops_complex);
    ArenaTestHelper ah(*a);
    std::string ss("hello world");
    std::string sss("fuck the world");

    auto* d2 = a->Create<mock_class_without_dstr>("fuck the world");
    CHECK_EQ(mock->alloc_sizes.front(), 4096);
    CHECK_EQ(0, a->cleanups());

    auto* d1 = a->Create<mock_class_need_dstr>(3, ss);
    CHECK_EQ(1, a->cleanups());
    CHECK(d1->verify(3, ss));
    CHECK(d2->verify(sss));

    auto r1 = ah.last_block()->remain();
    auto* r = a->Create<mock_struct>();
    CHECK(r != nullptr);
    CHECK_EQ(1, a->cleanups());  // mock_struct will not be register to cleanup
    auto r2 = ah.last_block()->remain();
    CHECK_EQ(r1 - r2, sizeof(mock_struct));

    // auto pass Arena*
    (void)a->Create<mock_class_with_arena>();
    CHECK_EQ(cstr->count, 2);

    delete a;

    CHECK_EQ(mock->free_ptrs.size(), 1);
    CHECK_EQ(cstr->count, 1);
    delete mock;
    delete cstr;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.CreateArrayTest") {
    mock = new alloc_class;
    auto* a = new Arena(ops_complex);
    ArenaTestHelper ah(*a);

    auto* r = a->CreateArray<uint64_t>(10);
    CHECK(r != nullptr);
    uint64_t s2 = ah.last_block()->remain();
    CHECK_EQ(s2, 4096 - kBlockHeaderSize - 10 * sizeof(uint64_t));

    struct test_struct
    {
        int i;
        char n;
        double d;
    };

    auto* r1 = a->CreateArray<test_struct>(10);
    CHECK(r1 != nullptr);
    uint64_t s3 = ah.last_block()->remain();

    CHECK_EQ(s3, 4096ULL - kBlockHeaderSize - 10ULL * sizeof(uint64_t) - 10ULL * sizeof(test_struct));
    // auto pass Arena*
    (void)a->CreateArray<mock_class_with_arena>(10);

    delete a;
    CHECK_EQ(mock->free_ptrs.size(), 1);
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.DstrTest") {
    auto* a = new Arena(ops_simple);
    ArenaTestHelper ah(*a);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    ah.last_block() = ah.newBlock(1024 - kBlockHeaderSize, nullptr);
    ah.last_block() = ah.newBlock(2048 - kBlockHeaderSize, ah.last_block());
    ah.last_block() = ah.newBlock(4096 - kBlockHeaderSize, ah.last_block());

    bool ok1 = ah.addCleanup(mock_cleaners, &cleanup_mock_fn1);
    bool ok2 = ah.addCleanup(mock_cleaners, &cleanup_mock_fn2);
    bool ok3 = ah.addCleanup(mock_cleaners, &cleanup_mock_fn3);
    CHECK(ok1);
    CHECK(ok2);
    CHECK(ok3);

    delete a;
    CHECK_EQ(mock->free_ptrs.size(), 3);
    CHECK(mock_cleaners->clean1);
    CHECK(mock_cleaners->clean2);
    CHECK(mock_cleaners->clean3);
    delete mock;
    delete mock_cleaners;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.SpaceTest") {
    auto* x = new Arena(ops_complex);
    ArenaTestHelper xh(*x);
    mock = new alloc_class;
    CHECK_EQ(x->SpaceAllocated(), 0ULL);

    auto* new_ptr = x->AllocateAligned(3500);
    CHECK_EQ(new_ptr, static_cast<char*>(mock->ptrs.front()) + sizeof(Arena::Block));

    auto* next_ptr = x->AllocateAligned(755);
    CHECK_EQ(next_ptr, static_cast<char*>(mock->ptrs.back()) + sizeof(Arena::Block));

    CHECK_EQ(x->SpaceAllocated(), 5120ULL);
    CHECK_EQ(xh.last_block(), mock->ptrs.back());

    delete x;
    CHECK_EQ(mock->free_ptrs.size(), 2);
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.RemainsTest") {
    auto* x = new Arena(ops_complex);
    ArenaTestHelper xh(*x);
    mock = new alloc_class;
    CHECK_EQ(x->SpaceAllocated(), 0ULL);

    auto* new_ptr = x->AllocateAligned(3500);
    CHECK_EQ(new_ptr, static_cast<char*>(mock->ptrs.front()) + sizeof(Arena::Block));

    auto* next_ptr = x->AllocateAligned(755);
    CHECK_EQ(next_ptr, static_cast<char*>(mock->ptrs.back()) + sizeof(Arena::Block));

    CHECK_EQ(x->SpaceRemains(), 1024ULL - kBlockHeaderSize - 760);
    CHECK_EQ(xh.last_block(), mock->ptrs.back());

    delete x;
    CHECK_EQ(mock->free_ptrs.size(), 2);
    delete mock;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.AllocateAlignedAndAddCleanupTest") {
    auto* a = new Arena(ops_complex);
    mock_cleaners = new cleanup_mock;
    mock = new alloc_class;
    auto* new_ptr = a->AllocateAlignedAndAddCleanup(3500, mock_cleaners, cleanup_mock_fn1);
    CHECK_EQ(new_ptr, static_cast<char*>(mock->ptrs.front()) + sizeof(Arena::Block));

    auto* next_ptr = a->AllocateAlignedAndAddCleanup(755, mock_cleaners, cleanup_mock_fn2);

    CHECK_EQ(next_ptr, static_cast<char*>(mock->ptrs.back()) + sizeof(Arena::Block));
    delete a;
    CHECK_EQ(mock->free_ptrs.size(), 2);
    delete mock;
    CHECK(mock_cleaners->clean1);
    CHECK(mock_cleaners->clean2);
    delete mock_cleaners;
}

TEST_CASE("ArenaTest.CheckTest") {
    struct destructible
    {
        ArenaFullManagedTag;
        destructible() { to_free = new char[5]; }
        ~destructible() { delete[] to_free; }
        int x{0};
        char* to_free;
    };
    struct skip_destructible
    {
        ArenaManagedCreateOnlyTag;
        skip_destructible() = default;
        ~skip_destructible() = default;
        int z{0};
    };
    Arena::Options realops = Arena::Options::GetDefaultOptions();
    Arena a(realops);
    int x = 0;
    std::string ss = "with dstr";
    std::unique_ptr<int> ptr = std::make_unique<int>();
    auto* arena_managed_ptr = a.AllocateAligned(100);
    auto* with_dstr = a.Create<destructible>();
    auto* without_dstr = a.Create<skip_destructible>();
    ArenaTestHelper helper(a);
    auto* block = helper.last_block();

    CHECK_EQ(a.check(reinterpret_cast<char*>(block)), ArenaContainStatus::BlockHeader);
    CHECK_EQ(a.check(reinterpret_cast<char*>(&x)), ArenaContainStatus::NotContain);
    CHECK_EQ(a.check(reinterpret_cast<char*>(ptr.get())), ArenaContainStatus::NotContain);
    CHECK_EQ(a.check(reinterpret_cast<char*>(with_dstr)), ArenaContainStatus::BlockUsed);
    CHECK_EQ(a.check(reinterpret_cast<char*>(without_dstr)), ArenaContainStatus::BlockUsed);
    CHECK_EQ(a.check(reinterpret_cast<char*>(arena_managed_ptr)), ArenaContainStatus::BlockUsed);
    CHECK_EQ(a.check(reinterpret_cast<char*>(with_dstr) + 200), ArenaContainStatus::BlockUnUsed);
    CHECK_EQ(a.check(reinterpret_cast<char*>(block) + 4090), ArenaContainStatus::BlockCleanup);
    CHECK_EQ(a.check(reinterpret_cast<char*>(block) + block->size() - kCleanupNodeSize),
             ArenaContainStatus::BlockCleanup);
    CHECK_EQ(a.check(reinterpret_cast<char*>(block) + block->size()), ArenaContainStatus::NotContain);
}

class mock_hook
{
   public:
    explicit mock_hook(void* cookie) : _cookie(cookie) {}
    auto arena_init_hook(Arena*) -> void* {
        inited++;
        return _cookie;
    }
    void arena_allocate_hook(const std::type_info*, uint64_t, void*) { allocated++; }
    auto arena_destruction_hook(Arena*, void*, uint64_t, uint64_t) -> void* {
        destructed++;
        return _cookie;
    }
    void arena_reset_hook(Arena*, void*, uint64_t, uint64_t) { reseted++; }
    int inited = 0;
    int allocated = 0;
    int destructed = 0;
    int reseted = 0;

   private:
    void* _cookie = nullptr;
};

thread_local mock_hook* hook_instance;

auto init_hook(Arena* a, [[maybe_unused]] const source_location& loc = source_location::current()) -> void* {
    return hook_instance->arena_init_hook(a);
}

void allocate_hook(const std::type_info* t, uint64_t s, void* c) { hook_instance->arena_allocate_hook(t, s, c); }

auto destruction_hook(Arena* a, void* c, uint64_t s, uint64_t w) -> void* {
    return hook_instance->arena_destruction_hook(a, c, s, w);
}

void reset_hook(Arena* a, void* cookie, uint64_t space_used, uint64_t space_wasted) {
    hook_instance->arena_reset_hook(a, cookie, space_used, space_wasted);
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.HookTest") {
    struct xx
    {
        int i;
        char y;
        double d;
    };

    void* cookie = std::malloc(128);
    Arena::Options ops_hook = ops_complex;
    ops_hook.on_arena_init = &init_hook;
    ops_hook.on_arena_allocation = &allocate_hook;
    ops_hook.on_arena_destruction = &destruction_hook;
    ops_hook.on_arena_reset = &reset_hook;
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    hook_instance = new mock_hook(cookie);
    auto* a = new Arena(ops_hook);
    ArenaTestHelper ah(*a);
    CHECK_EQ(ah.cookie(), cookie);
    auto* r = a->AllocateAligned(30);
    CHECK_NE(r, nullptr);
    CHECK_EQ(mock->alloc_sizes.front(), 4096);
    CHECK_EQ(mock->alloc_sizes.size(), 1);
    CHECK_EQ(hook_instance->allocated, 1);

    r = a->AllocateAligned(60);
    CHECK_NE(r, nullptr);
    CHECK_EQ(hook_instance->allocated, 2);

    auto* rr = a->AllocateAlignedAndAddCleanup(150, mock_cleaners, &cleanup_mock_fn1);
    CHECK_NE(rr, nullptr);
    CHECK_EQ(hook_instance->allocated, 3);

    auto* rrr = a->Create<xx>();
    CHECK_NE(rrr, nullptr);
    CHECK_EQ(hook_instance->allocated, 4);

    auto* rrrr = a->CreateArray<xx>(10);
    CHECK_NE(rrrr, nullptr);
    CHECK_EQ(hook_instance->allocated, 5);

    a->Reset();
    CHECK(mock_cleaners->clean1);
    CHECK_EQ(hook_instance->reseted, 1);

    delete a;
    CHECK_EQ(hook_instance->destructed, 1);
    CHECK_EQ(mock->free_ptrs.size(), 1);
    delete mock;
    delete mock_cleaners;
    delete hook_instance;
    std::free(cookie);
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.NullTest") {
    mock_cleaners = new cleanup_mock;
    mock = new alloc_fail_class;
    cstr = new cstr_class;
    cstr_class::count = 0;

    auto* a = new Arena(ops_complex);
    ArenaTestHelper ah(*a);

    auto* x = ah.newBlock(1024, nullptr);
    CHECK_EQ(x, nullptr);
    CHECK_EQ(ah.space_allocated(), 0ULL);

    char* y = a->AllocateAligned(1000);
    CHECK_EQ(y, nullptr);
    CHECK_EQ(ah.space_allocated(), 0ULL);

    y = a->AllocateAlignedAndAddCleanup(1000, mock_cleaners, cleanup_mock_fn1);
    CHECK_EQ(y, nullptr);
    CHECK_EQ(ah.space_allocated(), 0ULL);
    CHECK_EQ(a->cleanups(), 0ULL);

    std::string ss("hello world");
    std::string sss("fuck the world");
    CHECK_EQ(cstr->count, 0);

    auto* d2 = a->Create<mock_class_without_dstr>("fuck the world");
    CHECK_EQ(a->cleanups(), 0ULL);
    auto* d1 = a->Create<mock_class_need_dstr>(3, ss);
    CHECK_EQ(a->cleanups(), 0ULL);
    CHECK_EQ(d1, nullptr);
    CHECK_EQ(d2, nullptr);
    CHECK_EQ(ah.space_allocated(), 0ULL);

    auto* z = a->CreateArray<mock_struct>(100);
    CHECK_EQ(z, nullptr);
    CHECK_EQ(ah.space_allocated(), 0ULL);
    CHECK_EQ(a->cleanups(), 0ULL);

    delete mock_cleaners;
    delete mock;
    delete cstr;
    delete a;
}

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.MemoryResourceTest") {
    mock = new alloc_class;

    Arena::Options opts = Arena::Options::GetDefaultOptions();
    opts.normal_block_size = 256;
    opts.huge_block_size = 512;
    opts.suggested_init_block_size = 256;
    opts.block_alloc = &mock_alloc;
    opts.block_dealloc = &mock_dealloc;

    auto* arena = new Arena{opts};
    Arena::memory_resource res{arena};

    // char mem[256];

    // get_arena
    CHECK_EQ(arena, res.get_arena());

    // allocate
    // EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem)));
    void* ptr = res.allocate(128);
    char* address = static_cast<char*>(mock->ptrs.front());
    CHECK_EQ(ptr, address + kBlockHeaderSize);
    void* ptr2 = res.allocate(32);
    CHECK_EQ(mock->ptrs.size(), 1);

    // deallocate
    res.deallocate(ptr2, 32);
    res.deallocate(ptr, 128);

    SUBCASE("ptr ==") {
        auto* memory_resource_ptr1 = arena->get_memory_resource();
        auto* memory_resource_ptr2 = arena->get_memory_resource();
        CHECK_NE(memory_resource_ptr1, nullptr);
        CHECK_EQ(memory_resource_ptr1, memory_resource_ptr2);
    }
    // operator==
    SUBCASE("operator ==") {
        Arena::memory_resource& res2 = res;
        CHECK_EQ(res, res2);  // NOLINT

        Arena::memory_resource res3 = *arena->get_memory_resource();
        CHECK_EQ(res, res3);  // NOLINT
    }
    SUBCASE("operator !=") {
        Arena arena2{opts};
        auto res2 = *arena2.get_memory_resource();
        CHECK_NE(res, res2);  // NOLINT

        auto res3 = std::pmr::monotonic_buffer_resource{};
        CHECK_NE(res2, res3);  // NOLINT
    }

    delete arena;
    CHECK_EQ(mock->free_ptrs.size(), 1);
    delete mock;
    mock = nullptr;
}

class Foo
{
    using allocator_type = std::pmr::polymorphic_allocator<>;

   public:
    explicit Foo(allocator_type alloc) : allocator_(alloc), vec_{alloc} {};
    Foo(const Foo& foo, allocator_type alloc) : allocator_(alloc), vec_{foo.vec_, alloc} {};
    Foo(Foo&& foo) noexcept : Foo{std::move(foo), foo.allocator_} {};
    // NOLINTNEXTLINE
    Foo(Foo&& foo, allocator_type alloc) : allocator_(alloc), vec_(alloc) { vec_ = std::move(foo.vec_); }

    allocator_type allocator_;
    std::pmr::vector<int> vec_;
};

TEST_CASE_FIXTURE(ArenaTest, "ArenaTest.AllocatorAwareTest") {
    Arena::Options opts = Arena::Options::GetDefaultOptions();
    opts.normal_block_size = 256;
    opts.huge_block_size = 512;
    opts.suggested_init_block_size = 256;
    opts.block_alloc = &mock_alloc;
    opts.block_dealloc = &mock_dealloc;

    SUBCASE("CTOR") {  // ctor
        // char mem[256];
        mock = new alloc_class;
        auto* arena = new Arena(opts);
        Arena::memory_resource res{arena};
        Foo foo(&res);
        ArenaTestHelper ah(*arena);

        // EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem)));
        // EXPECT_CALL(*mock, dealloc(std::data(mem))).Times(1);

        foo.vec_.resize(2);
        CHECK_EQ(256 - 8 - kBlockHeaderSize, ah.last_block()->remain());
        foo.vec_.resize(1);
        CHECK_EQ(256 - 8 - kBlockHeaderSize, ah.last_block()->remain());
        foo.vec_.resize(4);
        CHECK_EQ(256 - 8 - 16 - kBlockHeaderSize, ah.last_block()->remain());

        delete arena;
        CHECK_EQ(mock->ptrs.size(), 1);
        CHECK_EQ(mock->free_ptrs.size(), 1);
        CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
        CHECK_EQ(mock->alloc_sizes.front(), 256);
        delete mock;
        mock = nullptr;
    }

    SUBCASE("COPY") {  // copy
        // char mem1[256];
        mock = new alloc_class;
        auto* arena1 = new Arena(opts);
        Arena::memory_resource res1{arena1};
        ArenaTestHelper ah(*arena1);
        // EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem1)));
        // EXPECT_CALL(*mock, dealloc(std::data(mem1))).Times(1);

        Foo foo1(&res1);
        foo1.vec_ = {1, 2, 3, 4};
        CHECK_EQ(256 - 16 - kBlockHeaderSize, ah.last_block()->remain());

        // copy with same arena
        Foo foo2(foo1, &res1);
        foo2.vec_[3] = 5;
        CHECK_EQ(256 - 16 - 16 - kBlockHeaderSize, ah.last_block()->remain());
        CHECK_EQ(0, memcmp(foo1.vec_.data(), foo2.vec_.data(), 4));

        // CHECK_EQ(foo1.vec_.data(), reinterpret_cast<int*>(mock->ptrs.front() + kBlockHeaderSize));
        CHECK_EQ(foo1.vec_.data() + 4, foo2.vec_.data());
        delete arena1;
        CHECK_EQ(mock->ptrs.size(), 1);
        CHECK_EQ(mock->free_ptrs.size(), 1);
        CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
        CHECK_EQ(mock->alloc_sizes.front(), 256);

        mock->reset();
        // copy with another arena
        auto* arena2 = new Arena(opts);
        Arena::memory_resource res2{arena2};
        ArenaTestHelper ah2(*arena2);
        // EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem2)));
        // EXPECT_CALL(*mock, dealloc(std::data(mem2))).Times(1);

        Foo foo3(foo2, &res2);
        foo3.vec_[0] = 6;
        CHECK_EQ(256 - 16 - kBlockHeaderSize, ah2.last_block()->remain());
        CHECK_EQ(foo3.vec_[0], 6);
        CHECK_EQ(foo3.vec_[1], 2);
        CHECK_EQ(foo3.vec_[2], 3);
        CHECK_EQ(foo3.vec_[3], 5);
        delete arena2;
        // EXPECT_THAT(foo3.vec_, ElementsAre(6, 2, 3, 5));
        CHECK_EQ(mock->ptrs.size(), 1);
        CHECK_EQ(mock->free_ptrs.size(), 1);
        CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
        CHECK_EQ(mock->alloc_sizes.front(), 256);
        delete mock;
        mock = nullptr;
    }

    SUBCASE("MOVE") {  // move
        // char mem1[256];
        mock = new alloc_class;
        auto* arena1 = new Arena(opts);
        Arena::memory_resource res1{arena1};
        ArenaTestHelper ah(*arena1);

        // move with same arena
        Foo foo1(&res1);
        foo1.vec_ = {1, 2, 3, 4};
        CHECK_EQ(256 - 16 - kBlockHeaderSize, ah.last_block()->remain());

        Foo foo2(std::move(foo1));
        CHECK_EQ(256 - 16 - kBlockHeaderSize, ah.last_block()->remain());
        // NOLINTNEXTLINE
        CHECK_EQ(0, foo1.vec_.size());
        CHECK_EQ(4, foo2.vec_.size());

        Foo foo3(std::move(foo2), &res1);
        CHECK_EQ(256 - 16 - kBlockHeaderSize, ah.last_block()->remain());
        // NOLINTNEXTLINE
        CHECK_EQ(0, foo2.vec_.size());
        CHECK_EQ(4, foo3.vec_.size());
        delete arena1;
        CHECK_EQ(mock->ptrs.size(), 1);
        CHECK_EQ(mock->free_ptrs.size(), 1);
        CHECK_EQ(mock->ptrs.front(), mock->free_ptrs.front());
        CHECK_EQ(mock->alloc_sizes.front(), 256);

        delete mock;
        mock = nullptr;
    }
}

TEST_CASE("ArenaTest.pmr-support") {
    auto options = Arena::Options::GetDefaultOptions();
    Arena arena(options);
    SUBCASE("String") {
        std::pmr::string str("stringstringstring..............213423423443242344", arena.get_memory_resource());
        CHECK_EQ(arena.check(str.c_str()), ArenaContainStatus::BlockUsed);
    }
    SUBCASE("vector") {
        std::pmr::vector<std::pmr::string> strings(arena.get_memory_resource());
        strings.emplace_back(std::pmr::string("123123123_+23423432423agsagasb+234324b1321312bsafs........a2423",
                                              arena.get_memory_resource()));
        CHECK_EQ(strings.size(), 1);
        CHECK_EQ(arena.check(strings.front().c_str()), ArenaContainStatus::BlockUsed);
    }
}

TEST_CASE("ArenaTest.Create-support-pmr") {
    auto options = Arena::Options::GetDefaultOptions();
    Arena arena(options);
    SUBCASE("String") {
        auto* pmr_str = arena.Create<std::pmr::string>("safsdaf_sadgsadf21321300000");
        CHECK_EQ(arena.check(pmr_str->c_str()), ArenaContainStatus::BlockUsed);
    }

    SUBCASE("vector-emplace") {
        auto* strings = arena.Create<std::pmr::vector<std::pmr::string>>();
        strings->emplace_back("123123123_+23423432423agsagasb+234324b1321312bsafs........a2423");
        CHECK_EQ(strings->size(), 1);
        CHECK_EQ(arena.check(strings->front().c_str()), ArenaContainStatus::BlockUsed);
    }

    SUBCASE("vector-push") {
        auto* strings = arena.Create<std::pmr::vector<std::pmr::string>>();
        auto* pmr_str = arena.Create<std::pmr::string>("safsdaf_sadgsadf21321300000");
        strings->push_back(*pmr_str);
        CHECK_EQ(strings->size(), 1);
        CHECK_EQ(arena.check(strings->front().c_str()), ArenaContainStatus::BlockUsed);
        CHECK_NE(pmr_str->c_str(), strings->front().c_str());
    }
}
struct class_with_allocator
{
    using allocator_type = std::pmr::polymorphic_allocator<char>;
    std::pmr::string name;
    explicit class_with_allocator(allocator_type alloc = {}) : name(alloc) {}
};

TEST_CASE("ArenaTest.Create_Object_with_allocator") {
    auto options = Arena::Options::GetDefaultOptions();
    Arena a(options);
    auto* obj = a.Create<class_with_allocator>();
    CHECK_EQ(a.check(obj->name.c_str()), ArenaContainStatus::BlockUsed);
}

struct simple_test_struct {
    int _s;
};

struct simple_test_struct_with_tag {
    int _s;
    ArenaFullManagedTag;
};

TEST_CASE("ArenaTagOverhead") {
    CHECK_EQ(sizeof(simple_test_struct), 4);
    CHECK_EQ(sizeof(simple_test_struct_with_tag), 4);
}


}  // namespace stdb::memory
