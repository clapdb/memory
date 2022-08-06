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
#include <iostream>
#include <typeinfo>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using stdb::memory::Arena;
using stdb::memory::kBlockHeaderSize;
using stdb::memory::align::AlignUp;
using stdb::memory::align::AlignUpTo;
namespace stdb::memory {

using ::testing::ElementsAre;

class alloc_class
{
   public:
    MOCK_METHOD1(alloc, void*(uint64_t));
    MOCK_METHOD1(dealloc, void(void*));
};

class BlockTest : public ::testing::Test
{
   protected:
    // NOLINTNEXTLINE
    void SetUp() override { ptr = malloc(1024); };
    // NOLINTNEXTLINE
    void TearDown() override { free(ptr); };
    auto Pointer() -> void* { return ptr; }

   private:
    void* ptr{nullptr};
};

class cleanup_mock
{
   public:
    MOCK_METHOD1(cleanup1, void(void*));
    MOCK_METHOD1(cleanup2, void(void*));
    MOCK_METHOD1(cleanup3, void(void*));
};

void cleanup_mock_fn1(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup1(p); }

void cleanup_mock_fn2(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup2(p); }

void cleanup_mock_fn3(void* p) { reinterpret_cast<cleanup_mock*>(p)->cleanup3(p); }

cleanup_mock* mock_cleaners;

TEST_F(BlockTest, CotrTest1) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    ASSERT_EQ(b->Pos(), reinterpret_cast<char*>(Pointer()) + kBlockHeaderSize);
    ASSERT_EQ(b->size(), 1024ULL);
    ASSERT_EQ(b->prev(), nullptr);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
}

TEST_F(BlockTest, CotrTest2) {
    // NOLINTNEXTLINE
    void* last = malloc(100);
    auto* last_b = reinterpret_cast<Arena::Block*>(last);
    auto* b = new (Pointer()) Arena::Block(1024, last_b);
    ASSERT_EQ(b->prev(), last_b);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    // NOLINTNEXTLINE
    free(last);
}

TEST_F(BlockTest, AllocTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc(200);
    ASSERT_NE(x, nullptr);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize - 200ULL);
    ASSERT_EQ(b->Pos() - x, 200LL);
}

TEST_F(BlockTest, AllocCleanupTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc_cleanup();
    ASSERT_NE(x, nullptr);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize);
    char* x1 = b->alloc_cleanup();
    ASSERT_EQ(x - x1, kCleanupNodeSize);
}

TEST_F(BlockTest, RegCleanupTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize);
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    b->run_cleanups();
    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST_F(BlockTest, RunCleanupTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    b->register_cleanup(mock_cleaners, &cleanup_mock_fn2);
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    EXPECT_CALL(*mock_cleaners, cleanup2(mock_cleaners)).Times(1);
    b->run_cleanups();
    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST_F(BlockTest, ResetTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    char* x = b->alloc(200);
    ASSERT_NE(x, nullptr);
    ASSERT_EQ(b->remain(), 1024ULL - 200ULL - kBlockHeaderSize);
    ASSERT_EQ(b->Pos() - x, 200LL);
    b->Reset();
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    ASSERT_EQ(b->Pos() - x, 0LL);
}

TEST_F(BlockTest, ResetwithCleanupTest) {
    auto* b = new (Pointer()) Arena::Block(1024, nullptr);
    mock_cleaners = new cleanup_mock;
    char* x = b->alloc(200);
    ASSERT_NE(x, nullptr);
    ASSERT_EQ(b->remain(), 1024ULL - 200ULL - kBlockHeaderSize);
    ASSERT_EQ(b->Pos() - x, 200LL);

    b->register_cleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize - kCleanupNodeSize - 200);
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);

    b->Reset();
    ASSERT_EQ(b->remain(), 1024ULL - kBlockHeaderSize);
    ASSERT_EQ(b->Pos() - x, 0LL);

    delete mock_cleaners;
    mock_cleaners = nullptr;
}

TEST(AlignTest, AlignUpToTest8) {
    ASSERT_EQ(AlignUpTo<8>(15), 16ULL);
    ASSERT_EQ(AlignUpTo<8>(1), 8ULL);
    ASSERT_EQ(AlignUpTo<8>(32), 32ULL);
    ASSERT_EQ(AlignUpTo<8>(255), 256ULL);
    ASSERT_EQ(AlignUpTo<8>(1024), 1024ULL);
}

