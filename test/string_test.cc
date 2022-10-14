#include "string/string.hpp"

#include <cxxabi.h>     // for __forced_unwind
#include <fmt/core.h>   // for format
#include <sys/types.h>  // for uint

#include <algorithm>    // for for_each
#include <atomic>       // for atomic, __atomic_base
#include <chrono>       // for duration, system_clock, system_clock::t...
#include <cstddef>      // for size_t
#include <iostream>     // for cout
#include <iterator>     // for move_iterator, make_move_iterator, oper...
#include <list>         // for list, operator==, _List_iterator, _List...
#include <random>       // for mt19937, uniform_int_distribution
#include <sstream>      // for operator<<, basic_istream, basic_string...
#include <type_traits>  // for is_same
#include <vector>       // for vector

#include "arena/arena.hpp"    // for size_t, Arena, Arena::Options
#include "doctest/doctest.h"  // for binary_assert, CHECK_EQ, TestCase, CHECK
#include "string/arena_string.hpp"

namespace stdb::memory {

static const uint seed = std::chrono::system_clock::now().time_since_epoch().count();
using RandomT = std::mt19937;
static RandomT rng(seed);
static const size_t maxString = 100;
static const bool avoidAliasing = true;

template <class Integral1, class Integral2>
auto random(Integral1 low, Integral2 up) -> Integral2 {
    std::uniform_int_distribution<Integral2> range(static_cast<Integral2>(low), up);
    return range(rng);
}

template <>
auto random<char, char>(char low, char high) -> char {
    int32_t int_low = static_cast<int32_t>(low);
    int32_t int_high = static_cast<int32_t>(high);
    return random(int_low, int_high);
}

template <class String>
void randomString(String* toFill, unsigned int maxSize = 1000) {
    assert(toFill);
    toFill->resize(random(0, maxSize));
    std::for_each(toFill->begin(), toFill->end(), [](auto& ch) { ch = random('a', 'z'); });
}

template <class String, class Integral>
void Num2String(String& str, Integral n) {
    std::string tmp = fmt::format("{}", n);
    str = String(tmp.begin(), tmp.end(), str.get_allocator());
}

std::list<char> RandomList(unsigned int maxSize) {
    std::list<char> lst(random(0, maxSize));
    auto i = lst.begin();
    for (; i != lst.end(); ++i) {
        *i = random('a', 'z');
    }
    return lst;
}

////////////////////////////////////////////////////////////////////////////////
// Tests begin here
////////////////////////////////////////////////////////////////////////////////
template <class String>
void clause11_21_4_2_a(String& test) {
    test.String::~String();
    new (&test) String();
}

template <class String>
void clause11_21_4_2_b(String& test) {
    String test2(test);  // NOLINT
    assert(test2 == test);
}

template <class String>
void arena_clause11_21_4_2_b(String& test) {
    String test2(test);  // NOLINT
    assert(test2 == test);
}

template <class String>
void clause11_21_4_2_c(String& test) {
    // Test move constructor. There is a more specialized test, see
    // testMoveCtor test
    String donor(test);
    String test2(std::move(donor));
    CHECK_EQ(test2, test);
    // Technically not required, but all implementations that actually
    // support move will move large strings. Make a guess for 128 as the
    // maximum small string optimization that's reasonable.
    CHECK_LE(donor.size(), 128);  // NOLINT
}

template <class String>
void arena_clause11_21_4_2_c(String& test) {
    // Test move constructor. There is a more specialized test, see
    // testMoveCtor test
    String donor(test);
    String test2(std::move(donor));
    CHECK_EQ(test2, test);
    // Technically not required, but all implementations that actually
    // support move will move large strings. Make a guess for 128 as the
    // maximum small string optimization that's reasonable.
    CHECK_LE(donor.size(), 128);  // NOLINT
}

template <class String>
void clause11_21_4_2_d(String& test) {
    // Copy constructor with position and length
    const size_t pos = random(0, test.size());
    String s(test, pos,
             random(0, 9) ? random(0, test.size() - pos) : String::npos);  // test for npos, too, in 10% of the cases
    test = s;
}

template <class String>
void arena_clause11_21_4_2_d(String& test) {
    // Copy constructor with position and length
    const size_t pos = random(0, test.size());
    String s(test, pos, random(0, 9) ? random(0, test.size() - pos) : String::npos,
             test.get_allocator());  // test for npos, too, in 10% of the cases
    test = s;
}

template <class String>
void clause11_21_4_2_e(String& test) {
    // Constructor from char*, size_t
    const size_t pos = random(0, test.size()), n = random(0, test.size() - pos);
    String before(test.data(), test.size());
    String s(test.c_str() + pos, n);
    String after(test.data(), test.size());
    CHECK_EQ(before, after);
    test.swap(s);
}

template <class String>
void arena_clause11_21_4_2_e(String& test) {
    // Constructor from char*, size_t
    const size_t pos = random(0, test.size()), n = random(0, test.size() - pos);
    String before(test.data(), test.size(), test.get_allocator());
    String s(test.c_str() + pos, n, test.get_allocator());
    String after(test.data(), test.size(), test.get_allocator());
    CHECK_EQ(before, after);
    test.swap(s);
}

template <class String>
void clause11_21_4_2_f(String& test) {
    // Constructor from char*
    const size_t pos = random(0, test.size());
    String before(test.data(), test.size());
    String s(test.c_str() + pos);
    String after(test.data(), test.size());
    CHECK_EQ(before, after);
    test.swap(s);
}

template <class String>
void arena_clause11_21_4_2_f(String& test) {
    // Constructor from char*
    const size_t pos = random(0, test.size());
    String before(test.data(), test.size(), test.get_allocator());
    String s(test.c_str() + pos, test.get_allocator());
    String after(test.data(), test.size(), test.get_allocator());
    CHECK_EQ(before, after);
    test.swap(s);
}

template <class String>
void clause11_21_4_2_g(String& test) {
    // Constructor from size_t, char
    const size_t n = random(0, test.size());
    const auto c = test.front();
    test = String(n, c);
}

template <class String>
void arena_clause11_21_4_2_g(String& test) {
    // Constructor from size_t, char
    const size_t n = random(0, test.size());
    const auto c = test.front();
    test = String(n, c, test.get_allocator());
}

template <class String>
void clause11_21_4_2_h(String& test) {
    // Constructors from various iterator pairs
    // Constructor from char*, char*
    String s1(test.begin(), test.end());
    CHECK_EQ(test, s1);
    String s2(test.data(), test.data() + test.size());
    CHECK_EQ(test, s2);
    // Constructor from other iterators
    std::list<char> lst;
    for (auto c : test) {
        lst.push_back(c);
    }
    String s3(lst.begin(), lst.end());
    CHECK_EQ(test, s3);
    // Constructor from wchar_t iterators
    std::list<wchar_t> lst1;
    for (auto c : test) {
        lst1.push_back(c);
    }
    String s4(lst1.begin(), lst1.end());
    CHECK_EQ(test, s4);
    // Constructor from wchar_t pointers
    wchar_t t[20];
    t[0] = 'a';
    t[1] = 'b';
    String s5(t, t + 2);
    CHECK_EQ("ab", s5);
}

template <class String>
void arena_clause11_21_4_2_h(String& test) {
    // Constructors from various iterator pairs
    // Constructor from char*, char*
    String s1(test.begin(), test.end(), test.get_allocator());
    CHECK_EQ(test, s1);
    String s2(test.data(), test.data() + test.size(), test.get_allocator());
    CHECK_EQ(test, s2);
    // Constructor from other iterators
    std::list<char> lst;
    for (auto c : test) {
        lst.push_back(c);
    }
    String s3(lst.begin(), lst.end(), test.get_allocator());
    CHECK_EQ(test, s3);
    // Constructor from wchar_t iterators
    std::list<wchar_t> lst1;
    for (auto c : test) {
        lst1.push_back(c);
    }
    String s4(lst1.begin(), lst1.end(), test.get_allocator());
    CHECK_EQ(test, s4);
    // Constructor from char_t pointers
    char cc[20];
    cc[0] = 'a';
    cc[1] = 'b';

    // Constructor from wchar_t pointers
    wchar_t t[20];
    t[0] = 'a';
    t[1] = 'b';
    String s5(t, t + 2, test.get_allocator());
    CHECK_EQ("ab", s5);
}

template <class String>
void clause11_21_4_2_i(String& test) {
    // From initializer_list<char>
    std::initializer_list<typename String::value_type> il = {'h', 'e', 'l', 'l', 'o'};
    String s(il);
    test.swap(s);
}

template <class String>
void arena_clause11_21_4_2_i(String& test) {
    // From initializer_list<char>
    std::initializer_list<typename String::value_type> il = {'h', 'e', 'l', 'l', 'o'};
    String s(il, test.get_allocator());
    test.swap(s);
}

template <class String>
void clause11_21_4_2_j(String& test) {
    // Assignment from const String&
    auto size = random(0, 2000U);
    String s(size, '\0');
    CHECK_EQ(s.size(), size);
    // FOR_EACH_RANGE(i, 0, s.size()) { s[i] = random('a', 'z'); }
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = s;
}

template <class String>
void arena_clause11_21_4_2_j(String& test) {
    // Assignment from const String&
    auto size = random(0, 2000U);
    String s(size, '\0', test.get_allocator());
    CHECK_EQ(s.size(), size);
    // FOR_EACH_RANGE(i, 0, s.size()) { s[i] = random('a', 'z'); }
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = s;
}

template <class String>
void clause11_21_4_2_k(String& test) {
    // Assignment from String&&
    auto size = random(0, 2000U);
    String s(size, '\0');
    CHECK_EQ(s.size(), size);
    // FOR_EACH_RANGE(i, 0, s.size()) { s[i] = random('a', 'z'); }
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = std::move(s);
    if (std::is_same<String, string>::value) {
        CHECK_LE(s.size(), 128);  // NOLINT
    }
}

template <class String>
void arena_clause11_21_4_2_k(String& test) {
    // Assignment from String&&
    auto size = random(0, 2000U);
    String s(size, '\0', test.get_allocator());
    CHECK_EQ(s.size(), size);
    // FOR_EACH_RANGE(i, 0, s.size()) { s[i] = random('a', 'z'); }
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = std::move(s);
    if (std::is_same<String, string>::value) {
        CHECK_LE(s.size(), 128);  // NOLINT
    }
}

template <class String>
void clause11_21_4_2_l(String& test) {
    // Assignment from char*
    String s(random(0, 1000U), '\0');
    size_t i = 0;
    for (; i != s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = s.c_str();  // NOLINT
}

template <class String>
void arena_clause11_21_4_2_l(String& test) {
    // Assignment from char*
    String s(random(0, 1000U), '\0', test.get_allocator());
    size_t i = 0;
    for (; i != s.size(); ++i) {
        s[i] = random('a', 'z');
    }
    test = s.c_str();  // NOLINT
}

template <class String>
void clause11_21_4_2_lprime(String& test) {
    // Aliased assign
    const size_t pos = random(0, test.size());
    if (avoidAliasing) {
        test = String(test.c_str() + pos);
    } else {
        test = test.c_str() + pos;
    }
}

template <class String>
void arena_clause11_21_4_2_lprime(String& test) {
    // Aliased assign
    const size_t pos = random(0, test.size());
    if (avoidAliasing) {
        test = String(test.c_str() + pos, test.get_allocator());
    } else {
        test = test.c_str() + pos;
    }
}

template <class String>
void clause11_21_4_2_m(String& test) {
    // Assignment from char
    using value_type = typename String::value_type;
    test = random(static_cast<value_type>('a'), static_cast<value_type>('z'));
}

template <class String>
void arena_clause11_21_4_2_m(String& test) {
    // Assignment from char
    using value_type = typename String::value_type;
    test = random(static_cast<value_type>('a'), static_cast<value_type>('z'));
}

template <class String>
void clause11_21_4_2_n(String& test) {
    // Assignment from initializer_list<char>
    std::initializer_list<typename String::value_type> il = {'h', 'e', 'l', 'l', 'o'};
    test = il;
}

template <class String>
void arena_clause11_21_4_2_n(String& test) {
    // Assignment from initializer_list<char>
    std::initializer_list<typename String::value_type> il = {'h', 'e', 'l', 'l', 'o'};
    test = il;
}

template <class String>
void clause11_21_4_3(String& test) {
    // Iterators. The code below should leave test unchanged
    CHECK_EQ(test.size(), test.end() - test.begin());
    CHECK_EQ(test.size(), test.rend() - test.rbegin());
    CHECK_EQ(test.size(), test.cend() - test.cbegin());
    CHECK_EQ(test.size(), test.crend() - test.crbegin());

    auto s = test.size();
    test.resize(size_t(test.end() - test.begin()));  // NOLINT
    CHECK_EQ(s, test.size());
    test.resize(size_t(test.rend() - test.rbegin()));  // NOLINT
    CHECK_EQ(s, test.size());
}

template <class String>
void arena_clause11_21_4_3(String& test) {
    // Iterators. The code below should leave test unchanged
    CHECK_EQ(test.size(), test.end() - test.begin());
    CHECK_EQ(test.size(), test.rend() - test.rbegin());
    CHECK_EQ(test.size(), test.cend() - test.cbegin());
    CHECK_EQ(test.size(), test.crend() - test.crbegin());

    auto s = test.size();
    test.resize(size_t(test.end() - test.begin()));  // NOLINT
    CHECK_EQ(s, test.size());
    test.resize(size_t(test.rend() - test.rbegin()));  // NOLINT
    CHECK_EQ(s, test.size());
}

template <class String>
void clause11_21_4_4(String& test) {
    // exercise capacity, size, max_size
    CHECK_EQ(test.size(), test.length());
    CHECK_LE(test.size(), test.max_size());
    CHECK_LE(test.capacity(), test.max_size());
    CHECK_LE(test.size(), test.capacity());

    // exercise shrink_to_fit. Nonbinding request so we can't really do
    // much beyond calling it.
    auto copy = test;
    copy.reserve(copy.capacity() * 3);
    copy.shrink_to_fit();
    CHECK_EQ(copy, test);

    // exercise empty
    std::string empty("empty");
    std::string notempty("not empty");
    if (test.empty()) {
        test = String(empty.begin(), empty.end());
    } else {
        test = String(notempty.begin(), notempty.end());
    }
}

template <class String>
void arena_clause11_21_4_4(String& test) {
    // exercise capacity, size, max_size
    CHECK_EQ(test.size(), test.length());
    CHECK_LE(test.size(), test.max_size());
    CHECK_LE(test.capacity(), test.max_size());
    CHECK_LE(test.size(), test.capacity());

    // exercise shrink_to_fit. Nonbinding request so we can't really do
    // much beyond calling it.
    auto copy = test;
    copy.reserve(copy.capacity() * 3);
    copy.shrink_to_fit();
    CHECK_EQ(copy, test);

    // exercise empty
    std::string empty("empty");
    std::string notempty("not empty");
    if (test.empty()) {
        test = String(empty.begin(), empty.end(), test.get_allocator());
    } else {
        test = String(notempty.begin(), notempty.end(), test.get_allocator());
    }
}

template <class String>
void clause11_21_4_5(String& test) {
    // exercise element access
    if (!test.empty()) {
        CHECK_EQ(test[0], test.front());
        CHECK_EQ(test[test.size() - 1], test.back());
        auto const i = random(0, test.size() - 1);
        CHECK_EQ(test[i], test.at(i));
        test = test[i];
    }

    CHECK_THROWS_AS(test.at(test.size()), std::out_of_range);
    CHECK_THROWS_AS(test.at(test.size()), std::out_of_range);
}

template <class String>
void arena_clause11_21_4_5(String& test) {
    // exercise element access
    if (!test.empty()) {
        CHECK_EQ(test[0], test.front());
        CHECK_EQ(test[test.size() - 1], test.back());
        auto const i = random(0, test.size() - 1);
        CHECK_EQ(test[i], test.at(i));
        test = test[i];
    }

    CHECK_THROWS_AS(test.at(test.size()), std::out_of_range);
    CHECK_THROWS_AS(test.at(test.size()), std::out_of_range);
}

template <class String>
void clause11_21_4_6_1(String& test) {
    // 21.3.5 modifiers (+=)
    String test1;
    randomString(&test1);
    assert(test1.size() == std::char_traits<typename String::value_type>::length(test1.c_str()));
    auto len = test.size();
    test += test1;
    CHECK_EQ(test.size(), test1.size() + len);
    // FOR_EACH_RANGE(i, 0, test1.size()) { CHECK_EQ(test[len + i], test1[i]); }
    for (size_t i = 0; i < test1.size(); ++i) {
        CHECK_EQ(test[len + i], test1[i]);
    }
    // aliasing modifiers
    String test2 = test;
    auto dt = test2.data();
    auto sz = test.c_str();
    len = test.size();
    CHECK_EQ(memcmp(sz, dt, len), 0);
    String copy(test.data(), test.size());
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), len);
    test += test;
    // test.append(test);
    CHECK_EQ(test.size(), 2 * len);
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), 2 * len);
    for (size_t i = 0; i < len; ++i) {
        CHECK_EQ(test[i], copy[i]);
        CHECK_EQ(test[i], test[len + i]);
    }
    len = test.size();
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), len);
    // more aliasing
    auto const pos = random(0, test.size());
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str() + pos), len - pos);
    if (avoidAliasing) {
        String addMe(test.c_str() + pos);
        CHECK_EQ(addMe.size(), len - pos);
        test += addMe;
    } else {
        test += test.c_str() + pos;
    }
    CHECK_EQ(test.size(), 2 * len - pos);
    // single char
    len = test.size();
    test += random('a', 'z');
    CHECK_EQ(test.size(), len + 1);
    // initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test += il;
}

