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

#include "container/stdb_vector.hpp"

#include <doctest/doctest.h>

#include <iostream>
#include <set>
#include <span>

#include "string/string.hpp"

namespace stdb::container {
// NOLINTBEGIN

TEST_CASE("Hilbert::stdb_vector::int") {
    SUBCASE("zero init (copy and move)") {
        stdb_vector<int> vec;
        CHECK_EQ(vec.empty(), true);
        CHECK_EQ(vec.size(), 0);
        CHECK_EQ(vec.capacity(), 0);
        CHECK_EQ(vec.begin(), vec.end());
        // stdb_vector will not start from 0 capacity;
        CHECK_EQ(vec.data(), nullptr);
        CHECK_EQ(vec.max_size(), kFastVectorMaxSize / sizeof(int));
        stdb_vector<int> another_vec;
        CHECK_EQ(vec, another_vec);
        CHECK_EQ(vec != another_vec, false);
        CHECK_EQ(vec >= another_vec, true);
        CHECK_EQ(vec <= another_vec, true);
        CHECK_EQ(vec < another_vec, false);
        CHECK_EQ(vec > another_vec, false);
        CHECK_EQ(vec <=> another_vec, std::strong_ordering::equal);
        auto moved_vec = std::move(vec);
        CHECK_EQ(moved_vec.empty(), true);
        auto another_moved_vec(std::move(another_vec));
        CHECK_EQ(another_moved_vec.empty(), true);
        stdb_vector<int> vec0(0);
        CHECK_EQ(vec0.empty(), true);
        CHECK_EQ(vec0, vec);
        stdb_vector<int> vec01(0, 1);
        CHECK_EQ(vec01.empty(), true);
        CHECK_EQ(vec01, vec);
    }

    SUBCASE("init with size (copy and move)") {
        stdb_vector<int> vec(10);
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.capacity(), 10);
        CHECK_NE(vec.begin(), vec.end());
        // NOLINTNEXTLINE
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            CHECK_EQ(*it, 0);
        }
        CHECK_NE(vec.data(), nullptr);
        CHECK_EQ(vec.max_size(), kFastVectorMaxSize / sizeof(int));
        stdb_vector<int> another_vec(3);
        CHECK_EQ(vec != another_vec, true);
        CHECK_EQ(another_vec.size(), 3);
        another_vec = std::move(vec);
        CHECK_EQ(another_vec.size(), 10);
        for (auto i : another_vec) {
            CHECK_EQ(i, 0);
        }
        auto* ptr = &another_vec;
        // check move assignment to itself
        another_vec = std::move(*ptr);
        CHECK_EQ(another_vec.size(), 10);
    }

    SUBCASE("init with size and value") {
        stdb_vector<int> vec(10, 1);
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.capacity(), 10);
        CHECK_NE(vec.data(), nullptr);
        // NOLINTNEXTLINE
        for (int v : vec) {
            CHECK_EQ(v, 1);
        }

        CHECK_EQ(vec.max_size(), kFastVectorMaxSize / sizeof(int));
        stdb_vector<int> another_vec({1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
        CHECK_EQ(vec, another_vec);
        CHECK_EQ(vec != another_vec, false);
        CHECK_EQ(vec >= another_vec, true);
        CHECK_EQ(vec <= another_vec, true);
        CHECK_EQ(vec < another_vec, false);
        CHECK_EQ(vec > another_vec, false);
        CHECK_EQ(vec <=> another_vec, std::strong_ordering::equal);
    }

    SUBCASE("init from iterators, and compare data") {
        std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        stdb_vector<int> vec(input.begin(), input.end());
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), input.size());
        CHECK_EQ(vec.capacity(), input.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            CHECK_EQ(vec[i], input[i]);
            CHECK_EQ(vec.at(i), input.at(i));
        }
        auto vec_iter = vec.begin();
        auto input_iter = input.begin();
        while (vec_iter != vec.end()) {
            CHECK_EQ(*vec_iter, *input_iter);
            ++vec_iter;
            ++input_iter;
        }
        CHECK_EQ(input_iter, input.end());

        CHECK_EQ(vec.max_size(), kFastVectorMaxSize / sizeof(int));
        CHECK_EQ(vec.front(), input.front());
        CHECK_EQ(vec.back(), input.back());

        stdb_vector<int> another_vec(vec.cbegin(), vec.cend());
        CHECK_EQ(vec, another_vec);
    }

    SUBCASE("element access") {
        const stdb::container::stdb_vector<int> input = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                                         11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        const stdb_vector<int>& vec = input;
        CHECK_EQ(*input.data(), 1);
        CHECK_EQ(*vec.data(), 1);
        CHECK_EQ(input.front(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(input.back(), 20);
        CHECK_EQ(vec.back(), 20);
    }

    SUBCASE("assign vector with single value") {
        stdb_vector<int> vec;
        vec.assign(10, 1);
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.capacity(), 10);
        CHECK_NE(vec.data(), nullptr);
        for (int v : vec) {
            CHECK_EQ(v, 1);
        }
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);

        vec.assign(200, 10);
        CHECK_EQ(vec.size(), 200);
        CHECK_EQ(vec.capacity(), 200);
        for (int v : vec) {
            CHECK_EQ(v, 10);
        }
        CHECK_EQ(vec.front(), 10);
        CHECK_EQ(vec.back(), 10);

        vec.assign(50, 5);
        CHECK_EQ(vec.size(), 50);
        // capacity will not shrink
        CHECK_EQ(vec.capacity(), 200);
        for (int v : vec) {
            CHECK_EQ(v, 5);
        }
        CHECK_EQ(vec.front(), 5);
        CHECK_EQ(vec.back(), 5);
    }

    SUBCASE("assign vector with iterators") {
        stdb_vector<int> vec;
        std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        vec.assign(input.begin(), input.end());
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), input.size());
        CHECK_EQ(vec.capacity(), input.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            CHECK_EQ(vec[i], input[i]);
            CHECK_EQ(vec.at(i), input.at(i));
        }
        auto vec_iter = vec.begin();
        auto input_iter = input.begin();
        while (vec_iter != vec.end()) {
            CHECK_EQ(*vec_iter, *input_iter);
            ++vec_iter;
            ++input_iter;
        }
    }

    SUBCASE("assign vector with different type") {
        stdb_vector<uint16_t> vec;
        std::vector<uint32_t> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
        vec.assign(input.begin(), input.end());
        CHECK_EQ(vec.size(), input.size());
        CHECK_EQ(vec.capacity(), input.size());
        for (std::size_t i = 0; i < vec.size(); i++) {
            CHECK_EQ(vec[i], input[i]);
            CHECK_EQ(vec.at(i), input.at(i));
        }
        auto vec_iter = vec.begin();
        auto input_iter = input.begin();
        while (vec_iter != vec.end()) {
            CHECK_EQ(*vec_iter, *input_iter);
            ++vec_iter;
            ++input_iter;
        }
    }

    SUBCASE("assign vector with initializer list") {
        stdb_vector<int> vec;
        vec.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.empty(), false);
        CHECK_EQ(vec.size(), 20);
        CHECK_EQ(vec.capacity(), 20);
        for (std::size_t i = 0; i < vec.size(); i++) {
            CHECK_EQ(vec[i], i + 1);
            CHECK_EQ(vec.at(i), i + 1);
        }
        auto vec_iter = vec.begin();
        for (std::size_t i = 0; i < vec.size(); i++) {
            CHECK_EQ(*vec_iter, i + 1);
            ++vec_iter;
        }
        CHECK_EQ(vec_iter, vec.end());
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 20);
    }

    SUBCASE("assign vector with set iterator") {
        std::set<int> set_to_assign = {1, 2, 3, 4, 5, 6};
        stdb_vector<int> vec;
        vec.assign(set_to_assign.begin(), set_to_assign.end());
        CHECK_EQ(vec.size(), 6);
        CHECK_EQ(vec.capacity(), 6);
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[5], 6);
    }

    SUBCASE("copy_vector") {
        stdb_vector<int> vec;
        auto vec_copy(vec);
        auto vec_copy_2 = vec;

        CHECK_EQ(vec, vec_copy);
        CHECK_EQ(vec, vec_copy_2);

        auto* ptr_vec_copy_2 = &vec_copy_2;
        vec_copy_2 = *ptr_vec_copy_2;
        CHECK_EQ(vec, vec_copy_2);
        CHECK_EQ(&vec_copy_2, ptr_vec_copy_2);

        stdb_vector<int> vec_full = {12, 3, 4, 14};
        auto vec_full_copy(vec_full);
        auto vec_full_copy_2 = vec_full;
        CHECK_EQ(vec_full, vec_full_copy);
        CHECK_EQ(vec_full, vec_full_copy_2);
        stdb_vector<int> small_vec = {1, 2, 3};
        vec_full = small_vec;
        CHECK_EQ(vec_full, small_vec);
        vec_full = vec;
        CHECK_EQ(vec_full, vec);
    }

    SUBCASE("move_vector") {
        stdb_vector<int> vec;
        stdb_vector<int> vec2;
        auto vec_move(std::move(vec));
        auto vec_move_2 = std::move(vec2);
        CHECK_EQ(vec.empty(), true);
        CHECK_EQ(vec_move.empty(), true);
        CHECK_EQ(vec_move_2.empty(), true);
        CHECK_EQ(vec, vec_move);
        CHECK_EQ(vec, vec_move_2);

        stdb_vector<int> vec_full = {12, 3, 4, 14};
        stdb_vector<int> vec_full2 = {12, 3, 4, 14};
        auto vec_full_move(std::move(vec_full));
        auto vec_full_move_2 = std::move(vec_full2);
        CHECK_EQ(vec_full.empty(), true);
        CHECK_EQ(vec_full2.empty(), true);
        CHECK_EQ(vec_full_move, vec_full_move_2);
    }

    SUBCASE("push_back") {
        stdb_vector<int> vec;
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], i);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 99);

        stdb_vector<int> vec2(100);
        for (int i = 0; i < 100; ++i) {
            vec2.push_back(i);
        }
        CHECK_EQ(vec2.size(), 200);
        CHECK_EQ(vec2.capacity(), 225);
    }

    SUBCASE("push_back_themselves") {
        stdb_vector<int> vec{1, 2, 3, 4, 5, 6, 7, 8};
        CHECK_EQ(vec.size(), 8);
        CHECK_EQ(vec.capacity(), 8);
        vec.push_back(vec.front());
        CHECK_EQ(vec.size(), 9);
        CHECK_EQ(vec.back(), 1);
        CHECK_EQ(vec.capacity(), 16);
    }

    SUBCASE("push_back_after_reserve") {
        stdb_vector<int> vec;
        vec.reserve(100);
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], i);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 99);

        stdb_vector<int> vec2(100);
        vec2.reserve(300);
        for (int i = 0; i < 100; ++i) {
            vec2.push_back(i);
        }
        CHECK_EQ(vec2.size(), 200);
        CHECK_EQ(vec2.capacity(), 300);
    }
    SUBCASE("push_back_unsafe_after_reserve") {
        stdb_vector<int> vec;
        vec.reserve(100);
        for (int i = 0; i < 100; ++i) {
            vec.push_back<Safety::Unsafe>(i);
        }
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], i);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 99);

        stdb_vector<int> vec2(100);
        vec2.reserve(300);
        for (int i = 0; i < 100; ++i) {
            vec2.push_back<Safety::Unsafe>(i);
        }
        CHECK_EQ(vec2.size(), 200);
        CHECK_EQ(vec2.capacity(), 300);
    }

    SUBCASE("clear") {
        stdb_vector<int> vec;
        vec.clear();  // check it should not crash.
        vec.assign(100, 10);
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        vec.clear();
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 100);
        vec.assign(100, 10);
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        vec.clear();
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 100);
    }

    SUBCASE("erase_by_pos") {
        stdb_vector<int> vec;
        vec.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_GE(vec.capacity(), 20);
        auto it = vec.erase(vec.begin());
        CHECK_EQ(it, vec.begin());
        CHECK_EQ(vec.size(), 19);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        it = vec.erase(vec.begin() + 5);
        CHECK_EQ(it, vec.begin() + 5);
        CHECK_EQ(vec.size(), 18);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        it = vec.erase(vec.begin() + 5, vec.begin() + 10);
        CHECK_EQ(it, vec.begin() + 10);
        CHECK_EQ(vec.size(), 13);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        auto old_end = vec.end();
        it = vec.erase(vec.cbegin(), vec.cend());
        CHECK_EQ(it, old_end);
        CHECK_EQ(vec.size(), 0);
        vec.assign({1, 2, 3, 4});
        old_end = vec.end();
        auto new_it = vec.erase(vec.begin(), vec.end());
        CHECK_EQ(new_it, old_end);
    }

    SUBCASE("pop_back") {
        stdb_vector<int> vec;
        vec.assign({1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
                    61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
                    81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100});
        CHECK_EQ(vec.size(), 100);
        CHECK_EQ(vec.capacity(), 100);
        vec.pop_back();
        CHECK_EQ(vec.size(), 99);
        CHECK_EQ(vec.capacity(), 100);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 99);
        vec.pop_back();
        CHECK_EQ(vec.size(), 98);
        CHECK_EQ(vec.capacity(), 100);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 98);
        vec.pop_back();
        CHECK_EQ(vec.size(), 97);
        CHECK_EQ(vec.capacity(), 100);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 97);
    }

    SUBCASE("resize") {
        stdb_vector<int> vec;
        vec.resize(100);
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], 0);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 0);

        vec.resize(50);
        CHECK_EQ(vec.size(), 50);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], 0);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 0);

        vec.resize(150);
        CHECK_EQ(vec.size(), 150);
        CHECK_GE(vec.capacity(), 150);
        for (int i = 0; i < 50; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], 0);
        }
        CHECK_EQ(vec.front(), 0);
        CHECK_EQ(vec.back(), 0);

        vec.resize(0);
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 150);
    }

    SUBCASE("resize_unsafe") {
        stdb_vector<int> vec;
        vec.resize(100, 444);

        CHECK_EQ(vec.size(), 100);
        CHECK_EQ(vec[4], 444);
        vec.resize<Safety::Unsafe>(200);
        CHECK_NE(vec[140], 444);
    }

    SUBCASE("resize with value") {
        stdb_vector<int> vec;
        vec.resize(100, 10);
        CHECK_EQ(vec.size(), 100);
        CHECK_GE(vec.capacity(), 100);
        for (int i = 0; i < 100; ++i) {
            CHECK_EQ(vec[static_cast<std::size_t>(i)], 10);
        }
        CHECK_EQ(vec.front(), 10);
        CHECK_EQ(vec.back(), 10);

        vec.resize(50, 100);
        CHECK_EQ(vec.size(), 50);
        CHECK_GE(vec.capacity(), 100);
        CHECK_EQ(vec.front(), 10);
        CHECK_EQ(vec.back(), 10);
    }

    SUBCASE("reserve") {
        stdb_vector<int> vec;
        vec.reserve(100);
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 100);
        vec.reserve(50);
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 100);
    }

    SUBCASE("growth") {
        stdb_vector<int> vec;
        CHECK_EQ(vec.capacity(), 0);
        vec.push_back(1);
        // kFastVectorInitialCapacity = 64
        // sizeof int == 4
        CHECK_EQ(vec.capacity(), 16);
        vec.push_back(1);
        CHECK_EQ(vec.capacity(), 16);
        for (int i = 0; i < 14; ++i) {
            vec.push_back(1);
        }
        CHECK_EQ(vec.capacity(), 16);
        vec.push_back(1);
        CHECK_EQ(vec.capacity(), 24);

        stdb_vector<int> huge(8 * 4096, 3);
        CHECK_EQ(huge.capacity(), 8 * 4096);
        huge.push_back(1);
        CHECK_EQ(huge.capacity(), 16 * 4096);
    }

    SUBCASE("shrink_to_fit") {
        stdb_vector<int> vec;
        vec.reserve(100);
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 100);
        vec.shrink_to_fit();
        CHECK_EQ(vec.size(), 0);
        CHECK_EQ(vec.capacity(), 0);

        vec.reserve(1000);
        CHECK_EQ(vec.size(), 0);
        CHECK_EQ(vec.capacity(), 1000);
        vec.push_back(1);
        vec.push_back(2);
        vec.shrink_to_fit();
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.capacity(), 2);
    }
    SUBCASE("swap") {
        stdb_vector<int> vec1;
        stdb_vector<int> vec2;
        vec1.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        vec2.assign({21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
                     61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
                     81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100});
        CHECK_EQ(vec1.size(), 20);
        CHECK_EQ(vec2.size(), 80);
        CHECK_GE(vec1.capacity(), 20);
        CHECK_GE(vec2.capacity(), 80);
        vec1.swap(vec2);
        CHECK_EQ(vec1.size(), 80);
        CHECK_EQ(vec2.size(), 20);
        CHECK_GE(vec1.capacity(), 80);
        CHECK_GE(vec2.capacity(), 20);
        CHECK_EQ(vec1.front(), 21);
        CHECK_EQ(vec1.back(), 100);
        CHECK_EQ(vec2.front(), 1);
        CHECK_EQ(vec2.back(), 20);

        stdb_vector<int> vec_full(4, 100);
        vec_full.shrink_to_fit();
        CHECK_EQ(vec_full.size(), 4);
        CHECK_EQ(vec_full.capacity(), 4);
        CHECK_EQ(vec_full.front(), 100);
        CHECK_EQ(vec_full.back(), 100);
        for (int i = 0; i < 4; ++i) {
            CHECK_EQ(vec_full[static_cast<std::size_t>(i)], 100);
        }
        stdb_vector<int> z;
        std::swap(z, vec_full);
        CHECK_EQ(z.size(), 4);
        CHECK_EQ(z.capacity(), 4);
        CHECK_EQ(z.front(), 100);
        CHECK_EQ(z.back(), 100);
        for (int i = 0; i < 4; ++i) {
            CHECK_EQ(z[static_cast<std::size_t>(i)], 100);
        }
        CHECK_EQ(vec_full.size(), 0);
        CHECK_EQ(vec_full.capacity(), 0);
    }

    SUBCASE("insert_with_single_value") {
        stdb_vector<int> vec;
        vec.insert(vec.cbegin(), 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.cbegin(), 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.cend(), 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert(vec.cbegin() + 1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        vec.insert(vec.cend() - 1, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[3], 5);
        vec.insert(vec.end(), 6);
        CHECK_EQ(vec.back(), 6);
    }

    SUBCASE("insert_unsafe_with_single_value") {
        stdb_vector<int> vec;
        vec.reserve(10);
        vec.insert<Safety::Unsafe>(vec.cbegin(), 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.cbegin(), 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.cend(), 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert<Safety::Unsafe>(vec.cbegin() + 1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        vec.insert<Safety::Unsafe>(vec.cend() - 1, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[3], 5);
    }

    SUBCASE("insert_with_multi_values") {
        stdb_vector<int> vec;
        vec.insert(vec.cbegin(), 3, 1);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.cbegin(), 2, 2);
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.cend(), 4, 3);
        CHECK_EQ(vec.size(), 9);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert(vec.cbegin() + 1, 5, 4);
        CHECK_EQ(vec.size(), 14);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        CHECK_EQ(vec[5], 4);
        vec.insert(vec.cend() - 1, 6, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[13], 5);
    }

    SUBCASE("insert_unsafe_with_multi_values") {
        stdb_vector<int> vec;
        vec.reserve(20);
        vec.insert<Safety::Unsafe>(vec.cbegin(), 3, 1);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.cbegin(), 2, 2);
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.cend(), 4, 3);
        CHECK_EQ(vec.size(), 9);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert<Safety::Unsafe>(vec.cbegin() + 1, 5, 4);
        CHECK_EQ(vec.size(), 14);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        CHECK_EQ(vec[5], 4);
        vec.insert<Safety::Unsafe>(vec.cend() - 1, 6, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[13], 5);
    }

    SUBCASE("insert_with_initializer_list") {
        stdb_vector<int> vec;
        vec.insert(vec.cbegin(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 10);
        vec.insert(vec.cbegin(), {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 10);
        vec.insert(vec.cend(), {21, 22, 23, 24, 25, 26, 27, 28, 29, 30});
        CHECK_EQ(vec.size(), 30);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        vec.insert(vec.cbegin() + 10, {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        CHECK_EQ(vec.size(), 40);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[10], 31);
        vec.insert(vec.cend() - 1, {12, 44});
        CHECK_EQ(vec.size(), 42);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[40], 44);
    }

    SUBCASE("insert_unsafe_with_initializer_list") {
        stdb_vector<int> vec;
        vec.reserve(100);
        vec.insert<Safety::Unsafe>(vec.cbegin(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 10);
        vec.insert<Safety::Unsafe>(vec.cbegin(), {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 10);
        vec.insert<Safety::Unsafe>(vec.cend(), {21, 22, 23, 24, 25, 26, 27, 28, 29, 30});
        CHECK_EQ(vec.size(), 30);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        vec.insert<Safety::Unsafe>(vec.cbegin() + 10, {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        CHECK_EQ(vec.size(), 40);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[10], 31);
        vec.insert<Safety::Unsafe>(vec.cend() - 1, {12, 44});
        CHECK_EQ(vec.size(), 42);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[40], 44);
    }

    SUBCASE("insert_from_another_vector") {
        stdb_vector<int> vec1;
        stdb_vector<int> vec2;
        std::vector<int> vec3 = {11, 22, 33, 44, 55, 66, 77, 88, 99, 100};
        vec1.insert(vec1.cbegin(), vec2.begin(), vec2.end());
        CHECK_EQ(vec1.size(), 0);
        vec1.insert(vec1.cbegin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1.size(), 10);
        vec2.insert(vec2.cbegin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1, vec2);
        std::vector<int> vec4 = {100, 100};
        vec1.insert(vec1.cend(), vec4.begin(), vec4.end());
        CHECK_EQ(vec1.size(), 12);
        CHECK_EQ(vec1.back(), 100);
        std::vector<int> vec5 = {200, 200};
        vec1.insert(vec1.cend() - 1, vec5.begin(), vec5.end());
        CHECK_EQ(vec1.size(), 14);
        CHECK_EQ(vec1.back(), 100);
        CHECK_EQ(vec1[12], 200);
    }

    SUBCASE("insert_unsafe_from_another_vector") {
        stdb_vector<int> vec1;
        vec1.reserve(100);
        stdb_vector<int> vec2;
        vec2.reserve(100);
        std::vector<int> vec3 = {11, 22, 33, 44, 55, 66, 77, 88, 99, 100};
        vec1.insert<Safety::Unsafe>(vec1.cbegin(), vec2.begin(), vec2.end());
        CHECK_EQ(vec1.size(), 0);
        vec1.insert<Safety::Unsafe>(vec1.cbegin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1.size(), 10);
        vec2.insert<Safety::Unsafe>(vec2.cbegin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1, vec2);
        std::vector<int> vec4 = {100, 100};
        vec1.insert<Safety::Unsafe>(vec1.cend(), vec4.begin(), vec4.end());
        CHECK_EQ(vec1.size(), 12);
        CHECK_EQ(vec1.back(), 100);
        std::vector<int> vec5 = {200, 200};
        vec1.insert<Safety::Unsafe>(vec1.cend() - 1, vec5.begin(), vec5.end());
        CHECK_EQ(vec1.size(), 14);
        CHECK_EQ(vec1.back(), 100);
        CHECK_EQ(vec1[12], 200);
    }

    SUBCASE("emplace") {
        stdb_vector<int> vec;
        vec.emplace(vec.begin(), 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.emplace(vec.begin(), 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.emplace(vec.end(), 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.emplace(vec.begin() + 1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
    }

    SUBCASE("emplace_with_index") {
        stdb_vector<int> vec;
        vec.emplace(0, 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.emplace(0, 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.emplace(2, 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.emplace(1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
    }

    SUBCASE("cmp") {
        // either vec is empty means equal
        stdb_vector<int> vec1;
        stdb_vector<int> vec2;
        CHECK_EQ(vec1 == vec2, true);
        CHECK_EQ(vec1 != vec2, false);
        CHECK_EQ(vec1 < vec2, false);
        CHECK_EQ(vec1 <= vec2, true);
        CHECK_EQ(vec1 > vec2, false);
        CHECK_EQ(vec1 >= vec2, true);
        CHECK_EQ(vec1 <=> vec2, std::strong_ordering::equal);
        CHECK_EQ(vec2 <=> vec1, std::strong_ordering::equal);
        vec1.push_back(1);
        CHECK_EQ(vec1 == vec2, false);
        CHECK_EQ(vec1 != vec2, true);
        CHECK_EQ(vec1 < vec2, false);
        CHECK_EQ(vec1 <= vec2, false);
        CHECK_EQ(vec1 > vec2, true);
        CHECK_EQ(vec1 >= vec2, true);
        CHECK_EQ(vec1 <=> vec2, std::strong_ordering::greater);
        CHECK_EQ(vec2 <=> vec1, std::strong_ordering::less);
        vec2.push_back(1);
        CHECK_EQ(vec1 == vec2, true);
        CHECK_EQ(vec1 != vec2, false);
        CHECK_EQ(vec1 < vec2, false);
        CHECK_EQ(vec1 <= vec2, true);
        CHECK_EQ(vec1 > vec2, false);
        CHECK_EQ(vec1 >= vec2, true);

        stdb_vector<int> vec3 = {1, 2, 3, 4, 5};
        stdb_vector<int> vec4 = {1, 2, 3, 4, 5};
        CHECK_EQ(vec3 == vec4, true);
        CHECK_EQ(vec3 != vec4, false);
        CHECK_EQ(vec3 < vec4, false);
        CHECK_EQ(vec3 <= vec4, true);
        CHECK_EQ(vec3 > vec4, false);
        CHECK_EQ(vec3 >= vec4, true);
        CHECK_EQ(vec3 <=> vec4, std::strong_ordering::equal);
        CHECK_EQ(vec4 <=> vec3, std::strong_ordering::equal);

        vec4[2] = 8;
        CHECK_EQ(vec3 == vec4, false);
        CHECK_EQ(vec3 != vec4, true);
        CHECK_EQ(vec3 >= vec4, false);
        CHECK_EQ(vec3 > vec4, false);
        CHECK_EQ(vec3 <= vec4, true);
        CHECK_EQ(vec3 < vec4, true);
        CHECK_EQ(vec3 <=> vec4, std::strong_ordering::less);
        CHECK_EQ(vec4 <=> vec3, std::strong_ordering::greater);
        CHECK_EQ(vec4 > vec3, true);
        CHECK_EQ(vec4 >= vec3, true);
        CHECK_EQ(vec4 < vec3, false);
        CHECK_EQ(vec4 <= vec3, false);
    }

    SUBCASE("erase_by_value") {
        stdb_vector<int> vec = {1, 2, 5, 4, 5, 6, 7, 5, 9, 10};
        auto num = std::erase(vec, 5);
        CHECK_EQ(num, 3);
        CHECK_EQ(vec.size(), 7);
        stdb_vector<int> result_vec = {1, 2, 4, 6, 7, 9, 10};
        CHECK_EQ(vec == result_vec, true);
    }

    SUBCASE("erase_if") {
        stdb_vector<int> vec = {1, 2, 5, 4, 5, 6, 7, 5, 9, 10};
        auto num = std::erase_if(vec, [](int i) { return i % 2 == 0; });
        CHECK_EQ(num, 4);
        CHECK_EQ(vec.size(), 6);
        stdb_vector<int> result_vec = {1, 5, 5, 7, 5, 9};
        CHECK_EQ(vec == result_vec, true);
    }
    SUBCASE("fmt") {
        stdb_vector<int> vec = {1, 2, 3, 4, 5};
        std::string str = fmt::format("{}", vec);
        CHECK_EQ(str, "[1, 2, 3, 4, 5]");
    }
}

TEST_CASE("Hilbert::stdb_vector::memory::string") {
    SUBCASE("default_constructor") {
        stdb_vector<memory::string> vec;
        CHECK_EQ(vec.size(), 0);
        CHECK_EQ(vec.capacity(), 0);
    }
    SUBCASE("constructor_followed_with_reserve") {
        stdb_vector<memory::string> vec;
        vec.reserve(10);
        CHECK_EQ(vec.size(), 0);
        CHECK_GE(vec.capacity(), 10);
    }
    SUBCASE("constructor_with_size_and_value") {
        stdb_vector<memory::string> vec(10, "hello");
        CHECK_EQ(vec.size(), 10);
        CHECK_GE(vec.capacity(), 10);
        for (const auto& s : vec) {
            CHECK_EQ(s, "hello");
        }
    }
    SUBCASE("constructor_with_initializer_list") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.size(), 3);
        CHECK_GE(vec.capacity(), 3);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
    }
    SUBCASE("constructor_with_another_vector") {
        stdb_vector<memory::string> vec1 = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2(vec1);
        CHECK_EQ(vec1.size(), 3);
        CHECK_GE(vec1.capacity(), 3);
        CHECK_EQ(vec1[0], "hello");
        CHECK_EQ(vec1[1], "world");
        CHECK_EQ(vec1[2], "!");
        CHECK_EQ(vec2.size(), 3);
        CHECK_GE(vec2.capacity(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
    }
    SUBCASE("constructor_with_another_vector_with_move") {
        stdb_vector<memory::string> vec1 = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2(std::move(vec1));
        CHECK_EQ(vec1.size(), 0);
        CHECK_EQ(vec1.capacity(), 0);
        CHECK_EQ(vec2.size(), 3);
        CHECK_GE(vec2.capacity(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
    }
    SUBCASE("constructor_with_another_vector_from_iterators") {
        stdb_vector<memory::string> vec1 = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2(vec1.begin(), vec1.end());
        CHECK_EQ(vec1.size(), 3);
        CHECK_GE(vec1.capacity(), 3);
        CHECK_EQ(vec1[0], "hello");
        CHECK_EQ(vec1[1], "world");
        CHECK_EQ(vec1[2], "!");
        CHECK_EQ(vec2.size(), 3);
        CHECK_GE(vec2.capacity(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
    }
    SUBCASE("operator = ") {
        stdb_vector<memory::string> vec1 = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2;
        vec2 = vec1;
        CHECK_EQ(vec1.size(), 3);
        CHECK_GE(vec1.capacity(), 3);
        CHECK_EQ(vec1[0], "hello");
        CHECK_EQ(vec1[1], "world");
        CHECK_EQ(vec1[2], "!");
        CHECK_EQ(vec2.size(), 3);
        CHECK_GE(vec2.capacity(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
    }
    SUBCASE("assign_with_iterators") {
        stdb_vector<memory::string> vec1 = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2;
        vec2.assign(vec1.begin(), vec1.end());
        CHECK_EQ(vec1.size(), 3);
        CHECK_GE(vec1.capacity(), 3);
        CHECK_EQ(vec1[0], "hello");
        CHECK_EQ(vec1[1], "world");
        CHECK_EQ(vec1[2], "!");
        CHECK_EQ(vec2.size(), 3);
        CHECK_GE(vec2.capacity(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
        stdb_vector<memory::string> vec3 = {"1", "2"};
        vec2.assign(vec3.cbegin(), vec3.cend());
        CHECK_EQ(vec3, vec2);
    }
    SUBCASE("assign_with_initializer_list") {
        stdb_vector<memory::string> vec;
        vec.assign({"hello", "world", "!"});
        CHECK_EQ(vec.size(), 3);
        CHECK_GE(vec.capacity(), 3);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
    }
    SUBCASE("assign_with_size_and_value") {
        stdb_vector<memory::string> vec;
        vec.assign(10, "hello");
        CHECK_EQ(vec.size(), 10);
        CHECK_GE(vec.capacity(), 10);
        for (const auto& s : vec) {
            CHECK_EQ(s, "hello");
        }
    }
    SUBCASE("at") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.at(0), "hello");
        CHECK_EQ(vec.at(1), "world");
        CHECK_EQ(vec.at(2), "!");
    }
    SUBCASE("operator []") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
    }
    SUBCASE("front") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.front(), "hello");
    }
    SUBCASE("back") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.back(), "!");
    }
    SUBCASE("data") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.data()[0], "hello");
        CHECK_EQ(vec.data()[1], "world");
        CHECK_EQ(vec.data()[2], "!");
    }
    SUBCASE("begin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.begin(), "hello");
    }
    SUBCASE("cbegin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.cbegin(), "hello");
        const stdb_vector<memory::string>& cvec = vec;
        CHECK_EQ(*cvec.begin(), "hello");
    }
    SUBCASE("end") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.end() - 1), "!");
    }
    SUBCASE("cend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.cend() - 1), "!");
        const stdb_vector<memory::string> vec2 = {"hello", "world", "!"};
        CHECK_EQ(*(vec2.end() - 1), "!");
    }
    SUBCASE("rbegin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.rbegin(), "!");
    }
    SUBCASE("crbegin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.crbegin(), "!");
        const stdb_vector<memory::string> vec2 = {"hello", "world", "!"};
        CHECK_EQ(*vec2.rbegin(), "!");
    }
    SUBCASE("rend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.rend() - 1), "hello");
    }
    SUBCASE("crend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.crend() - 1), "hello");
        const stdb_vector<memory::string> vec2 = {"hello", "world", "!"};
        CHECK_EQ(*(vec2.rend() - 1), "hello");
    }
    SUBCASE("empty") {
        stdb_vector<memory::string> vec;
        CHECK(vec.empty());
        vec.push_back({"hello"});
        CHECK(!vec.empty());
    }
    SUBCASE("size") {
        stdb_vector<memory::string> vec;
        CHECK_EQ(vec.size(), 0);
        vec.push_back("hello");
        CHECK_EQ(vec.size(), 1);
    }
    SUBCASE("max_size") {
        stdb_vector<memory::string> vec;
        CHECK_EQ(vec.max_size(), kFastVectorMaxSize / sizeof(memory::string));
    }
    SUBCASE("capacity") {
        stdb_vector<memory::string> vec;
        CHECK_EQ(vec.capacity(), 0);
        vec.push_back("hello");
        CHECK_EQ(vec.capacity(), kFastVectorDefaultCapacity / sizeof(memory::string));
        vec.push_back("world");
        CHECK_EQ(vec.capacity(), 2);
        vec.push_back("!");
        CHECK_GT(vec.capacity(), kFastVectorDefaultCapacity / sizeof(memory::string));
    }
    SUBCASE("reserve") {
        stdb_vector<memory::string> vec;
        vec.reserve(100);
        CHECK_GE(vec.capacity(), 100);
    }
    SUBCASE("shrink_to_fit") {
        stdb_vector<memory::string> vec;
        vec.reserve(100);
        CHECK_GE(vec.capacity(), 100);
        vec.shrink_to_fit();
        CHECK_GE(vec.capacity(), 0UL);
    }
    SUBCASE("clear") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.size(), 3);
        vec.clear();
        CHECK_EQ(vec.size(), 0);
    }
    SUBCASE("insert_with_single_element") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.insert(vec.cbegin() + 1, "inserted");
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "world");
        CHECK_EQ(vec[3], "!");
        vec.insert(vec.cend(), "inserted");
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[4], "inserted");
        vec.insert(vec.cend() - 1, "end-1");
        CHECK_EQ(vec.size(), 6);
        CHECK_EQ(vec[4], "end-1");

        stdb::memory::string str = "inserted";
        vec.insert(vec.cend() - 1, str);
        CHECK_EQ(vec.size(), 7);
        CHECK_EQ(vec[5], "inserted");
    }
    SUBCASE("insert_with_multiple_elements") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.insert(vec.cbegin() + 1, 2, memory::string("inserted"));
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
    }
    SUBCASE("insert_with_initializer_list") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.insert(vec.cbegin() + 1, {"inserted", "inserted"});
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
        vec.insert(vec.cend(), {"final"});
        CHECK_EQ(vec.size(), 6);
        CHECK_EQ(vec[5], "final");
        vec.insert(vec.cend() - 1, {"middle"});
        CHECK_EQ(vec[5], "middle");
        CHECK_EQ(vec[6], "final");
    }
    SUBCASE("insert_with_range") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        std::vector<memory::string> vec2 = {"inserted", "inserted"};
        vec.insert(vec.cbegin() + 1, vec2.begin(), vec2.end());
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
        vec.insert(vec.cend(), vec2.begin(), vec2.end());
        CHECK_EQ(vec.size(), 7);
        CHECK_EQ(vec[5], "inserted");
        CHECK_EQ(vec[6], "inserted");
        std::vector<memory::string> vec3 = {"final"};
        vec.insert(vec.cend() - 1, vec3.begin(), vec3.end());
        CHECK_EQ(vec.size(), 8);
        CHECK_EQ(vec[6], "final");
        CHECK_EQ(vec[7], "inserted");
    }
    SUBCASE("erase_with_single_element") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.erase(vec.begin() + 1);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "!");
        vec.erase(vec.cbegin());
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "!");
    }
    SUBCASE("erase_with_range") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.erase(vec.begin() + 1, vec.end());
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "hello");
        vec.assign({"hello", "word", "?"});
        CHECK_EQ(vec.size(), 3);
        vec.erase(vec.begin(), vec.begin());
        CHECK_EQ(vec.size(), 3);
        vec.erase(vec.cend(), vec.cend());
        CHECK_EQ(vec.size(), 3);
    }
    SUBCASE("push_back") {
        stdb_vector<memory::string> vec;
        vec.push_back({"hello"});
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "hello");
    }

    SUBCASE("push_back_themselves") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.capacity(), 3);
        vec.push_back(vec.back());
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.capacity(), 5);

        for (auto str : vec) {
            std::cout << str << std::endl;
        }
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
        CHECK_EQ(vec[3], "!");
    }

    SUBCASE("emplace_back") {
        stdb_vector<memory::string> vec1;
        auto& str = vec1.emplace_back("hello");
        CHECK_EQ(vec1.size(), 1);
        CHECK_EQ(vec1[0], "hello");
        CHECK_EQ(str, "hello");

        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.capacity(), 3);
        auto& str1 = vec.emplace_back(vec.back());
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.capacity(), 5);
        CHECK_EQ(str1, "!");
    }

    SUBCASE("push_back_unsafe") {
        stdb_vector<memory::string> vec;
        vec.reserve(5);
        vec.push_back<Safety::Unsafe>({"hello"});
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "hello");
    }

    SUBCASE("emplace_back_unsafe") {
        stdb_vector<memory::string> vec;
        vec.reserve(5);
        vec.emplace_back<Safety::Unsafe>("hello");
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "hello");
    }
    SUBCASE("pop_back") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(vec.size(), 3);
        vec.pop_back();
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
    }
    SUBCASE("resize") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.resize(5);
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
        CHECK_EQ(vec[3], "");
        CHECK_EQ(vec[4], "");
    }
    SUBCASE("resize_with_value") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.resize(5, "value");
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
        CHECK_EQ(vec[3], "value");
        CHECK_EQ(vec[4], "value");
    }
    SUBCASE("swap") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        stdb_vector<memory::string> vec2 = {"inserted", "inserted"};
        vec.swap(vec2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "inserted");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec2.size(), 3);
        CHECK_EQ(vec2[0], "hello");
        CHECK_EQ(vec2[1], "world");
        CHECK_EQ(vec2[2], "!");
    }
    SUBCASE("emplace") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.emplace(vec.begin() + 1, "inserted");
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "world");
        CHECK_EQ(vec[3], "!");
        vec.emplace(vec.end(), "end");
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec.back(), "end");
    }