TEST(AlignTest, AlignUpToTest16) {
    ASSERT_EQ(AlignUpTo<16>(15), 16ULL);
    ASSERT_EQ(AlignUpTo<16>(1), 16ULL);
    ASSERT_EQ(AlignUpTo<16>(32), 32ULL);
    ASSERT_EQ(AlignUpTo<16>(255), 256ULL);
    ASSERT_EQ(AlignUpTo<16>(1024), 1024ULL);
}

TEST(AlignTest, AlignUpToTest4) {
    ASSERT_EQ(AlignUpTo<4>(15), 16ULL);
    ASSERT_EQ(AlignUpTo<4>(1), 4ULL);
    ASSERT_EQ(AlignUpTo<4>(32), 32ULL);
    ASSERT_EQ(AlignUpTo<4>(255), 256ULL);
    ASSERT_EQ(AlignUpTo<4>(1024), 1024ULL);
}

// test without on_arena_* trigger
TEST(AlignTest, AlignUpTest) {
    ASSERT_EQ(AlignUp(63 + 2048, 1024), 3072ULL);
    ASSERT_EQ(AlignUp(2048, 1024), 2048ULL);
}

class ArenaTest : public ::testing::Test
{
   protected:
    Arena::Options ops_complex;
    Arena::Options ops_simple;

    static alloc_class* mock;

    static auto mock_alloc(uint64_t size) -> void* { return mock->alloc(size); }

    static void mock_dealloc(void* ptr) { return mock->dealloc(ptr); }

    void SetUp() override {
        // initialize the ops_complex
        ops_complex.block_alloc = &mock_alloc;
        ops_complex.block_dealloc = &mock_dealloc;
        ops_complex.normal_block_size = 1024ULL;
        ops_complex.suggested_initblock_size = 4096ULL;
        ops_complex.huge_block_size = 1024ULL * 1024ULL;

        // initialize the ops_simple
        ops_simple.block_alloc = &mock_alloc;
        ops_simple.block_dealloc = &mock_dealloc;
        ops_simple.normal_block_size = 1024ULL;
        ops_simple.suggested_initblock_size = 0ULL;
        ops_simple.huge_block_size = 0ULL;
    }

    void TearDown() override {}
};

alloc_class* ArenaTest::mock = nullptr;

using ::testing::Return;

TEST_F(ArenaTest, CtorTest) {
    Arena a(ops_complex);
    ASSERT_EQ(a._last_block, nullptr);
    ASSERT_EQ(a._options.normal_block_size, 1024ULL);
    ASSERT_EQ(a._options.block_alloc, &mock_alloc);
    ASSERT_EQ(a._options.block_dealloc, &mock_dealloc);
    Arena b(ops_simple);
    ASSERT_EQ(b._options.normal_block_size, 1024ULL);
    ASSERT_EQ(b._options.suggested_initblock_size, 1024ULL);
    ASSERT_EQ(b._options.huge_block_size, 1024ULL);
    ASSERT_EQ(b._last_block, nullptr);
    ASSERT_EQ(b._options.block_alloc, &mock_alloc);
    ASSERT_EQ(b._options.block_dealloc, &mock_dealloc);
    Arena::Options o;
    o.block_alloc = mock_alloc;
    o.block_dealloc = mock_dealloc;
    Arena c(o);
    ASSERT_EQ(c._options.normal_block_size, 4096ULL);
    ASSERT_EQ(c._options.suggested_initblock_size, 4096ULL);
    ASSERT_EQ(c._options.huge_block_size, 2ULL * 1024ULL * 1024ULL);
    ASSERT_EQ(c._options.block_alloc, &mock_alloc);
    ASSERT_EQ(c._options.block_dealloc, &mock_dealloc);
}