template <class String>
void arena_clause11_21_4_6_1(String& test) {
    // 21.3.5 modifiers (+=)
    String test1(test.get_allocator());
    randomString(&test1);
    assert(test1.size() == std::char_traits<typename String::value_type>::length(test1.c_str()));
    auto len = test.size();
    test += test1;
    CHECK_EQ(test.size(), test1.size() + len);
    // FOR_EACH_RANGE(i, 0, test1.size()) { CHECK_EQ(test[len + i], test1[i]); }
    for (size_t i = 0; i < test1.size(); ++i) {
        CHECK_EQ(test[len + i], test1[i]);
    }
    // aliasing modifiers
    String test2 = test;
    auto dt = test2.data();
    auto sz = test.c_str();
    len = test.size();
    CHECK_EQ(memcmp(sz, dt, len), 0);
    String copy(test.data(), test.size(), test.get_allocator());
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), len);
    test += test;
    // test.append(test);
    CHECK_EQ(test.size(), 2 * len);
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), 2 * len);
    for (size_t i = 0; i < len; ++i) {
        CHECK_EQ(test[i], copy[i]);
        CHECK_EQ(test[i], test[len + i]);
    }
    len = test.size();
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str()), len);
    // more aliasing
    auto const pos = random(0, test.size());
    CHECK_EQ(std::char_traits<typename String::value_type>::length(test.c_str() + pos), len - pos);
    if (avoidAliasing) {
        String addMe(test.c_str() + pos, test.get_allocator());
        CHECK_EQ(addMe.size(), len - pos);
        test += addMe;
    } else {
        test += test.c_str() + pos;
    }
    CHECK_EQ(test.size(), 2 * len - pos);
    // single char
    len = test.size();
    test += random('a', 'z');
    CHECK_EQ(test.size(), len + 1);
    // initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test += il;
}

template <class String>
void clause11_21_4_6_2(String& test) {
    // 21.3.5 modifiers (append, push_back)
    String s;

    // Test with a small string first
    char c = random('a', 'z');
    s.push_back(c);
    CHECK_EQ(s[s.size() - 1], c);
    CHECK_EQ(s.size(), 1);
    s.resize(s.size() - 1);

    randomString(&s, maxString);
    test.append(s);
    randomString(&s, maxString);
    test.append(s, random(0, s.size()), random(0, maxString));
    randomString(&s, maxString);
    test.append(s.c_str(), random(0, s.size()));
    randomString(&s, maxString);
    test.append(s.c_str());  // NOLINT
    test.append(random(0, maxString), random('a', 'z'));
    std::list<char> lst(RandomList(maxString));
    test.append(lst.begin(), lst.end());
    c = random('a', 'z');
    test.push_back(c);
    CHECK_EQ(test[test.size() - 1], c);
    // initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test.append(il);
}

template <class String>
void arena_clause11_21_4_6_2(String& test) {
    // 21.3.5 modifiers (append, push_back)
    String s(test.get_allocator());

    // Test with a small string first
    char c = random('a', 'z');
    s.push_back(c);
    CHECK_EQ(s[s.size() - 1], c);
    CHECK_EQ(s.size(), 1);
    s.resize(s.size() - 1);

    randomString(&s, maxString);
    test.append(s);
    randomString(&s, maxString);
    test.append(s, random(0, s.size()), random(0, maxString));
    randomString(&s, maxString);
    test.append(s.c_str(), random(0, s.size()));
    randomString(&s, maxString);
    test.append(s.c_str());  // NOLINT
    test.append(random(0, maxString), random('a', 'z'));
    std::list<char> lst(RandomList(maxString));
    test.append(lst.begin(), lst.end());
    c = random('a', 'z');
    test.push_back(c);
    CHECK_EQ(test[test.size() - 1], c);
    // initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test.append(il);
}

template <class String>
void clause11_21_4_6_3_a(String& test) {
    // assign
    String s;
    randomString(&s);
    test.assign(s);
    CHECK_EQ(test, s);
    // move assign
    test.assign(std::move(s));
    if (std::is_same<String, string>::value) {
        CHECK_LE(s.size(), 128);  // NOLINT
    }
}

template <class String>
void arena_clause11_21_4_6_3_a(String& test) {
    // assign
    String s(test.get_allocator());
    randomString(&s);
    test.assign(s);
    CHECK_EQ(test, s);
    // move assign
    test.assign(std::move(s));
    if (std::is_same<String, string>::value) {
        CHECK_LE(s.size(), 128);  // NOLINT
    }
}

template <class String>
void clause11_21_4_6_3_b(String& test) {
    // assign
    String s;
    randomString(&s, maxString);
    test.assign(s, random(0, s.size()), random(0, maxString));
}

template <class String>
void arena_clause11_21_4_6_3_b(String& test) {
    // assign
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.assign(s, random(0, s.size()), random(0, maxString));
}

template <class String>
void clause11_21_4_6_3_c(String& test) {
    // assign
    String s;
    randomString(&s, maxString);
    test.assign(s.c_str(), random(0, s.size()));
}
template <class String>
void arena_clause11_21_4_6_3_c(String& test) {
    // assign
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.assign(s.c_str(), random(0, s.size()));
}

template <class String>
void clause11_21_4_6_3_d(String& test) {
    // assign
    String s;
    randomString(&s, maxString);
    test.assign(s.c_str());  // NOLINT
}

template <class String>
void arena_clause11_21_4_6_3_d(String& test) {
    // assign
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.assign(s.c_str());  // NOLINT
}

