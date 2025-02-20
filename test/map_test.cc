#include "container/small_dense.hpp"
#include "doctest/doctest.h"
#include "string/small_string.hpp"

namespace stdb::container {

using small_string = memory::small_byte_string;

TEST_CASE("calculate_shifts") {
    CHECK_EQ(calculate_shifts(4), 62);
    CHECK_EQ(calc_num_buckets_by_shift<uint32_t>(62), 4);
    CHECK_EQ(calculate_shifts(5), 61);
    CHECK_EQ(calculate_shifts(8), 61);
    CHECK_EQ(calc_num_buckets_by_shift<uint32_t>(61), 8);
    CHECK_EQ(calculate_shifts(16), 60);
    CHECK_EQ(calc_num_buckets_by_shift<uint32_t>(60), 16);
    CHECK_EQ(calculate_shifts(32), 59);
    CHECK_EQ(calc_num_buckets_by_shift<uint32_t>(59), 32);
}

TEST_CASE("inplace_table.constructors") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    CHECK_EQ(table.size(), 0);
    CHECK_EQ(table.bucket_count(), 4);
    auto table2 = table;
    CHECK_EQ(table2.size(), 0);
    CHECK_EQ(table2.bucket_count(), 4);
    table.emplace(uint64_t{1}, uint32_t{1});
    auto table3 = std::move(table);
    CHECK_EQ(table3.size(), 1);
    CHECK_EQ(table3.bucket_count(), 4);
    CHECK_EQ(table.size(), 0);
    CHECK_EQ(table.bucket_count(), 0);
}

TEST_CASE("inplace_set.constructors") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    CHECK_EQ(set.size(), 0);
    CHECK_EQ(set.bucket_count(), 4);
    auto set2 = set;
    CHECK_EQ(set2.size(), 0);
    CHECK_EQ(set2.bucket_count(), 4);
    set.emplace(1UL);
    auto set3 = std::move(set);
    CHECK_EQ(set3.size(), 1);
    CHECK_EQ(set3.bucket_count(), 4);
    CHECK_EQ(set.size(), 0);
    CHECK_EQ(set.bucket_count(), 0);
}

TEST_CASE("inplace_table<small_string>.constructors") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    CHECK_EQ(table.size(), 0);
    CHECK_EQ(table.bucket_count(), 4);
    auto table2 = table;
    CHECK_EQ(table2.size(), 0);
    CHECK_EQ(table2.bucket_count(), 4);
    table2.emplace(small_string{"hello"}, uint32_t{1});
    CHECK_EQ(table2.size(), 1);
    CHECK_EQ(table2.bucket_count(), 4);
    auto table3 = std::move(table2);
    CHECK_EQ(table3.size(), 1);
    CHECK_EQ(table3.bucket_count(), 4);
    CHECK_EQ(table2.size(), 0);
    CHECK_EQ(table2.bucket_count(), 0);
}

TEST_CASE("inplace_table.smoke") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{2});
    CHECK_EQ(table.size(), 1);
    auto it = table.find(1);
    CHECK(it != table.end());
    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 2);

    auto non_found_it = table.find(2);
    CHECK(non_found_it == table.end());
}

TEST_CASE("inplace_table<small_string>.smoke") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace(small_string{"hello"}, uint32_t{1});
    CHECK_EQ(table.size(), 1);
    CHECK_EQ(table.bucket_count(), 4);
    auto it = table.find(small_string{"hello"});
    CHECK(it != table.end());
    CHECK_EQ(it->first, small_string{"hello"});
    CHECK_EQ(it->second, 1);
}

TEST_CASE("inplace_set.smoke") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    set.emplace(1);
    set.emplace(2);
    set.emplace(3);
    CHECK_EQ(set.size(), 3);
    CHECK_EQ(set.bucket_count(), 4);

    CHECK(set.contains(1));
    CHECK(set.contains(2));
    CHECK(set.contains(3));
    CHECK(not set.contains(4));
}

struct no_hash_hash
{
    using is_avalanching = void;
    auto operator()(const uint64_t& key) const { return key; }
};