TEST_F(ArenaTest, NewBlockTest) {
    // the case bearing the a and b is leaking.
    // because I just want to test the NewBlock
    auto* a = new Arena(ops_simple);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x = std::malloc(1024);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x));
    Arena::Block* bb = a->newBlock(100, nullptr);
    ASSERT_EQ(bb, x);
    ASSERT_EQ(a->_space_allocated, 1024ULL);
    ASSERT_EQ(bb->remain(), 1024 - kBlockHeaderSize);

    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x));
    auto* b = new Arena(ops_complex);
    // will ignore the init block from building cleanups
    // this situation will never occurs in the real world.
    auto* xx = b->newBlock(200, nullptr);
    ASSERT_EQ(xx, x);

    EXPECT_CALL(*mock, alloc(3072)).WillOnce(Return(x));
    bb = b->newBlock(2500, bb);

    EXPECT_CALL(*mock, alloc(1024 * 1024)).WillOnce(Return(x));
    bb = b->newBlock(300 * 1024 + 100, bb);

    EXPECT_CALL(*mock, alloc(2 * 1024 * 1024 + kBlockHeaderSize)).WillOnce(Return(x));
    bb = b->newBlock(2 * 1024 * 1024, bb);
    EXPECT_CALL(*mock, alloc(2 * 1024 * 1024 + kBlockHeaderSize)).WillOnce(Return(x));
    bb = b->newBlock(2 * 1024 * 1024, static_cast<Arena::Block*>(x));
    ASSERT_EQ(bb, x);
    ASSERT_EQ(bb->prev(), x);
    // cleanup
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x);
    delete mock;
    mock = nullptr;
    delete a;
    delete b;
}

TEST_F(ArenaTest, AllocateTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* m = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* mm = std::malloc(1024);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(m));
    auto* x = new Arena(ops_complex);
    auto* new_ptr = x->AllocateAligned(3500);
    ASSERT_EQ(new_ptr, static_cast<char*>(m) + sizeof(Arena::Block));

    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(mm));
    auto* next_ptr = x->AllocateAligned(755);

    EXPECT_CALL(*mock, dealloc(m)).Times(1);
    EXPECT_CALL(*mock, dealloc(mm)).Times(1);
    ASSERT_EQ(next_ptr, static_cast<char*>(mm) + sizeof(Arena::Block));
    delete x;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(m);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mm);
    delete mock;
    mock = nullptr;
}

TEST_F(ArenaTest, AddCleanupTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);
    auto* a = new Arena(ops_complex);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);
    mock_cleaners = new cleanup_mock;
    bool ok = a->addCleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_TRUE(ok);
    EXPECT_EQ(1, a->cleanups());

    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* obj = malloc(120);  // will be free automatically
    bool ok2 = a->addCleanup(obj, &std::free);
    ASSERT_TRUE(ok2);
    ASSERT_EQ(2, a->cleanups());
    ASSERT_EQ(a->_last_block->remain(), a->_last_block->size() - kBlockHeaderSize - kCleanupNodeSize * 2);

    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    delete a;
    delete mock;
    delete mock_cleaners;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
}

TEST_F(ArenaTest, AddCleanupFailTest) {
    auto* a = new Arena(ops_complex);
    mock = new alloc_class;
    // this line means malloc failed, so no space for cleanup Node.
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(nullptr);

    mock_cleaners = new cleanup_mock;
    bool ok = a->addCleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_FALSE(ok);

    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(0);
    delete a;
    delete mock;
    delete mock_cleaners;
}

TEST_F(ArenaTest, FreeBlocksExceptFirstTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x1 = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x2 = std::malloc(2048);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);

    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x1));
    EXPECT_CALL(*mock, alloc(2048)).WillOnce(Return(x2));
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));

    a->_last_block = a->newBlock(1024 - kBlockHeaderSize, nullptr);
    a->_last_block = a->newBlock(2048 - kBlockHeaderSize, a->_last_block);
    a->_last_block = a->newBlock(4096 - kBlockHeaderSize, a->_last_block);
    EXPECT_EQ(a->_last_block, x3);

    // FreeBlocks should not be call out of the class, just use ~Arena
    EXPECT_CALL(*mock, dealloc(x1)).Times(0);
    EXPECT_CALL(*mock, dealloc(x2)).Times(1);
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);

    auto wasted = a->free_blocks_except_head();
    EXPECT_EQ(wasted, 1024 * 7 - 3 * kBlockHeaderSize);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    a->_last_block = nullptr;
    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x1);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x2);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
    delete mock;
}

TEST_F(ArenaTest, ResetTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x1 = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x2 = std::malloc(2048);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);

    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x1));
    EXPECT_CALL(*mock, alloc(2048)).WillOnce(Return(x2));
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));

    a->_last_block = a->newBlock(1024 - kBlockHeaderSize, nullptr);
    a->_last_block = a->newBlock(2048 - kBlockHeaderSize, a->_last_block);
    a->_last_block = a->newBlock(4096 - kBlockHeaderSize, a->_last_block);

    EXPECT_CALL(*mock, dealloc(x1)).Times(0);
    EXPECT_CALL(*mock, dealloc(x2)).Times(1);
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);

    a->Reset();

    ASSERT_EQ(a->_last_block, x1);
    ASSERT_EQ(a->_space_allocated, 1024);
    ASSERT_EQ(a->_last_block->remain(), 1024 - kBlockHeaderSize);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    a->_last_block = nullptr;
    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x1);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x2);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
    delete mock;
}