#ifndef __APPLE__
    SUBCASE("get_span") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        std::span<memory::string> span(vec.begin(), vec.size());
        CHECK_EQ(span.size(), 3);
        CHECK_EQ(span[0], "hello");
        CHECK_EQ(span[1], "world");
        CHECK_EQ(span[2], "!");
    }
#endif
    SUBCASE("erase_by_value") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        auto num = vec.erase("world");
        CHECK_EQ(num, 1);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "!");
    }
    SUBCASE("erase_by_value_not_found") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        auto num = vec.erase("not found");
        CHECK_EQ(num, 0);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
    }
    SUBCASE("erase_by_value_get_more_than_1") {
        stdb_vector<memory::string> vec = {"hello", "!", "world", "hello"};
        auto num = vec.erase("hello");
        CHECK_EQ(num, 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "!");
        CHECK_EQ(vec[1], "world");
    }
    SUBCASE("erase_if") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        auto num = vec.erase_if([](const memory::string& str) { return str == "world"; });
        CHECK_EQ(num, 1);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "!");
    }
    SUBCASE("erase_if_not_found") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        auto num = vec.erase_if([](const memory::string& str) { return str == "not found"; });
        CHECK_EQ(num, 0);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "world");
        CHECK_EQ(vec[2], "!");
    }
    SUBCASE("erase_if_get_more_than_1") {
        stdb_vector<memory::string> vec = {"hello", "!", "world", "hello"};
        auto num = vec.erase_if([](const memory::string& str) { return str == "hello"; });
        CHECK_EQ(num, 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec[0], "!");
        CHECK_EQ(vec[1], "world");
    }
    SUBCASE("format") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        fmt::print("==={}===", vec);
    }
    SUBCASE("assign_empty") {
        auto a = memory::string("");
        auto b = stdb_vector<char>(0);
        b.assign(a.begin(), a.end());
        CHECK_EQ(b.size(), 0);
        CHECK_EQ(b.capacity(), 0);
        CHECK_EQ(b.data(), nullptr);
        CHECK_EQ(b.empty(), true);
    }
    SUBCASE("init from empty range") {
        auto a = memory::string("");
        auto b = stdb_vector<char>(a.begin(), a.end());
        CHECK_EQ(b.size(), 0);
        CHECK_EQ(b.capacity(), 0);
        CHECK_EQ(b.data(), nullptr);
        CHECK_EQ(b.empty(), true);
    }
    SUBCASE("init from empty initializaer list") {
        auto b = stdb_vector<char>({});
        CHECK_EQ(b.size(), 0);
        CHECK_EQ(b.capacity(), 0);
        CHECK_EQ(b.data(), nullptr);
        CHECK_EQ(b.empty(), true);
    }
}

