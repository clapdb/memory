/* 
* Copyright (C) 2020 Beijing Jinyi Data Technology Co., Ltd.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     https://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
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

#include <benchmark/benchmark.h>

#include <iostream>
#include <vector>

#include "container/stdb_vector.hpp"
#include "string/string.hpp"
namespace stdb::container {

// NOLINTBEGIN
constexpr size_t times = 1024 * 64;

template <typename T>
void push_back() {
    std::vector<T> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    benchmark::DoNotOptimize(vec);
}

template <typename T>
void push_back_stdb() {
    stdb::container::stdb_vector<T> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    benchmark::DoNotOptimize(vec);
}

template <typename T>
void init_vector() {
    std::vector<T> vec(times, std::numeric_limits<T>::max());
}

template <typename T>
void init_vector_stdb() {
    stdb::container::stdb_vector<T> vec(times, std::numeric_limits<T>::max());
}

template <typename T>
void assign() {
    std::vector<T> vec;
    vec.assign(times, std::numeric_limits<T>::max());
}

template <typename T>
void assign_stdb() {
    stdb::container::stdb_vector<T> vec;
    vec.assign(times, std::numeric_limits<T>::max());
}

template <typename T>
void push_back_unsafe() {
    stdb_vector<T> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.template push_back<Safety::Unsafe>(i);
    }
    benchmark::DoNotOptimize(vec);
}

template <typename T>
void for_loop(const std::vector<T>& vec) {
    T sum = 0;
    for (T i : vec) {
        sum += i;
    }
    benchmark::DoNotOptimize(sum);
}

template <typename T>
void for_loop(const stdb::container::stdb_vector<T>& vec) {
    T sum = 0;
    for (T i : vec) {
        sum += i;
    }
    benchmark::DoNotOptimize(sum);
}

template <template <typename, typename> typename Vec>
void push_back_small_str() {
    memory::string input("hello world");
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

template <template <typename> typename Vec>
void push_back_small_str() {
    memory::string input("hello world");
    Vec<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

void push_back_small_str_unsafe() {
    memory::string input("hello world");
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
    }
    benchmark::DoNotOptimize(vec);
}

template <template <typename, typename> typename Vec>
void push_back_median_str() {
    memory::string input(
      "hello world! for testing! "
      "1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

template <template <typename> typename Vec>
void push_back_median_str() {
    memory::string input(
      "hello world! for testing! "
      "1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    Vec<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

void push_back_median_str_unsafe() {
    memory::string input(
      "hello world! for testing! "
      "1223141234123453214132142314123421421412sdfsadbbagasdfgasfsdfasfasfdsafasfasfsadfasbaabasabababbaabaabab");
    assert(input.size() < 250 and input.size() > 30);
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
    }
    benchmark::DoNotOptimize(vec);
}

template <template <typename, typename> typename Vec>
void push_back_large_str() {
    memory::string input(
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890");
    assert(input.size() > 400);
    Vec<memory::string, std::allocator<memory::string>> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

template <template <typename> typename Vec>
void push_back_large_str() {
    memory::string input(
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890");
    assert(input.size() > 400);
    Vec<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(input);
    }
    benchmark::DoNotOptimize(vec);
}

void push_back_large_str_unsafe() {
    memory::string input(
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890"
      "123456789012345678901234567890123456789012345678901234567890");
    assert(input.size() > 400);
    stdb_vector<memory::string> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(input);
    }
    benchmark::DoNotOptimize(vec);
}

static void pushback_std_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        push_back<size_t>();
    }
}

static void pushback_std_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int32_t>();
    }
}

static void pushback_std_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int16_t>();
    }
}

static void pushback_std_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        push_back<int8_t>();
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
        push_back_stdb<size_t>();
    }
}

static void pushback_stdb_vector_64_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<size_t>();
    }
}

static void init_std_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        init_vector<int64_t>();
    }
}

static void init_std_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        init_vector<int32_t>();
    }
}

static void init_std_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        init_vector<int16_t>();
    }
}

static void init_std_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        init_vector<int8_t>();
    }
}

static void init_stdb_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        init_vector_stdb<int64_t>();
    }
}

static void init_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        init_vector_stdb<int32_t>();
    }
}

static void init_stdb_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        init_vector_stdb<int16_t>();
    }
}

static void init_stdb_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        init_vector_stdb<int8_t>();
    }
}

static void assign_std_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        assign<int64_t>();
    }
}

static void assign_std_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        assign<int32_t>();
    }
}

static void assign_std_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        assign<int16_t>();
    }
}

static void assign_std_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        assign<int8_t>();
    }
}

static void assign_stdb_vector_64(benchmark::State& state) {
    for (auto _ : state) {
        assign_stdb<int64_t>();
    }
}

static void assign_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        assign_stdb<int32_t>();
    }
}

static void assign_stdb_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        assign_stdb<int16_t>();
    }
}

static void assign_stdb_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        assign_stdb<int8_t>();
    }
}

static void forloop_std_vector_64(benchmark::State& state) {
    std::vector<int64_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int64_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_stdb_vector_64(benchmark::State& state) {
    stdb::container::stdb_vector<int64_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int64_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_std_vector_32(benchmark::State& state) {
    std::vector<int32_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int32_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_stdb_vector_32(benchmark::State& state) {
    stdb::container::stdb_vector<int32_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int32_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_std_vector_16(benchmark::State& state) {
    std::vector<int16_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int16_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_stdb_vector_16(benchmark::State& state) {
    stdb::container::stdb_vector<int16_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int16_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_std_vector_8(benchmark::State& state) {
    std::vector<int8_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int8_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void forloop_stdb_vector_8(benchmark::State& state) {
    stdb::container::stdb_vector<int8_t> data;
    data.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        data.push_back((int16_t)i);
    }
    for (auto _ : state) {
        for_loop(data);
    }
}

static void pushback_stdb_vector_32(benchmark::State& state) {
    for (auto _ : state) {
        push_back_stdb<int32_t>();
    }
}

static void pushback_stdb_vector_32_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int32_t>();
    }
}

static void pushback_stdb_vector_16(benchmark::State& state) {
    for (auto _ : state) {
        push_back_stdb<int16_t>();
    }
}

static void pushback_stdb_vector_16_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int16_t>();
    }
}

static void pushback_stdb_vector_8(benchmark::State& state) {
    for (auto _ : state) {
        push_back_stdb<int8_t>();
    }
}

static void pushback_stdb_vector_8_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        push_back_unsafe<int8_t>();
    }
}

static void pushback_stdb_vector_8_unsafe_simulate(benchmark::State& state) {
    for (auto _ : state) {
        stdb_vector<int32_t> vec;
        vec.reserve(times / 4);
        int32_t temp = 0;
        for (size_t i = 0; i < times; ++i) {
            if (i % 4 == 3) {
                vec.push_back<Safety::Unsafe>(temp);
                temp = 0;
            } else {
                temp <<= 8;
                temp |= (int8_t)i;
            }
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_stdb_str(benchmark::State& state) {
    for (auto _ : state) {
        stdb::memory::string str;
        str.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            str.push_back((char)i);
        }
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
BENCHMARK(pushback_std_vector_16);
BENCHMARK(pushback_std_vector_8);
BENCHMARK(pushback_stdb_vector_64);
BENCHMARK(pushback_stdb_vector_32);
BENCHMARK(pushback_stdb_vector_16);
BENCHMARK(pushback_stdb_vector_8);

BENCHMARK(init_std_vector_64);
BENCHMARK(init_std_vector_32);
BENCHMARK(init_std_vector_16);
BENCHMARK(init_std_vector_8);
BENCHMARK(init_stdb_vector_64);
BENCHMARK(init_stdb_vector_32);
BENCHMARK(init_stdb_vector_16);
BENCHMARK(init_stdb_vector_8);

BENCHMARK(assign_std_vector_64);
BENCHMARK(assign_std_vector_32);
BENCHMARK(assign_std_vector_16);
BENCHMARK(assign_std_vector_8);
BENCHMARK(assign_stdb_vector_64);
BENCHMARK(assign_stdb_vector_32);
BENCHMARK(assign_stdb_vector_16);
BENCHMARK(assign_stdb_vector_8);

BENCHMARK(forloop_std_vector_64);
BENCHMARK(forloop_stdb_vector_64);
BENCHMARK(forloop_std_vector_32);
BENCHMARK(forloop_stdb_vector_32);
BENCHMARK(forloop_std_vector_16);
BENCHMARK(forloop_stdb_vector_16);
BENCHMARK(forloop_std_vector_8);
BENCHMARK(forloop_stdb_vector_8);

BENCHMARK(pushback_stdb_str);

BENCHMARK(pushback_stdb_vector_64_unsafe);
BENCHMARK(pushback_stdb_vector_32_unsafe);
BENCHMARK(pushback_stdb_vector_16_unsafe);
BENCHMARK(pushback_stdb_vector_8_unsafe);
BENCHMARK(pushback_stdb_vector_8_unsafe_simulate);

BENCHMARK(pushback_std_vector_small_str);
BENCHMARK(pushback_stdb_vector_small_str);
BENCHMARK(pushback_stdb_vector_small_str_unsafe);
BENCHMARK(pushback_std_vector_median_str);
BENCHMARK(pushback_stdb_vector_median_str);
BENCHMARK(pushback_stdb_vector_median_str_unsafe);
BENCHMARK(pushback_std_vector_large_str);
BENCHMARK(pushback_stdb_vector_large_str);
BENCHMARK(pushback_stdb_vector_large_str_unsafe);

class just_move
{
   public:
    int value;
    void* buf = nullptr;
    just_move(int v) : value(v), buf(malloc(1024)) { memset(buf, value, 1024); }
    just_move(just_move&& other) noexcept : value(other.value), buf(other.buf) {
        other.buf = nullptr;
        value = 0;
    }
    just_move& operator=(just_move&& other) noexcept {
        buf = std::exchange(other.buf, nullptr);
        value = std::exchange(other.value, 0);
        return *this;
    }
    just_move(const just_move&) = delete;
    ~just_move() { free(buf); }
};

class just_copy
{
   public:
    int value;
    void* buf = nullptr;
    just_copy(int v) : value(v), buf(malloc(1024)) { memset(buf, value, 1024); }
    just_copy(const just_copy& other) noexcept : value(other.value), buf(malloc(1024)) { memcpy(buf, other.buf, 1024); }
    just_copy& operator=(const just_copy& other) noexcept {
        value = other.value;
        memcpy(buf, other.buf, 1024);
        return *this;
    }
    just_copy(just_copy&&) = delete;
    ~just_copy() { free(buf); }
};

static void pushback_std_vector_just_move(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_stdb_vector_just_move(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_stdb_vector_just_move_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back<Safety::Unsafe>(std::move(m));
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_std_vector_just_copy(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_stdb_vector_just_copy(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void pushback_stdb_vector_just_copy_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back<Safety::Unsafe>(m);
        }
        benchmark::DoNotOptimize(vec);
    }
}

BENCHMARK(pushback_std_vector_just_move);
BENCHMARK(pushback_stdb_vector_just_move);
BENCHMARK(pushback_stdb_vector_just_move_unsafe);
BENCHMARK(pushback_std_vector_just_copy);
BENCHMARK(pushback_stdb_vector_just_copy);
BENCHMARK(pushback_stdb_vector_just_copy_unsafe);

static void init_std_vector(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<size_t> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            vec.push_back(i);
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_pushback_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<int64_t> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            vec.push_back<Safety::Unsafe>((int64_t)i);
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_resize(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.resize(times);
        for (size_t i = 0; i < times; ++i) {
            vec.at(i) = i;
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_resize_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.resize<Safety::Unsafe>(times);
        for (size_t i = 0; i < times; ++i) {
            vec[i] = i;
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_get_buffer(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_writebuffer(times);
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_get_buffer_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_writebuffer<Safety::Unsafe>(times);
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
        benchmark::DoNotOptimize(vec);
    }
}

static auto filler(size_t* buffer) -> size_t {
    if (buffer) {
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
    }
    return times;
}

static void init_stdb_vector_with_fill(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        vec.fill(&filler);
        benchmark::DoNotOptimize(vec);
    }
}

static void init_stdb_vector_with_fill_unsafe(benchmark::State& state) {
    for (auto _ : state) {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        vec.fill<Safety::Unsafe>(&filler);
        benchmark::DoNotOptimize(vec);
    }
}

struct trivially_copyable
{
    int x;
    double y;
    int z;
    void* ptr;
};

struct non_trivially_copyable
{
    int x;
    double y;
    int z;
    void* ptr;
    ~non_trivially_copyable() {}
};

static_assert(IsRelocatable<trivially_copyable>, "trivially_copyable is not relocatable");
static_assert(!IsRelocatable<non_trivially_copyable>, "non_trivially_copyable is relocatable");

template <typename T>
static void reserve_std_vector(benchmark::State& state) {
    std::vector<T> vec;
    for (auto _ : state) {
        vec.reserve(times * 2);
    }
    vec.push_back(T());
}

template <typename T>
static void reserve_stdb_vector(benchmark::State& state) {
    stdb::container::stdb_vector<T> vec;
    for (auto _ : state) {
        vec.reserve(times * 2);
    }
    vec.push_back(T());
}
template <typename T>
auto generate_t() -> T {
    if constexpr(std::is_pointer_v<T>) {
        return T(-1);
    } else {
        return T{1};
    }
}

template <typename T>
static void stack_like_std_vector(benchmark::State& state) {
    std::vector<T> vec;
    vec.reserve(16);
    for (auto _ : state) {
        vec.push_back(T{});
        if (vec.back() == T{}) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<T>());
        }
        while (not vec.empty()) {
            vec.pop_back();
        }
        benchmark::DoNotOptimize(vec);
    }
}

template <typename T>
static void stack_like_stdb_vector(benchmark::State& state) {
    stdb_vector<T> vec;
    vec.reserve(16);
    for (auto _ : state) {
        vec.push_back(T{});
        if (vec.back() == T{}) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<T>());
        }
        while (not vec.empty()) {
            vec.pop_back();
        }
        benchmark::DoNotOptimize(vec);
    }
}

template <typename T>
static void stack_like_std_vector_with_size(benchmark::State& state) {
    std::vector<T> vec;
    vec.reserve(16);
    for (auto _ : state) {
        vec.push_back(T{});
        if (vec.back() == T{}) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<T>());
        }
        auto size = vec.size();
        for (size_t  i = 0; i < size; ++i) {
            vec.pop_back();
        }
        benchmark::DoNotOptimize(vec);
    }

}

template <typename T>
static void stack_like_stdb_vector_with_size(benchmark::State& state) {
    stdb_vector<T> vec;
    vec.reserve(16);
    for (auto _ : state) {
        vec.push_back(T{});
        if (vec.back() == T{}) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<T>());
        }
        auto size = vec.size();
        for (size_t  i = 0; i < size; ++i) {
            vec.pop_back();
        }
        benchmark::DoNotOptimize(vec);
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

BENCHMARK(reserve_std_vector<trivially_copyable>);
BENCHMARK(reserve_std_vector<non_trivially_copyable>);
BENCHMARK(reserve_stdb_vector<trivially_copyable>);
BENCHMARK(reserve_stdb_vector<non_trivially_copyable>);

BENCHMARK(stack_like_std_vector<char*>);
BENCHMARK(stack_like_stdb_vector<char*>);
BENCHMARK(stack_like_std_vector_with_size<char*>);
BENCHMARK(stack_like_stdb_vector_with_size<char*>);

BENCHMARK_MAIN();

// NOLINTEND

}  // namespace stdb::container
