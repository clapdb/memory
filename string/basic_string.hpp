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
#include <string>

namespace stdb {

namespace internal {
/*
 * basic_string was inspired by meta Folly::FBSting and seastar::sstring.
 * it can be allocated in arena, and any other memory allocator.
 *
 * it has 3 category type.
 * 1. small string, it will occupy 24-byte in stack / heap / arena
 * 2. median string, it will use 24-byte meta struct and a heap memory block.
 * 3. large string, it will use 24-byte meta struct and a heap CoW memory block
 * // borrow from FBString
 * The storage is selected as follows (assuming we store one-byte
 * characters on a 64-bit machine): (a) "small" strings between 0 and
 * 23 chars are stored in-situ without allocation (the rightmost byte
 * stores the size); (b) "medium" strings from 24 through 254 chars
 * are stored in malloc-allocated memory that is copied eagerly; (c)
 * "large" strings of 255 chars and above are stored in a similar
 * structure as medium arrays, except that the string is
 * reference-counted and copied lazily. the reference count is
 * allocated right before the character array.
 *
 * The discriminator between these three strategies sits in two
 * bits of the rightmost char of the storage:
 * - If neither is set, then the string is small. Its length is represented by
 *   the lower-order bits on little-endian or the high-order bits on big-endian
 *   of that rightmost character. The value of these six bits is
 *   `maxSmallSize - size`, so this quantity must be subtracted from
 *   `maxSmallSize` to compute the `size` of the string (see `smallSize()`).
 *   This scheme ensures that when `size == `maxSmallSize`, the last byte in the
 *   storage is \0. This way, storage will be a null-terminated sequence of
 *   bytes, even if all 23 bytes of data are used on a 64-bit architecture.
 *   This enables `c_str()` and `data()` to simply return a pointer to the
 *   storage.
 *
 * - If the MSb is set, the string is medium width.
 *
 * - If the second MSb is set, then the string is large. On little-endian,
 *   these 2 bits are the 2 MSbs of MediumLarge::capacity_, while on
 *   big-endian, these 2 bits are the 2 LSbs. This keeps both little-endian
 *   and big-endian fbstring_core equivalent with merely different ops used
 *   to extract capacity/category.
 */
template <typename Char>
class basic_string
{
   private:
    struct MediumLargeMeta {
        Char* _data;
        int64_t size_; // use signed int to avoid overflow
        uint64_t capacity_; // the capacity of heap memblock, and highest n bits used to represent category of strincategory of string
    };
    static_assert(sizeof(MediumLargeMeta) == 3 * sizeof(void*));

    // use anonymous union will be better, just like FBString?
    union contents {
        uint8_t _bytes[sizeof(MediumLargeMeta)];  // for accessing the last byte, to check the category.
        Char small_[sizeof(MediumLargeMeta) / sizeof(Char)];
        MediumLargeMeta meta_;
    };

};
} // namespace internal
using STring = internal::basic_string<char>;
using String16 = internal::basic_string<char16_t>;
using String32 = internal::basic_string<char32_t>;
} // namespace stdb