TEST_F(ArenaTest, ResetWithCleanupTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x1 = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x2 = std::malloc(2048);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);

    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x1));
    EXPECT_CALL(*mock, alloc(2048)).WillOnce(Return(x2));
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));

    a->_last_block = a->newBlock(1024 - kBlockHeaderSize, nullptr);
    a->_last_block = a->newBlock(2048 - kBlockHeaderSize, a->_last_block);
    a->_last_block = a->newBlock(4096 - kBlockHeaderSize, a->_last_block);

    EXPECT_CALL(*mock, dealloc(x1)).Times(0);
    EXPECT_CALL(*mock, dealloc(x2)).Times(1);
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);
    bool ok = a->addCleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_TRUE(ok);
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);

    a->Reset();

    ASSERT_EQ(a->_last_block, x1);
    ASSERT_EQ(a->_space_allocated, 1024);
    ASSERT_EQ(a->_last_block->remain(), 1024 - kBlockHeaderSize);

    // set last_block_ to nullptr, to avoid the dealloc for this block.
    a->_last_block = nullptr;
    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x1);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x2);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
    delete mock_cleaners;
    delete mock;
}

TEST_F(ArenaTest, FreeBlocksTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x1 = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x2 = std::malloc(2048);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);

    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x1));
    EXPECT_CALL(*mock, alloc(2048)).WillOnce(Return(x2));
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));

    a->_last_block = a->newBlock(1024 - kBlockHeaderSize, nullptr);
    a->_last_block = a->newBlock(2048 - kBlockHeaderSize, a->_last_block);
    a->_last_block = a->newBlock(4096 - kBlockHeaderSize, a->_last_block);

    EXPECT_CALL(*mock, dealloc(x1)).Times(1);
    EXPECT_CALL(*mock, dealloc(x2)).Times(1);
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);
    auto wasted = a->free_all_blocks();
    ASSERT_EQ(wasted, 7 * 1024 - 3 * kBlockHeaderSize);

    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x1);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x2);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
    delete mock;
}

TEST_F(ArenaTest, DoCleanupTest) {
    auto* a = new Arena(ops_complex);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x = std::malloc(4096);
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x));
    EXPECT_CALL(*mock, dealloc(x)).Times(1);
    bool ok = a->addCleanup(mock_cleaners, &cleanup_mock_fn1);
    ASSERT_TRUE(ok);
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);

    delete a;
    delete mock_cleaners;
    delete mock;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    free(x);
}

class mock_own
{
   public:
    MOCK_METHOD0(dealloc, void());
    ~mock_own() { dealloc(); }
    int x_{0};
};

TEST_F(ArenaTest, OwnTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x = std::malloc(4096);
    auto* m = new mock_own();
    auto* a = new Arena(ops_complex);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x));
    EXPECT_CALL(*mock, dealloc(x)).Times(1);
    bool ok = a->Own<mock_own>(m);
    ASSERT_TRUE(ok);
    EXPECT_CALL(*m, dealloc()).Times(1);
    delete a;
    // EXPECT_CALL(*m, dealloc()).Times(1);
    // delete m;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x);
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
    MOCK_METHOD1(construct, void(const std::string&));
    MOCK_METHOD1(destruct, void(const std::string&));
};

cstr_class* cstr;

class mock_class_need_dstr
{
   public:
    ACstrTag;
    mock_class_need_dstr(int i, STring name) : index_(i), n_(std::move(name)) { cstr->construct(n_); }
    ~mock_class_need_dstr() { cstr->destruct(n_); }
    auto verify(int i, const STring& name) -> bool { return (i == index_) && (name == n_); }

   private:
    int index_;
    const STring n_;
};

class mock_class_without_dstr
{
   public:
    ACstrTag;
    ADstrSkipTag;
    MOCK_METHOD0(construct, void());
    MOCK_METHOD0(destruct, void());
    explicit mock_class_without_dstr(STring name) : n_(std::move(name)) { cstr->construct(n_); }
    ~mock_class_without_dstr() { cstr->destruct(n_); }
    auto verify(const STring& n) -> bool { return n_ == n; }

   private:
    const STring n_;
};

class mock_class_with_arena
{
   public:
    ACstrTag;
    // explicit mock_class_with_arena(Arena* arena) : arena_(arena) {}
    mock_class_with_arena() = default;

