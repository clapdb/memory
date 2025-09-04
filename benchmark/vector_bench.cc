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

#define ANKERL_NANOBENCH_IMPLEMENT
#include "../nanobench/src/include/nanobench.h"

#include <iostream>
#include <vector>

#include "container/stdb_vector.hpp"

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
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void push_back_stdb() {
    stdb::container::stdb_vector<T> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void init_vector() {
    std::vector<T> vec(times, std::numeric_limits<T>::max());
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void init_vector_stdb() {
    stdb::container::stdb_vector<T> vec(times, std::numeric_limits<T>::max());
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void assign() {
    std::vector<T> vec;
    vec.assign(times, std::numeric_limits<T>::max());
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void assign_stdb() {
    stdb::container::stdb_vector<T> vec;
    vec.assign(times, std::numeric_limits<T>::max());
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void push_back_unsafe() {
    stdb_vector<T> vec;
    vec.reserve(times);
    for (size_t i = 0; i < times; ++i) {
        vec.template push_back<Safety::Unsafe>(i);
    }
    ankerl::nanobench::doNotOptimizeAway(vec);
}

template <typename T>
void for_loop(const std::vector<T>& vec) {
    T sum = 0;
    for (T i : vec) {
        sum += i;
    }
    ankerl::nanobench::doNotOptimizeAway(sum);
}

template <typename T>
void for_loop(const stdb::container::stdb_vector<T>& vec) {
    T sum = 0;
    for (T i : vec) {
        sum += i;
    }
    ankerl::nanobench::doNotOptimizeAway(sum);
}

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
    just_copy(const just_copy& other) noexcept : value(other.value), buf(malloc(1024)) { 
        memcpy(buf, other.buf, 1024); 
    }
    just_copy& operator=(const just_copy& other) noexcept {
        value = other.value;
        memcpy(buf, other.buf, 1024);
        return *this;
    }
    just_copy(just_copy&&) = delete;
    ~just_copy() { free(buf); }
};

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
auto generate_t() -> T {
    if constexpr (std::is_pointer_v<T>) {
        return T(-1);
    } else {
        return T{1};
    }
}

auto filler(size_t* buffer) -> size_t {
    if (buffer) {
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
    }
    return times;
}

int main() {
    ankerl::nanobench::Bench bench;
    bench.title("Vector Benchmarks").unit("op").warmup(100).epochs(1000);

    // Push back benchmarks
    bench.run("push_back std::vector<size_t>", []() { push_back<size_t>(); });
    bench.run("push_back std::vector<int32_t>", []() { push_back<int32_t>(); });
    bench.run("push_back std::vector<int16_t>", []() { push_back<int16_t>(); });
    bench.run("push_back std::vector<int8_t>", []() { push_back<int8_t>(); });
    
    bench.run("push_back stdb_vector<size_t>", []() { push_back_stdb<size_t>(); });
    bench.run("push_back stdb_vector<int32_t>", []() { push_back_stdb<int32_t>(); });
    bench.run("push_back stdb_vector<int16_t>", []() { push_back_stdb<int16_t>(); });
    bench.run("push_back stdb_vector<int8_t>", []() { push_back_stdb<int8_t>(); });
    
    bench.run("push_back stdb_vector<size_t> unsafe", []() { push_back_unsafe<size_t>(); });
    bench.run("push_back stdb_vector<int32_t> unsafe", []() { push_back_unsafe<int32_t>(); });
    bench.run("push_back stdb_vector<int16_t> unsafe", []() { push_back_unsafe<int16_t>(); });
    bench.run("push_back stdb_vector<int8_t> unsafe", []() { push_back_unsafe<int8_t>(); });

    // Init benchmarks
    bench.run("init std::vector<int64_t>", []() { init_vector<int64_t>(); });
    bench.run("init std::vector<int32_t>", []() { init_vector<int32_t>(); });
    bench.run("init std::vector<int16_t>", []() { init_vector<int16_t>(); });
    bench.run("init std::vector<int8_t>", []() { init_vector<int8_t>(); });
    
    bench.run("init stdb_vector<int64_t>", []() { init_vector_stdb<int64_t>(); });
    bench.run("init stdb_vector<int32_t>", []() { init_vector_stdb<int32_t>(); });
    bench.run("init stdb_vector<int16_t>", []() { init_vector_stdb<int16_t>(); });
    bench.run("init stdb_vector<int8_t>", []() { init_vector_stdb<int8_t>(); });

    // Assign benchmarks
    bench.run("assign std::vector<int64_t>", []() { assign<int64_t>(); });
    bench.run("assign std::vector<int32_t>", []() { assign<int32_t>(); });
    bench.run("assign std::vector<int16_t>", []() { assign<int16_t>(); });
    bench.run("assign std::vector<int8_t>", []() { assign<int8_t>(); });
    
    bench.run("assign stdb_vector<int64_t>", []() { assign_stdb<int64_t>(); });
    bench.run("assign stdb_vector<int32_t>", []() { assign_stdb<int32_t>(); });
    bench.run("assign stdb_vector<int16_t>", []() { assign_stdb<int16_t>(); });
    bench.run("assign stdb_vector<int8_t>", []() { assign_stdb<int8_t>(); });

    // For loop benchmarks
    bench.run("for_loop std::vector<int64_t>", []() {
        std::vector<int64_t> data;
        data.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            data.push_back((int64_t)i);
        }
        for_loop(data);
    });
    
    bench.run("for_loop stdb_vector<int64_t>", []() {
        stdb::container::stdb_vector<int64_t> data;
        data.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            data.push_back((int64_t)i);
        }
        for_loop(data);
    });

    bench.run("for_loop std::vector<int32_t>", []() {
        std::vector<int32_t> data;
        data.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            data.push_back((int32_t)i);
        }
        for_loop(data);
    });
    
    bench.run("for_loop stdb_vector<int32_t>", []() {
        stdb::container::stdb_vector<int32_t> data;
        data.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            data.push_back((int32_t)i);
        }
        for_loop(data);
    });

    // Move/Copy benchmarks
    bench.run("push_back std::vector<just_move>", []() {
        std::vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("push_back stdb_vector<just_move>", []() {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back(std::move(m));
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("push_back stdb_vector<just_move> unsafe", []() {
        stdb::container::stdb_vector<just_move> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            just_move m(i);
            vec.push_back<Safety::Unsafe>(std::move(m));
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("push_back std::vector<just_copy>", []() {
        std::vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("push_back stdb_vector<just_copy>", []() {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back(m);
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("push_back stdb_vector<just_copy> unsafe", []() {
        stdb::container::stdb_vector<just_copy> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            stdb::container::just_copy m(i);
            vec.push_back<Safety::Unsafe>(m);
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    // Advanced stdb_vector benchmarks
    bench.run("init std::vector push_back", []() {
        std::vector<size_t> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            vec.push_back(i);
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector push_back unsafe", []() {
        stdb::container::stdb_vector<int64_t> vec;
        vec.reserve(times);
        for (size_t i = 0; i < times; ++i) {
            vec.push_back<Safety::Unsafe>((int64_t)i);
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector resize", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.resize(times);
        for (size_t i = 0; i < times; ++i) {
            vec.at(i) = i;
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector resize unsafe", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.resize<Safety::Unsafe>(times);
        for (size_t i = 0; i < times; ++i) {
            vec[i] = i;
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector get_buffer", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_writebuffer(times);
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector get_buffer unsafe", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        auto buffer = vec.get_writebuffer<Safety::Unsafe>(times);
        for (size_t i = 0; i < times; ++i) {
            buffer[i] = i;
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector fill", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        vec.fill(&filler);
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("init stdb_vector fill unsafe", []() {
        stdb::container::stdb_vector<size_t> vec;
        vec.reserve(times);
        vec.fill<Safety::Unsafe>(&filler);
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    // Reserve benchmarks
    bench.run("reserve std::vector<trivially_copyable>", []() {
        std::vector<trivially_copyable> vec;
        vec.reserve(times * 2);
        vec.push_back(trivially_copyable());
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("reserve std::vector<non_trivially_copyable>", []() {
        std::vector<non_trivially_copyable> vec;
        vec.reserve(times * 2);
        vec.push_back(non_trivially_copyable());
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("reserve stdb_vector<trivially_copyable>", []() {
        stdb::container::stdb_vector<trivially_copyable> vec;
        vec.reserve(times * 2);
        vec.push_back(trivially_copyable());
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("reserve stdb_vector<non_trivially_copyable>", []() {
        stdb::container::stdb_vector<non_trivially_copyable> vec;
        vec.reserve(times * 2);
        vec.push_back(non_trivially_copyable());
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    // Stack-like operations
    bench.run("stack_like std::vector<char*>", []() {
        std::vector<char*> vec;
        vec.reserve(16);
        vec.push_back(nullptr);
        if (vec.back() == nullptr) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<char*>());
        }
        while (not vec.empty()) {
            vec.pop_back();
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    bench.run("stack_like stdb_vector<char*>", []() {
        stdb_vector<char*> vec;
        vec.reserve(16);
        vec.push_back(nullptr);
        if (vec.back() == nullptr) {
            vec.pop_back();
        }
        for (uint64_t i = 0; i < 8; ++i) {
            vec.push_back(generate_t<char*>());
        }
        while (not vec.empty()) {
            vec.pop_back();
        }
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    // Special stdb_vector benchmark
    bench.run("push_back stdb_vector int8->int32 simulate", []() {
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
        ankerl::nanobench::doNotOptimizeAway(vec);
    });

    return 0;
}

// NOLINTEND

}  // namespace stdb::container

int main() {
    return stdb::container::main();
}