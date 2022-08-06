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
#include "arena/metrics.hpp"

#include <doctest/doctest.h>

#include <thread>

#include "arena/arena.hpp"
#include "doctest/doctest.h"

using stdb::memory::Arena;
namespace stdb::memory {

class ThreadLocalArenaMetricsTest
{
   protected:
    Arena::Options ops;
    // NOLINTNEXTLINE
    static auto alloc(uint64_t size) -> void* { return std::malloc(size); }
    // NOLINTNEXTLINE
    static void dealloc(void* ptr) { std::free(ptr); }

    ThreadLocalArenaMetricsTest() {
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

    ~ThreadLocalArenaMetricsTest() {
        local_arena_metrics.reset();
        global_arena_metrics.reset();
    };
};

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsInit") {
    auto* a = new Arena(ops);
    a->init();
    delete a;
    auto& m = local_arena_metrics;
    CHECK_EQ(m.init_count, 1);
}

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsReset") {
    auto* a = new Arena(ops);
    a->init();
    auto* _ = a->AllocateAligned(124);
    a->Reset();
    delete a;
    auto& m = local_arena_metrics;
    CHECK(_);
    CHECK_EQ(m.reset_count, 1);
    CHECK_EQ(m.space_allocated, 124);
}

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsAllocation") {
    {
        auto* a = new Arena(ops);
        a->init();
        auto* _ = a->AllocateAligned(10);
        delete a;
        auto& m = local_arena_metrics;
        CHECK(_);
        CHECK_EQ(m.alloc_count, 1);
        CHECK_EQ(m.space_allocated, 10);
    }

    {
        auto* a = new Arena(ops);
        a->init();
        auto* _ = a->AllocateAligned(100);
        delete a;
        auto& m = local_arena_metrics;
        CHECK(_);
        CHECK_EQ(m.alloc_count, 2);
        CHECK_EQ(m.alloc_size_bucket_counter[0], 1);
        CHECK_EQ(m.alloc_size_bucket_counter[1], 1);
        CHECK_EQ(m.space_allocated, 110);
    }

    local_arena_metrics.reset();

    {
        auto& m = local_arena_metrics;
        auto loc = source_location::current();
        STring key = loc.file_name();
        key += ":" + std::to_string(loc.line());

        CHECK_FALSE(m.arena_alloc_counter.contains(key));

        auto* a = new Arena(ops);

        a->init(loc);
        auto* _ = a->AllocateAligned(100);
        delete a;

        CHECK(_);
        CHECK_EQ(m.arena_alloc_counter[key], 100);
    }
}

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsNewBlock") {
    SUBCASE("reuse block") {  // reuse block
        auto* a = new Arena(ops);
        a->init();
        auto* p1 = a->AllocateAligned(10);
        auto* p2 = a->AllocateAligned(100);
        delete a;
        auto& m = local_arena_metrics;
        CHECK(p1 != nullptr);
        CHECK(p2 != nullptr);
        CHECK_EQ(m.alloc_count, 2);
        CHECK_EQ(m.newblock_count, 1);
    }

    local_arena_metrics.reset();

    SUBCASE("non-fully reuse block lead to space waste") {  // non-fully reuse block lead to space_waste
        auto* a = new Arena(ops);
        a->init();
        auto* p1 = a->AllocateAligned(10);
        auto* p2 = a->AllocateAligned(65535);
        delete a;
        auto& m = local_arena_metrics;
        CHECK(p1 != nullptr);
        CHECK(p2 != nullptr);
        CHECK_EQ(m.alloc_count, 2);
        CHECK_EQ(m.newblock_count, 2);
        CHECK_GT(m.space_wasted, 0);
    }
}

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsDestruction") {
    auto* a = new Arena(ops);
    a->init();
    auto* _ = a->AllocateAligned(10);
    delete a;
    CHECK(_);
    auto& m = local_arena_metrics;
    CHECK_EQ(m.destruct_count, 1);
}

TEST_CASE_FIXTURE(ThreadLocalArenaMetricsTest, "MetricsReportToGlobalMetrics") {
    auto* a = new Arena(ops);
    a->init();
    uint64_t alloc_count = 1024;
    for (uint64_t i = 0; i < alloc_count; i++) {
        auto* _ = a->AllocateAligned(10 * i);
        CHECK(_);
    }
    delete a;

    {
        auto& global = global_arena_metrics;
        CHECK_EQ(global.init_count, 0);
        CHECK_EQ(global.alloc_count, 0);
        CHECK_EQ(global.destruct_count, 0);
        auto& m = local_arena_metrics;
        CHECK_EQ(m.init_count, 1);
        CHECK_EQ(m.alloc_count, alloc_count);
        CHECK_EQ(m.destruct_count, 1);
    }

    {
        local_arena_metrics.report_to_global_metrics();
        auto& global = global_arena_metrics;
        CHECK_EQ(global.init_count, 1);
        CHECK_EQ(global.alloc_count, alloc_count);
        CHECK_EQ(global.destruct_count, 1);

        auto& m = local_arena_metrics;
        CHECK_EQ(m.init_count, 0);
        CHECK_EQ(m.alloc_count, 0);
        CHECK_EQ(m.destruct_count, 0);

        global.reset();
    }

    {  // multi-thread
        bool alloc_success1 = false;
        auto f1 = [=, this, &alloc_success1]() {
            // g_cs = *cs;
            auto* aa = new Arena(ops);
            aa->init();
            uint64_t alloc_count_ = 1024;
            for (uint64_t i = 0; i < alloc_count_; i++) {
                alloc_success1 = (aa->AllocateAligned(10 * i) != nullptr);
            }
            delete aa;
            local_arena_metrics.report_to_global_metrics();
        };
        bool alloc_success2 = false;
        auto f2 = [=, this, &alloc_success2]() {
            // g_cs = *cs;
            auto* aa = new Arena(ops);
            aa->init();
            uint64_t alloc_count_ = 1024;
            for (uint64_t i = 0; i < alloc_count_; i++) {
                alloc_success2 = (aa->AllocateAligned(10 * i) != nullptr);
            }
            delete aa;
            local_arena_metrics.report_to_global_metrics();
        };

        auto& global = global_arena_metrics;
        CHECK_EQ(global.init_count, 0);
        CHECK_EQ(global.alloc_count, 0);
        CHECK_EQ(global.destruct_count, 0);

        std::thread t1(f1);
        std::thread t2(f2);
        t1.join();
        t2.join();

        CHECK(alloc_success1);
        CHECK(alloc_success2);
        CHECK_EQ(global.init_count, 2);
        CHECK_EQ(global.alloc_count, alloc_count * 2);
        CHECK_EQ(global.destruct_count, 2);
        // std::cout << global.STring() << std::endl;
    }
}

}  // namespace stdb::memory
