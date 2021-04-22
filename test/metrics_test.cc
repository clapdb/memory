#include "metrics.hpp"

#include <thread>

#include "arena.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using stdb::memory::Arena;
namespace stdb {
namespace memory {

class alloc_class
{
 public:
  MOCK_METHOD1(alloc, void*(uint64_t));
  MOCK_METHOD1(dealloc, void(void*));
  ~alloc_class() {}
};

class ThreadLocalArenaMetrics_Test : public ::testing::Test
{
 protected:
  Arena::Options ops;
  static void* alloc(uint64_t size) { return std::malloc(size); }
  static void dealloc(void* ptr) { std::free(ptr); }

  void SetUp() override {
    ops.block_alloc = &alloc;
    ops.block_dealloc = &dealloc;
    ops.normal_block_size = 1024ULL;
    ops.suggested_initblock_size = 0ULL;
    ops.huge_block_size = 0ULL;

    ops.on_arena_init = &metrics_probe_on_arena_init;
    ops.on_arena_reset = &metrics_probe_on_arena_reset;
    ops.on_arena_allocation = &metrics_probe_on_arena_allocation;
    ops.on_arena_newblock = &metrics_probe_on_arena_newblock;
    ops.on_arena_destruction = &metrics_probe_on_arena_destruction;
  };

  void TearDown() override {
    local_arena_metrics.reset();
    global_arena_metrics.reset();
  };
};

TEST_F(ThreadLocalArenaMetrics_Test, Init) {
  Arena* a = new Arena(ops);
  a->init();
  delete a;
  auto& m = local_arena_metrics;
  ASSERT_EQ(m.init_count, 1);
}

TEST_F(ThreadLocalArenaMetrics_Test, Rest) {
  Arena* a = new Arena(ops);
  a->init();
  auto p0 = a->AllocateAligned(124);
  a->Reset();
  delete a;
  auto& m = local_arena_metrics;
  ASSERT_EQ(m.reset_count, 1);
}

TEST_F(ThreadLocalArenaMetrics_Test, Allocation) {
  {
    Arena* a = new Arena(ops);
    a->init();
    auto p0 = a->AllocateAligned(10);
    delete a;
    auto& m = local_arena_metrics;
    ASSERT_EQ(m.alloc_count, 1);
    ASSERT_EQ(m.space_allocated, 10);
  }

  {
    Arena* a = new Arena(ops);
    a->init();
    auto p0 = a->AllocateAligned(100);
    delete a;
    auto& m = local_arena_metrics;
    ASSERT_EQ(m.alloc_count, 2);
    ASSERT_EQ(m.alloc_size_bucket_counter[0], 1);
    ASSERT_EQ(m.alloc_size_bucket_counter[1], 1);
    ASSERT_EQ(m.space_allocated, 110);
  }
}

TEST_F(ThreadLocalArenaMetrics_Test, NewBlock) {
  {  // reuse block
    Arena* a = new Arena(ops);
    a->init();
    auto p0 = a->AllocateAligned(10);
    auto p1 = a->AllocateAligned(100);
    delete a;
    auto& m = local_arena_metrics;
    ASSERT_EQ(m.alloc_count, 2);
    ASSERT_EQ(m.newblock_count, 1);
  }

  local_arena_metrics.reset();

  {  // non-fully reuse block lead to space_waste
    Arena* a = new Arena(ops);
    a->init();
    auto p0 = a->AllocateAligned(10);
    auto p1 = a->AllocateAligned(65535);
    delete a;
    auto& m = local_arena_metrics;
    ASSERT_EQ(m.alloc_count, 2);
    ASSERT_EQ(m.newblock_count, 2);
    ASSERT_GT(m.space_wasted, 0);
  }
}

TEST_F(ThreadLocalArenaMetrics_Test, Destruction) {
  Arena* a = new Arena(ops);
  a->init();
  auto p0 = a->AllocateAligned(10);
  delete a;
  auto& m = local_arena_metrics;
  ASSERT_EQ(m.destruct_count, 1);
}

TEST_F(ThreadLocalArenaMetrics_Test, ReportToGlobalMetrics) {
  Arena* a = new Arena(ops);
  a->init();
  auto alloc_count = 1024;
  for (auto i = 0; i < alloc_count; i++) {
    auto p0 = a->AllocateAligned(10 * i);
  }
  delete a;

  {
    auto& global = global_arena_metrics;
    ASSERT_EQ(global.init_count, 0);
    ASSERT_EQ(global.alloc_count, 0);
    ASSERT_EQ(global.destruct_count, 0);
    auto& m = local_arena_metrics;
    ASSERT_EQ(m.init_count, 1);
    ASSERT_EQ(m.alloc_count, alloc_count);
    ASSERT_EQ(m.destruct_count, 1);
  }

  local_arena_metrics.report_to_global_metrics();

  {
    auto& global = global_arena_metrics;
    ASSERT_EQ(global.init_count, 1);
    ASSERT_EQ(global.alloc_count, alloc_count);
    ASSERT_EQ(global.destruct_count, 1);

    auto& m = local_arena_metrics;
    ASSERT_EQ(m.init_count, 0);
    ASSERT_EQ(m.alloc_count, 0);
    ASSERT_EQ(m.destruct_count, 0);

    // std::cout << global.string() << std::endl;
    global.reset();
  }

  {  // multi-thread
    auto f = [=, this]() {
      Arena* a = new Arena(ops);
      a->init();
      auto alloc_count = 1024;
      for (auto i = 0; i < alloc_count; i++) {
        auto p0 = a->AllocateAligned(10 * i);
      }
      delete a;
      local_arena_metrics.report_to_global_metrics();
    };

    auto& global = global_arena_metrics;
    ASSERT_EQ(global.init_count, 0);
    ASSERT_EQ(global.alloc_count, 0);
    ASSERT_EQ(global.destruct_count, 0);

    std::thread t1(f);
    std::thread t2(f);
    t1.join();
    t2.join();

    ASSERT_EQ(global.init_count, 2);
    ASSERT_EQ(global.alloc_count, alloc_count * 2);
    ASSERT_EQ(global.destruct_count, 2);
    // std::cout << global.string() << std::endl;
  }
}

}  // namespace memory
}  // namespace stdb