class non_movable
{
   private:
    int* ptr;

   public:
    non_movable() : ptr(new int(0)) { std::cout << "non_movable constructor default" << std::endl; }
    non_movable(int i) : ptr(new int(i)) { std::cout << "non_movable constructor with : " << i << std::endl; }
    non_movable(const non_movable& rhs) : ptr(new int(*rhs.ptr)) {
        std::cout << "copy non_movable with : " << *ptr << std::endl;
    }
    non_movable(non_movable&&) = delete;
    non_movable& operator=(const non_movable&) = delete;
    non_movable& operator=(non_movable&&) = delete;
    auto operator==(const int rhs) const -> bool { return *ptr == rhs; }
    auto operator==(const non_movable& rhs) const -> bool { return *ptr == *rhs.ptr; }
    void print() const { std::cout << *ptr << std::endl; }
    ~non_movable() {
        std::cout << "non_movable destructor with : " << *ptr << std::endl;
        delete ptr;
    }
};

class non_copyable
{
   private:
    int* ptr;
    int size;

   public:
    non_copyable() : ptr(new int(0)), size(0) { std::cout << "non_copyable constructor default" << std::endl; }
    non_copyable(int i) : ptr(new int(i)), size(0) {
        std::cout << "non_copyable constructor with : " << i << std::endl;
    }
    non_copyable(int v, int s) : ptr(new int(v)), size(s) {
        std::cout << "non_copyable constructor with : " << v << " and size : " << s << std::endl;
    }
    non_copyable(const non_copyable&) = delete;
    non_copyable(non_copyable&& rhs) noexcept : ptr(rhs.ptr) {
        std::cout << "move non_copyable with : " << *ptr << std::endl;
        rhs.ptr = nullptr;
    }
    non_copyable& operator=(const non_copyable&) = delete;
    non_copyable& operator=(non_copyable&&) = delete;
    auto operator==(const int rhs) const -> bool { return *ptr == rhs; }
    void print() const { std::cout << *ptr << std::endl; }
    ~non_copyable() {
        if (ptr) {
            std::cout << "non_copyable destructor with : " << *ptr << std::endl;
        } else {
            std::cout << "non_copyable destructor with : nullptr" << std::endl;
        }

        delete ptr;
    }
};