TEST_CASE("inplace_table.smake_need_shape_up") {
    inplace_table<uint64_t, uint32_t, no_hash_hash, std::equal_to<uint64_t>> table;

    auto [it1, success1] = table.emplace(uint64_t{1}, uint32_t{1});
    CHECK(success1);
    CHECK_EQ(it1->first, 1);
    CHECK_EQ(it1->second, 1);
    CHECK_EQ(table.size(), 1);
    auto it = table.find(1);
    CHECK(it != table.end());

    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 1);
    CHECK_EQ(table.bucket_count(), 4);
    auto [it5, suc5] = table.emplace(uint64_t{5}, uint32_t{5});
    CHECK(suc5);
    CHECK_EQ(it5->first, 5);
    CHECK_EQ(it5->second, 5);
    auto it2 = table.find(5);
    CHECK(it2 != table.end());
    CHECK_EQ(it2->first, 5);
    CHECK_EQ(it2->second, 5);
    CHECK_EQ(table.size(), 2);

    auto it3 = table.find(1);
    CHECK(it3 != table.end());
    CHECK_EQ(it3->first, 1);
    CHECK_EQ(it3->second, 1);

    table.emplace(uint64_t{10}, uint32_t{10});
    auto it4 = table.find(10);
    CHECK(it4 != table.end());
    CHECK_EQ(it4->first, 10);
    CHECK_EQ(it4->second, 10);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.bucket_count(), 4);

    auto [_, success] = table.emplace(uint64_t{9}, uint32_t{9});
    CHECK(success);
    auto find_it5 = table.find(9);
    auto found = find_it5 != table.end();
    REQUIRE(found);
    CHECK_EQ(find_it5->first, 9);
    CHECK_EQ(find_it5->second, 9);
    CHECK_EQ(table.size(), 4);
    CHECK_EQ(table.bucket_count(), 8);
    auto non_found_it = table.find(2);
    CHECK(non_found_it == table.end());
}

TEST_CASE("inplace_set.smoke_need_shape_up") {
    inplace_table<uint64_t, void, std::hash<uint64_t>, std::equal_to<uint64_t>> set;
    set.emplace(uint64_t{1});
    set.emplace(uint64_t{2});
    set.emplace(uint64_t{3});
    CHECK_EQ(set.size(), 3);
    CHECK_EQ(set.bucket_count(), 4);
    auto it = set.find(1);
    CHECK(it != set.end());
    CHECK_EQ(*it, 1);
    auto [it2, success] = set.try_emplace(uint64_t{1});
    CHECK(!success);
    CHECK_EQ(*it2, 1);
    CHECK_EQ(set.size(), 3);
    CHECK_EQ(set.bucket_count(), 4);

    set.emplace(uint64_t{4});
    CHECK_EQ(set.size(), 4);
    CHECK_EQ(set.bucket_count(), 8);
}

TEST_CASE("inplace_table.try_emplace") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    // dense_map<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> dense_table;

    auto [it, success] = table.try_emplace(uint64_t{1}, uint32_t{1});
    CHECK(success);
    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 1);
    CHECK_EQ(table.size(), 1);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it2, success2] = table.try_emplace(uint64_t{1}, uint32_t{2});
    CHECK(!success2);
    CHECK_EQ(it2->first, 1);
    CHECK_EQ(it2->second, 1);
    CHECK_EQ(table.size(), 1);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it3, success3] = table.try_emplace(uint64_t{2}, uint32_t{2});
    CHECK(success3);
    auto new_it = table.find(2);
    CHECK(new_it != table.end());
    CHECK_EQ(new_it->first, 2);
    CHECK_EQ(new_it->second, 2);
    CHECK_EQ(it3->first, 2);
    CHECK_EQ(it3->second, 2);
    CHECK_EQ(table.size(), 2);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it4, success4] = table.try_emplace(uint64_t{2}, uint32_t{3});
    CHECK(!success4);
    CHECK_EQ(it4->first, 2);
    CHECK_EQ(it4->second, 2);
    CHECK_EQ(table.size(), 2);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it5, success5] = table.try_emplace(uint64_t{3}, uint32_t{3});
    CHECK(success5);
    CHECK_EQ(it5->first, 3);
    CHECK_EQ(it5->second, 3);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it6, success6] = table.try_emplace(uint64_t{3}, uint32_t{4});
    CHECK(!success6);
    CHECK_EQ(it6->first, 3);
    CHECK_EQ(it6->second, 3);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.bucket_count(), 4);
}

TEST_CASE("inplace_set.try_emplace") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    set.emplace(1);
    set.emplace(2);
    set.emplace(3);
    set.try_emplace(1);
    CHECK_EQ(set.size(), 3);
    CHECK_EQ(set.bucket_count(), 4);
}