template <class String>
void clause11_21_4_6_3_e(String& test) {
    // assign
    String s;
    randomString(&s, maxString);
    test.assign(random(0, maxString), random('a', 'z'));
}

template <class String>
void arena_clause11_21_4_6_3_e(String& test) {
    // assign
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.assign(random(0, maxString), random('a', 'z'));
}

template <class String>
void clause11_21_4_6_3_f(String& test) {
    // assign from bidirectional iterator
    std::list<char> lst(RandomList(maxString));
    test.assign(lst.begin(), lst.end());
}

template <class String>
void arena_clause11_21_4_6_3_f(String& test) {
    // assign from bidirectional iterator
    std::list<char> lst(RandomList(maxString));
    test.assign(lst.begin(), lst.end());
}

template <class String>
void clause11_21_4_6_3_g(String& test) {
    // assign from aliased source
    test.assign(test);
}

template <class String>
void arena_clause11_21_4_6_3_g(String& test) {
    // assign from aliased source
    test.assign(test);
}

template <class String>
void clause11_21_4_6_3_h(String& test) {
    // assign from aliased source
    test.assign(test, random(0, test.size()), random(0, maxString));
}

template <class String>
void arena_clause11_21_4_6_3_h(String& test) {
    // assign from aliased source
    test.assign(test, random(0, test.size()), random(0, maxString));
}

template <class String>
void clause11_21_4_6_3_i(String& test) {
    // assign from aliased source
    test.assign(test.c_str(), random(0, test.size()));
}

template <class String>
void arena_clause11_21_4_6_3_i(String& test) {
    // assign from aliased source
    test.assign(test.c_str(), random(0, test.size()));
}

template <class String>
void clause11_21_4_6_3_j(String& test) {
    // assign from aliased source
    test.assign(test.c_str());  // NOLINT
}

template <class String>
void arena_clause11_21_4_6_3_j(String& test) {
    // assign from aliased source
    test.assign(test.c_str());  // NOLINT
}

template <class String>
void clause11_21_4_6_3_k(String& test) {
    // assign from initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test.assign(il);
}

template <class String>
void arena_clause11_21_4_6_3_k(String& test) {
    // assign from initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    test.assign(il);
}

template <class String>
void clause11_21_4_6_4(String& test) {
    // insert
    String s;
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s);
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s, random(0, s.size()), random(0, maxString));
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s.c_str(), random(0, s.size()));
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s.c_str());  // NOLINT
    test.insert(random(0, test.size()), random(0, maxString), random('a', 'z'));
    typename String::size_type pos = random(0, test.size());
    typename String::iterator res = test.insert(test.begin() + int(pos), random('a', 'z'));  // NOLINT
    CHECK_EQ(res - test.begin(), pos);
    std::list<char> lst(RandomList(maxString));
    pos = random(0, test.size());
    // Uncomment below to see a bug in gcc
    /*res = */ test.insert(test.begin() + int(pos), lst.begin(), lst.end());  // NOLINT
    // insert from initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    pos = random(0, test.size());
    // Uncomment below to see a bug in gcc
    /*res = */ test.insert(test.begin() + int(pos), il);  // NOLINT

    // Test with actual input iterators
    std::stringstream ss;
    ss << "hello cruel world";
    auto i = std::istream_iterator<char>(ss);
    test.insert(test.begin(), i, std::istream_iterator<char>());
}

template <class String>
void arena_clause11_21_4_6_4(String& test) {
    // insert
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s);
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s, random(0, s.size()), random(0, maxString));
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s.c_str(), random(0, s.size()));
    randomString(&s, maxString);
    test.insert(random(0, test.size()), s.c_str());  // NOLINT
    test.insert(random(0, test.size()), random(0, maxString), random('a', 'z'));
    typename String::size_type pos = random(0, test.size());
    typename String::iterator res = test.insert(test.begin() + int(pos), random('a', 'z'));  // NOLINT
    CHECK_EQ(res - test.begin(), pos);
    std::list<char> lst(RandomList(maxString));
    pos = random(0, test.size());
    // Uncomment below to see a bug in gcc
    /*res = */ test.insert(test.begin() + int(pos), lst.begin(), lst.end());  // NOLINT
    // insert from initializer_list
    std::initializer_list<typename String::value_type> il{'a', 'b', 'c'};
    pos = random(0, test.size());
    // Uncomment below to see a bug in gcc
    /*res = */ test.insert(test.begin() + int(pos), il);  // NOLINT

    // Test with actual input iterators
    std::stringstream ss;
    ss << "hello cruel world";
    auto i = std::istream_iterator<char>(ss);
    test.insert(test.begin(), i, std::istream_iterator<char>());
}

template <class String>
void clause11_21_4_6_5(String& test) {
    // erase and pop_back
    if (!test.empty()) {
        test.erase(random(0, test.size()), random(0, maxString));
    }
    if (!test.empty()) {
        // TODO(longqimin): is erase(end()) allowed?
        test.erase(test.begin() + int(random(0, test.size() - 1)));  // NOLINT
    }
    if (!test.empty()) {
        auto const i = test.begin() + int(random(0, test.size()));  // NOLINT
        if (i != test.end()) {
            test.erase(i, i + random(0, test.end() - i));
        }
    }
    if (!test.empty()) {
        // Can't test pop_back with std::string, doesn't support it yet.
        // test.pop_back();
    }
}

template <class String>
void arena_clause11_21_4_6_5(String& test) {
    // erase and pop_back
    if (!test.empty()) {
        test.erase(random(0, test.size()), random(0, maxString));
    }
    if (!test.empty()) {
        // TODO(longqimin): is erase(end()) allowed?
        test.erase(test.begin() + int(random(0, test.size() - 1)));  // NOLINT
    }
    if (!test.empty()) {
        auto const i = test.begin() + int(random(0, test.size()));  // NOLINT
        if (i != test.end()) {
            test.erase(i, i + random(0, test.end() - i));
        }
    }
    if (!test.empty()) {
        // Can't test pop_back with std::string, doesn't support it yet.
        // test.pop_back();
    }
}

template <class String>
void clause11_21_4_6_6(String& test) {
    auto pos = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos, random(0, test.size() - pos), String(test));
    } else {
        test.replace(pos, random(0, test.size() - pos), test);
    }
    pos = random(0, test.size());
    String s;
    randomString(&s, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), s);
    auto pos1 = random(0, test.size());
    auto pos2 = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos1, pos1 + random(0, test.size() - pos1), String(test), pos2,
                     pos2 + random(0, test.size() - pos2));
    } else {
        test.replace(pos1, pos1 + random(0, test.size() - pos1), test, pos2, pos2 + random(0, test.size() - pos2));
    }
    pos1 = random(0, test.size());
    String str;
    randomString(&str, maxString);
    pos2 = random(0, str.size());
    test.replace(pos1, pos1 + random(0, test.size() - pos1), str, pos2, pos2 + random(0, str.size() - pos2));
    pos = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos, random(0, test.size() - pos), String(test).c_str(), test.size());
    } else {
        test.replace(pos, random(0, test.size() - pos), test.c_str(), test.size());
    }
    pos = random(0, test.size());
    randomString(&str, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), str.c_str(), str.size());
    pos = random(0, test.size());
    randomString(&str, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), str.c_str());
    pos = random(0, test.size());
    test.replace(pos, random(0, test.size() - pos), random(0, maxString), random('a', 'z'));
    pos = random(0, test.size());
    if (avoidAliasing) {
        auto newString = String(test);
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), newString);
    } else {
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), test);
    }
    pos = random(0, test.size());
    if (avoidAliasing) {
        auto newString = String(test);
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), newString.c_str(),
                     test.size() - random(0, test.size()));
    } else {
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), test.c_str(),
                     test.size() - random(0, test.size()));
    }
    pos = random(0, test.size());
    auto const n = random(0, test.size() - pos);
    typename String::iterator b = test.begin();
    String str1;
    randomString(&str1, maxString);
    const String& str3 = str1;
    // NOLINTNEXTLINE
    const typename String::value_type* ss = str3.c_str();
    // NOLINTNEXTLINE
    test.replace(b + int(pos), b + int(pos) + int(n), ss);
    pos = random(0, test.size());
    // NOLINTNEXTLINE
    test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), random(0, maxString),
                 random('a', 'z'));
}

template <class String>
void arena_clause11_21_4_6_6(String& test) {
    auto pos = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos, random(0, test.size() - pos), String(test));
    } else {
        test.replace(pos, random(0, test.size() - pos), test);
    }
    pos = random(0, test.size());
    String s(test.get_allocator());
    randomString(&s, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), s);
    auto pos1 = random(0, test.size());
    auto pos2 = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos1, pos1 + random(0, test.size() - pos1), String(test), pos2,
                     pos2 + random(0, test.size() - pos2));
    } else {
        test.replace(pos1, pos1 + random(0, test.size() - pos1), test, pos2, pos2 + random(0, test.size() - pos2));
    }
    pos1 = random(0, test.size());
    String str(test.get_allocator());
    randomString(&str, maxString);
    pos2 = random(0, str.size());
    test.replace(pos1, pos1 + random(0, test.size() - pos1), str, pos2, pos2 + random(0, str.size() - pos2));
    pos = random(0, test.size());
    if (avoidAliasing) {
        test.replace(pos, random(0, test.size() - pos), String(test).c_str(), test.size());
    } else {
        test.replace(pos, random(0, test.size() - pos), test.c_str(), test.size());
    }
    pos = random(0, test.size());
    randomString(&str, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), str.c_str(), str.size());
    pos = random(0, test.size());
    randomString(&str, maxString);
    test.replace(pos, pos + random(0, test.size() - pos), str.c_str());
    pos = random(0, test.size());
    test.replace(pos, random(0, test.size() - pos), random(0, maxString), random('a', 'z'));
    pos = random(0, test.size());
    if (avoidAliasing) {
        auto newString = String(test);
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), newString);
    } else {
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), test);
    }
    pos = random(0, test.size());
    if (avoidAliasing) {
        auto newString = String(test);
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), newString.c_str(),
                     test.size() - random(0, test.size()));
    } else {
        // NOLINTNEXTLINE
        test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), test.c_str(),
                     test.size() - random(0, test.size()));
    }
    pos = random(0, test.size());
    auto const n = random(0, test.size() - pos);
    typename String::iterator b = test.begin();
    String str1(test.get_allocator());
    randomString(&str1, maxString);
    const String& str3 = str1;
    // NOLINTNEXTLINE
    const typename String::value_type* ss = str3.c_str();
    // NOLINTNEXTLINE
    test.replace(b + int(pos), b + int(pos) + int(n), ss);
    pos = random(0, test.size());
    // NOLINTNEXTLINE
    test.replace(test.begin() + int(pos), test.begin() + int(pos + random(0, test.size() - pos)), random(0, maxString),
                 random('a', 'z'));
}

template <class String>
void clause11_21_4_6_7(String& test) {
    std::vector<typename String::value_type> vec(random(0, maxString));
    if (vec.empty()) {
        return;
    }
    test.copy(vec.data(), vec.size(), random(0, test.size()));
}

template <class String>
void arena_clause11_21_4_6_7(String& test) {
    std::vector<typename String::value_type> vec(random(0, maxString));
    if (vec.empty()) {
        return;
    }
    test.copy(vec.data(), vec.size(), random(0, test.size()));
}

template <class String>
void clause11_21_4_6_8(String& test) {
    String s;
    randomString(&s, maxString);
    s.swap(test);
}

template <class String>
void arena_clause11_21_4_6_8(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    s.swap(test);
}

