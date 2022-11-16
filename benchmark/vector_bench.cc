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

#include "seastar/testing/perf_tests.hh"
#include "fast_vector.h"
#include "hilbert/string.hpp"
#include "hilbert/container/stdb_vector.hpp"
#include "hilbert/container/fixed_vec.hpp"

namespace stdb::container {

using std::vector;

constexpr int64_t times = 1024 * 32;

PERF_TEST(VectorPushBackInt, std_vector) {
    perf_tests::start_measuring_time();
    std::vector<int> vec;
    vec.reserve(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackInt, stdb_vector) {
    perf_tests::start_measuring_time();
    stdb::container::stdb_vector<int> vec(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}
PERF_TEST(VectorPushBackInt, stdb_vector_unsafe) {
    perf_tests::start_measuring_time();
    stdb::container::stdb_vector<int> vec(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackInt, fast_vector) {
    perf_tests::start_measuring_time();
    fast_vector<int> vec;
    vec.reserve(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackInt, fixed_vector) {
    perf_tests::start_measuring_time();
    auto vec = stdb::container::make_unique_fixed_vec<int>(times);
    for (int i = 0; i < times; ++i) {
        vec->push_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackInt, array) {
    perf_tests::start_measuring_time();
    std::array<int, times> vec;
    for (int i = 0; i < times; ++i) {
        vec[i] = i;
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorForLoopInt, std_vector) {
    std::vector<int> vec;
    vec.reserve(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    int sum = 0;
    perf_tests::start_measuring_time();
    for (auto i : vec) {
        sum += i;
    }
    perf_tests::do_not_optimize(sum);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorForLoopInt, stdb_vector) {
    stdb::container::stdb_vector<int> vec(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(i);
    }
    int sum = 0;
    perf_tests::start_measuring_time();
    for (auto i : vec) {
        sum += i;
    }
    perf_tests::do_not_optimize(sum);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorForLoopInt, fast_vector) {
    fast_vector<int> vec;
    vec.reserve(times);
    for (int i = 0; i < times; ++i) {
        vec.push_back(i);
    }
    int sum = 0;
    perf_tests::start_measuring_time();
    for (auto i : vec) {
        sum += i;
    }
    perf_tests::do_not_optimize(sum);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorForLoopInt, fixed_vector) {
    auto vec = stdb::container::make_unique_fixed_vec<int>(times);
    for (int i = 0; i < times; ++i) {
        vec->push_back(i);
    }
    int sum = 0;
    perf_tests::start_measuring_time();
    for (auto i : *vec) {
        sum += i;
    }
    perf_tests::do_not_optimize(sum);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorForLoopInt, array) {
    std::array<int, times> vec;
    for (int i = 0; i < times; ++i) {
        vec[i] = i;
    }
    int sum = 0;
    perf_tests::start_measuring_time();
    for (auto i : vec) {
        sum += i;
    }
    perf_tests::do_not_optimize(sum);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorReserve, std_vector) {
    std::vector<int> vec;
    perf_tests::start_measuring_time();
    vec.reserve(times);
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorReserve, stdb_vector) {
    stdb::container::stdb_vector<int> vec;
    perf_tests::start_measuring_time();
    vec.reserve(times);
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackStr, std_vector) {
    stdb::STring str{"123456789012344567890"};
    std::vector<stdb::STring> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.push_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackStr, stdb_vector) {
    stdb::STring str{"123456789012344567890"};
    stdb::container::stdb_vector<stdb::STring> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.push_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackStr, fast_vector) {
    stdb::STring str{"123456789012344567890"};
    fast_vector<stdb::STring> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.push_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackStr, stdb_vector_unsafe) {
    stdb::STring str{"123456789012344567890"};
    stdb::container::stdb_vector<stdb::STring> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.push_back<Safety::Unsafe>(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}
class movable {
   public:
    int value;
    void* buf = nullptr;
    movable(int v) : value(v), buf (malloc(1024)) {
        memset(buf, value, 1024);
    }
    movable(movable&& other) noexcept: value(other.value), buf(other.buf) {
        other.buf = nullptr;
        value = 0;
    }
    movable& operator=(movable&& other) noexcept {
        buf = std::exchange(other.buf, nullptr);
        value = std::exchange(other.value, 0);
        return *this;
    }
    movable(const movable&) = delete;
    ~movable() {
        free(buf);
    }
};

PERF_TEST(VectorPushBackMove, stdb_vector) {
    stdb::container::stdb_vector<movable> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        movable move(i);
        vec.push_back(std::move(move));
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBack, stdb_vector) {
    stdb::container::stdb_vector<movable> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        vec.emplace_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackMove, std_vector) {
    std::vector<movable> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        movable move(i);
        vec.push_back(std::move(move));
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackMove, std_vector) {
    std::vector<movable> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        vec.emplace_back(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorPushBackMove, stdb_vector_unsafe) {
    stdb::container::stdb_vector<movable> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        movable move(i);
        vec.push_back<Safety::Unsafe>(std::move(move));
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackMove, stdb_vector_unsafe) {
    stdb::container::stdb_vector<movable> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < 512; ++i) {
        vec.emplace_back<Safety::Unsafe>(i);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackStr, std_vector) {
    stdb::STring str{"123456789012344567890"};
    std::vector<stdb::STring> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.emplace_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackStr, stdb_vector) {
    stdb::STring str{"123456789012344567890"};
    stdb::container::stdb_vector<stdb::STring> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.emplace_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackStr, fast_vector) {
    stdb::STring str{"123456789012344567890"};
    fast_vector<stdb::STring> vec;
    vec.reserve(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.emplace_back(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

PERF_TEST(VectorEmplaceBackStr, stdb_vector_unsafe) {
    stdb::STring str{"123456789012344567890"};
    stdb::container::stdb_vector<stdb::STring> vec(times);
    perf_tests::start_measuring_time();
    for (int i = 0; i < times; ++i) {
        vec.emplace_back<Safety::Unsafe>(str);
    }
    perf_tests::do_not_optimize(vec);
    perf_tests::stop_measuring_time();
}

}  // namespace stdb::container
