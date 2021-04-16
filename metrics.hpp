#ifndef ARENA_METRICS_HPP_
#define ARENA_METRICS_HPP_

#include <fmt/core.h>

#include <atomic>

#include "arena.hpp"

namespace stdb {
namespace memory {

void* metrics_probe_on_arena_init(Arena* arena);
void metrics_probe_on_arena_reset(Arena* arena, void* cookie, uint64_t space_used);
void metrics_probe_on_arena_allocation(const std::type_info* alloc_type, uint64_t alloc_size, void* cookie);
void* metrics_probe_on_arena_destruction(Arena* arena, void* cookie, uint64_t space_used);

constexpr static int kAllocBucketSize = 8;
constexpr static uint64_t alloc_size_bucket[kAllocBucketSize] = {64, 128, 256, 512, 1024, 2048, 4096, 1UL << 20};

struct GlobalArenaMetrics
{
  std::atomic<uint64_t> init_count = 0;
  std::atomic<uint64_t> destruct_count = 0;
  std::atomic<uint64_t> alloc_count = 0;
  std::atomic<uint64_t> reset_count = 0;
  std::atomic<uint64_t> space_allocated = 0;
  std::atomic<uint64_t> space_reseted = 0;
  std::atomic<uint64_t> space_used = 0;
  // space_allocated > space_used means memory reused;
  // space_allocated < space_used means memory fragment or arena used extra memory；

  // TODO(longqimin): other considerable metrics： fragments, arena-lifetime

  std::atomic<uint64_t> alloc_size_bucket_counter[kAllocBucketSize] = {0};

  inline void reset() {  // lockless and races for metric-data is acceptable
    init_count = 0;
    destruct_count = 0;
    alloc_count = 0;
    reset_count = 0;
    space_allocated = 0;
    space_reseted = 0;
    space_used = 0;
    for (int i = 0; i < kAllocBucketSize; ++i) {
      alloc_size_bucket_counter[i] = 0;
    }
    return;
  }

  inline std::string string() const {
    std::string str;
    str.reserve(512);
    str += fmt::format(
      "Summary:\n"
      "  init_count: {}\n"
      "  reset_count: {}\n"
      "  destruct_count: {}\n"
      "  alloc_count: {}\n"
      "  space_allocated: {}\n"
      "  space_used: {}\n"
      "  space_reseted: {}\n",
      init_count, reset_count, destruct_count, alloc_count, space_allocated, space_used, space_reseted);

    str += "\nAllocSize distribution:";
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
  uint64_t reset_count = 0;
  uint64_t space_allocated = 0;
  uint64_t space_reseted = 0;
  uint64_t space_used = 0;  // space_allocated > space_used means memory reused;
                            // space_allocated < space_used means memory fragment or arena used extra memory；

  // TODO(longqimin): other considerable metrics： fragments, arena-lifetime

  uint64_t alloc_size_bucket_counter[kAllocBucketSize] = {0};
  inline void increse_alloc_size_counter(uint64_t alloc_size) {
    for (int i = 0; i < kAllocBucketSize; ++i) {
      if (alloc_size <= alloc_size_bucket[i]) {
        ++alloc_size_bucket_counter[i];
        break;
      }
    }
  }

  void reset() { memset(this, 0, sizeof(*this)); }

  inline void report_to_global_metrics() {
    global_arena_metrics.init_count += init_count;
    global_arena_metrics.reset_count += reset_count;
    global_arena_metrics.alloc_count += alloc_count;
    global_arena_metrics.destruct_count += destruct_count;
    global_arena_metrics.space_allocated += space_allocated;
    global_arena_metrics.space_used += space_used;
    global_arena_metrics.space_reseted += space_reseted;
    for (int i = 0; i < kAllocBucketSize; ++i) {
      global_arena_metrics.alloc_size_bucket_counter[i] += alloc_size_bucket_counter[i];
    }

    reset();
  };
};

extern thread_local LocalArenaMetrics local_arena_metrics;

}  // namespace memory
}  // namespace stdb

#endif  // ARENA_METRICS_HPP_
