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

#include "container/stdb_vector.hpp"

#include <doctest/doctest.h>

#include <iostream>

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
        CHECK_EQ(vec.max_size(), kFastVectorMaxSize/ sizeof(int));
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
    SUBCASE("copy_vector") {
        stdb_vector<int> vec;
        auto vec_copy(vec);
        auto vec_copy_2 = vec;

        CHECK_EQ(vec, vec_copy);
        CHECK_EQ(vec, vec_copy_2);
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

    SUBCASE("erase") {
        stdb_vector<int> vec;
        vec.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_GE(vec.capacity(), 20);
        vec.erase(vec.begin());
        CHECK_EQ(vec.size(), 19);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        vec.erase(vec.begin() + 5);
        CHECK_EQ(vec.size(), 18);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        vec.erase(vec.begin() + 5, vec.begin() + 10);
        CHECK_EQ(vec.size(), 13);
        CHECK_GE(vec.capacity(), 20);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 20);
        vec.erase(vec.cbegin(), vec.cend());
        CHECK_EQ(vec.size(), 0);
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
    }

    SUBCASE("insert_with_single_value") {
        stdb_vector<int> vec;
        vec.insert(vec.begin(), 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.begin(), 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.end(), 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert(vec.begin() + 1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        vec.insert(vec.end() - 1, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[3], 5);
    }

    SUBCASE("insert_unsafe_with_single_value") {
        stdb_vector<int> vec;
        vec.reserve(10);
        vec.insert<Safety::Unsafe>(vec.begin(), 1);
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.begin(), 2);
        CHECK_EQ(vec.size(), 2);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.end(), 3);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert<Safety::Unsafe>(vec.begin() + 1, 4);
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        vec.insert<Safety::Unsafe>(vec.end() - 1, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[3], 5);
    }

    SUBCASE("insert_with_multi_values") {
        stdb_vector<int> vec;
        vec.insert(vec.begin(), 3, 1);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.begin(), 2, 2);
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert(vec.end(), 4, 3);
        CHECK_EQ(vec.size(), 9);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert(vec.begin() + 1, 5, 4);
        CHECK_EQ(vec.size(), 14);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        CHECK_EQ(vec[5], 4);
        vec.insert(vec.end() - 1, 6, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[13], 5);
    }

    SUBCASE("insert_unsafe_with_multi_values") {
        stdb_vector<int> vec;
        vec.reserve(20);
        vec.insert<Safety::Unsafe>(vec.begin(), 3, 1);
        CHECK_EQ(vec.size(), 3);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.begin(), 2, 2);
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 1);
        vec.insert<Safety::Unsafe>(vec.end(), 4, 3);
        CHECK_EQ(vec.size(), 9);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        vec.insert<Safety::Unsafe>(vec.begin() + 1, 5, 4);
        CHECK_EQ(vec.size(), 14);
        CHECK_EQ(vec.front(), 2);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[1], 4);
        CHECK_EQ(vec[5], 4);
        vec.insert<Safety::Unsafe>(vec.end() - 1, 6, 5);
        CHECK_EQ(vec.back(), 3);
        CHECK_EQ(vec[13], 5);
    }

    SUBCASE("insert_with_initializer_list") {
        stdb_vector<int> vec;
        vec.insert(vec.begin(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 10);
        vec.insert(vec.begin(), {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 10);
        vec.insert(vec.end(), {21, 22, 23, 24, 25, 26, 27, 28, 29, 30});
        CHECK_EQ(vec.size(), 30);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        vec.insert(vec.begin() + 10, {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        CHECK_EQ(vec.size(), 40);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[10], 31);
        vec.insert(vec.end() - 1, {12, 44});
        CHECK_EQ(vec.size(), 42);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[40], 44);
    }

    SUBCASE("insert_unsafe_with_initializer_list") {
        stdb_vector<int> vec;
        vec.reserve(100);
        vec.insert<Safety::Unsafe>(vec.begin(), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        CHECK_EQ(vec.size(), 10);
        CHECK_EQ(vec.front(), 1);
        CHECK_EQ(vec.back(), 10);
        vec.insert<Safety::Unsafe>(vec.begin(), {11, 12, 13, 14, 15, 16, 17, 18, 19, 20});
        CHECK_EQ(vec.size(), 20);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 10);
        vec.insert<Safety::Unsafe>(vec.end(), {21, 22, 23, 24, 25, 26, 27, 28, 29, 30});
        CHECK_EQ(vec.size(), 30);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        vec.insert<Safety::Unsafe>(vec.begin() + 10, {31, 32, 33, 34, 35, 36, 37, 38, 39, 40});
        CHECK_EQ(vec.size(), 40);
        CHECK_EQ(vec.front(), 11);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[10], 31);
        vec.insert<Safety::Unsafe>(vec.end() - 1, {12, 44});
        CHECK_EQ(vec.size(), 42);
        CHECK_EQ(vec.back(), 30);
        CHECK_EQ(vec[40], 44);
    }

    SUBCASE("insert_from_another_vector") {
        stdb_vector<int> vec1;
        stdb_vector<int> vec2;
        std::vector<int> vec3 = {11, 22, 33, 44, 55, 66, 77, 88, 99, 100};
        vec1.insert(vec1.begin(), vec2.begin(), vec2.end());
        CHECK_EQ(vec1.size(), 0);
        vec1.insert(vec1.begin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1.size(), 10);
        vec2.insert(vec2.begin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1, vec2);
        std::vector<int> vec4 = {100, 100};
        vec1.insert(vec1.end(), vec4.begin(), vec4.end());
        CHECK_EQ(vec1.size(), 12);
        CHECK_EQ(vec1.back(), 100);
        std::vector<int> vec5 = {200, 200};
        vec1.insert(vec1.end() - 1, vec5.begin(), vec5.end());
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
        vec1.insert<Safety::Unsafe>(vec1.begin(), vec2.begin(), vec2.end());
        CHECK_EQ(vec1.size(), 0);
        vec1.insert<Safety::Unsafe>(vec1.begin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1.size(), 10);
        vec2.insert<Safety::Unsafe>(vec2.begin(), vec3.begin(), vec3.end());
        CHECK_EQ(vec1, vec2);
        std::vector<int> vec4 = {100, 100};
        vec1.insert<Safety::Unsafe>(vec1.end(), vec4.begin(), vec4.end());
        CHECK_EQ(vec1.size(), 12);
        CHECK_EQ(vec1.back(), 100);
        std::vector<int> vec5 = {200, 200};
        vec1.insert<Safety::Unsafe>(vec1.end() - 1, vec5.begin(), vec5.end());
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
    }
    SUBCASE("end") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.end() - 1), "!");
    }
    SUBCASE("cend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.cend() - 1), "!");
    }
    SUBCASE("rbegin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.rbegin(), "!");
    }
    SUBCASE("crbegin") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*vec.crbegin(), "!");
    }
    SUBCASE("rend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.rend() - 1), "hello");
    }
    SUBCASE("crend") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        CHECK_EQ(*(vec.crend() - 1), "hello");
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
        CHECK_EQ(vec.capacity(), kFastVectorDefaultCapacity / sizeof(memory::string));
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
        vec.insert(vec.begin() + 1, "inserted");
        CHECK_EQ(vec.size(), 4);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "world");
        CHECK_EQ(vec[3], "!");
    }
    SUBCASE("insert_with_multiple_elements") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.insert(vec.begin() + 1, 2, memory::string("inserted"));
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
    }
    SUBCASE("insert_with_initializer_list") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        vec.insert(vec.begin() + 1, {"inserted", "inserted"});
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
        vec.insert(vec.end(), {"final"});
        CHECK_EQ(vec.size(), 6);
        CHECK_EQ(vec[5], "final");
        vec.insert(vec.end() - 1, {"middle"});
        CHECK_EQ(vec[5], "middle");
        CHECK_EQ(vec[6], "final");
    }
    SUBCASE("insert_with_range") {
        stdb_vector<memory::string> vec = {"hello", "world", "!"};
        std::vector<memory::string> vec2 = {"inserted", "inserted"};
        vec.insert(vec.begin() + 1, vec2.begin(), vec2.end());
        CHECK_EQ(vec.size(), 5);
        CHECK_EQ(vec[0], "hello");
        CHECK_EQ(vec[1], "inserted");
        CHECK_EQ(vec[2], "inserted");
        CHECK_EQ(vec[3], "world");
        CHECK_EQ(vec[4], "!");
        vec.insert(vec.end(), vec2.begin(), vec2.end());
        CHECK_EQ(vec.size(), 7);
        CHECK_EQ(vec[5], "inserted");
        CHECK_EQ(vec[6], "inserted");
        std::vector<memory::string> vec3 = {"final"};
        vec.insert(vec.end() - 1, vec3.begin(), vec3.end());
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
    }
    SUBCASE("push_back") {
        stdb_vector<memory::string> vec;
        vec.push_back({"hello"});
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], "hello");
    }
    SUBCASE("emplace_back") {
        stdb_vector<memory::string> vec;
        vec.emplace_back("hello");
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

   public:
    non_copyable() : ptr(new int(0)) { std::cout << "non_copyable constructor default" << std::endl; }
    non_copyable(int i) : ptr(new int(i)) { std::cout << "non_copyable constructor with : " << i << std::endl; }
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
    vec.insert(vec.begin(), nm);
    CHECK_EQ(vec.size(), 11);

    vec.insert(vec.end(), nm);
    CHECK_EQ(vec.size(), 12);
    /*
    for (auto& non : vec) {
        non.print();
    }
    */
    CHECK_EQ(vec[0] == 1000, true);
    CHECK_EQ(vec[11] == 1000, true);
    for (int i = 1; i < 11; ++i) {
        CHECK_EQ(vec[static_cast<std::size_t>(i)] == i - 1, true);
    }
}