   private:
    // Arena* arena_{nullptr};
};

TEST_F(ArenaTest, CreateTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x = std::malloc(4096);
    mock = new alloc_class;
    cstr = new cstr_class;
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x));
    EXPECT_CALL(*cstr, construct("hello world")).Times(1);
    EXPECT_CALL(*cstr, construct("fuck the world")).Times(1);
    auto* a = new Arena(ops_complex);
    STring ss("hello world");
    STring sss("fuck the world");

    auto* d2 = a->Create<mock_class_without_dstr>("fuck the world");
    ASSERT_EQ(0, a->cleanups());

    auto* d1 = a->Create<mock_class_need_dstr>(3, ss);
    ASSERT_EQ(1, a->cleanups());
    ASSERT_TRUE(d1->verify(3, ss));
    ASSERT_TRUE(d2->verify(sss));

    EXPECT_CALL(*cstr, destruct("hello world")).Times(1);
    EXPECT_CALL(*cstr, destruct("fuck the world")).Times(0);
    EXPECT_CALL(*mock, dealloc(x)).Times(1);
    auto r1 = a->_last_block->remain();
    auto* r = a->Create<mock_struct>();
    ASSERT_TRUE(r != nullptr);
    ASSERT_EQ(1, a->cleanups());  // mock_struct will not be register to cleanup
    auto r2 = a->_last_block->remain();
    ASSERT_EQ(r1 - r2, sizeof(mock_struct));

    // auto pass Arena*
    (void)a->Create<mock_class_with_arena>();

    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x);
    delete mock;
    delete cstr;
}

TEST_F(ArenaTest, CreateArrayTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x = std::malloc(4096);
    mock = new alloc_class;
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x));
    auto* a = new Arena(ops_complex);

    auto* r = a->CreateArray<uint64_t>(10);
    ASSERT_TRUE(r != nullptr);
    uint64_t s2 = a->_last_block->remain();
    ASSERT_EQ(s2, 4096 - kBlockHeaderSize - 10 * sizeof(uint64_t));

    struct test_struct
    {
        int i;
        char n;
        double d;
    };

    auto* r1 = a->CreateArray<test_struct>(10);
    ASSERT_TRUE(r1 != nullptr);
    uint64_t s3 = a->_last_block->remain();

    ASSERT_EQ(s3, 4096ULL - kBlockHeaderSize - 10ULL * sizeof(uint64_t) - 10ULL * sizeof(test_struct));
    EXPECT_CALL(*mock, dealloc(x)).Times(1);

    // auto pass Arena*
    (void)a->CreateArray<mock_class_with_arena>(10);

    delete a;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x);
    delete mock;
}

TEST_F(ArenaTest, DstrTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x1 = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x2 = std::malloc(2048);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* x3 = std::malloc(4096);

    auto* a = new Arena(ops_simple);
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(x1));
    EXPECT_CALL(*mock, alloc(2048)).WillOnce(Return(x2));
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(x3));
    a->_last_block = a->newBlock(1024 - kBlockHeaderSize, nullptr);
    a->_last_block = a->newBlock(2048 - kBlockHeaderSize, a->_last_block);
    a->_last_block = a->newBlock(4096 - kBlockHeaderSize, a->_last_block);

    EXPECT_CALL(*mock, dealloc(x1)).Times(1);
    EXPECT_CALL(*mock, dealloc(x2)).Times(1);
    EXPECT_CALL(*mock, dealloc(x3)).Times(1);

    bool ok1 = a->addCleanup(mock_cleaners, &cleanup_mock_fn1);
    bool ok2 = a->addCleanup(mock_cleaners, &cleanup_mock_fn2);
    bool ok3 = a->addCleanup(mock_cleaners, &cleanup_mock_fn3);
    ASSERT_TRUE(ok1);
    ASSERT_TRUE(ok2);
    ASSERT_TRUE(ok3);

    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    EXPECT_CALL(*mock_cleaners, cleanup2(mock_cleaners)).Times(1);
    EXPECT_CALL(*mock_cleaners, cleanup3(mock_cleaners)).Times(1);
    delete a;
    delete mock;
    delete mock_cleaners;

    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x1);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x2);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(x3);
}