TEST_CASE("inplace_table<small_string>.try_emplace") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    auto [it1, success1] = table.emplace(small_string{"hellogogogogogoggo"}, uint32_t{1});
    CHECK(success1);
    const small_string s1{"hellogogogogogoggo"};
    CHECK_EQ(it1->first, s1);
    CHECK_EQ(it1->second, 1);
    CHECK_EQ(table.size(), 1);
    CHECK_EQ(table.bucket_count(), 4);

    auto [it2, success2] = table.try_emplace(small_string{"hellogogogogogoggo"}, uint32_t{2});
    CHECK(!success2);
    CHECK_EQ(it2->first, s1);
    CHECK_EQ(it2->second, 1);
    CHECK_EQ(table.size(), 1);
    CHECK_EQ(table.bucket_count(), 4);

    table.emplace(small_string{"hellorunrunrun"}, uint32_t{3});
    const small_string s2{"hellorunrunrun"};
    auto it3 = table.find(small_string{"hellorunrunrun"});
    CHECK(it3 != table.end());
    CHECK_EQ(it3->first, s2);
    CHECK_EQ(it3->second, 3);
    CHECK_EQ(table.size(), 2);
    CHECK_EQ(table.bucket_count(), 4);
}

TEST_CASE("inplace_set<small_string>.try_emplace with const char*") {
    inplace_table<small_string, void, std::hash<small_string>, std::equal_to<small_string>> set;
    set.emplace("hello");
    set.emplace("world");
    set.emplace("foo");
    auto it = set.find("hello");
    CHECK(it != set.end());
    auto it2 = set.find("world");
    CHECK(it2 != set.end());
    auto it3 = set.find("foo");
    CHECK(it3 != set.end());

    auto it_not_found = set.find("hhh");
    CHECK(it_not_found == set.end());

    auto [it4, success4] = set.try_emplace("hello");
    CHECK(!success4);
    CHECK(it4 != set.end());
    CHECK_EQ(*it4, "hello");
}

TEST_CASE("inplace_table<small_string>.try_emplace with const char*") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace("hellorunrunrun", uint32_t{3});
    const small_string s2{"hellorunrunrun"};
    auto it3 = table.find("hellorunrunrun");
    CHECK(it3 != table.end());
    CHECK_EQ(it3->first, s2);
    CHECK_EQ(it3->second, 3);
}

TEST_CASE("inplace_table<small_string>.try_emplace with std::string") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace(std::string{"hellorunrunrun"}, uint32_t{3});
    const small_string s2{"hellorunrunrun"};
    auto it3 = table.find(std::string{"hellorunrunrun"});
    CHECK(it3 != table.end());
    CHECK_EQ(it3->first, s2);
    CHECK_EQ(it3->second, 3);
}

TEST_CASE("inplace_table.erase") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    CHECK_EQ(table.size(), 3);
    auto it = table.find(1);
    CHECK(it != table.end());
    table.erase(it);
    CHECK_EQ(table.size(), 2);
    auto it_after_erase1 = table.find(1);
    CHECK(it_after_erase1 == table.end());

    auto r = table.erase(3);
    auto it_after_erase3 = table.find(3);
    CHECK(it_after_erase3 == table.end());
    auto find_2 = table.find(2);
    CHECK(find_2 != table.end());
    CHECK_EQ(find_2->first, 2);
    CHECK_EQ(find_2->second, 2);

    CHECK_EQ(r, 1);
    CHECK_EQ(table.size(), 1);

    r = table.erase(2);
    auto it3 = table.find(2);
    CHECK(it3 == table.end());
    CHECK_EQ(r, 1);
    CHECK_EQ(table.size(), 0);
}

TEST_CASE("inplace_set.erase") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    set.emplace(1);
    set.emplace(2);
    set.emplace(3);
    auto it = set.find(1);
    CHECK(it != set.end());
    CHECK_EQ(*it, 1);
    set.erase(it);
    CHECK_EQ(set.size(), 2);
    auto it2 = set.find(1);
    CHECK(it2 == set.end());
}

TEST_CASE("inplace_table<small_string>.erase") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace("hello", uint32_t{1});
    table.emplace("world", uint32_t{2});
    table.emplace("foo", uint32_t{3});
    auto it = table.find("hello");
    CHECK(it != table.end());
    table.erase(it);
    CHECK_EQ(table.size(), 2);
    table.erase("world");
    auto it2 = table.find("hello");
    CHECK(it2 == table.end());
}