TEST_CASE("Hilbert::stdb_vector::non_movable") {
    stdb_vector<non_movable> vec;
    vec.reserve(10);
    for (int i = 0; i < 10; i++) {
        vec.emplace_back(i);
    }
    non_movable nm(1000);
    vec.insert(vec.cbegin(), nm);
    CHECK_EQ(vec.size(), 11);

    vec.insert(vec.cend(), nm);
    CHECK_EQ(vec.size(), 12);
    vec.erase(vec.begin());
    CHECK_EQ(vec.size(), 11);
    vec.insert(vec.cbegin(), nm);
    /*
    for (auto& non : vec) {
        non.print();
    }
    */
    CHECK_EQ(vec[0], 1000);
    CHECK_EQ(vec[11] == 1000, true);
    for (int i = 1; i < 10; ++i) {
        CHECK_EQ(vec[static_cast<std::size_t>(i)] == i - 1, true);
    }
}

TEST_CASE("Hilbert::non_movable.erase by value") {
    stdb_vector<non_movable> vector = {1, 3, 3, 4, 5, 6, 7, 8, 9, 3};
    non_movable nm(5);
    auto num = vector.erase(nm);
    CHECK_EQ(num, 1);
    CHECK_EQ(vector.size(), 9);
    non_movable nm3(3);
    num = vector.erase(nm3);
    CHECK_EQ(num, 3);
    CHECK_EQ(vector.size(), 6);
    stdb_vector<non_movable> result = {1, 4, 6, 7, 8, 9};
    CHECK_EQ(vector, result);
}
TEST_CASE("Hilbert::non_movable.erase_if") {
    stdb_vector<non_movable> vector = {1, 3, 3, 4, 5, 6, 7, 8, 9, 3};
    auto num = vector.erase_if([](const non_movable& nm) { return nm == 3; });
    CHECK_EQ(num, 3);
    CHECK_EQ(vector.size(), 7);
    stdb_vector<non_movable> result = {1, 4, 5, 6, 7, 8, 9};
    CHECK_EQ(vector, result);
}

