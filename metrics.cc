#include "metrics.hpp"

namespace stdb {
namespace memory {

GlobalArenaMetrics global_arena_metrics = GlobalArenaMetrics();

thread_local LocalArenaMetrics local_arena_metrics = LocalArenaMetrics();

void* metrics_probe_on_arena_init(Arena* arena) {
  local_arena_metrics.init_count += 1;
  return nullptr;
}
void metrics_probe_on_arena_reset(Arena* arena, void* cookie, uint64_t space_used) {
  local_arena_metrics.reset_count += 1;
  local_arena_metrics.space_reseted += space_used;
}
void metrics_probe_on_arena_allocation(const std::type_info* alloc_type, uint64_t alloc_size, void* cookie) {
  local_arena_metrics.alloc_count += 1;
  local_arena_metrics.space_allocated += alloc_size;
  local_arena_metrics.increse_alloc_size_counter(alloc_size);
}
void* metrics_probe_on_arena_destruction(Arena* arena, void* cookie, uint64_t space_used) {
  local_arena_metrics.destruct_count += 1;
  local_arena_metrics.space_used += space_used;
  return nullptr;
}

// thread_local LocalArenaMetrics ThreadLocalCollector::metrics = LocalArenaMetrics();

}  // namespace memory
}  // namespace stdb
