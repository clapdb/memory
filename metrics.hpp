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

#ifndef ARENA_METRICS_HPP_
#define ARENA_METRICS_HPP_

#include <bits/stdint-uintn.h>
#include <fmt/core.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <source_location>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "arena.hpp"

namespace stdb {
namespace memory {

using std::atomic;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

constexpr static int kAllocBucketSize = 8;
constexpr static uint64_t alloc_size_bucket[kAllocBucketSize] = {
  64,   // alloc_size <= 64 will counter into alloc_size_bucket_counter[0]
  128,  // 64 < alloc_size <= 128 will counter into alloc_size_bucket_counter[1]. same to the followings
  256, 512, 1024, 2048, 4096, 1UL << 20};

constexpr static int kLifetimeBucketSize = 8;
using namespace std::chrono_literals;
constexpr static milliseconds destruct_lifetime_bucket[kAllocBucketSize] = {
  1ms,  // destruct_lifetime <= 1ms will counter into destruct_lifetime_bucket[0]
  5ms,  // 1ms < destruct_lifetime <= 5ms will counter into destruct_lifetime_bucket[1].
  10ms, 50ms, 100ms, 200ms, 500ms, 1s,
};

struct GlobalArenaMetrics
{
  atomic<uint64_t> init_count = 0;
  atomic<uint64_t> destruct_count = 0;
  atomic<uint64_t> alloc_count = 0;
  atomic<uint64_t> newblock_count = 0;
  atomic<uint64_t> reset_count = 0;
  atomic<uint64_t> space_allocated = 0;
  atomic<uint64_t> space_reseted = 0;
  atomic<uint64_t> space_used = 0;
  atomic<uint64_t> space_wasted = 0;
  // space_allocated > space_used means memory reused;
  // space_allocated < space_used means memory fragment or arena used extra memory；

  // TODO(longqimin): other considerable metrics： fragments, arena-lifetime

  atomic<uint64_t> alloc_size_bucket_counter[kAllocBucketSize] = {0};
  atomic<uint64_t> destruct_lifetime_bucket_counter[kLifetimeBucketSize] = {0};
  std::unordered_map<std::string, atomic<uint64_t>> arena_alloc_counter = {};  // arena identified by init() location

  void reset() {  // lockless and races for metric-data is acceptable
    init_count.store(0, std::memory_order::relaxed);
    destruct_count.store(0, std::memory_order::relaxed);
    alloc_count.store(0, std::memory_order::relaxed);
    newblock_count.store(0, std::memory_order::relaxed);
    reset_count.store(0, std::memory_order::relaxed);
    space_allocated.store(0, std::memory_order::relaxed);
    space_reseted.store(0, std::memory_order::relaxed);
    space_used.store(0, std::memory_order::relaxed);
    space_wasted.store(0, std::memory_order::relaxed);
    for (auto& counter : alloc_size_bucket_counter) {
      counter.store(0, std::memory_order::relaxed);
    }
    for (auto& counter : destruct_lifetime_bucket_counter) {
      counter.store(0, std::memory_order::relaxed);
    }
    for (auto& [key, couter] : arena_alloc_counter) {
      couter.store(0, std::memory_order::relaxed);
    }
    return;
  }

  std::string string() const {
    std::string str;
    str.reserve(1024);
    str += fmt::format(
      "Summary:\n"
      "  init_count: {}\n"
      "  reset_count: {}\n"
      "  destruct_count: {}\n"
      "  alloc_count: {}\n"
      "  newblock_count: {}\n"
      "  space_allocated: {}\n"
      "  space_used: {}\n"
      "  space_wasted: {}\n"
      "  space_reseted: {}\nAllocSize distribution:",
      init_count, reset_count, destruct_count, alloc_count, newblock_count, space_allocated, space_used, space_wasted,
      space_reseted);

    for (uint64_t i = 0, count = 0; i < kAllocBucketSize; i++) {
      count += alloc_size_bucket_counter[i];
      str += fmt::format("\n  le={}: {}", alloc_size_bucket[i], static_cast<float>(count) / alloc_count);
    }

    str += "\nLifetime distribution:";
    for (uint64_t i = 0, count = 0; i < kLifetimeBucketSize; i++) {
      count += destruct_lifetime_bucket_counter[i];
      str +=
        fmt::format("\n  le={}ms: {}", destruct_lifetime_bucket[i].count(), static_cast<float>(count) / destruct_count);
    }

    str += "\nArena Location/AllocSize:";  // TODO(longqimin): re-evaluate str.reserve size
    for (const auto& [loc, couter] : arena_alloc_counter) {
      str += fmt::format("\n  {}: {}", loc, couter);
    }

    return str;
  }
};

// process level global metrics
extern GlobalArenaMetrics global_arena_metrics;

struct LocalArenaMetrics
{
  uint64_t init_count = 0;
  uint64_t destruct_count = 0;
  uint64_t alloc_count = 0;
  uint64_t newblock_count = 0;
  uint64_t reset_count = 0;
  uint64_t space_allocated = 0;
  uint64_t space_reseted = 0;
  uint64_t space_used = 0;  // space_allocated > space_used means memory reused;
                            // space_allocated < space_used means memory fragment or arena used extra memory；
  uint64_t space_wasted = 0;

  // TODO(longqimin): other considerable metrics： fragments, arena-lifetime

  uint64_t alloc_size_bucket_counter[kAllocBucketSize] = {0};
  uint64_t destruct_lifetime_bucket_counter[kLifetimeBucketSize] = {0};
  std::unordered_map<std::string, uint64_t> arena_alloc_counter = {};  // arena identified by init() location