TEST_CASE("inplace_table.clear") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    CHECK_EQ(table.size(), 3);
    auto it = table.find(1);
    CHECK(it != table.end());
    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 1);

    table.clear();
    CHECK(table.empty());
    CHECK_EQ(table.size(), 0);
    auto it2 = table.find(1);
    CHECK(it2 == table.end());
    auto it3 = table.find(2);
    CHECK(it3 == table.end());
    auto it4 = table.find(3);
    CHECK(it4 == table.end());
}

TEST_CASE("inplace_set.clear") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    set.emplace(1);
    set.emplace(2);
    set.emplace(3);
    auto it = set.find(1);
    CHECK(it != set.end());
    CHECK_EQ(*it, 1);
    set.clear();
    CHECK(set.empty());
    CHECK_EQ(set.size(), 0);
    auto it2 = set.find(1);
    CHECK(it2 == set.end());
}

TEST_CASE("inplace_table<small_string>.clear") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace("hello", uint32_t{1});
    table.emplace("world", uint32_t{2});
    table.emplace("foo", uint32_t{3});

    table.clear();
    CHECK(table.empty());
    CHECK_EQ(table.size(), 0);
    auto it = table.find("hello");
    CHECK(it == table.end());
    auto it2 = table.find("world");
    CHECK(it2 == table.end());
    auto it3 = table.find("foo");
    CHECK(it3 == table.end());
}

TEST_CASE("inplace_table.insert") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    auto [it, success] = table.insert(std::make_pair(uint64_t{1}, uint32_t{1}));
    CHECK(success);
    CHECK_EQ(table.size(), 1);
    auto it_find = table.find(1);
    CHECK(it_find != table.end());
    CHECK_EQ(it_find->first, 1);
    CHECK_EQ(it_find->second, 1);

    auto pair = std::make_pair(uint64_t{2}, uint32_t{2});
    auto [it2, success2] = table.insert(pair);
    CHECK(success2);
    CHECK(it2 != table.end());
    CHECK_EQ(it2->first, 2);
    CHECK_EQ(it2->second, 2);
    CHECK_EQ(table.size(), 2);
}

TEST_CASE("inplace_set.insert") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    auto [it, success] = set.insert(1);
    CHECK(success);
    CHECK(it != set.end());
    CHECK_EQ(*it, 1);
    auto [it2, success2] = set.insert(1);
    CHECK(!success2);
    CHECK_EQ(*it2, 1);
    CHECK_EQ(set.size(), 1);
}

TEST_CASE("inplace_table<small_string>.insert") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.insert(std::make_pair(small_string{"hello"}, uint32_t{1}));
    auto it = table.find("hello");
    CHECK(it != table.end());
    CHECK_EQ(it->first, small_string{"hello"});
    CHECK_EQ(it->second, 1);
    auto [it2, success2] = table.insert(std::make_pair(small_string{"hello"}, uint32_t{2}));
    CHECK(!success2);
    CHECK_EQ(it2->first, small_string{"hello"});
}

TEST_CASE("inplace_table.at") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);
    CHECK_THROWS((void)table.at(uint64_t{4}));
}

TEST_CASE("inplace_table<small_string>.at") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace("hello", uint32_t{1});
    table.emplace("world", uint32_t{2});
    table.emplace("foo", uint32_t{3});
    CHECK_EQ(table.at("hello"), 1);
    CHECK_EQ(table.at("world"), 2);
    CHECK_THROWS_AS((void)table.at("hhh"), std::out_of_range);
}

TEST_CASE("inplace_table.operator[]") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    CHECK_EQ(table[uint64_t{1}], 1);
    CHECK_EQ(table[uint64_t{2}], 2);
    CHECK_EQ(table[uint64_t{3}], 3);
    CHECK_THROWS((void)table[4]);
}

TEST_CASE("inplace_table.extract") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    auto node = table.extract(uint64_t{1});
    CHECK_EQ(node.first, 1);
    CHECK_EQ(node.second, 1);
    CHECK_EQ(table.size(), 2);
    auto it = table.find(1);
    CHECK(it == table.end());
}

TEST_CASE("inplace_table<small_string>.extract") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> table;
    table.emplace("hello", uint32_t{1});
    table.emplace("world", uint32_t{2});
    table.emplace("foo", uint32_t{3});
    auto node = table.extract("hello");
    CHECK_EQ(node.first, small_string{"hello"});
    CHECK_EQ(node.second, 1);
    CHECK_EQ(table.size(), 2);
    auto it = table.find("hello");
    CHECK(it == table.end());
}