TEST_F(ArenaTest, SpaceTest) {
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* m = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* mm = std::malloc(1024);
    auto* x = new Arena(ops_complex);
    mock = new alloc_class;
    ASSERT_EQ(x->SpaceAllocated(), 0ULL);

    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(m));
    auto* new_ptr = x->AllocateAligned(3500);
    ASSERT_EQ(new_ptr, static_cast<char*>(m) + sizeof(Arena::Block));

    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(mm));
    auto* next_ptr = x->AllocateAligned(755);
    ASSERT_EQ(next_ptr, static_cast<char*>(mm) + sizeof(Arena::Block));

    ASSERT_EQ(x->SpaceAllocated(), 5120ULL);
    ASSERT_EQ(x->_last_block, mm);
    EXPECT_CALL(*mock, dealloc(m)).Times(1);
    EXPECT_CALL(*mock, dealloc(mm)).Times(1);

    delete x;
    delete mock;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(m);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mm);
}

TEST_F(ArenaTest, AllocateAlignedAndAddCleanupTest) {
    auto* a = new Arena(ops_complex);
    mock_cleaners = new cleanup_mock;
    mock = new alloc_class;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* m = std::malloc(4096);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* mm = std::malloc(1024);
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(m));
    auto* new_ptr = a->AllocateAlignedAndAddCleanup(3500, mock_cleaners, cleanup_mock_fn1);
    ASSERT_EQ(new_ptr, static_cast<char*>(m) + sizeof(Arena::Block));

    EXPECT_CALL(*mock, alloc(1024)).WillOnce(Return(mm));
    auto* next_ptr = a->AllocateAlignedAndAddCleanup(755, mock_cleaners, cleanup_mock_fn2);

    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    EXPECT_CALL(*mock_cleaners, cleanup2(mock_cleaners)).Times(1);

    EXPECT_CALL(*mock, dealloc(m)).Times(1);
    EXPECT_CALL(*mock, dealloc(mm)).Times(1);
    ASSERT_EQ(next_ptr, static_cast<char*>(mm) + sizeof(Arena::Block));
    delete a;
    delete mock;
    delete mock_cleaners;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(m);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mm);
}

class mock_hook
{
   public:
    MOCK_METHOD1(arena_init_hook, void*(Arena*));
    MOCK_METHOD3(arena_allocate_hook, void(const std::type_info*, uint64_t, void*));
    MOCK_METHOD4(arena_destruction_hook, void*(Arena*, void*, uint64_t, uint64_t));
    MOCK_METHOD4(arena_reset_hook, void(Arena*, void*, uint64_t, uint64_t));
};

mock_hook* hook_instance;

auto init_hook(Arena* a, [[maybe_unused]] const source_location& loc = source_location::current()) -> void* {
    return hook_instance->arena_init_hook(a);
}

void allocate_hook(const std::type_info* t, uint64_t s, void* c) { return hook_instance->arena_allocate_hook(t, s, c); }

auto destruction_hook(Arena* a, void* c, uint64_t s, uint64_t w) -> void* {
    return hook_instance->arena_destruction_hook(a, c, s, w);
}

void reset_hook(Arena* a, void* cookie, uint64_t space_used, uint64_t space_wasted) {
    return hook_instance->arena_reset_hook(a, cookie, space_used, space_wasted);
}

