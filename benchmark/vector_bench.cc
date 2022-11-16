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
#include <iostream>
namespace stdb::container {

constexpr int64_t times = 1024 * 64;

template <typename T, template <typename, typename> typename Vec>
void push_back() {
    std::vector<T> vec;
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

template <typename T>
void push_back_unsafe() {
    stdb_vector<T> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        T t = static_cast<T>(i);
        vec.template push_back<Safety::Unsafe>(t);
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

void push_back_small_str_unsafe() {
    memory::string input("hello world");
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
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

void push_back_median_str_unsafe() {
    memory::string input("hello world! for testing! 1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
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

void push_back_large_str_unsafe() {
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
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (int64_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
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

static void pushback_stdb_vector_64_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int64_t>();
    }
}

static void pushback_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int32_t, stdb_vector>();
    }
}

static void pushback_stdb_vector_32_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int32_t>();
    }
}

static void pushback_stdb_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int8_t, stdb_vector>();
    }
}

static void pushback_stdb_vector_8_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int8_t>();
    }
}

static void pushback_stdb_vector_small_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_small_str<stdb_vector>();
    }
}

static void pushback_stdb_vector_small_str_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_small_str_unsafe();
    }
}

static void pushback_stdb_vector_median_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_median_str<stdb_vector>();
    }
}

static void pushback_stdb_vector_median_str_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_median_str_unsafe();
    }
}

static void pushback_stdb_vector_large_str(benchmark::State& state) {
    for (auto _ : state) {
        push_back_large_str<stdb_vector>();
    }
}

static void pushback_stdb_vector_large_str_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_large_str_unsafe();
    }
}

BENCHMARK(pushback_std_vector_64);
BENCHMARK(pushback_std_vector_32);
BENCHMARK(pushback_std_vector_8);
BENCHMARK(pushback_stdb_vector_64);
BENCHMARK(pushback_stdb_vector_32);
BENCHMARK(pushback_stdb_vector_8);

BENCHMARK(pushback_stdb_vector_64_unsafe);
BENCHMARK(pushback_stdb_vector_32_unsafe);
BENCHMARK(pushback_stdb_vector_8_unsafe);

BENCHMARK(pushback_std_vector_small_str);
BENCHMARK(pushback_stdb_vector_small_str);
BENCHMARK(pushback_stdb_vector_small_str_unsafe);
BENCHMARK(pushback_std_vector_median_str);
BENCHMARK(pushback_stdb_vector_median_str);
BENCHMARK(pushback_stdb_vector_median_str_unsafe);
BENCHMARK(pushback_std_vector_large_str);
BENCHMARK(pushback_stdb_vector_large_str);
BENCHMARK(pushback_stdb_vector_large_str_unsafe);

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

static void pushback_stdb_vector_just_move_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back<Safety::Unsafe>(std::move(m));
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

static void pushback_stdb_vector_just_copy_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back<Safety::Unsafe>(m);
        }
    }
}

BENCHMARK(pushback_std_vector_just_move);
BENCHMARK(pushback_stdb_vector_just_move);
BENCHMARK(pushback_stdb_vector_just_move_unsafe);
BENCHMARK(pushback_std_vector_just_copy);
BENCHMARK(pushback_stdb_vector_just_copy);
BENCHMARK(pushback_stdb_vector_just_copy_unsafe);

static void init_std_vector(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        std::vector<int64_t> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            vec.push_back(i);
        }
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_pushback_unsafe(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.reserve(times);
        for (int64_t i = 0; i < times; ++i) {
            vec.push_back<Safety::Unsafe>(i);
        }
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_resize(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.resize(times);
        for (int64_t i = 0; i < times; ++i) {
            vec.at(static_cast<size_t>(i)) = i;
        }
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_resize_unsafe(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.resize<Safety::Unsafe>(times);
        for (int64_t i = 0; i < times; ++i) {
            vec[static_cast<size_t>(i)] = i;
        }
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_get_buffer(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_buffer(times);
        for (int64_t i = 0; i < times; ++i) {
            buffer[static_cast<size_t>(i)] = i;
        }
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_get_buffer_unsafe(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_buffer<Safety::Unsafe>(times);
        for (int64_t i = 0; i < times; ++i) {
            buffer[static_cast<size_t>(i)] = i;
        }
        results.push_back(vec.size());
    }
}

static auto filler(int64_t* buffer) -> size_t {
    if (buffer)  {
        for (int64_t i = 0; i < times; ++i) {
            buffer[static_cast<size_t>(i)] = i;
        }
    }
    return times;
}

static void init_stdb_vector_with_fill(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.resize(times);
        vec.fill(&filler);
        results.push_back(vec.size());
    }
}

static void init_stdb_vector_with_fill_unsafe(benchmark::State& state) {
    stdb::container::stdb_vector<size_t> results;
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.resize(times);
        vec.fill<Safety::Unsafe>(&filler);
        results.push_back(vec.size());
    }
}
BENCHMARK(init_std_vector);
BENCHMARK(init_stdb_vector_with_pushback_unsafe);
BENCHMARK(init_stdb_vector_with_resize);
BENCHMARK(init_stdb_vector_with_resize_unsafe);
BENCHMARK(init_stdb_vector_with_get_buffer);
BENCHMARK(init_stdb_vector_with_get_buffer_unsafe);
BENCHMARK(init_stdb_vector_with_fill);
BENCHMARK(init_stdb_vector_with_fill_unsafe);

BENCHMARK_MAIN();

} // namespace stdb::container