TEST_CASE("Hilbert::stdb_vector::non_copyable") {
    stdb_vector<non_copyable> vec;
    vec.reserve(10);
    for (int i = 0; i < 10; i++) {
        non_copyable nc(i);
        vec.push_back(std::move(nc));
    }
    vec.insert(vec.begin(), non_copyable(1000));
    CHECK_EQ(vec.size(), 11);

    vec.insert(vec.end(), non_copyable(1000));
    CHECK_EQ(vec.size(), 12);
    /*
    for (auto& non : vec) {
        non.print();
    }
    */
    CHECK_EQ(vec[0] == 1000, true);
    CHECK_EQ(vec[11] == 1000, true);
    for (int i = 1; i < 11; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)] == i - 1, true);
    }
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
    vec.reserve(1000);
    auto fill = [](int* ptr) -> std::size_t {
        if (ptr != nullptr) {
            for (int i = 0; i < 700; ++i) {
                ptr[i] = i;
            }
        }
        return 700;
    };
    vec.fill(fill);
    CHECK_EQ(vec.size(), 700);
    for (int i = 0; i < 700; ++i) {
        CHECK_EQ(vec[static_cast<size_t>(i)], i);
    }
}

TEST_CASE("Hilbert::stdb_vector::get_writebuffer") {
    stdb_vector<int> vec;
    vec.reserve(1000);
    auto buffer = vec.get_writebuffer(40);
    CHECK_EQ(buffer.size(), 40);
    CHECK_EQ(vec.size(), 40);
    CHECK_EQ(vec.capacity(), 1000);
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

// NOLINTEND
}  // namespace container
}  // namespace stdb