TEST_F(ArenaTest, HookTest) {
    struct xx
    {
        int i;
        char y;
        double d;
    };

    // std::function<void()> c1 = [] { mock_cleaners->cleanup1(); };
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* mem = std::malloc(4096);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* cookie = std::malloc(128);
    Arena::Options ops_hook(ops_complex);
    ops_hook.on_arena_init = &init_hook;
    ops_hook.on_arena_allocation = &allocate_hook;
    ops_hook.on_arena_destruction = &destruction_hook;
    ops_hook.on_arena_reset = &reset_hook;
    mock = new alloc_class;
    mock_cleaners = new cleanup_mock;
    hook_instance = new mock_hook;
    auto* a = new Arena(ops_hook);
    EXPECT_CALL(*hook_instance, arena_init_hook(a)).WillOnce(Return(cookie));
    a->init();
    ASSERT_EQ(a->_cookie, cookie);
    EXPECT_CALL(*mock, alloc(4096)).WillOnce(Return(mem));
    EXPECT_CALL(*hook_instance, arena_allocate_hook(nullptr, 30, cookie));
    auto* r = a->AllocateAligned(30);
    ASSERT_TRUE(r != nullptr);

    EXPECT_CALL(*hook_instance, arena_allocate_hook(nullptr, 60, cookie));
    r = a->AllocateAligned(60);
    ASSERT_TRUE(r != nullptr);

    EXPECT_CALL(*hook_instance, arena_allocate_hook(nullptr, 150, cookie));
    auto* rr = a->AllocateAlignedAndAddCleanup(150, mock_cleaners, &cleanup_mock_fn1);
    ASSERT_TRUE(rr != nullptr);

    EXPECT_CALL(*hook_instance, arena_allocate_hook(&typeid(xx), sizeof(xx), cookie));
    auto* rrr = a->Create<xx>();
    ASSERT_TRUE(rrr != nullptr);

    EXPECT_CALL(*hook_instance, arena_allocate_hook(&typeid(xx), sizeof(xx) * 10, cookie));
    auto* rrrr = a->CreateArray<xx>(10);
    ASSERT_TRUE(rrrr != nullptr);

    EXPECT_CALL(*hook_instance, arena_reset_hook(a, cookie, a->SpaceAllocated(), a->SpaceRemains()));
    EXPECT_CALL(*mock_cleaners, cleanup1(mock_cleaners)).Times(1);
    a->Reset();

    EXPECT_CALL(*hook_instance, arena_destruction_hook(a, cookie, a->SpaceAllocated(), a->SpaceRemains()))
      .WillOnce(Return(cookie));
    EXPECT_CALL(*mock, dealloc(mem)).Times(1);
    // EXPECT_CALL(*mock_cleaners, cleanup1()).Times(1);
    delete a;
    delete mock;
    delete mock_cleaners;
    delete hook_instance;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mem);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(cookie);
}

TEST_F(ArenaTest, NullTest) {
    mock_cleaners = new cleanup_mock;
    mock = new alloc_class;
    cstr = new cstr_class;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* m = std::malloc(4096);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    void* mm = std::malloc(1024);

    EXPECT_CALL(*mock, alloc(4096)).Times(6).WillRepeatedly(Return(nullptr));

    auto* a = new Arena(ops_complex);

    auto* x = a->newBlock(1024, nullptr);
    ASSERT_EQ(x, nullptr);
    ASSERT_EQ(a->_space_allocated, 0ULL);

    char* y = a->AllocateAligned(1000);
    ASSERT_EQ(y, nullptr);
    ASSERT_EQ(a->_space_allocated, 0ULL);

    y = a->AllocateAlignedAndAddCleanup(1000, mock_cleaners, cleanup_mock_fn1);
    ASSERT_EQ(y, nullptr);
    ASSERT_EQ(a->_space_allocated, 0ULL);
    ASSERT_EQ(a->cleanups(), 0ULL);

    EXPECT_CALL(*cstr, construct("hello world")).Times(0);
    EXPECT_CALL(*cstr, construct("fuck the world")).Times(0);
    STring ss("hello world");
    STring sss("fuck the world");

    auto* d2 = a->Create<mock_class_without_dstr>("fuck the world");
    ASSERT_EQ(a->cleanups(), 0ULL);
    auto* d1 = a->Create<mock_class_need_dstr>(3, ss);
    ASSERT_EQ(a->cleanups(), 0ULL);
    ASSERT_EQ(d1, nullptr);
    ASSERT_EQ(d2, nullptr);
    ASSERT_EQ(a->_space_allocated, 0ULL);

    auto* z = a->CreateArray<mock_struct>(100);
    ASSERT_EQ(z, nullptr);
    ASSERT_EQ(a->_space_allocated, 0ULL);
    ASSERT_EQ(a->cleanups(), 0ULL);

    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(m);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    std::free(mm);
    delete mock_cleaners;
    delete mock;
    delete cstr;
    delete a;
}

TEST_F(ArenaTest, MemoryResourceTest) {
    mock = new alloc_class{};

    Arena::Options opts;
    opts.normal_block_size = 256;
    opts.huge_block_size = 512;
    opts.suggested_initblock_size = 256;
    opts.block_alloc = &mock_alloc;
    opts.block_dealloc = &mock_dealloc;

    auto* arena = new Arena{opts};
    Arena::memory_resource res{arena};

    char mem[256];

    // get_arena
    EXPECT_EQ(arena, res.get_arena());

    // allocate
    EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem)));
    void* ptr = res.allocate(100);
    ASSERT_EQ(ptr, std::data(mem) + kBlockHeaderSize);
    void* ptr2 = res.allocate(30);

    // deallocate
    EXPECT_CALL(*mock, dealloc(std::data(mem))).Times(1);
    res.deallocate(ptr2, 30);
    res.deallocate(ptr, 100);

    // operator==
    {
        Arena::memory_resource& res2 = res;
        EXPECT_EQ(res, res2);  // NOLINT

        Arena::memory_resource res3 = arena->get_memory_resource();
        EXPECT_EQ(res, res3);  // NOLINT
    }
    {
        Arena arena2{opts};
        auto res2 = arena2.get_memory_resource();
        EXPECT_NE(res, res2);  // NOLINT

        auto res3 = std::pmr::monotonic_buffer_resource{};
        EXPECT_NE(res2, res3);  // NOLINT
    }

    delete arena;
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

