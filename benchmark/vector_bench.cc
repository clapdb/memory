/*
 * Copyright (C) 2022 hurricane <l@stdb.io>. All rights reserved.
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

#include <vector>
#include "container/stdb_vector.hpp"
#include <benchmark/benchmark.h>

namespace stdb::container {

using std::vector;

constexpr int64_t times = 1024 * 32;

template <typename Vec>
void push_back() {
    Vec vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(i);
    }
}

static void pushback_std_vector(benchmark::State& state) {
    for (auto _ : state) {
        push_back<vector<int64_t>>();
    }
}

static void pushback_std_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<vector<int32_t>>();
    }
}

static void pushback_stdb_vector(benchmark::State& state) {
    for (auto _ : state) {
        push_back<stdb_vector<int64_t>>();
    }
}

static void pushback_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<stdb_vector<int32_t>>();
    }
}

BENCHMARK(pushback_std_vector);
BENCHMARK(pushback_std_vector_32);
BENCHMARK(pushback_stdb_vector);
BENCHMARK(pushback_stdb_vector_32);

BENCHMARK_MAIN();

} // namespace stdb::container
