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
#include "string/string.hpp"

namespace stdb::container {

constexpr int64_t times = 1024 * 8;

template <typename T, template <typename, typename> typename Vec>
void push_back() {
    Vec<T, std::allocator<T>> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(static_cast<T>(i));
    }
}

template <typename T, template <typename> typename Vec>
void push_back() {
    Vec<T> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(static_cast<T>(i));
    }
}

template <template <typename, typename> typename Vec>
void push_back_small_str() {
    memory::string input("hello world");
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}

template <template <typename> typename Vec>
void push_back_small_str() {
    memory::string input("hello world");
    Vec<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}

template <template <typename, typename> typename Vec>
void push_back_median_str() {
    memory::string input("hello world! for testing! 1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}

template <template <typename> typename Vec>
void push_back_median_str() {
    memory::string input("hello world! for testing! 1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    Vec<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}

template <template <typename, typename> typename Vec>
void push_back_large_str() {
    memory::string input("123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
      );
    assert(input.size() > 400);
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}
template <template <typename> typename Vec>
void push_back_large_str() {
    memory::string input("123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
                         "123456789012345678901234567890123456789012345678901234567890"
      );
    assert(input.size() > 400);
    Vec<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
}

static void pushback_std_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int64_t, std::vector>();
    }
}

static void pushback_std_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int32_t, std::vector>();
    }
}

static void pushback_std_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int8_t , std::vector>();
    }
}

static void pushback_std_vector_small_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_small_str<std::vector>();
    }
}
static void pushback_std_vector_median_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_median_str<std::vector>();
    }
}
static void pushback_std_vector_large_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_large_str<std::vector>();
    }
}

static void pushback_stdb_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int64_t, stdb_vector>();
    }
}

static void pushback_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int32_t, stdb_vector>();
    }
}

static void pushback_stdb_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int8_t, stdb_vector>();
    }
}

static void pushback_stdb_vector_small_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_small_str<stdb_vector>();
    }
}

static void pushback_stdb_vector_median_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_median_str<stdb_vector>();
    }
}

static void pushback_stdb_vector_large_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_large_str<stdb_vector>();
    }
}

BENCHMARK(pushback_std_vector_64);
BENCHMARK(pushback_std_vector_32);
BENCHMARK(pushback_std_vector_8);
BENCHMARK(pushback_stdb_vector_64);
BENCHMARK(pushback_stdb_vector_32);
BENCHMARK(pushback_stdb_vector_8);
BENCHMARK(pushback_std_vector_small_str);
BENCHMARK(pushback_stdb_vector_small_str);
BENCHMARK(pushback_std_vector_median_str);
BENCHMARK(pushback_stdb_vector_median_str);
BENCHMARK(pushback_std_vector_large_str);
BENCHMARK(pushback_stdb_vector_large_str);

class just_move {
   public:
    int value;
    void* buf = nullptr;
    just_move(int v) : value(v), buf (malloc(1024)) {
        memset(buf, value, 1024);
    }
    just_move(just_move&& other) noexcept: value(other.value), buf(other.buf) {
        other.buf = nullptr;
        value = 0;
    }
    just_move& operator=(just_move&& other) noexcept {
        buf = std::exchange(other.buf, nullptr);
        value = std::exchange(other.value, 0);
        return *this;
    }
    just_move(const just_move&) = delete;
    ~just_move() {
        free(buf);
    }
};

class just_copy {
    public:
      int value;
      void* buf = nullptr;
      just_copy(int v) : value(v), buf (malloc(1024)) {
          memset(buf, value, 1024);
      }
      just_copy(const just_copy& other) noexcept: value(other.value), buf(malloc(1024)) {
          memcpy(buf, other.buf, 1024);
      }
      just_copy& operator=(const just_copy& other) noexcept {
          value = other.value;
          memcpy(buf, other.buf, 1024);
          return *this;
      }
      just_copy(just_copy&&) = delete;
      ~just_copy() {
          free(buf);
      }
  };

static void pushback_std_vector_just_move(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<just_move> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
    }
}

static void pushback_stdb_vector_just_move(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
    }
}

static void pushback_std_vector_just_copy(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<just_copy> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
    }
}

static void pushback_stdb_vector_just_copy(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
    }
}

BENCHMARK(pushback_std_vector_just_move);
BENCHMARK(pushback_stdb_vector_just_move);
BENCHMARK(pushback_std_vector_just_copy);
BENCHMARK(pushback_stdb_vector_just_copy);

BENCHMARK_MAIN();

} // namespace stdb::container
