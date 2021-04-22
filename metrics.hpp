#ifndef ARENA_METRICS_HPP_
#define ARENA_METRICS_HPP_

#include <bits/stdint-uintn.h>
#include <fmt/core.h>

#include <atomic>

#include "arena.hpp"

namespace stdb {
namespace memory {

using std::atomic;

constexpr static int kAllocBucketSize = 8;
constexpr static uint64_t alloc_size_bucket[kAllocBucketSize] = {
  64,   // alloc_size <= 64 will counter into alloc_size_bucket_counter[0]
  128,  // 64 < alloc_size <= 128 will counter into alloc_size_bucket_counter[1]. same to the followings
  256, 512, 1024, 2048, 4096, 1UL << 20};

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
    for (int i = 0; i < kAllocBucketSize; ++i) {
      alloc_size_bucket_counter[i].store(0, std::memory_order::relaxed);
    }
    return;
  }

  std::string string() const {
    std::string str;
    str.reserve(512);
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

    for (auto i = 0, count = 0; i < kAllocBucketSize; i++) {
      count += alloc_size_bucket_counter[i];
      str += fmt::format("\n  le={}: {}", alloc_size_bucket[i], static_cast<float>(count) / alloc_count);
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
  [[gnu::always_inline]] inline void increse_alloc_size_counter(uint64_t alloc_size) {
    for (int i = 0; i < kAllocBucketSize; ++i) {
      if (alloc_size <= alloc_size_bucket[i]) {
        ++alloc_size_bucket_counter[i];
        break;
      }
    }
  }

  void reset() { memset(this, 0, sizeof(*this)); }

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

    reset();
  };
};

extern thread_local LocalArenaMetrics local_arena_metrics;

[[gnu::always_inline]] inline void* metrics_probe_on_arena_init(Arena* arena) {
  local_arena_metrics.init_count += 1;
  return nullptr;
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_reset(Arena* arena, void* cookie, uint64_t space_used,
                                                                uint64_t space_wasted) {
  local_arena_metrics.reset_count += 1;
  local_arena_metrics.space_reseted += space_used;
  local_arena_metrics.space_wasted += space_wasted;
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_allocation(const std::type_info* alloc_type,
                                                                     uint64_t alloc_size, void* cookie) {
  local_arena_metrics.alloc_count += 1;
  local_arena_metrics.space_allocated += alloc_size;
  local_arena_metrics.increse_alloc_size_counter(alloc_size);
}
[[gnu::always_inline]] inline void metrics_probe_on_arena_newblock(uint64_t blk_num, uint64_t blk_size, void* cookie) {
  local_arena_metrics.newblock_count += 1;
}
[[gnu::always_inline]] inline void* metrics_probe_on_arena_destruction(Arena* arena, void* cookie, uint64_t space_used,
                                                                       uint64_t space_wasted) {
  local_arena_metrics.destruct_count += 1;
  local_arena_metrics.space_used += space_used;
  local_arena_metrics.space_wasted += space_wasted;
  return nullptr;
}

}  // namespace memory
}  // namespace stdb

#endif  // ARENA_METRICS_HPP_
