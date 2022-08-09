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

#pragma once
#include <cstdint>
#include <string>
#include <cstring>
#include "arena/arenahelper.hpp"

namespace stdb {

/*
 * FixedString is a Char Array with fixed length, and FixedString should be terminated by '0'.
 * FixedString can be an ascii string or UTF-8 string, UTF-16 string, or UTF-32 string.
 *
 * CharT is can be a char_t, char16_t, or char32_t
 *
 */
template<typename CharT, typename CharTraits = std::char_traits<CharT>, uint32_t Capacity = 16>
class FixedString {
   private:
    // make sure the Capacity should be times of size of CharT.
    static_assert(Capacity % sizeof(CharT) == 0);
    CharT _data[Capacity / sizeof(CharT)];
   public:
    // FixedString can be allocated in Arena
    ArenaManagedCreateOnlyTag;
    // cstr;
    explicit FixedString(const std::basic_string<CharT, CharTraits, std::allocator<CharT>>& str){}
    explicit FixedString(std::basic_string<CharT, CharTraits, std::allocator<CharT>>&& str){}

    explicit FixedString(const std::basic_string<CharT, CharTraits, std::pmr::polymorphic_allocator<CharT>>& str){}
    explicit FixedString(std::basic_string<CharT, CharTraits, std::pmr::polymorphic_allocator<CharT>>&& str){}
    explicit FixedString(const CharT* str){}
    FixedString(const CharT* str, uint32_t len){}

    // default cstr
    FixedString() = default;

    // dstr, no need for special cleanup
    ~FixedString() = default;

    // copy cstr
    FixedString(const FixedString& other) {
        std::memcpy(_data, other._data, Capacity);
    }
    auto operator = (const FixedString& other) -> FixedString& {
        std::memcpy(_data, other._data, Capacity);
        return *this;
    }

    // move cstr
    FixedString(FixedString&& other) noexcept {
        std::memcpy(_data, other._data, Capacity);
        std::memset(other._data, 0, Capacity);
    }
    auto operator = (FixedString&& other) noexcept -> FixedString& {
        std::memcpy(_data, other._data, Capacity);
        std::memset(other._data, 0, Capacity);
        return *this;
    }

    // assign

    // at

    // operator[]

    // front

    // back

    // data

    // c_str

    // operator basic_string_view

    // iterators

    // empty

    // size

    // length

    // max_size

    // reserve

    // capacity

    // clear

    // insert

    // erase

    // push_back

    // pop_back

    // append

    // operator +=

    // compare

    // starts_with

    // ends_with

    // replace

    // substr

    // copy

    // swap

    // find

    // rfind

    // find_first_of

    // find_first_not_of

    // find_last_of

    // find_last_not_of

    // operator +
    // operator ==
    // operator !=
    // operator <
    // operator <=
    // operator >
    // operator >=
    // operator <=>

    // std::swap

    // erase
    // erase_if


    // operator <<
    // operator >>

    // fmt::formatter

    // std::hash support
};  // class FixedString

}  // namespace stdb