TEST_F(ArenaTest, AllocatorAwareTest) {
    mock = new alloc_class{};

    Arena::Options opts;
    opts.normal_block_size = 256;
    opts.huge_block_size = 512;
    opts.suggested_initblock_size = 256;
    opts.block_alloc = &mock_alloc;
    opts.block_dealloc = &mock_dealloc;

    {  // ctor
        char mem[256];
        Arena arena{opts};
        Arena::memory_resource res{&arena};
        Foo foo(&res);

        EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem)));
        EXPECT_CALL(*mock, dealloc(std::data(mem))).Times(1);

        foo.vec_.resize(2);
        EXPECT_EQ(256 - 8 - kBlockHeaderSize, arena._last_block->remain());
        foo.vec_.resize(1);
        EXPECT_EQ(256 - 8 - kBlockHeaderSize, arena._last_block->remain());
        foo.vec_.resize(4);
        EXPECT_EQ(256 - 8 - 16 - kBlockHeaderSize, arena._last_block->remain());
    }

    {  // copy
        char mem1[256];
        Arena arena1{opts};
        Arena::memory_resource res1{&arena1};
        EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem1)));
        EXPECT_CALL(*mock, dealloc(std::data(mem1))).Times(1);

        Foo foo1(&res1);
        foo1.vec_ = {1, 2, 3, 4};
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena1._last_block->remain());

        // copy with same arena
        Foo foo2(foo1, &res1);
        foo2.vec_[3] = 5;
        EXPECT_EQ(256 - 16 - 16 - kBlockHeaderSize, arena1._last_block->remain());
        EXPECT_EQ(0, memcmp(foo1.vec_.data(), foo2.vec_.data(), 4));
        EXPECT_EQ(foo1.vec_.data(), reinterpret_cast<int*>(std::data(mem1) + kBlockHeaderSize));
        EXPECT_EQ(foo1.vec_.data() + 4, foo2.vec_.data());

        // copy with another arena
        char mem2[256];
        Arena arena2{opts};
        Arena::memory_resource res2{&arena2};
        EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem2)));
        EXPECT_CALL(*mock, dealloc(std::data(mem2))).Times(1);

        Foo foo3(foo2, &res2);
        foo3.vec_[0] = 6;
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena2._last_block->remain());
        EXPECT_THAT(foo3.vec_, ElementsAre(6, 2, 3, 5));
    }

    {  // move
        char mem1[256];
        Arena arena1{opts};
        Arena::memory_resource res1{&arena1};
        EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem1)));
        EXPECT_CALL(*mock, dealloc(std::data(mem1))).Times(1);

        // move with same arena
        Foo foo1(&res1);
        foo1.vec_ = {1, 2, 3, 4};
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena1._last_block->remain());

        Foo foo2(std::move(foo1));
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena1._last_block->remain());
        // NOLINTNEXTLINE
        EXPECT_EQ(0, foo1.vec_.size());
        EXPECT_EQ(4, foo2.vec_.size());

        Foo foo3(std::move(foo2), &res1);
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena1._last_block->remain());
        // NOLINTNEXTLINE
        EXPECT_EQ(0, foo2.vec_.size());
        EXPECT_EQ(4, foo3.vec_.size());

        // move with another arena
        char mem2[256];
        Arena arena2{opts};
        Arena::memory_resource res2{&arena2};
        EXPECT_CALL(*mock, alloc(256)).WillOnce(Return(std::data(mem2)));
        EXPECT_CALL(*mock, dealloc(std::data(mem2))).Times(1);

        Foo foo4(std::move(foo3), &res2);
        EXPECT_EQ(256 - 16 - kBlockHeaderSize, arena2._last_block->remain());
        // NOLINTNEXTLINE
        EXPECT_EQ(0, foo3.vec_.size());
        EXPECT_EQ(4, foo4.vec_.size());
    }

    delete mock;
    mock = nullptr;
}

}  // namespace stdb::memory