  void reset() {
    init_count = 0;
    destruct_count = 0;
    alloc_count = 0;
    newblock_count = 0;
    reset_count = 0;
    space_allocated = 0;
    space_reseted = 0;
    space_used = 0;
    space_wasted = 0;

    memset(alloc_size_bucket_counter, 0, sizeof(alloc_size_bucket_counter));
    memset(destruct_lifetime_bucket_counter, 0, sizeof(destruct_lifetime_bucket_counter));

    arena_alloc_counter.clear();
  }

  [[gnu::always_inline]] inline void increase_alloc_size_counter(uint64_t alloc_size) {
    for (int i = 0; i < kAllocBucketSize; ++i) {
      if (alloc_size <= alloc_size_bucket[i]) {
        ++alloc_size_bucket_counter[i];
        break;
      }
    }
  }

  [[gnu::always_inline]] inline void increase_destruct_lifetime_counter(milliseconds destruct_lifetime) {
    for (int i = 0; i < kLifetimeBucketSize; ++i) {
      if (destruct_lifetime <= destruct_lifetime_bucket[i]) {
        ++destruct_lifetime_bucket_counter[i];
        break;
      }
    }
  }

  [[gnu::always_inline]] inline void increase_arena_alloc_couter(const std::source_location& loc, uint64_t size) {
    const std::string key = std::string(loc.file_name()) + ":" + std::to_string(loc.line());
    arena_alloc_counter[key] += size;
  }

  void report_to_global_metrics() {
    // only guaranteed atomicity with `relaxed` order and is enough
    global_arena_metrics.init_count.fetch_add(init_count, std::memory_order::relaxed);
    global_arena_metrics.reset_count.fetch_add(reset_count, std::memory_order::relaxed);
    global_arena_metrics.alloc_count.fetch_add(alloc_count, std::memory_order::relaxed);
    global_arena_metrics.newblock_count.fetch_add(newblock_count, std::memory_order::relaxed);
    global_arena_metrics.destruct_count.fetch_add(destruct_count, std::memory_order::relaxed);
    global_arena_metrics.space_allocated.fetch_add(space_allocated, std::memory_order::relaxed);
    global_arena_metrics.space_used.fetch_add(space_used, std::memory_order::relaxed);
    global_arena_metrics.space_wasted.fetch_add(space_wasted, std::memory_order::relaxed);
    global_arena_metrics.space_reseted.fetch_add(space_reseted, std::memory_order::relaxed);
    for (int i = 0; i < kAllocBucketSize; ++i) {
      global_arena_metrics.alloc_size_bucket_counter[i].fetch_add(alloc_size_bucket_counter[i],
                                                                  std::memory_order::relaxed);
    }
    for (int i = 0; i < kLifetimeBucketSize; ++i) {
      global_arena_metrics.destruct_lifetime_bucket_counter[i].fetch_add(destruct_lifetime_bucket_counter[i],
                                                                         std::memory_order::relaxed);
    }
    for (const auto& [loc, count] : arena_alloc_counter) {
      global_arena_metrics.arena_alloc_counter[loc].fetch_add(count, std::memory_order::relaxed);
    }

    reset();
  };
};

extern thread_local LocalArenaMetrics local_arena_metrics;

struct ArenaMetricsCookie
{
  steady_clock::time_point init_timepoint;
  std::source_location init_location;  // arena.init() source_location
  ArenaMetricsCookie(steady_clock::time_point init_tp, const std::source_location& init_loc)
      : init_timepoint(init_tp), init_location(init_loc) {}
};

[[gnu::always_inline]] inline void* metrics_probe_on_arena_init([[maybe_unused]] Arena* arena,
                                                                const std::source_location& loc) {
  ++local_arena_metrics.init_count;
  auto cookie = new ArenaMetricsCookie(steady_clock::now(), loc);
  return cookie;
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_allocation([[maybe_unused]] const std::type_info* alloc_type,
                                                                     uint64_t alloc_size, void* cookie) {
  ++local_arena_metrics.alloc_count;
  local_arena_metrics.space_allocated += alloc_size;
  local_arena_metrics.increase_alloc_size_counter(alloc_size);

  auto c = static_cast<ArenaMetricsCookie*>(cookie);
  local_arena_metrics.increase_arena_alloc_couter(c->init_location, alloc_size);
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_newblock([[maybe_unused]] uint64_t blk_num,
                                                                   [[maybe_unused]] uint64_t blk_size,
                                                                   [[maybe_unused]] void* cookie) {
  ++local_arena_metrics.newblock_count;
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_reset([[maybe_unused]] Arena* arena,
                                                                [[maybe_unused]] void* cookie, uint64_t space_used,
                                                                uint64_t space_wasted) {
  ++local_arena_metrics.reset_count;
  local_arena_metrics.space_reseted += space_used;
  local_arena_metrics.space_wasted += space_wasted;
}
[[gnu::always_inline]] inline void* metrics_probe_on_arena_destruction([[maybe_unused]] Arena* arena, void* cookie,
                                                                       uint64_t space_used, uint64_t space_wasted) {
  ++local_arena_metrics.destruct_count;
  local_arena_metrics.space_used += space_used;
  local_arena_metrics.space_wasted += space_wasted;

  std::unique_ptr<ArenaMetricsCookie> c(static_cast<ArenaMetricsCookie*>(cookie));
  auto destruct_lifetime = steady_clock::now() - c->init_timepoint;
  local_arena_metrics.increase_destruct_lifetime_counter(std::chrono::duration_cast<milliseconds>(destruct_lifetime));
  return nullptr;
}

}  // namespace memory
}  // namespace stdb

#endif  // ARENA_METRICS_HPP_