TEST_CASE("Hilbert::stdb_vector::non_copyable") {
    stdb_vector<non_copyable> vec;
    vec.reserve(20);
    for (int i = 0; i < 10; i++) {
        non_copyable nc(i);
        vec.push_back(std::move(nc));
    }
    for (int i = 0; i < 10; i++) {
        vec.emplace_back(i, 10);
    }

    vec.emplace(vec.begin(), 10, 1000);

    vec.insert(vec.cbegin(), non_copyable(1000));
    CHECK_EQ(vec.size(), 22);

    vec.insert(vec.cend(), non_copyable(1000));
    CHECK_EQ(vec.size(), 23);
    for (auto& non : vec) {
        non.print();
    }
    CHECK_EQ(vec[0] == 1000, true);
    CHECK_EQ(vec[1] == 10, true);
    CHECK_EQ(vec[22] == 1000, true);
    for (int i = 2; i < 12; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)] == i - 2, true);
    }
    for (int i = 12; i < 22; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)] == i - 12, true);
    }
    vec.reserve(20);
    non_copyable nc(1000);
    vec.emplace_back(std::move(nc));
    non_copyable nc2(1000);
    vec.emplace_back<Safety::Unsafe>(std::move(nc2));
}

TEST_CASE("Hilbert::stdb_vector::std_vector_push") {
    uint32_t times = 100;
    memory::string str{"123456789012344567890"};
    std::vector<memory::string> vec;
    stdb_vector<memory::string> stdb_vec(times);
    vec.reserve(times);
    for (uint32_t i = 0; i < times; ++i) {
        vec.emplace_back(str);
        stdb_vec.emplace_back(str);
    }
}
TEST_CASE("Hilbert::stdb_vector::fill") {
    stdb_vector<int> vec;
    vec.reserve(200);
    auto fill = [](int* ptr) -> std::size_t {
        if (ptr != nullptr) {
            for (int i = 0; i < 70; ++i) {
                ptr[i] = i;
            }
        }
        return 70;
    };
    vec.fill<Safety::Unsafe>(fill);
    CHECK_EQ(vec.size(), 70);
    for (int i = 0; i < 70; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)], i);
    }
    vec.fill(fill);
    CHECK_EQ(vec.size(), 140);
    for (int i = 0; i < 140; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)], i % 70);
    }
    vec.fill(fill);
    CHECK_EQ(vec.size(), 210);
    for (int i = 0; i < 210; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)], i % 70);
    }
}