template <class String>
void clause11_21_4_7_1(String& test) {
    // 21.3.6 string operations
    // exercise c_str() and data()
    assert(test.c_str() == test.data());
    // exercise get_allocator()
    String s;
    randomString(&s, maxString);
    CHECK(test.get_allocator() == s.get_allocator());
}

template <class String>
void arena_clause11_21_4_7_1(String& test) {
    // 21.3.6 string operations
    // exercise c_str() and data()
    assert(test.c_str() == test.data());
    // exercise get_allocator()
    String s(test.get_allocator());
    randomString(&s, maxString);
    CHECK(test.get_allocator() == s.get_allocator());
}

template <class String>
void clause11_21_4_7_2_a(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_a(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_a1(String& test) {
    String str = String(test).substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_a1(String& test) {
    String str = String(test).substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_a2(String& test) {
    auto const& cTest = test;
    String str = cTest.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_a2(String& test) {
    auto const& cTest = test;
    String str = cTest.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.find(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_b(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    String str = test.substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_2_b(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    String str = test.substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_2_b1(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    String str = String(test).substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_2_b1(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    String str = String(test).substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_2_b2(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    const auto& cTest = test;
    String str = cTest.substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_2_b2(String& test) {
    auto from = random(0, test.size());
    auto length = random(0, test.size() - from);
    const auto& cTest = test;
    String str = cTest.substr(from, length);
    Num2String(test, test.find(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_2_c(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_c(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_c1(String& test) {
    String str = String(test).substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_c1(String& test) {
    String str = String(test).substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_c2(String& test) {
    const auto& cTest = test;
    String str = cTest.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_c2(String& test) {
    const auto& cTest = test;
    String str = cTest.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.find(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_2_d(String& test) {
    Num2String(test, test.find(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_2_d(String& test) {
    Num2String(test, test.find(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_3_a(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.rfind(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_3_a(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.rfind(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_3_b(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.rfind(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_3_b(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    Num2String(test, test.rfind(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_3_c(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.rfind(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_3_c(String& test) {
    String str = test.substr(random(0, test.size()), random(0, test.size()));
    // NOLINTNEXTLINE
    Num2String(test, test.rfind(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_3_d(String& test) {
    Num2String(test, test.rfind(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_3_d(String& test) {
    Num2String(test, test.rfind(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_4_a(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_first_of(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_4_a(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_first_of(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_4_b(String& test) {
    String str;
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_4_b(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_4_c(String& test) {
    String str;
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_of(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_4_c(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_of(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_4_d(String& test) {
    Num2String(test, test.find_first_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_4_d(String& test) {
    Num2String(test, test.find_first_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_5_a(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_last_of(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_5_a(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_last_of(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_5_b(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_last_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_5_b(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_last_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_5_c(String& test) {
    String str;
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_last_of(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_5_c(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_last_of(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_5_d(String& test) {
    Num2String(test, test.find_last_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_5_d(String& test) {
    Num2String(test, test.find_last_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_6_a(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_first_not_of(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_6_a(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_first_not_of(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_6_b(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_first_not_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_6_b(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_first_not_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_6_c(String& test) {
    String str;
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_not_of(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_6_c(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_first_not_of(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_6_d(String& test) {
    Num2String(test, test.find_first_not_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_6_d(String& test) {
    Num2String(test, test.find_first_not_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_7_a(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_last_not_of(str, random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_7_a(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_last_not_of(str, random(0, test.size())));
}

template <class String>
void clause11_21_4_7_7_b(String& test) {
    String str;
    randomString(&str, maxString);
    Num2String(test, test.find_last_not_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void arena_clause11_21_4_7_7_b(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    Num2String(test, test.find_last_not_of(str.c_str(), random(0, test.size()), random(0, str.size())));
}

template <class String>
void clause11_21_4_7_7_c(String& test) {
    String str;
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_last_not_of(str.c_str(), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_7_c(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    // NOLINTNEXTLINE
    Num2String(test, test.find_last_not_of(str.c_str(), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_7_d(String& test) {
    Num2String(test, test.find_last_not_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void arena_clause11_21_4_7_7_d(String& test) {
    Num2String(test, test.find_last_not_of(random('a', 'z'), random(0, test.size())));
}

template <class String>
void clause11_21_4_7_8(String& test) {
    test = test.substr(random(0, test.size()), random(0, test.size()));
}

template <class String>
void arena_clause11_21_4_7_8(String& test) {
    test = test.substr(random(0, test.size()), random(0, test.size()));
}

template <class String>
void clause11_21_4_7_9_a(String& test) {
    String s;
    randomString(&s, maxString);
    int tristate = test.compare(s);
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void arena_clause11_21_4_7_9_a(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    int tristate = test.compare(s);
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void clause11_21_4_7_9_b(String& test) {
    String s;
    randomString(&s, maxString);
    int tristate = test.compare(random(0, test.size()), random(0, test.size()), s);
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void arena_clause11_21_4_7_9_b(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    int tristate = test.compare(random(0, test.size()), random(0, test.size()), s);
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void clause11_21_4_7_9_c(String& test) {
    String str;
    randomString(&str, maxString);
    int tristate =
      test.compare(random(0, test.size()), random(0, test.size()), str, random(0, str.size()), random(0, str.size()));
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void arena_clause11_21_4_7_9_c(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    int tristate =
      test.compare(random(0, test.size()), random(0, test.size()), str, random(0, str.size()), random(0, str.size()));
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void clause11_21_4_7_9_d(String& test) {
    String s;
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    int tristate = test.compare(s.c_str());
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void arena_clause11_21_4_7_9_d(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    int tristate = test.compare(s.c_str());
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void clause11_21_4_7_9_e(String& test) {
    String str;
    randomString(&str, maxString);
    int tristate = test.compare(random(0, test.size()), random(0, test.size()), str.c_str(), random(0, str.size()));
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void arena_clause11_21_4_7_9_e(String& test) {
    String str(test.get_allocator());
    randomString(&str, maxString);
    int tristate = test.compare(random(0, test.size()), random(0, test.size()), str.c_str(), random(0, str.size()));
    if (tristate > 0) {
        tristate = 1;
    } else if (tristate < 0) {
        tristate = 2;
    }
    Num2String(test, tristate);
}

template <class String>
void clause11_21_4_8_1_a(String& test) {
    String s1;
    randomString(&s1, maxString);
    String s2;
    randomString(&s2, maxString);
    test = s1 + s2;
}

template <class String>
void arena_clause11_21_4_8_1_a(String& test) {
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    String s2(test.get_allocator());
    randomString(&s2, maxString);
    test = s1 + s2;
}

template <class String>
void clause11_21_4_8_1_b(String& test) {
    String s1;
    randomString(&s1, maxString);
    String s2;
    randomString(&s2, maxString);
    test = std::move(s1) + s2;
}

template <class String>
void arena_clause11_21_4_8_1_b(String& test) {
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    String s2(test.get_allocator());
    randomString(&s2, maxString);
    test = std::move(s1) + s2;
}

template <class String>
void clause11_21_4_8_1_c(String& test) {
    String s1;
    randomString(&s1, maxString);
    String s2;
    randomString(&s2, maxString);
    test = s1 + std::move(s2);
}

template <class String>
void arena_clause11_21_4_8_1_c(String& test) {
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    String s2(test.get_allocator());
    randomString(&s2, maxString);
    test = s1 + std::move(s2);
}

template <class String>
void clause11_21_4_8_1_d(String& test) {
    String s1;
    randomString(&s1, maxString);
    String s2;
    randomString(&s2, maxString);
    test = std::move(s1) + std::move(s2);
}

template <class String>
void arena_clause11_21_4_8_1_d(String& test) {
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    String s2(test.get_allocator());
    randomString(&s2, maxString);
    test = std::move(s1) + std::move(s2);
}

template <class String>
void clause11_21_4_8_1_e(String& test) {
    String s;
    randomString(&s, maxString);
    String s1;
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s.c_str() + s1;
}

template <class String>
void arena_clause11_21_4_8_1_e(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s.c_str() + s1;
}

template <class String>
void clause11_21_4_8_1_f(String& test) {
    String s;
    randomString(&s, maxString);
    String s1;
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s.c_str() + std::move(s1);
}

template <class String>
void arena_clause11_21_4_8_1_f(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s.c_str() + std::move(s1);
}

template <class String>
void clause11_21_4_8_1_g(String& test) {
    String s;
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = typename String::value_type(random('a', 'z')) + s;
}

template <class String>
void arena_clause11_21_4_8_1_g(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = typename String::value_type(random('a', 'z')) + s;
}

template <class String>
void clause11_21_4_8_1_h(String& test) {
    String s;
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = typename String::value_type(random('a', 'z')) + std::move(s);
}

template <class String>
void arena_clause11_21_4_8_1_h(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = typename String::value_type(random('a', 'z')) + std::move(s);
}

template <class String>
void clause11_21_4_8_1_i(String& test) {
    String s;
    randomString(&s, maxString);
    String s1;
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s + s1.c_str();
}

template <class String>
void arena_clause11_21_4_8_1_i(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = s + s1.c_str();
}

template <class String>
void clause11_21_4_8_1_j(String& test) {
    String s;
    randomString(&s, maxString);
    String s1;
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = std::move(s) + s1.c_str();
}

template <class String>
void arena_clause11_21_4_8_1_j(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = std::move(s) + s1.c_str();
}

template <class String>
void clause11_21_4_8_1_k(String& test) {
    String s;
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = s + typename String::value_type(random('a', 'z'));
}

template <class String>
void arena_clause11_21_4_8_1_k(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    // NOLINTNEXTLINE
    test = s + typename String::value_type(random('a', 'z'));
}

template <class String>
void clause11_21_4_8_1_l(String& test) {
    String s;
    randomString(&s, maxString);
    String s1;
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = std::move(s) + s1.c_str();
}

template <class String>
void arena_clause11_21_4_8_1_l(String& test) {
    String s(test.get_allocator());
    randomString(&s, maxString);
    String s1(test.get_allocator());
    randomString(&s1, maxString);
    // NOLINTNEXTLINE
    test = std::move(s) + s1.c_str();
}

// Numbering here is from C++11
template <class String>
void clause11_21_4_8_9_a(String& test) {
    std::basic_stringstream<typename String::value_type> stst(test.c_str());
    String str;
    while (stst) {
        stst >> str;
        test += str + test;
    }
}

// Numbering here is from C++11
template <class String>
void arena_clause11_21_4_8_9_a(String& test) {
    std::basic_stringstream<typename String::value_type> stst(test.c_str());
    String str(test.get_allocator());
    while (stst) {
        stst >> str;
        test += str + test;
    }
}

TEST_CASE("c++20 string starts_with") {
    SUBCASE("starts_with_char") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.starts_with('h'), true);
        CHECK_EQ(x.starts_with('x'), false);

        stdb::memory::string  y;
        CHECK_EQ(y.starts_with('h'), false);
    }
    SUBCASE("starts_with_cstr") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.starts_with("hello"), true);
        CHECK_EQ(x.starts_with("ello"), false);
        CHECK_EQ(x.starts_with("helloworld"), true);
        CHECK_EQ(x.starts_with("helloworld "), false);
        stdb::memory::string  y;
        CHECK_EQ(y.starts_with("helloworld"), false);
        CHECK_EQ(y.starts_with("ello"), false);
    }
    SUBCASE("starts_with_string_view") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.starts_with(std::string_view ("hello")), true);
        CHECK_EQ(x.starts_with(std::string_view ("ello")), false);
        CHECK_EQ(x.starts_with(std::string_view ("helloworld")), true);
        CHECK_EQ(x.starts_with(std::string_view ("helloworld ")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.starts_with(std::string_view ("helloworld")), false);
        CHECK_EQ(y.starts_with(std::string_view ("ello")), false);
    }
    SUBCASE("starts_with_string") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.starts_with(stdb::memory::string("hello")), true);
        CHECK_EQ(x.starts_with(stdb::memory::string ("ello")), false);
        CHECK_EQ(x.starts_with(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.starts_with(stdb::memory::string("helloworld ")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.starts_with(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.starts_with(stdb::memory::string("ello")), false);
    }
}

TEST_CASE("c++20 arena_string starts_with") {
    Arena arena(Arena::Options::GetDefaultOptions());
    SUBCASE("starts_with_char") {
        stdb::memory::arena_string x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.starts_with('h'), true);
        CHECK_EQ(x.starts_with('x'), false);

        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.starts_with('h'), false);
        CHECK_EQ(y.starts_with(' '), false);
    }
    SUBCASE("starts_with_cstr") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.starts_with("hello"), true);
        CHECK_EQ(x.starts_with("ello"), false);
        CHECK_EQ(x.starts_with("helloworld"), true);
        CHECK_EQ(x.starts_with("helloworld "), false);
        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.starts_with("helloworld"), false);
        CHECK_EQ(y.starts_with("ello"), false);
        CHECK_EQ(y.starts_with(""), true);
    }
    SUBCASE("starts_with_string_view") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.starts_with(std::string_view ("hello")), true);
        CHECK_EQ(x.starts_with(std::string_view ("ello")), false);
        CHECK_EQ(x.starts_with(std::string_view ("helloworld")), true);
        CHECK_EQ(x.starts_with(std::string_view ("helloworld ")), false);
        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.starts_with(std::string_view ("helloworld")), false);
        CHECK_EQ(y.starts_with(std::string_view ("ello")), false);
        CHECK_EQ(y.starts_with(std::string_view ("")), true);
    }
    SUBCASE("starts_with_string") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.starts_with(stdb::memory::string("hello")), true);
        CHECK_EQ(x.starts_with(stdb::memory::string ("ello")), false);
        CHECK_EQ(x.starts_with(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.starts_with(stdb::memory::string("helloworld ")), false);
        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.starts_with(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.starts_with(stdb::memory::string("ello")), false);
        CHECK_EQ(y.starts_with(stdb::memory::string("")), true);
    }
}

TEST_CASE("c++20 string ends_with") {
    SUBCASE("ends_with_char") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.ends_with('d'), true);
        CHECK_EQ(x.ends_with('x'), false);

        stdb::memory::string  y;
        CHECK_EQ(y.ends_with('h'), false);
        CHECK_EQ(y.ends_with(' '), false);
    }
    SUBCASE("ends_with_cstr") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.ends_with("world"), true);
        CHECK_EQ(x.ends_with("llo"), false);
        CHECK_EQ(x.ends_with("helloworld"), true);
        CHECK_EQ(x.ends_with(" helloworld"), false);
        stdb::memory::string  y;
        CHECK_EQ(y.ends_with("helloworld"), false);
        CHECK_EQ(y.ends_with("ello"), false);
        CHECK_EQ(y.ends_with(""), true);
    }
    SUBCASE("ends_with_string_view") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.ends_with(std::string_view ("world")), true);
        CHECK_EQ(x.ends_with(std::string_view ("hello")), false);
        CHECK_EQ(x.ends_with(std::string_view ("helloworld")), true);
        CHECK_EQ(x.ends_with(std::string_view (" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.ends_with(std::string_view ("helloworld")), false);
        CHECK_EQ(y.ends_with(std::string_view ("ello")), false);
        CHECK_EQ(y.ends_with(std::string_view ("")), true);
    }
    SUBCASE("ends_with_string") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.ends_with(stdb::memory::string("world")), true);
        CHECK_EQ(x.ends_with(stdb::memory::string ("ello")), false);
        CHECK_EQ(x.ends_with(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.ends_with(stdb::memory::string(" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.ends_with(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.ends_with(stdb::memory::string("ello")), false);
        CHECK_EQ(y.ends_with(stdb::memory::string("")), true);
    }
}

TEST_CASE("c++20 arena_string ends_with") {
    Arena arena(Arena::Options::GetDefaultOptions());
    SUBCASE("ends_with_char") {
        stdb::memory::arena_string x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.ends_with('d'), true);
        CHECK_EQ(x.ends_with('x'), false);

        stdb::memory::string  y;
        CHECK_EQ(y.ends_with('h'), false);
    }
    SUBCASE("ends_with_cstr") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.ends_with("world"), true);
        CHECK_EQ(x.ends_with("ello"), false);
        CHECK_EQ(x.ends_with("helloworld"), true);
        CHECK_EQ(x.ends_with(" helloworld"), false);
        stdb::memory::arena_string  y(arena.get_memory_resource());
        CHECK_EQ(y.ends_with("helloworld"), false);
        CHECK_EQ(y.ends_with("ello"), false);
    }
    SUBCASE("ends_with_string_view") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.ends_with(std::string_view ("world")), true);
        CHECK_EQ(x.ends_with(std::string_view ("ello")), false);
        CHECK_EQ(x.ends_with(std::string_view ("helloworld")), true);
        CHECK_EQ(x.ends_with(std::string_view (" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.starts_with(std::string_view ("helloworld")), false);
        CHECK_EQ(y.starts_with(std::string_view ("ello")), false);
    }
    SUBCASE("ends_with_string") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.ends_with(stdb::memory::string("world")), true);
        CHECK_EQ(x.ends_with(stdb::memory::string ("ello")), false);
        CHECK_EQ(x.ends_with(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.ends_with(stdb::memory::string(" helloworld")), false);
        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.ends_with(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.ends_with(stdb::memory::string("ello")), false);
        CHECK_EQ(y.ends_with(stdb::memory::string("")), true);
    }
}

TEST_CASE("c++20 string contains") {
    SUBCASE("contains_with_char") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.contains('l'), true);
        CHECK_EQ(x.contains('z'), false);

        stdb::memory::string  y;
        CHECK_EQ(y.contains('y'), false);
        CHECK_EQ(y.contains(' '), false);
    }
    SUBCASE("contains_with_cstr") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.contains("world"), true);
        CHECK_EQ(x.contains("llo"), true);
        CHECK_EQ(x.contains("lol"), false);
        CHECK_EQ(x.contains("helloworld"), true);
        CHECK_EQ(x.contains(" helloworld"), false);
        stdb::memory::string  y;
        CHECK_EQ(y.contains("helloworld"), false);
        CHECK_EQ(y.contains("ello"), false);
        CHECK_EQ(y.contains(""), true);
    }
    SUBCASE("contains_with_string_view") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.contains(std::string_view ("world")), true);
        CHECK_EQ(x.contains(std::string_view ("hello")), true);
        CHECK_EQ(x.contains(std::string_view ("xxx")), false);
        CHECK_EQ(x.contains(std::string_view ("helloworld")), true);
        CHECK_EQ(x.contains(std::string_view (" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.contains(std::string_view ("helloworld")), false);
        CHECK_EQ(y.contains(std::string_view ("ello")), false);
        CHECK_EQ(y.contains(std::string_view ("")), true);
    }
    SUBCASE("contains_with_string") {
        stdb::memory::string  x("helloworld");
        CHECK_EQ(x.contains(stdb::memory::string("world")), true);
        CHECK_EQ(x.contains(stdb::memory::string ("ello")), true);
        CHECK_EQ(x.contains(stdb::memory::string ("xello")), false);
        CHECK_EQ(x.contains(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.contains(stdb::memory::string(" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.contains(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.contains(stdb::memory::string("ello")), false);
        CHECK_EQ(y.contains(stdb::memory::string("")), true);
    }
}

TEST_CASE("c++20 arena_string contains") {
    Arena arena(Arena::Options::GetDefaultOptions());
    SUBCASE("contains_with_char") {
        stdb::memory::arena_string x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.contains('d'), true);
        CHECK_EQ(x.contains('x'), false);

        stdb::memory::string  y;
        CHECK_EQ(y.contains('h'), false);
    }
    SUBCASE("contains_with_cstr") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.contains("world"), true);
        CHECK_EQ(x.contains("ello"), true);
        CHECK_EQ(x.contains("zzello"), false);
        CHECK_EQ(x.contains("helloworld"), true);
        CHECK_EQ(x.contains(" helloworld"), false);
        stdb::memory::arena_string  y(arena.get_memory_resource());
        CHECK_EQ(y.contains("helloworld"), false);
        CHECK_EQ(y.contains("ello"), false);
        CHECK_EQ(y.contains(""), true);
    }
    SUBCASE("contains_with_string_view") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.contains(std::string_view ("world")), true);
        CHECK_EQ(x.contains(std::string_view ("ello")), true);
        CHECK_EQ(x.contains(std::string_view ("zzello")), false);
        CHECK_EQ(x.contains(std::string_view ("helloworld")), true);
        CHECK_EQ(x.contains(std::string_view (" helloworld")), false);
        stdb::memory::string  y;
        CHECK_EQ(y.contains(std::string_view ("helloworld")), false);
        CHECK_EQ(y.contains(std::string_view ("ello")), false);
        CHECK_EQ(y.contains(std::string_view ("")), true);
    }
    SUBCASE("contains_with_string") {
        stdb::memory::arena_string  x("helloworld", arena.get_memory_resource());
        CHECK_EQ(x.contains(stdb::memory::string("world")), true);
        CHECK_EQ(x.contains(stdb::memory::string ("ello")), true);
        CHECK_EQ(x.contains(stdb::memory::string ("xello")), false);
        CHECK_EQ(x.contains(stdb::memory::string("helloworld")), true);
        CHECK_EQ(x.contains(stdb::memory::string(" helloworld")), false);
        stdb::memory::arena_string y(arena.get_memory_resource());
        CHECK_EQ(y.contains(stdb::memory::string("helloworld")), false);
        CHECK_EQ(y.contains(stdb::memory::string("ello")), false);
        CHECK_EQ(y.contains(stdb::memory::string("")), true);
    }
}


TEST_CASE("string::testAllClauses") {
    std::cout << "Starting with seed: " << seed << std::endl;
    std::string r;
    string c;

    uint count = 0;

    auto l = [&](const char* const clause, void (*f_string)(std::string&), void (*f_fbstring)(string&)) {
        do {
            // NOLINTNEXTLINE
            if (true) {
            } else {
                std::cout << "Testing clause " << clause << std::endl;
            }
            randomString(&r);
            c = r;
            CHECK_EQ(c, r);

            auto localSeed = seed + count;
            rng = RandomT(localSeed);
            f_string(r);
            rng = RandomT(localSeed);
            f_fbstring(c);
        } while (++count % 100 != 0);
    };

#define TEST_CLAUSE(x) l(#x, clause11_##x<std::string>, clause11_##x<string>);

    TEST_CLAUSE(21_4_2_a);
    TEST_CLAUSE(21_4_2_b);
    TEST_CLAUSE(21_4_2_c);
    TEST_CLAUSE(21_4_2_d);
    TEST_CLAUSE(21_4_2_e);
    TEST_CLAUSE(21_4_2_f);
    TEST_CLAUSE(21_4_2_g);
    TEST_CLAUSE(21_4_2_h);
    TEST_CLAUSE(21_4_2_i);
    TEST_CLAUSE(21_4_2_j);
    TEST_CLAUSE(21_4_2_k);
    TEST_CLAUSE(21_4_2_l);
    TEST_CLAUSE(21_4_2_lprime);
    TEST_CLAUSE(21_4_2_m);
    TEST_CLAUSE(21_4_2_n);
    TEST_CLAUSE(21_4_3);
    TEST_CLAUSE(21_4_4);
    TEST_CLAUSE(21_4_5);
    TEST_CLAUSE(21_4_6_1);
    TEST_CLAUSE(21_4_6_2);
    TEST_CLAUSE(21_4_6_3_a);
    TEST_CLAUSE(21_4_6_3_b);
    TEST_CLAUSE(21_4_6_3_c);
    TEST_CLAUSE(21_4_6_3_d);
    TEST_CLAUSE(21_4_6_3_e);
    TEST_CLAUSE(21_4_6_3_f);
    TEST_CLAUSE(21_4_6_3_g);
    TEST_CLAUSE(21_4_6_3_h);
    TEST_CLAUSE(21_4_6_3_i);
    TEST_CLAUSE(21_4_6_3_j);
    TEST_CLAUSE(21_4_6_3_k);
    TEST_CLAUSE(21_4_6_4);
    TEST_CLAUSE(21_4_6_5);
    TEST_CLAUSE(21_4_6_6);
    TEST_CLAUSE(21_4_6_7);
    TEST_CLAUSE(21_4_6_8);
    TEST_CLAUSE(21_4_7_1);

    TEST_CLAUSE(21_4_7_2_a);
    TEST_CLAUSE(21_4_7_2_a1);
    TEST_CLAUSE(21_4_7_2_a2);
    TEST_CLAUSE(21_4_7_2_b);
    TEST_CLAUSE(21_4_7_2_b1);
    TEST_CLAUSE(21_4_7_2_b2);
    TEST_CLAUSE(21_4_7_2_c);
    TEST_CLAUSE(21_4_7_2_c1);
    TEST_CLAUSE(21_4_7_2_c2);
    TEST_CLAUSE(21_4_7_2_d);
    TEST_CLAUSE(21_4_7_3_a);
    TEST_CLAUSE(21_4_7_3_b);
    TEST_CLAUSE(21_4_7_3_c);
    TEST_CLAUSE(21_4_7_3_d);
    TEST_CLAUSE(21_4_7_4_a);
    TEST_CLAUSE(21_4_7_4_b);
    TEST_CLAUSE(21_4_7_4_c);
    TEST_CLAUSE(21_4_7_4_d);
    TEST_CLAUSE(21_4_7_5_a);
    TEST_CLAUSE(21_4_7_5_b);
    TEST_CLAUSE(21_4_7_5_c);
    TEST_CLAUSE(21_4_7_5_d);
    TEST_CLAUSE(21_4_7_6_a);
    TEST_CLAUSE(21_4_7_6_b);
    TEST_CLAUSE(21_4_7_6_c);
    TEST_CLAUSE(21_4_7_6_d);
    TEST_CLAUSE(21_4_7_7_a);
    TEST_CLAUSE(21_4_7_7_b);
    TEST_CLAUSE(21_4_7_7_c);
    TEST_CLAUSE(21_4_7_7_d);
    TEST_CLAUSE(21_4_7_8);
    TEST_CLAUSE(21_4_7_9_a);
    TEST_CLAUSE(21_4_7_9_b);
    TEST_CLAUSE(21_4_7_9_c);
    TEST_CLAUSE(21_4_7_9_d);
    TEST_CLAUSE(21_4_7_9_e);
    TEST_CLAUSE(21_4_8_1_a);
    TEST_CLAUSE(21_4_8_1_b);
    TEST_CLAUSE(21_4_8_1_c);
    TEST_CLAUSE(21_4_8_1_d);
    TEST_CLAUSE(21_4_8_1_e);
    TEST_CLAUSE(21_4_8_1_f);
    TEST_CLAUSE(21_4_8_1_g);
    TEST_CLAUSE(21_4_8_1_h);
    TEST_CLAUSE(21_4_8_1_i);
    TEST_CLAUSE(21_4_8_1_j);
    TEST_CLAUSE(21_4_8_1_k);
    TEST_CLAUSE(21_4_8_1_l);
    TEST_CLAUSE(21_4_8_9_a);
}

TEST_CASE("string::arena") {
    auto arena = Arena(Arena::Options::GetDefaultOptions());

    {
        auto* str = arena.Create<string>();
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 0);
        CHECK_EQ(*str, "");
    }
    {
        auto* str = arena.Create<string>("");
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 0);
        CHECK_EQ(*str, "");
    }
    {
        auto* str = arena.Create<string>("\0");
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 0);
        CHECK_EQ(*str, "");
    }

    {
        auto* str = arena.Create<string>("12345");
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 5);
        CHECK_EQ(*str, "12345");
    }
    {
        auto* str = arena.Create<string>("12345\0");
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 5);
        CHECK_EQ(*str, "12345");
    }
    {
        auto* str = arena.Create<string>("1234567890abcdefghijklmnopqrstuvwxyz");
        CHECK(str != nullptr);
        CHECK_EQ(str->size(), 36);
        CHECK_EQ(*str, "1234567890abcdefghijklmnopqrstuvwxyz");
    }
}

TEST_CASE("string::testMoveCtor") {
    // Move constructor. Make sure we allocate a large string, so the
    // small string optimization doesn't kick in.
    size_t size = random(100, 2000U);
    string s(size, 'a');
    string test = std::move(s);
    // NOLINTNEXTLINE
    CHECK(s.empty());
    CHECK_EQ(size, test.size());
}

TEST_CASE("string::testMoveAssign") {
    // Move constructor. Make sure we allocate a large string, so the
    // small string optimization doesn't kick in.
    size_t size = random(100, 2000U);
    string s(size, 'a');
    string test;
    test = std::move(s);
    // NOLINTNEXTLINE
    CHECK(s.empty());
    CHECK_EQ(size, test.size());
}

TEST_CASE("string::testMoveOperatorPlusLhs") {
    // Make sure we allocate a large string, so the
    // small string optimization doesn't kick in.
    size_t size1 = random(100, 2000U);
    size_t size2 = random(100, 2000U);
    string s1(size1, 'a');
    string s2(size2, 'b');
    string test;
    test = std::move(s1) + s2;
    // NOLINTNEXTLINE
    CHECK(s1.empty());
    CHECK_EQ(size1 + size2, test.size());
}

TEST_CASE("string::testMoveOperatorPlusRhs") {
    // Make sure we allocate a large string, so the
    // small string optimization doesn't kick in.
    size_t size1 = random(100, 2000U);
    size_t size2 = random(100, 2000U);
    string s1(size1, 'a');
    string s2(size2, 'b');
    string test;
    test = s1 + std::move(s2);
    CHECK_EQ(size1 + size2, test.size());
}

// The GNU C++ standard library throws an std::logic_error when an std::string
// is constructed with a null pointer. Verify that we mirror this behavior.
//
// N.B. We behave this way even if the C++ library being used is something
//      other than libstdc++. Someday if we deem it important to present
//      identical undefined behavior for other platforms, we can re-visit this.
TEST_CASE("string::testConstructionFromLiteralZero") {
    CHECK_THROWS_AS(string s(nullptr), std::logic_error);
}  // NOLINT

TEST_CASE("string::testFixedBugs_D479397") {
    string str(1337, 'f');
    string cp = str;
    cp.clear();
    (void)cp.c_str();
    CHECK_EQ(str.front(), 'f');
}

TEST_CASE("string::testFixedBugs_D481173") {
    string str(1337, 'f');
    for (int i = 0; i < 2; ++i) {
        string cp = str;
        cp[1] = 'b';
        CHECK_EQ(cp.c_str()[cp.size()], '\0');
        cp.push_back('?');
    }
}

TEST_CASE("string::testFixedBugs_D580267_push_back") {
    string str(1337, 'f');
    string cp = str;
    cp.push_back('f');
}

TEST_CASE("string::testFixedBugs_D580267_operator_add_assign") {
    string str(1337, 'f');
    string cp = str;
    cp += "bb";
}

TEST_CASE("string::testFixedBugs_D661622") {
    stdb::memory::basic_string<wchar_t> s;
    CHECK_EQ(0, s.size());
}

TEST_CASE("string::testFixedBugs_D785057") {
    string str(1337, 'f');
    std::swap(str, str);
    CHECK_EQ(1337, str.size());
}

TEST_CASE("string::testFixedBugs_D1012196_allocator_malloc") {
    string str(128, 'f');
    str.clear();       // Empty medium string.
    string copy(str);  // Medium string of 0 capacity.
    copy.push_back('b');
    CHECK(copy.capacity() >= 1);
}

TEST_CASE("string::testFixedBugs_D2813713") {
    string s1("a");
    s1.reserve(8);  // Trigger the optimized code path.
    auto test1 = '\0' + std::move(s1);
    CHECK_EQ(2, test1.size());

    string s2(1, '\0');
    s2.reserve(8);
    auto test2 = "a" + std::move(s2);
    CHECK_EQ(2, test2.size());
}

TEST_CASE("string::testFixedBugs_D3698862") { CHECK_EQ(string().find(string(), 4), string::npos); }

TEST_CASE("string::findWithNpos") {
    string fbstr("localhost:80");
    CHECK_EQ(string::npos, fbstr.find(":", string::npos));
}

TEST_CASE("string::testHash") {
    string a;
    string b;
    a.push_back(0);
    a.push_back(1);
    b.push_back(0);
    b.push_back(2);
    std::hash<string> hashfunc;
    CHECK_NE(hashfunc(a), hashfunc(b));
}

TEST_CASE("string::testFrontBack") {
    string str("hello");
    CHECK_EQ(str.front(), 'h');
    CHECK_EQ(str.back(), 'o');
    str.front() = 'H';
    CHECK_EQ(str.front(), 'H');
    str.back() = 'O';
    CHECK_EQ(str.back(), 'O');
    CHECK_EQ(str, "HellO");
}

TEST_CASE("string::noexcept") {
    CHECK(noexcept(string()));
    string x;
    CHECK_FALSE(noexcept(string(x)));
    CHECK(noexcept(string(std::move(x))));
    string y;
    CHECK_FALSE(noexcept(y = x));
    CHECK(noexcept(y = std::move(x)));
}

TEST_CASE("string::rvalueIterators") {
    // you cannot take &* of a move-iterator, so use that for testing
    string s = "base";
    string r = "hello";
    r.replace(r.begin(), r.end(), std::make_move_iterator(s.begin()), std::make_move_iterator(s.end()));
    CHECK_EQ("base", r);

    // The following test is probably not required by the standard.
    // i.e. this could be in the realm of undefined behavior.
    string b = "123abcXYZ";
    auto ait = b.begin() + 3;  // NOLINT
    auto Xit = b.begin() + 6;  // NOLINT
    b.replace(ait, b.end(), b.begin(), Xit);
    CHECK_EQ("123123abc", b);  // if things go wrong, you'd get "123123123"
}

TEST_CASE("string::moveTerminator") {
    // The source of a move must remain in a valid state
    string s(100, 'x');  // too big to be in-situ
    string k;
    k = std::move(s);

    CHECK_EQ(0, s.size());  // NOLINT
    CHECK_EQ('\0', *s.c_str());
}

/*
 * t8968589: Clang 3.7 refused to compile w/ certain constructors (specifically
 * those that were "explicit" and had a defaulted parameter, if they were used
 * in structs which were default-initialized).  Exercise these just to ensure
 * they compile.
 *
 * In diff D2632953 the old constructor:
 *   explicit basic_string(const A& a = A()) noexcept;
 *
 * was split into these two, as a workaround:
 *   basic_string() noexcept;
 *   explicit basic_string(const A& a) noexcept;
 */

struct TestStructDefaultAllocator
{
    stdb::memory::basic_string<char> stringMember;
};

std::atomic<size_t> allocatorConstructedCount(0);
struct TestStructStringAllocator : std::allocator<char>
{
    TestStructStringAllocator() { ++allocatorConstructedCount; }
};

TEST_CASE("FBStringCtorTest::DefaultInitStructDefaultAlloc") {
    TestStructDefaultAllocator t1{};
    CHECK(t1.stringMember.empty());
}

TEST_CASE("FBStringCtorTest::NullZeroConstruction") {
    char* p = nullptr;
    size_t n = 0;
    stdb::memory::string f(p, n);
    CHECK_EQ(f.size(), 0);
}

// Tests for the comparison operators. I use CHECK rather than CHECK_LE
// because what's under test is the operator rather than the relation between
// the objects.

TEST_CASE("string::compareToStdString") {
    using stdb::memory::string;
    using namespace std::string_literals;
    auto stdA = "a"s;
    auto stdB = "b"s;
    string fbA("a");
    string fbB("b");
    CHECK(stdA == fbA);
    CHECK(fbB == stdB);
    CHECK(stdA != fbB);
    CHECK(fbA != stdB);
    CHECK(stdA < fbB);
    CHECK(fbA < stdB);
    CHECK(stdB > fbA);
    CHECK(fbB > stdA);
    CHECK(stdA <= fbB);
    CHECK(fbA <= stdB);
    CHECK(stdA <= fbA);
    CHECK(fbA <= stdA);
    CHECK(stdB >= fbA);
    CHECK(fbB >= stdA);
    CHECK(stdB >= fbB);
    CHECK(fbB >= stdB);
}

// TEST_CASE("U16FBString::compareToStdU16String") {
//     using stdb::memory::basic_string;
//     using namespace std::string_literals;
//     auto stdA = u"a"s;
//     auto stdB = u"b"s;
//     basic_string<char16_t> fbA(u"a");
//     basic_string<char16_t> fbB(u"b");
//     CHECK(stdA == fbA);
//     CHECK(fbB == stdB);
//     CHECK(stdA != fbB);
//     CHECK(fbA != stdB);
//     CHECK(stdA < fbB);
//     CHECK(fbA < stdB);
//     CHECK(stdB > fbA);
//     CHECK(fbB > stdA);
//     CHECK(stdA <= fbB);
//     CHECK(fbA <= stdB);
//     CHECK(stdA <= fbA);
//     CHECK(fbA <= stdA);
//     CHECK(stdB >= fbA);
//     CHECK(fbB >= stdA);
//     CHECK(stdB >= fbB);
//     CHECK(fbB >= stdB);
// }

// TEST_CASE("U32FBString::compareToStdU32String") {
//     using stdb::memory::basic_string;
//     using namespace std::string_literals;
//     auto stdA = U"a"s;
//     auto stdB = U"b"s;
//     basic_string<char32_t> fbA(U"a");
//     basic_string<char32_t> fbB(U"b");
//     CHECK(stdA == fbA);
//     CHECK(fbB == stdB);
//     CHECK(stdA != fbB);
//     CHECK(fbA != stdB);
//     CHECK(stdA < fbB);
//     CHECK(fbA < stdB);
//     CHECK(stdB > fbA);
//     CHECK(fbB > stdA);
//     CHECK(stdA <= fbB);
//     CHECK(fbA <= stdB);
//     CHECK(stdA <= fbA);
//     CHECK(fbA <= stdA);
//     CHECK(stdB >= fbA);
//     CHECK(fbB >= stdA);
//     CHECK(stdB >= fbB);
//     CHECK(fbB >= stdB);
// }

// TEST_CASE("WFBString::compareToStdWString") {
//     using stdb::memory::basic_string;
//     using namespace std::string_literals;
//     auto stdA = L"a"s;
//     auto stdB = L"b"s;
//     basic_string<wchar_t> fbA(L"a");
//     basic_string<wchar_t> fbB(L"b");
//     CHECK(stdA == fbA);
//     CHECK(fbB == stdB);
//     CHECK(stdA != fbB);
//     CHECK(fbA != stdB);
//     CHECK(stdA < fbB);
//     CHECK(fbA < stdB);
//     CHECK(stdB > fbA);
//     CHECK(fbB > stdA);
//     CHECK(stdA <= fbB);
//     CHECK(fbA <= stdB);
//     CHECK(stdA <= fbA);
//     CHECK(fbA <= stdA);
//     CHECK(stdB >= fbA);
//     CHECK(fbB >= stdA);
//     CHECK(stdB >= fbB);
//     CHECK(fbB >= stdB);
// }

// Same again, but with a more challenging input - a common prefix and different
// lengths.

TEST_CASE("string::compareToStdStringLong") {
    using stdb::memory::string;
    using namespace std::string_literals;
    auto stdA = "1234567890a"s;
    auto stdB = "1234567890ab"s;
    string fbA("1234567890a");
    string fbB("1234567890ab");
    CHECK(stdA == fbA);
    CHECK(fbB == stdB);
    CHECK(stdA != fbB);
    CHECK(fbA != stdB);
    CHECK(stdA < fbB);
    CHECK(fbA < stdB);
    CHECK(stdB > fbA);
    CHECK(fbB > stdA);
    CHECK(stdA <= fbB);
    CHECK(fbA <= stdB);
    CHECK(stdA <= fbA);
    CHECK(fbA <= stdA);
    CHECK(stdB >= fbA);
    CHECK(fbB >= stdA);
    CHECK(stdB >= fbB);
    CHECK(fbB >= stdB);
}

// TEST_CASE("U16FBString::compareToStdU16StringLong") {
//     using stdb::memory::basic_string;
//     using namespace std::string_literals;
//     auto stdA = u"1234567890a"s;
//     auto stdB = u"1234567890ab"s;
//     basic_string<char16_t> fbA(u"1234567890a");
//     basic_string<char16_t> fbB(u"1234567890ab");
//     CHECK(stdA == fbA);
//     CHECK(fbB == stdB);
//     CHECK(stdA != fbB);
//     CHECK(fbA != stdB);
//     CHECK(stdA < fbB);
//     CHECK(fbA < stdB);
//     CHECK(stdB > fbA);
//     CHECK(fbB > stdA);
//     CHECK(stdA <= fbB);
//     CHECK(fbA <= stdB);
//     CHECK(stdA <= fbA);
//     CHECK(fbA <= stdA);
//     CHECK(stdB >= fbA);
//     CHECK(fbB >= stdA);
//     CHECK(stdB >= fbB);
//     CHECK(fbB >= stdB);
// }

struct custom_traits : public std::char_traits<char>
{};

TEST_CASE("string::convertToStringView") {
    string s("foo");
    std::string_view sv = s;
    CHECK_EQ(sv, "foo");
    basic_string<char, custom_traits> s2("bar");
    std::basic_string_view<char, custom_traits> sv2 = s2;
    CHECK_EQ(sv2, "bar");
}

TEST_CASE("string::Format") { CHECK_EQ("  foo", fmt::format("{:>5}", stdb::memory::string("foo"))); }

TEST_CASE("string::OverLarge") {
    CHECK_THROWS_AS(string().reserve((size_t)0xFFFF'FFFF'FFFF'FFFF), std::length_error);
    CHECK_THROWS_AS(string_core<char32_t>().reserve((size_t)0x4000'0000'4000'0000), std::length_error);
}

TEST_CASE("string::Clone") {
    SUBCASE("stack-based-small") {
        string s1("foo");
        auto s2 = s1.clone();
        CHECK_EQ(s1, s2);
        auto* data1 = s1.data();
        auto* data2 = s2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("stack-based-medium") {
        string m1("1234567890123456789012345678901234567890");
        auto m2 = m1.clone();
        CHECK_EQ(m1.length(), 40);
        CHECK_EQ(m1, m2);
        auto* data1 = m1.data();
        auto* data2 = m2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("stack-based-large") {
        string m1("1234567890123456789012345678901234567890");
        string l1;
        for (int i = 0; i < 125; ++i) {
            l1.append(m1);
        }
        CHECK_EQ(l1.length(), 5000);
        auto l2_cow = l1;
        auto l2 = l1.clone();
        CHECK_EQ(l1, l2);
        CHECK_EQ(l1, l2_cow);
        CHECK_EQ(l2_cow.data(), l1.data());
        CHECK_NE(l2.data(), l1.data());
    }

    SUBCASE("heap-based-small") {
        auto s1_up = std::make_unique<string>("123");
        auto s2_up = std::make_unique<string>(s1_up->clone());
        CHECK_EQ(*s1_up, *s2_up);
        auto* data1 = s1_up->data();
        auto* data2 = s2_up->data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("heap-based-medium") {
        auto m1_up = std::make_unique<string>("1234567890123456789012345678901234567890");
        auto m2_up = std::make_unique<string>(m1_up->clone());
        CHECK_EQ(m1_up->length(), 40);
        CHECK_EQ(*m1_up, *m2_up);
        auto* data1 = m1_up->data();
        auto* data2 = m2_up->data();
        CHECK_NE(data1, data2);
    }
    SUBCASE("heap-based-large") {
        string m1("1234567890123456789012345678901234567890");
        auto l1_up = std::make_unique<string>();
        for (int i = 0; i < 125; ++i) {
            l1_up->append(m1);
        }
        CHECK_EQ(l1_up->length(), 5000);
        auto l2_up = std::make_unique<string>(l1_up->clone());
        auto l2_cow = *l1_up;
        CHECK_EQ(*l1_up, *l2_up);
        auto* data1 = l1_up->data();
        auto* data2 = l2_up->data();
        auto* data2_cow = l2_cow.data();
        CHECK_EQ(data1, data2_cow);
        CHECK_NE(data1, data2);
    }
}
TEST_CASE("arena_string::normal") {
    Arena arena(Arena::Options::GetDefaultOptions());
    SUBCASE("Create") {
        auto* str = arena.Create<arena_string>();
        CHECK_EQ(*str, "");
        CHECK_EQ(arena.check(reinterpret_cast<const char*>(str)), ArenaContainStatus::BlockUsed);
        CHECK_EQ(arena.check(str->data()), ArenaContainStatus::BlockUsed);
        auto* str1 = arena.Create<arena_string>("1234567");
        CHECK_EQ(*str1, "1234567");
        CHECK_EQ(arena.check(reinterpret_cast<const char*>(str1)), ArenaContainStatus::BlockUsed);
        CHECK_EQ(arena.check(str1->data()), ArenaContainStatus::BlockUsed);
    }
    SUBCASE("cstr") {
        arena_string str(arena.get_memory_resource());
        CHECK_EQ(str, "");
        CHECK_EQ(arena.check(str.data()), ArenaContainStatus::NotContain);
        arena_string str1("1234567", arena.get_memory_resource());
        CHECK_EQ(str1, "1234567");
        CHECK_EQ(arena.check(str1.data()), ArenaContainStatus::NotContain);

        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        CHECK_EQ(arena.check(str_long.data()), ArenaContainStatus::BlockUsed);
        str1.append(str_long.cbegin(), str_long.cend());
    }

    SUBCASE("copy") {
        arena_string str("1234567", arena.get_memory_resource());
        arena_string copied_str(str);
        CHECK_EQ(arena.check(copied_str.data()), ArenaContainStatus::NotContain);
        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        arena_string copied_str_long(str_long);
        CHECK_EQ(arena.check(copied_str_long.data()), ArenaContainStatus::BlockUsed);
    }

    SUBCASE("move") {
        arena_string str("1234567", arena.get_memory_resource());
        arena_string copied_str(std::move(str));
        CHECK_EQ(arena.check(copied_str.data()), ArenaContainStatus::NotContain);
        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        arena_string copied_str_long(std::move(str_long));
        CHECK_EQ(arena.check(copied_str_long.data()), ArenaContainStatus::BlockUsed);
    }
}

TEST_CASE("arena_string::Clone") {
    Arena arena(Arena::Options::GetDefaultOptions());

    SUBCASE("create_small") {
        auto* as1 = arena.Create<arena_string>("123");
        auto as2 = as1->clone();
        CHECK_EQ(*as1, as2);
        auto* data1 = as1->data();
        auto* data2 = as2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("small") {
        arena_string as1(arena.get_memory_resource());
        auto as2 = as1.clone();
        CHECK_EQ(as1, as2);
        auto* data1 = as1.data();
        auto* data2 = as2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("create_medium") {
        auto* as1 = arena.Create<arena_string>("1234567890123456789012345678901234567890");
        auto as2 = as1->clone();
        CHECK_EQ(*as1, as2);
        auto* data1 = as1->data();
        auto* data2 = as2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("medium") {
        arena_string as1("1234567890123456789012345678901234567890", arena.get_memory_resource());
        auto as2 = as1.clone();
        CHECK_EQ(as1, as2);
        auto* data1 = as1.data();
        auto* data2 = as2.data();
        CHECK_NE(data1, data2);
    }

    SUBCASE("create_large") {
        arena_string* m1 = arena.Create<arena_string>("1234567890123456789012345678901234567890");
        auto* l1 = arena.Create<arena_string>();
        for (int i = 0; i < 125; ++i) {
            l1->append(*m1);
        }
        CHECK_EQ(l1->length(), 5000);
        auto l2(l1->clone());
        auto l2_cow = *l1;
        CHECK_EQ(*l1, l2);
        auto* data1 = l1->data();
        auto* data2 = l2.data();
        auto* data2_cow = l2_cow.data();
        CHECK_EQ(data1, data2_cow);
        CHECK_NE(data1, data2);
    }

    SUBCASE("large") {
        arena_string m1("1234567890123456789012345678901234567890", arena.get_memory_resource());
        arena_string l1(arena.get_memory_resource());
        for (int i = 0; i < 125; ++i) {
            l1.append(m1);
        }
        CHECK_EQ(l1.length(), 5000);
        auto l2(l1.clone());
        auto l2_cow = l1;
        CHECK_EQ(l1, l2);
        auto* data1 = l1.data();
        auto* data2 = l2.data();
        auto* data2_cow = l2_cow.data();
        CHECK_EQ(data1, data2_cow);
        CHECK_NE(data1, data2);
    }
}
TEST_CASE("arena_string::normal") {
    Arena arena(Arena::Options::GetDefaultOptions());
    SUBCASE("Create") {
        auto* str = arena.Create<arena_string>();
        CHECK_EQ(*str, "");
        CHECK_EQ(arena.check(reinterpret_cast<const char*>(str)), ArenaContainStatus::BlockUsed);
        CHECK_EQ(arena.check(str->data()), ArenaContainStatus::BlockUsed);
        auto* str1 = arena.Create<arena_string>("1234567");
        CHECK_EQ(*str1, "1234567");
        CHECK_EQ(arena.check(reinterpret_cast<const char*>(str1)), ArenaContainStatus::BlockUsed);
        CHECK_EQ(arena.check(str1->data()), ArenaContainStatus::BlockUsed);
    }
    SUBCASE("cstr") {
        arena_string str(arena.get_memory_resource());
        CHECK_EQ(str, "");
        CHECK_EQ(arena.check(str.data()), ArenaContainStatus::NotContain);
        arena_string str1("1234567", arena.get_memory_resource());
        CHECK_EQ(str1, "1234567");
        CHECK_EQ(arena.check(str1.data()), ArenaContainStatus::NotContain);

        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        CHECK_EQ(arena.check(str_long.data()), ArenaContainStatus::BlockUsed);
        str1.append(str_long.cbegin(), str_long.cend());
    }

    SUBCASE("copy") {
        arena_string str("1234567", arena.get_memory_resource());
        arena_string copied_str(str);
        CHECK_EQ(arena.check(copied_str.data()), ArenaContainStatus::NotContain);
        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        arena_string copied_str_long(str_long);
        CHECK_EQ(arena.check(copied_str_long.data()), ArenaContainStatus::BlockUsed);
    }

    SUBCASE("move") {
        arena_string str("1234567", arena.get_memory_resource());
        arena_string copied_str(std::move(str));
        CHECK_EQ(arena.check(copied_str.data()), ArenaContainStatus::NotContain);
        arena_string str_long("12345671234567123456712345671234567", arena.get_memory_resource());
        arena_string copied_str_long(std::move(str_long));
        CHECK_EQ(arena.check(copied_str_long.data()), ArenaContainStatus::BlockUsed);
    }
}

TEST_CASE("arena_string::testAllClauses") {
    std::cout << "Starting with seed: " << seed << std::endl;
    std::string r;

    uint count = 0;

    Arena arena(Arena::Options::GetDefaultOptions());
    arena_string c(arena.get_memory_resource());
    auto l = [&](const char* const clause, void (*f_string)(std::string&), void (*f_arena_string)(arena_string&)) {
        do {
            // NOLINTNEXTLINE
            if (true) {
            } else {
                std::cout << "Testing clause " << clause << std::endl;
            }
            randomString(&r);
            c = r;
            CHECK_EQ(c, r);

            auto localSeed = seed + count;
            rng = RandomT(localSeed);
            f_string(r);
            rng = RandomT(localSeed);
            f_arena_string(c);
        } while (++count % 100 != 0);
    };

#define TEST_CLAUSE_ARENA(x) l(#x, arena_clause11_##x<std::string>, arena_clause11_##x<arena_string>);

    //    TEST_CLAUSE_ARENA(21_4_2_a);
    TEST_CLAUSE_ARENA(21_4_2_b);
    TEST_CLAUSE_ARENA(21_4_2_c);
    TEST_CLAUSE_ARENA(21_4_2_d);
    TEST_CLAUSE_ARENA(21_4_2_e);
    TEST_CLAUSE_ARENA(21_4_2_f);
    TEST_CLAUSE_ARENA(21_4_2_g);
    TEST_CLAUSE_ARENA(21_4_2_h);
    TEST_CLAUSE_ARENA(21_4_2_i);
    TEST_CLAUSE_ARENA(21_4_2_j);
    TEST_CLAUSE_ARENA(21_4_2_k);
    TEST_CLAUSE_ARENA(21_4_2_l);
    TEST_CLAUSE_ARENA(21_4_2_lprime);
    TEST_CLAUSE_ARENA(21_4_2_m);
    TEST_CLAUSE_ARENA(21_4_2_n);

    TEST_CLAUSE_ARENA(21_4_3);
    TEST_CLAUSE_ARENA(21_4_4);
    TEST_CLAUSE_ARENA(21_4_5);
    TEST_CLAUSE_ARENA(21_4_6_1);
    TEST_CLAUSE_ARENA(21_4_6_2);
    TEST_CLAUSE_ARENA(21_4_6_3_a);
    TEST_CLAUSE_ARENA(21_4_6_3_b);
    TEST_CLAUSE_ARENA(21_4_6_3_c);
    TEST_CLAUSE_ARENA(21_4_6_3_d);
    TEST_CLAUSE_ARENA(21_4_6_3_e);

    TEST_CLAUSE_ARENA(21_4_6_3_f);
    TEST_CLAUSE_ARENA(21_4_6_3_g);
    TEST_CLAUSE_ARENA(21_4_6_3_h);
    TEST_CLAUSE_ARENA(21_4_6_3_i);
    TEST_CLAUSE_ARENA(21_4_6_3_j);
    TEST_CLAUSE_ARENA(21_4_6_3_k);
    TEST_CLAUSE_ARENA(21_4_6_4);
    TEST_CLAUSE_ARENA(21_4_6_5);
    TEST_CLAUSE_ARENA(21_4_6_6);
    TEST_CLAUSE_ARENA(21_4_6_7);
    TEST_CLAUSE_ARENA(21_4_6_8);
    TEST_CLAUSE_ARENA(21_4_7_1);

    TEST_CLAUSE_ARENA(21_4_7_2_a);
    TEST_CLAUSE_ARENA(21_4_7_2_a1);
    TEST_CLAUSE_ARENA(21_4_7_2_a2);
    TEST_CLAUSE_ARENA(21_4_7_2_b);
    TEST_CLAUSE_ARENA(21_4_7_2_b1);
    TEST_CLAUSE_ARENA(21_4_7_2_b2);
    TEST_CLAUSE_ARENA(21_4_7_2_c);
    TEST_CLAUSE_ARENA(21_4_7_2_c1);
    TEST_CLAUSE_ARENA(21_4_7_2_c2);
    TEST_CLAUSE_ARENA(21_4_7_2_d);
    TEST_CLAUSE_ARENA(21_4_7_3_a);
    TEST_CLAUSE_ARENA(21_4_7_3_b);
    TEST_CLAUSE_ARENA(21_4_7_3_c);
    TEST_CLAUSE_ARENA(21_4_7_3_d);
    TEST_CLAUSE_ARENA(21_4_7_4_a);
    TEST_CLAUSE_ARENA(21_4_7_4_b);
    TEST_CLAUSE_ARENA(21_4_7_4_c);
    TEST_CLAUSE_ARENA(21_4_7_4_d);
    TEST_CLAUSE_ARENA(21_4_7_5_a);
    TEST_CLAUSE_ARENA(21_4_7_5_b);
    TEST_CLAUSE_ARENA(21_4_7_5_c);
    TEST_CLAUSE_ARENA(21_4_7_5_d);
    TEST_CLAUSE_ARENA(21_4_7_6_a);
    TEST_CLAUSE_ARENA(21_4_7_6_b);
    TEST_CLAUSE_ARENA(21_4_7_6_c);
    TEST_CLAUSE_ARENA(21_4_7_6_d);
    TEST_CLAUSE_ARENA(21_4_7_7_a);
    TEST_CLAUSE_ARENA(21_4_7_7_b);
    TEST_CLAUSE_ARENA(21_4_7_7_c);
    TEST_CLAUSE_ARENA(21_4_7_7_d);
    TEST_CLAUSE_ARENA(21_4_7_8);
    TEST_CLAUSE_ARENA(21_4_7_9_a);
    TEST_CLAUSE_ARENA(21_4_7_9_b);
    TEST_CLAUSE_ARENA(21_4_7_9_c);
    TEST_CLAUSE_ARENA(21_4_7_9_d);
    TEST_CLAUSE_ARENA(21_4_7_9_e);
    TEST_CLAUSE_ARENA(21_4_8_1_a);
    TEST_CLAUSE_ARENA(21_4_8_1_b);
    TEST_CLAUSE_ARENA(21_4_8_1_c);
    TEST_CLAUSE_ARENA(21_4_8_1_d);
    TEST_CLAUSE_ARENA(21_4_8_1_e);
    TEST_CLAUSE_ARENA(21_4_8_1_f);
    TEST_CLAUSE_ARENA(21_4_8_1_g);
    TEST_CLAUSE_ARENA(21_4_8_1_h);
    TEST_CLAUSE_ARENA(21_4_8_1_i);
    TEST_CLAUSE_ARENA(21_4_8_1_j);
    TEST_CLAUSE_ARENA(21_4_8_1_k);
    TEST_CLAUSE_ARENA(21_4_8_1_l);
    TEST_CLAUSE_ARENA(21_4_8_9_a);
}

}  // namespace stdb::memory