TEST_CASE("inplace_table.reserve_rehash") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    CHECK_EQ(table.bucket_count(), 4);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);
    table.reserve(9);
    CHECK_EQ(table.bucket_count(), 16);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);

    table.reserve(1000);

    CHECK_EQ(table.bucket_count(), 2048);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);

    table.rehash(8);
    CHECK_EQ(table.bucket_count(), 16);
    CHECK_EQ(table.size(), 3);
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);
}

TEST_CASE("inplace_table.merge") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});
    auto table2 = table;
    table2.emplace(uint64_t{4}, uint32_t{4});
    table2.emplace(uint64_t{5}, uint32_t{5});
    CHECK_EQ(table2.size(), 5);
    table.merge(table2);
    CHECK_EQ(table.size(), 5);
    CHECK_EQ(table.bucket_count(), 8);
    CHECK_EQ(table.at(uint64_t{1}), 1);
    CHECK_EQ(table.at(uint64_t{2}), 2);
    CHECK_EQ(table.at(uint64_t{3}), 3);
    CHECK_EQ(table.at(uint64_t{4}), 4);
}

TEST_CASE("inplace_set.merge") {
    inplace_table<uint32_t, void, std::hash<uint32_t>, std::equal_to<uint32_t>> set;
    set.emplace(1);
    set.emplace(2);
    set.emplace(3);
    auto set2 = set;
    set2.emplace(4);
    set2.emplace(5);
    set.merge(set2);
    CHECK_EQ(set.size(), 5);
    CHECK_EQ(set.bucket_count(), 8);
    CHECK(set.contains(1));
    CHECK(set.contains(2));
    CHECK(set.contains(3));
    CHECK(set.contains(4));
    CHECK(set.contains(5));
}

TEST_CASE("inplace_set<small_string>.merge") {
    inplace_table<small_string, void, std::hash<small_string>, std::equal_to<small_string>> set;
    set.emplace("hello");
    set.emplace("world");
    set.emplace("foo");
    auto set2 = set;
    set2.emplace("bar");
    set2.emplace("hellohello");
    set.merge(set2);
    CHECK_EQ(set.size(), 5);
    CHECK(set.contains("hello"));
    CHECK(set.contains("world"));
    CHECK(set.contains("foo"));
    CHECK(set.contains("bar"));
    CHECK(set.contains("hellohello"));
}

TEST_CASE("inplace_table.insert_or_assign") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    auto [it, success] = table.insert_or_assign(uint64_t{1}, uint32_t{1});
    CHECK(success);
    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 1);
    auto [it2, success2] = table.insert_or_assign(uint64_t{1}, uint32_t{2});
    CHECK(not success2);
    CHECK_EQ(it2->first, 1);
    CHECK_EQ(it2->second, 2);
    CHECK_EQ(table.size(), 1);
    auto [it3, success3] = table.insert_or_assign(uint64_t{2}, uint32_t{3});
    CHECK(success3);
    CHECK_EQ(it3->first, 2);
    CHECK_EQ(it3->second, 3);
    CHECK_EQ(table.size(), 2);
}

TEST_CASE("inplace_set<small_string>.insert_or_assign") {
    inplace_table<small_string, uint32_t, std::hash<small_string>, std::equal_to<small_string>> set;
    set.emplace("hello", uint32_t{1});
    set.emplace("world", uint32_t{2});
    set.emplace("foo", uint32_t{3});
    auto [it, success] = set.insert_or_assign("hello", uint32_t{4});
    CHECK(not success);
    CHECK_EQ(it->first, "hello");
    CHECK_EQ(it->second, 4);
    CHECK_EQ(set.size(), 3);
}

TEST_CASE("inplace_table.equal_range") {
    inplace_table<uint64_t, uint32_t, std::hash<uint64_t>, std::equal_to<uint64_t>> table;
    table.emplace(uint64_t{1}, uint32_t{1});
    table.emplace(uint64_t{2}, uint32_t{2});
    table.emplace(uint64_t{3}, uint32_t{3});

    auto [it, it2] = table.equal_range(1);
    CHECK(it != table.end());
    CHECK_EQ(it->first, 1);
    CHECK_EQ(it->second, 1);
    bool check = (it2 == table.end() || it2->first != 1);
    CHECK(check);
}

}  // namespace stdb::container