TEST_CASE("Hilbert::stdb_vector::get_writebuffer") {
    stdb_vector<int> vec;
    vec.reserve(1000);
    auto buffer = vec.get_writebuffer<Safety::Unsafe>(40);
    CHECK_EQ(buffer.size(), 40);
    CHECK_EQ(vec.size(), 40);
    CHECK_EQ(vec.capacity(), 1000);
    auto buffer1 = vec.get_writebuffer(100);
    CHECK_EQ(buffer1.size(), 100);
    CHECK_EQ(vec.size(), 140);
    CHECK_EQ(vec.capacity(), 1000);
    auto buffer2 = vec.get_writebuffer(2000);
    CHECK_EQ(buffer2.size(), 2000);
    CHECK_EQ(vec.size(), 2140);
}

struct relocate
{
    int x;
    int y;
};

class normal_class
{
   private:
    [[maybe_unused]] int x;
    [[maybe_unused]] int* ptr;

   public:
    normal_class() : x(0), ptr(new int(5)) { std::cout << "normal_class constructor default" << std::endl; }
    normal_class(const normal_class&) = delete;
    ~normal_class() {
        std::cout << "normal_class destructor" << std::endl;
        delete ptr;
    }
};

class normal_class_with_traits
{
   private:
    [[maybe_unused]] int x;
    [[maybe_unused]] int* ptr;

   public:
    normal_class_with_traits() : x(0), ptr(new int(5)) { std::cout << "normal_class constructor default" << std::endl; }
    normal_class_with_traits(const normal_class&) = delete;
};

static_assert(IsRelocatable<int>, "int should be relocatable");
static_assert(IsRelocatable<double>, "double should be relocatable");
static_assert(IsRelocatable<relocate>, "relocate should be relocatable");
static_assert(!IsRelocatable<std::string>, "std::string should not be relocatable");
static_assert(!IsRelocatable<stdb::memory::string>, "stdb::memory::string should not be relocatable");
static_assert(!IsRelocatable<normal_class>, "normal_class should not be relocatable");

static_assert(IsZeroInitable<int>, "int should be zero copyable");
static_assert(IsZeroInitable<double>, "double should be zero copyable");
static_assert(IsZeroInitable<relocate>, "relocate should be zero copyable");
static_assert(!IsZeroInitable<std::string>, "std::string should not be zero copyable");
static_assert(!IsZeroInitable<stdb::memory::string>, "stdb::memory::string should not be zero copyable");

TEST_CASE_TEMPLATE("Hilbert::iterator test", T, stdb_vector<int>::iterator, stdb_vector<int>::const_iterator) {
    T it;
    it.~T();
    T it2 = T();
    CHECK_EQ(it, it2);
    CHECK_EQ(it == it2, true);
    CHECK_EQ(it != it2, false);
    CHECK_EQ(it < it2, false);
    CHECK_EQ(it <= it2, true);
    CHECK_EQ(it > it2, false);
    CHECK_EQ(it >= it2, true);
    CHECK_EQ(it.operator->(), nullptr);
    int buf[3] = {1, 2, 3};
    it = T(buf);
    CHECK_EQ(it.operator->(), buf);
    CHECK_EQ(it.operator*(), 1);
    CHECK_EQ(it[0], 1);
    auto it33 = 1 + it;
    CHECK_EQ(it33.operator->(), buf + 1);
    it2 = it;
    CHECK_EQ(it2.operator->(), buf);
    CHECK_EQ(it2.operator*(), 1);
    ++it2;
    CHECK_EQ(it2.operator->(), buf + 1);
    CHECK_EQ(it2.operator*(), 2);
    --it2;
    CHECK_EQ(it2.operator->(), buf);
    CHECK_EQ(it2.operator*(), 1);
    it2 += 2;
    CHECK_EQ(it2.operator->(), buf + 2);
    CHECK_EQ(it2.operator*(), 3);
    it2 -= 2;
    CHECK_EQ(it2.operator->(), buf);
    CHECK_EQ(it2.operator*(), 1);
    it2 += 1;
    CHECK_EQ(it2 > it, true);
    CHECK_EQ(it2 >= it, true);
    CHECK_EQ(it2 < it, false);
    CHECK_EQ(it2 <= it, false);
    it += 1;
    CHECK_EQ(it2 == it, true);
    CHECK_EQ(it2 != it, false);
    CHECK_EQ(it2 > it, false);
    CHECK_EQ(it2 >= it, true);
    CHECK_EQ(it2 < it, false);
    CHECK_EQ(it2 <= it, true);
    auto it3(it2--);
    CHECK_EQ(it3.operator->(), buf + 1);
    CHECK_EQ(it3.operator*(), 2);
    CHECK_EQ(it2.operator->(), buf);
    CHECK_EQ(it2.operator*(), 1);

    auto it4 = it3++;
    CHECK_EQ(it4.operator->(), buf + 1);
    CHECK_EQ(it4.operator*(), 2);
    CHECK_EQ(it3.operator->(), buf + 2);
    CHECK_EQ(it3.operator*(), 3);

    auto it5 = it4 + 1;
    CHECK_EQ(it5.operator->(), buf + 2);
    CHECK_EQ(it5.operator*(), 3);
    it5 = it4 - 1;
    CHECK_EQ(it4 - it5, 1);
    CHECK_EQ(it5.operator->(), buf);
    CHECK_EQ(it5.operator*(), 1);
    auto it6(it5);
    CHECK_EQ(it5, it6);
    auto it7 = it6;
    CHECK_EQ(it6, it7);
}

TEST_CASE("Hilbert::iterator implicit cast") {
    stdb_vector<int> vec;
    stdb_vector<int>::iterator it = vec.begin();
    stdb_vector<int>::const_iterator cit(it);
    CHECK_EQ(vec.cbegin(), cit);
}

TEST_CASE("Hilbert::2 dimension stdb_Vector") {
    auto dp = stdb_vector<stdb_vector<int>>(4, stdb_vector<int>(8));
    CHECK_EQ(dp.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            CHECK_EQ(dp[i][j], 0);
        }
    }
}

TEST_CASE_TEMPLATE("Hilbert::reverse iterator test", T, stdb_vector<int>::reverse_iterator,
                   stdb_vector<int>::const_reverse_iterator) {
    int buf[3] = {1, 2, 3};
    auto it = T{typename T::iterator_type(buf + 3)};
    CHECK_EQ(it.operator->(), buf + 2);
    CHECK_EQ(it.operator*(), 3);
    auto it2 = it;
    CHECK_EQ(it2.operator->(), buf + 2);
    CHECK_EQ(it2.operator*(), 3);
    ++it2;
    CHECK_EQ(it2.operator->(), buf + 1);
    CHECK_EQ(it2.operator*(), 2);
    --it2;
    CHECK_EQ(it2.operator->(), buf + 2);
    CHECK_EQ(it2.operator*(), 3);
    it2 += 2;
    CHECK_EQ(it2.operator->(), buf);
    CHECK_EQ(it2.operator*(), 1);
    it2 -= 2;
    CHECK_EQ(it2.operator->(), buf + 2);
    CHECK_EQ(it2.operator*(), 3);
    it2 += 1;
    CHECK_EQ(it2 > it, true);
    CHECK_EQ(it2 < it, false);
    CHECK_EQ(it2 <= it, false);
    it += 1;
    CHECK_EQ(it2 == it, true);
    CHECK_EQ(it2 != it, false);
    CHECK_EQ(it2 > it, false);
    CHECK_EQ(it2 >= it, true);
    CHECK_EQ(it2 < it, false);
    CHECK_EQ(it2 <= it, true);
    auto it3 = it2--;
    CHECK_EQ(it3.operator->(), buf + 1);
    CHECK_EQ(it3.operator*(), 2);
    CHECK_EQ(it2.operator->(), buf + 2);
    CHECK_EQ(it2.operator*(), 3);

    auto it4(it3++);
    CHECK_EQ(it4.operator->(), buf + 1);
    CHECK_EQ(it4.operator*(), 2);

    auto it5 = it4 + 1;
    CHECK_EQ(it5 - it4, 1);
    CHECK_EQ(it5.operator->(), buf);
    CHECK_EQ(it5.operator*(), 1);
    it5 = it4 - 1;
    CHECK_EQ(it5.operator->(), buf + 2);
    CHECK_EQ(it5.operator*(), 3);
}

TEST_CASE("vector of char") {
    stdb_vector<char> v(10, 'a');
    CHECK_EQ(v.size(), 10);
    CHECK_EQ(v.capacity(), 10);
    CHECK_EQ(v[0], 'a');
    CHECK_EQ(v[9], 'a');
    v.push_back('b');
    CHECK_EQ(v.size(), 11);
    CHECK_EQ(v.capacity(), 64);
    CHECK_EQ(v[0], 'a');
    CHECK_EQ(v[9], 'a');
    CHECK_EQ(v[10], 'b');
}

TEST_CASE("vector of bool works well") {
    stdb_vector<bool> v(10, true);
    CHECK_EQ(sizeof(bool), 1);
    for (size_t i = 0; i < 10; ++i) {
        CHECK_EQ(v[i], true);
    }
    v.push_back(false);
    CHECK_EQ(v.size(), 11);
    CHECK_EQ(v[10], false);
}

TEST_CASE("format stdb_vector<STring>") {
    stdb_vector<memory::string> vec_str{"ab", "cd"};
    auto result = fmt::format("{}", vec_str);
    CHECK_EQ("[\"ab\", \"cd\"]", result);
}

}  // namespace stdb::container

namespace stdb {
template <>
struct Relocatable<container::normal_class_with_traits> : std::true_type
{};
template <>
struct ZeroInitable<container::normal_class_with_traits> : std::true_type
{};

namespace container {
static_assert(IsRelocatable<normal_class_with_traits>, "normal_class_with_traits should be relocatable");
static_assert(IsZeroInitable<normal_class_with_traits>, "normal_class_with_traits should be zero copyable");

}  // namespace container
}  // namespace stdb

// NOLINTEND
