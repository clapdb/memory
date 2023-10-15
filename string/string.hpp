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
// ported from https://github.com/facebook/folly/blob/9142bed8ded0add0492eaac3d250902015a08684/folly/FBString.h

#pragma once

#include <ctype.h>     // for isspace
#include <fmt/core.h>  // for string_view, formatter, formatter<>...
#include <fmt/format.h>
#include <stdint.h>  // for uint8_t, int64_t, int32_t
#include <stdio.h>   // for getline, ssize_t
#include <stdlib.h>  // for free, malloc, realloc

#include <algorithm>  // for min, max, copy, fill
#include <bit>        // for endian, endian::little, endian::native
#include <compare>
#include <cstddef>           // for size_t, offsetof
#include <cstring>           // for memcpy, memcmp, memmove, memset
#include <functional>        // for less_equal
#include <initializer_list>  // for initializer_list
#include <iosfwd>            // for basic_istream
#include <istream>           // for basic_istream, basic_ostream, __ost...
#include <iterator>          // for forward_iterator_tag, input_iterato...
#include <limits>            // for numeric_limits
#include <memory>            // for allocator_traits
#include <new>               // for bad_alloc, operator new
#include <optional>
#include <stdexcept>    // for out_of_range, length_error, logic_e...
#include <string>       // for basic_string, allocator, string
#include <string_view>  // for hash, basic_string_view
#include <thread>
#include <type_traits>  // for integral_constant, true_type, is_same
#include <utility>      // for move, make_pair, pair

#include "arena/arenahelper.hpp"
#include "assert_config.hpp"
#include "xxhash.h"  // for XXH32

namespace stdb::memory {

// port from folly/lang/CheckedMath.h

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
auto checked_add(T* result, T a, T b) noexcept -> bool {
    if (!__builtin_add_overflow(a, b, result)) [[likely]] {
        return true;
    }
    *result = {};
    return false;
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
auto checked_mul(T* result, T a, T b) noexcept -> bool {
    if (!__builtin_mul_overflow(a, b, result)) [[likely]] {
        return true;
    }
    *result = {};
    return false;
}

template <typename T, typename = std::enable_if_t<std::is_unsigned<T>::value>>
auto checked_muladd(T* result, T base, T mul, T add) noexcept -> bool {
    T tmp{};
    if (!checked_mul(&tmp, base, mul)) [[unlikely]] {
        *result = {};
        return false;
    }
    if (!checked_add(&tmp, tmp, add)) [[unlikely]] {
        *result = {};
        return false;
    }
    *result = tmp;
    return true;
}

// port from folly/memory/Malloc.h

/**
 * Trivial wrappers around malloc, calloc, realloc that check for allocation
 * failure and throw std::bad_alloc in that case.
 */
inline auto checkedMalloc(size_t size) -> void* {
    void* ptr = malloc(size);
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

inline auto checkedRealloc(void* ptr, size_t size) -> void* {
    auto* new_ptr = realloc(ptr, size);
    if (new_ptr == nullptr) {
        free(ptr);
        throw std::bad_alloc();
    }
    return new_ptr;
}

/**
 * This function tries to reallocate a buffer of which only the first
 * currentSize bytes are used. The problem with using realloc is that
 * if currentSize is relatively small _and_ if realloc decides it
 * needs to move the memory chunk to a new buffer, then realloc ends
 * up copying data that is not used. It's generally not a win to try
 * to hook in to realloc() behavior to avoid copies - at least in
 * jemalloc, realloc() almost always ends up doing a copy, because
 * there is little fragmentation / slack space to take advantage of.
 */
inline auto smartRealloc(void* ptr, const size_t currentSize, const size_t currentCapacity, const size_t newCapacity)
  -> void* {
    Assert(ptr);
    Assert(currentSize <= currentCapacity && currentCapacity < newCapacity);

    auto const slack = currentCapacity - currentSize;
    if (slack * 2 > currentSize) {
        // Too much slack, malloc-copy-free cycle:
        auto* const result = checkedMalloc(newCapacity);
        std::memcpy(result, ptr, currentSize);
        free(ptr);
        return result;
    }
    // If there's not too much slack, we realloc in hope of coalescing
    return checkedRealloc(ptr, newCapacity);
}

constexpr auto isLittleEndian() noexcept -> bool { return std::endian::native == std::endian::little; }

// When compiling with ASan, always heap-allocate the string even if
// it would fit in-situ, so that ASan can detect access to the string
// buffer after it has been invalidated (destroyed, resized, etc.).
// Note that this flag doesn't remove support for in-situ strings, as
// that would break ABI-compatibility and wouldn't allow linking code
// compiled with this flag with code compiled without.
#ifndef NDEBUG
#define FBSTRING_DISABLE_SSO true
#else
#define FBSTRING_DISABLE_SSO false
#endif

namespace string_detail {

template <class InIt, class OutIt>
inline auto copy_n(InIt begin, typename std::iterator_traits<InIt>::difference_type n, OutIt dest) noexcept
  -> std::pair<InIt, OutIt> {
    for (; n != 0; --n, ++begin, ++dest) {
        *dest = *begin;
    }
    return std::make_pair(begin, dest);
}

template <class Pod, class T>
inline void podFill(Pod* begin, Pod* end, T c) noexcept {
    Assert(begin && end && begin <= end);
    constexpr auto kUseMemset = sizeof(T) == 1;
    if constexpr (kUseMemset) {
        memset(begin, c, size_t(end - begin));
    } else {
        auto const ee = begin + (size_t(end - begin) & ~7U);
        for (; begin != ee; begin += 8) {
            begin[0] = c;
            begin[1] = c;
            begin[2] = c;
            begin[3] = c;
            begin[4] = c;
            begin[5] = c;
            begin[6] = c;
            begin[7] = c;
        }
        // Leftovers
        for (; begin != end; ++begin) {
            *begin = c;
        }
    }
}

/*
 * Lightly structured memcpy, simplifies copying PODs and introduces
 * some asserts. Unfortunately using this function may cause
 * measurable overhead (presumably because it adjusts from a begin/end
 * convention to a pointer/size convention, so it does some extra
 * arithmetic even though the caller might have done the inverse
 * adaptation outside).
 */
template <class Pod>
inline void podCopy(const Pod* begin, const Pod* end, Pod* dest) noexcept {
    Assert(begin != nullptr);
    Assert(end != nullptr);
    Assert(dest != nullptr);
    Assert(end >= begin);
    Assert(dest >= end || dest + size_t(end - begin) <= begin);
    memcpy(dest, begin, size_t(end - begin) * sizeof(Pod));
}

/*
 * Lightly structured memmove, simplifies copying PODs and introduces
 * some asserts
 */
template <class Pod>
inline void podMove(const Pod* begin, const Pod* end, Pod* dest) noexcept {
    Assert(end >= begin);
    memmove(dest, begin, size_t(end - begin) * sizeof(*begin));
}
}  // namespace string_detail

/*
 * string_core_model is a mock-up type that defines all required
 * signatures of a string core. The string class itself uses such
 * a core object to implement all of the numerous member functions
 * required by the standard.
 *
 * If you want to define a new core, copy the definition below and
 * implement the primitives. Then plug the core into basic_string as
 * a template argument.

template <class Char>
class string_core_model {
 public:
  string_core_model();
  string_core_model(const string_core_model &);
  string_core_model& operator=(const string_core_model &) = delete;
  ~string_core_model();
  // Returns a pointer to string's buffer (currently only contiguous
  // strings are supported). The pointer is guaranteed to be valid
  // until the next call to a non-const member function.
  const Char * data() const;
  // Much like data(), except the string is prepared to support
  // character-level changes. This call is a signal for
  // e.g. reference-counted implementation to fork the data. The
  // pointer is guaranteed to be valid until the next call to a
  // non-const member function.
  Char* mutableData();
  // Returns a pointer to string's buffer and guarantees that a
  // readable '\0' lies right after the buffer. The pointer is
  // guaranteed to be valid until the next call to a non-const member
  // function.
  const Char * c_str() const;
  // Shrinks the string by delta characters. Asserts that delta <=
  // size().
  void shrink(size_t delta);
  // Expands the string by delta characters (i.e. after this call
  // size() will report the old size() plus delta) but without
  // initializing the expanded region. The expanded region is
  // zero-terminated. Returns a pointer to the memory to be
  // initialized (the beginning of the expanded portion). The caller
  // is expected to fill the expanded area appropriately.
  // If expGrowth is true, exponential growth is guaranteed.
  // It is not guaranteed not to reallocate even if size() + delta <
  // capacity(), so all references to the buffer are invalidated.
  Char* expandNoinit(size_t delta, bool expGrowth);
  // Expands the string by one character and sets the last character
  // to c.
  void push_back(Char c);
  // Returns the string's size.
  size_t size() const;
  // Returns the string's capacity, i.e. maximum size that the string
  // can grow to without reallocation. Note that for reference counted
  // strings that's technically a lie - even assigning characters
  // within the existing size would cause a reallocation.
  size_t capacity() const;
  // Returns true if the data underlying the string is actually shared
  // across multiple strings (in a refcounted fashion).
  bool isShared() const;
  // Makes sure that at least minCapacity characters are available for
  // the string without reallocation. For reference-counted strings,
  // it should fork the data even if minCapacity < size().
  void reserve(size_t minCapacity);
};
*/

/**
 * This is the core of the string. The code should work on 32- and
 * 64-bit and both big- and little-endianan architectures with any
 * Char size.
 *
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
 *   and big-endian string_core equivalent with merely different ops used
 *   to extract capacity/category.
 */
template <class Char>
class string_core
{
    template <typename E, class T, class A, class Storage>
    friend class basic_string;

   public:
    string_core() noexcept { reset(); }

    explicit string_core(const std::allocator<Char>& /*noused*/) noexcept { reset(); }

    string_core(const string_core& rhs) noexcept {
        Assert(&rhs != this);
        // make sure all copy occur in same thread, or from a unshared string
        // if rhs is a unshared string, set the cpu_ to current thread_id
#ifndef NDEBUG
        auto thread_id = std::this_thread::get_id();
        Assert(not rhs.cpu_.has_value() or rhs.cpu_.value() == thread_id);
        // thread::id class do not has operator =, so no overwrite occurs in any case.
        if (not rhs.cpu_.has_value()) {
            cpu_ = thread_id;
            rhs.cpu_ = thread_id;
        }
#endif
        switch (rhs.category()) {
            case Category::isSmall:
                copySmall(rhs);
                break;
            case Category::isMedium:
                copyMedium(rhs);
                break;
            case Category::isLarge:
                // forbid cross thread copy for Large, refCount++/-- over cross is not safe.
                copyLarge(rhs);
                break;
            default:
                __builtin_unreachable();
        }
        Assert(size() == rhs.size());
        Assert(memcmp(data(), rhs.data(), size() * sizeof(Char)) == 0);
    }

    auto operator=(const string_core& rhs) -> string_core& = delete;

    string_core(string_core&& goner) noexcept {
        // move just work same as normal
#ifndef NDEBUG
        cpu_ = std::move(goner.cpu_);  // NOLINT
#endif
        // Take goner's guts
        ml_ = goner.ml_;
        // Clean goner's carcass
        goner.reset();
    }
    auto operator=(string_core&& rhs) -> string_core& = delete;

    string_core(const Char* const data, const size_t size,
                const std::allocator<Char>& /*unused*/ = std::allocator<Char>(),
                bool disableSSO = FBSTRING_DISABLE_SSO) {
        if (!disableSSO && size <= maxSmallSize) {
            initSmall(data, size);
        } else if (size <= maxMediumSize) {
            initMedium(data, size);
        } else {
            initLarge(data, size);
        }
        Assert(this->size() == size);
        Assert(size == 0 || memcmp(this->data(), data, size * sizeof(Char)) == 0);
    }

    ~string_core() noexcept {
#ifndef NDEBUG
        auto thread_id = std::this_thread::get_id();
        Assert(not cpu_.has_value() or thread_id == cpu_.value());
#endif

        if (category() == Category::isSmall) {
            return;
        }
        destroyMediumLarge();
    }

    // Snatches a previously mallocated string. The parameter "size"
    // is the size of the string, and the parameter "allocatedSize"
    // is the size of the mallocated block.  The string must be
    // \0-terminated, so allocatedSize >= size + 1 and data[size] == '\0'.
    //
    // So if you want a 2-character string, pass malloc(3) as "data",
    // pass 2 as "size", and pass 3 as "allocatedSize".
    string_core(Char* const data, const size_t size, const size_t allocatedSize) {
        if (size > 0) {
            Assert(allocatedSize >= size + 1);
            Assert(data[size] == '\0');
            // Use the medium string storage
            ml_.data_ = data;
            ml_.size_ = size;
            // Don't forget about null terminator
            ml_.setCapacity(allocatedSize - 1, Category::isMedium);
        } else {
            // No need for the memory
            free(data);
            reset();
        }
    }

    // swap below doesn't test whether &rhs == this (and instead
    // potentially does extra work) on the premise that the rarity of
    // that situation actually makes the check more expensive than is
    // worth.
    void swap(string_core& rhs) noexcept {
        auto const t = ml_;
        ml_ = rhs.ml_;
        rhs.ml_ = t;
    }

    // In C++11 data() and c_str() are 100% equivalent.
    [[nodiscard]] auto data() const noexcept -> const Char* { return c_str(); }

    [[nodiscard]] auto data() noexcept -> Char* { return c_str(); }

    auto mutableData() noexcept -> Char* {
        switch (category()) {
            case Category::isSmall:
                return small_;
            case Category::isMedium:
                return ml_.data_;
            case Category::isLarge:
                return mutableDataLarge();
        }
        __builtin_unreachable();
    }

    [[nodiscard]] auto c_str() const noexcept -> const Char* {
        const Char* ptr = ml_.data_;
        // With this syntax, GCC and Clang generate a CMOV instead of a branch.
        ptr = (category() == Category::isSmall) ? small_ : ptr;
        return ptr;
    }

    void shrink(const size_t delta) {
        if (category() == Category::isSmall) {
            shrinkSmall(delta);
        } else if (category() == Category::isMedium || RefCounted::refs(ml_.data_) == 1) {
            shrinkMedium(delta);
        } else {
            shrinkLarge(delta);
        }
    }

    void reserve(size_t minCapacity, bool disableSSO = FBSTRING_DISABLE_SSO) {
        switch (category()) {
            case Category::isSmall:
                reserveSmall(minCapacity, disableSSO);
                break;
            case Category::isMedium:
                reserveMedium(minCapacity);
                break;
            case Category::isLarge:
                reserveLarge(minCapacity);
                break;
            default:
                __builtin_unreachable();
        }

        Assert(capacity() >= minCapacity);
    }

    auto expandNoinit(size_t delta, bool expGrowth = false, bool disableSSO = FBSTRING_DISABLE_SSO) -> Char*;

    void push_back(Char c) { *expandNoinit(1, /* expGrowth = */ true) = c; }

    [[nodiscard]] auto size() const noexcept -> size_t {
        size_t ret = ml_.size_;
        if constexpr (isLittleEndian()) {
            // We can save a couple instructions, because the category is
            // small iff the last char, as unsigned, is <= maxSmallSize.
            using UChar = typename std::make_unsigned<Char>::type;
            auto maybeSmallSize = size_t(maxSmallSize) - static_cast<size_t>(static_cast<UChar>(small_[maxSmallSize]));
            // With this syntax, GCC and Clang generate a CMOV instead of a branch.
            ret = (static_cast<ssize_t>(maybeSmallSize) >= 0) ? maybeSmallSize : ret;
        } else {
            ret = (category() == Category::isSmall) ? smallSize() : ret;
        }
        return ret;
    }

    [[nodiscard]] auto capacity() const noexcept -> size_t {
        switch (category()) {
            case Category::isSmall:
                return maxSmallSize;
            case Category::isLarge:
                // For large-sized strings, a multi-referenced chunk has no
                // available capacity. This is because any attempt to append
                // data would trigger a new allocation.
                if (RefCounted::refs(ml_.data_) > 1) {
                    return ml_.size_;
                }
                break;
            case Category::isMedium:
                break;
            default:
                __builtin_unreachable();
        }

        return ml_.capacity();
    }

    [[nodiscard]] auto isShared() const noexcept -> bool {
        return category() == Category::isLarge && RefCounted::refs(ml_.data_) > 1;
    }

   private:
    auto c_str() noexcept -> Char* {
        Char* ptr = ml_.data_;
        // With this syntax, GCC and Clang generate a CMOV instead of a branch.
        ptr = (category() == Category::isSmall) ? small_ : ptr;
        return ptr;
    }

    void reset() noexcept { setSmallSize(0); }

    void destroyMediumLarge() noexcept {
        auto const c = category();
        Assert(c != Category::isSmall);
        if (c == Category::isMedium) {
            free(ml_.data_);
        } else {
            RefCounted::decrementRefs(ml_.data_);
        }
    }

    struct RefCounted
    {
        // std::atomic<size_t> refCount_;
        size_t refCount_;  // no need atomic on seastar without access cross cpu
        Char data_[1];

        constexpr static auto getDataOffset() noexcept -> size_t { return offsetof(RefCounted, data_); }

        static auto fromData(Char* ptr) noexcept -> RefCounted* {
            return static_cast<RefCounted*>(
              static_cast<void*>(static_cast<unsigned char*>(static_cast<void*>(ptr)) - getDataOffset()));
        }

        // static size_t refs(Char* p) { return fromData(p)->refCount_.load(std::memory_order_acquire); }
        static auto refs(Char* ptr) noexcept -> size_t { return fromData(ptr)->refCount_; }

        // static void incrementRefs(Char* p) { fromData(p)->refCount_.fetch_add(1, std::memory_order_acq_rel); }
        static void incrementRefs(Char* ptr) noexcept { fromData(ptr)->refCount_++; }

        static void decrementRefs(Char* ptr) noexcept {
            auto const dis = fromData(ptr);
            // size_t oldcnt = dis->refCount_.fetch_sub(1, std::memory_order_acq_rel);
            size_t oldcnt = dis->refCount_--;
            Assert(oldcnt > 0);
            if (oldcnt == 1) {
                free(dis);
            }
        }

        static auto create(size_t* size) -> RefCounted* {
            size_t capacityBytes = 0;
            if (!checked_add(&capacityBytes, *size, static_cast<size_t>(1))) {
                throw(std::length_error(""));
            }
            if (!checked_muladd(&capacityBytes, capacityBytes, sizeof(Char), getDataOffset())) {
                throw(std::length_error(""));
            }
            //            const size_t allocSize = capacityBytes;
            auto result = static_cast<RefCounted*>(checkedMalloc(capacityBytes));
            // result->refCount_.store(1, std::memory_order_release);
            result->refCount_ = 1;
            *size = (capacityBytes - getDataOffset()) / sizeof(Char) - 1;
            return result;
        }

        static auto create(const Char* data, size_t* size) -> RefCounted* {
            const size_t effectiveSize = *size;
            auto result = create(size);
            if (effectiveSize > 0) [[likely]] {
                string_detail::podCopy(data, data + effectiveSize, result->data_);
            }
            return result;
        }

        static auto reallocate(Char* const data, const size_t currentSize, const size_t currentCapacity,
                               size_t* newCapacity) -> RefCounted* {
            Assert(*newCapacity > 0 && *newCapacity > currentSize);
            size_t capacityBytes = 0;
            if (!checked_add(&capacityBytes, *newCapacity, static_cast<size_t>(1))) {
                throw(std::length_error(""));
            }
            if (!checked_muladd(&capacityBytes, capacityBytes, sizeof(Char), getDataOffset())) {
                throw(std::length_error(""));
            }
            //            const size_t allocNewCapacity = goodMallocSize(capacityBytes);
            auto const dis = fromData(data);
            // assert(dis->refCount_.load(std::memory_order_acquire) == 1);
            Assert(dis->refCount_ == 1);
            auto result = static_cast<RefCounted*>(smartRealloc(dis, getDataOffset() + (currentSize + 1) * sizeof(Char),
                                                                getDataOffset() + (currentCapacity + 1) * sizeof(Char),
                                                                capacityBytes));
            // assert(dis->refCount_.load(std::memory_order_acquire) == 1);
            Assert(result->refCount_ == 1);
            *newCapacity = (capacityBytes - getDataOffset()) / sizeof(Char) - 1;
            return result;
        }
    };

    using category_type = uint8_t;

    enum class Category : category_type
    {
        isSmall = 0,
        isMedium = isLittleEndian() ? 0x80 : 0x2,
        isLarge = isLittleEndian() ? 0x40 : 0x1,
    };

    [[nodiscard]] auto category() const noexcept -> Category {
        // works for both big-endian and little-endian
        return static_cast<Category>(bytes_[lastChar] & categoryExtractMask);
    }

    struct MediumLarge
    {
        Char* data_;
        size_t size_;
        size_t capacity_;

        [[nodiscard]] auto capacity() const noexcept -> size_t {
            return isLittleEndian() ? capacity_ & capacityExtractMask : capacity_ >> 2U;
        }

        void setCapacity(size_t cap, Category cat) noexcept {
            capacity_ = isLittleEndian() ? cap | (static_cast<size_t>(cat) << kCategoryShift)
                                         : (cap << 2U) | static_cast<size_t>(cat);
        }
    };

    union
    {
        uint8_t bytes_[sizeof(MediumLarge)]{};  // For accessing the last byte.
        Char small_[sizeof(MediumLarge) / sizeof(Char)];
        MediumLarge ml_;
    };

// thread_id for contention checking in debug mode
#ifndef NDEBUG
    mutable std::optional<std::thread::id> cpu_ = std::nullopt;
#endif

    constexpr static size_t lastChar = sizeof(MediumLarge) - 1;
    constexpr static size_t maxSmallSize = lastChar / sizeof(Char);
    constexpr static size_t maxMediumSize = 254 / sizeof(Char);
    constexpr static uint8_t categoryExtractMask = isLittleEndian() ? 0xC0 : 0x3;
    constexpr static size_t kCategoryShift = (sizeof(size_t) - 1) * 8;
    constexpr static size_t capacityExtractMask =
      isLittleEndian() ? ~(static_cast<size_t>(categoryExtractMask) << kCategoryShift) : 0x0 /* unused */;

    static_assert((sizeof(MediumLarge) % sizeof(Char)) == 0U, "Corrupt memory layout for string.");

    [[nodiscard]] auto smallSize() const noexcept -> size_t {
        Assert(category() == Category::isSmall);
        constexpr auto shift = isLittleEndian() ? 0U : 2U;
        auto smallShifted = static_cast<size_t>(small_[maxSmallSize]) >> shift;
        Assert(static_cast<size_t>(maxSmallSize) >= smallShifted);
        return static_cast<size_t>(maxSmallSize) - smallShifted;
    }

    void setSmallSize(size_t newSize) noexcept {
        // Warning: this should work with uninitialized strings too,
        // so don't assume anything about the previous value of
        // small_[maxSmallSize].
        Assert(newSize <= maxSmallSize);
        constexpr auto shift = isLittleEndian() ? 0U : 2U;
        small_[maxSmallSize] = Char(static_cast<char>((maxSmallSize - newSize) << shift));
        small_[newSize] = '\0';
        Assert(category() == Category::isSmall && size() == newSize);
    }

    void copySmall(const string_core& /*rhs*/) noexcept;
    void copyMedium(const string_core& /*rhs*/) noexcept;
    void copyLarge(const string_core& /*rhs*/) noexcept;

    void initSmall(const Char* data, size_t size) noexcept;
    void initMedium(const Char* data, size_t size);
    void initLarge(const Char* data, size_t size);

    void reserveSmall(size_t minCapacity, bool disableSSO);
    void reserveMedium(size_t minCapacity);
    void reserveLarge(size_t minCapacity);

    void shrinkSmall(size_t delta) noexcept;
    void shrinkMedium(size_t delta) noexcept;
    void shrinkLarge(size_t delta);

    void unshare(size_t minCapacity = 0);
    auto mutableDataLarge() -> Char*;
};  // class string_core

template <class Char>
inline void string_core<Char>::copySmall(const string_core& rhs) noexcept {
    static_assert(offsetof(MediumLarge, data_) == 0, "string layout failure");
    static_assert(offsetof(MediumLarge, size_) == sizeof(ml_.data_), "string layout failure");
    static_assert(offsetof(MediumLarge, capacity_) == 2 * sizeof(ml_.data_), "string layout failure");
    // Just write the whole thing, don't look at details. In
    // particular we need to copy capacity anyway because we want
    // to set the size (don't forget that the last character,
    // which stores a short string's length, is shared with the
    // ml_.capacity field).
    ml_ = rhs.ml_;
    Assert(category() == Category::isSmall && this->size() == rhs.size());
}

template <class Char>
void string_core<Char>::copyMedium(const string_core& rhs) noexcept {
    // Medium strings are copied eagerly. Don't forget to allocate
    // one extra Char for the null terminator.
    //    auto const allocSize = goodMallocSize((1 + rhs.ml_.size_) * sizeof(Char));
    auto const allocSize = (1 + rhs.ml_.size_) * sizeof(Char);
    ml_.data_ = static_cast<Char*>(checkedMalloc(allocSize));
    // Also copies terminator.
    string_detail::podCopy(rhs.ml_.data_, rhs.ml_.data_ + rhs.ml_.size_ + 1, ml_.data_);
    ml_.size_ = rhs.ml_.size_;
    ml_.setCapacity(allocSize / sizeof(Char) - 1, Category::isMedium);
    Assert(category() == Category::isMedium);
}

template <class Char>
void string_core<Char>::copyLarge(const string_core& rhs) noexcept {
    // Large strings are just refcounted
    ml_ = rhs.ml_;
    RefCounted::incrementRefs(ml_.data_);
    Assert(category() == Category::isLarge && size() == rhs.size());
}

// Small strings are bitblitted
template <class Char>
inline void string_core<Char>::initSmall(const Char* const data, const size_t size) noexcept {
// Layout is: Char* data_, size_t size_, size_t capacity_
#ifndef NDEBUG
    static_assert(sizeof(*this) == sizeof(Char*) + 2 * sizeof(size_t) + sizeof(std::optional<std::thread::id>),
                  "string has unexpected size");
#else
    static_assert(sizeof(*this) == sizeof(Char*) + 2 * sizeof(size_t), "string has unexpected size");
#endif
    static_assert(sizeof(Char*) == sizeof(size_t), "string size assumption violation");
    // sizeof(size_t) must be a power of 2
    static_assert((sizeof(size_t) & (sizeof(size_t) - 1)) == 0, "string size assumption violation");

// If data is aligned, use fast word-wise copying. Otherwise,
// use conservative memcpy.
// The word-wise path reads bytes which are outside the range of
// the string, and makes ASan unhappy, so we disable it when
// compiling with ASan.
#ifdef NDEBUG
    if ((reinterpret_cast<size_t>(data) & (sizeof(size_t) - 1)) == 0) {
        const size_t byteSize = size * sizeof(Char);
        constexpr size_t wordWidth = sizeof(size_t);
        switch ((byteSize + wordWidth - 1) / wordWidth) {  // Number of words.
            case 3:
                ml_.capacity_ = reinterpret_cast<const size_t*>(data)[2];
                [[fallthrough]];
            case 2:
                ml_.size_ = reinterpret_cast<const size_t*>(data)[1];
                [[fallthrough]];
            case 1:
                ml_.data_ = *reinterpret_cast<Char**>(const_cast<Char*>(data));
                [[fallthrough]];
            case 0:
                break;
        }
    } else
#endif
    {
        if (size != 0) {
            string_detail::podCopy(data, data + size, small_);
        }
    }
    setSmallSize(size);
}

template <class Char>
void string_core<Char>::initMedium(const Char* const data, const size_t size) {
    // Medium strings are allocated normally. Don't forget to
    // allocate one extra Char for the terminating null.
    //    auto const allocSize = goodMallocSize((1 + size) * sizeof(Char));
    auto const allocSize = (1 + size) * sizeof(Char);
    ml_.data_ = static_cast<Char*>(checkedMalloc(allocSize));
    if (size > 0) [[likely]] {
        string_detail::podCopy(data, data + size, ml_.data_);
    }
    ml_.size_ = size;
    ml_.setCapacity(allocSize / sizeof(Char) - 1, Category::isMedium);
    ml_.data_[size] = '\0';
}

template <class Char>
void string_core<Char>::initLarge(const Char* const data, const size_t size) {
    // Large strings are allocated differently
    size_t effectiveCapacity = size;
    auto const newRC = RefCounted::create(data, &effectiveCapacity);
    ml_.data_ = newRC->data_;
    ml_.size_ = size;
    ml_.setCapacity(effectiveCapacity, Category::isLarge);
    ml_.data_[size] = '\0';
}

template <class Char>
void string_core<Char>::unshare(size_t minCapacity) {
    Assert(category() == Category::isLarge);
    size_t effectiveCapacity = std::max(minCapacity, ml_.capacity());
    auto const newRC = RefCounted::create(&effectiveCapacity);
    // If this fails, someone placed the wrong capacity in an
    // string.
    Assert(effectiveCapacity >= ml_.capacity());
    // Also copies terminator.
    string_detail::podCopy(ml_.data_, ml_.data_ + ml_.size_ + 1, newRC->data_);
    RefCounted::decrementRefs(ml_.data_);
    ml_.data_ = newRC->data_;
    ml_.setCapacity(effectiveCapacity, Category::isLarge);
    // size_ remains unchanged.
}

template <class Char>
inline auto string_core<Char>::mutableDataLarge() -> Char* {
    Assert(category() == Category::isLarge);
    if (RefCounted::refs(ml_.data_) > 1) {  // Ensure unique.
        unshare();
    }
    return ml_.data_;
}

template <class Char>
void string_core<Char>::reserveLarge(size_t minCapacity) {
    Assert(category() == Category::isLarge);
    if (RefCounted::refs(ml_.data_) > 1) {  // Ensure unique
        // We must make it unique regardless; in-place reallocation is
        // useless if the string is shared. In order to not surprise
        // people, reserve the new block at current capacity or
        // more. That way, a string's capacity never shrinks after a
        // call to reserve.
        unshare(minCapacity);
    } else {
        // String is not shared, so let's try to realloc (if needed)
        if (minCapacity > ml_.capacity()) {
            // Asking for more memory
            auto const newRC = RefCounted::reallocate(ml_.data_, ml_.size_, ml_.capacity(), &minCapacity);
            ml_.data_ = newRC->data_;
            ml_.setCapacity(minCapacity, Category::isLarge);
        }
        Assert(capacity() >= minCapacity);
    }
}

template <class Char>
void string_core<Char>::reserveMedium(const size_t minCapacity) {
    Assert(category() == Category::isMedium);
    // String is not shared
    if (minCapacity <= ml_.capacity()) {
        return;  // nothing to do, there's enough room
    }
    if (minCapacity <= maxMediumSize) {
        // Keep the string at medium size. Don't forget to allocate
        // one extra Char for the terminating null.
        //        size_t capacityBytes = goodMallocSize((1 + minCapacity) * sizeof(Char));
        size_t capacityBytes = (1 + minCapacity) * sizeof(Char);
        // Also copies terminator.
        ml_.data_ = static_cast<Char*>(
          smartRealloc(ml_.data_, (ml_.size_ + 1) * sizeof(Char), (ml_.capacity() + 1) * sizeof(Char), capacityBytes));
        ml_.setCapacity(capacityBytes / sizeof(Char) - 1, Category::isMedium);
    } else {
        // Conversion from medium to large string
        string_core nascent;
        // Will recurse to another branch of this function
        nascent.reserve(minCapacity);
        nascent.ml_.size_ = ml_.size_;
        // Also copies terminator.
        string_detail::podCopy(ml_.data_, ml_.data_ + ml_.size_ + 1, nascent.ml_.data_);
        nascent.swap(*this);
        Assert(capacity() >= minCapacity);
    }
}

template <class Char>
void string_core<Char>::reserveSmall(size_t minCapacity, const bool disableSSO) {
    Assert(category() == Category::isSmall);
    if (!disableSSO && minCapacity <= maxSmallSize) {
        // small
        // Nothing to do, everything stays put
    } else if (minCapacity <= maxMediumSize) {
        // medium
        // Don't forget to allocate one extra Char for the terminating null
        //        auto const allocSizeBytes = goodMallocSize((1 + minCapacity) * sizeof(Char));
        auto const allocSizeBytes = (1 + minCapacity) * sizeof(Char);
        auto const pData = static_cast<Char*>(checkedMalloc(allocSizeBytes));
        auto const size = smallSize();
        // Also copies terminator.
        string_detail::podCopy(small_, small_ + size + 1, pData);
        ml_.data_ = pData;
        ml_.size_ = size;
        ml_.setCapacity(allocSizeBytes / sizeof(Char) - 1, Category::isMedium);
    } else {
        // large
        auto const newRC = RefCounted::create(&minCapacity);
        auto const size = smallSize();
        // Also copies terminator.
        string_detail::podCopy(small_, small_ + size + 1, newRC->data_);
        ml_.data_ = newRC->data_;
        ml_.size_ = size;
        ml_.setCapacity(minCapacity, Category::isLarge);
        Assert(capacity() >= minCapacity);
    }
}

template <class Char>
inline auto string_core<Char>::expandNoinit(const size_t delta, bool expGrowth, /* = false */
                                            bool disableSSO /* = FBSTRING_DISABLE_SSO */) -> Char* {
    // Strategy is simple: make room, then change size
    Assert(capacity() >= size());
    size_t oldSz = 0;
    size_t newSz = 0;
    if (category() == Category::isSmall) {
        oldSz = smallSize();
        newSz = oldSz + delta;
        if (!disableSSO && newSz <= maxSmallSize) {
            setSmallSize(newSz);
            return small_ + oldSz;
        }
        reserveSmall(expGrowth ? std::max(newSz, 2 * maxSmallSize) : newSz, disableSSO);
    } else {
        oldSz = ml_.size_;
        newSz = oldSz + delta;
        if (newSz > capacity()) [[unlikely]] {
            // ensures not shared
            reserve(expGrowth ? std::max(newSz, 1 + capacity() * 3 / 2) : newSz);
        }
    }
    Assert(capacity() >= newSz);
    // Category can't be small - we took care of that above
    Assert(category() == Category::isMedium || category() == Category::isLarge);
    ml_.size_ = newSz;
    ml_.data_[newSz] = '\0';
    Assert(size() == newSz);
    return ml_.data_ + oldSz;
}

template <class Char>
inline void string_core<Char>::shrinkSmall(const size_t delta) noexcept {
    // Check for underflow
    Assert(delta <= smallSize());
    setSmallSize(smallSize() - delta);
}

template <class Char>
inline void string_core<Char>::shrinkMedium(const size_t delta) noexcept {
    // Medium strings and unique large strings need no special
    // handling.
    Assert(ml_.size_ >= delta);
    ml_.size_ -= delta;
    ml_.data_[ml_.size_] = '\0';
}

template <class Char>
inline void string_core<Char>::shrinkLarge(const size_t delta) {
    Assert(ml_.size_ >= delta);
    // Shared large string, must make unique. This is because of the
    // durn terminator must be written, which may trample the shared
    // data.
    if (delta != 0U) {
        string_core(ml_.data_, ml_.size_ - delta).swap(*this);
    }
    // No need to write the terminator.
}

/**
 * This is the basic_string replacement. For conformity,
 * basic_string takes the same template parameters, plus the last
 * one which is the core.
 */
template <typename E, class T = std::char_traits<E>, class A = std::allocator<E>, class Storage = string_core<E>>
class basic_string
{
    static_assert(std::is_same<A, std::allocator<E>>::value or std::is_same<A, pmr::polymorphic_allocator<E>>::value,
                  "string ignores custom allocators");

    template <typename Ex, typename... Args>
    [[gnu::always_inline]] static void enforce(bool condition, Args&&... args) {
        if (!condition) {
            throw Ex(static_cast<Args&&>(args)...);
        }
    }

    [[nodiscard]] auto isSane() const noexcept -> bool {
        return begin() <= end() && empty() == (size() == 0) && empty() == (begin() == end()) && size() <= max_size() &&
               capacity() <= max_size() && size() <= capacity() && begin()[size()] == '\0';
    }

    struct Invariant
    {
        Invariant() = delete;
        Invariant(const Invariant&) = delete;
        auto operator=(const Invariant&) -> Invariant& = delete;
        Invariant(Invariant&&) noexcept = delete;
        auto operator=(Invariant&&) noexcept -> Invariant& = delete;
        explicit Invariant(const basic_string& str) noexcept : s_(str) { Assert(s_.isSane()); }
        ~Invariant() noexcept { Assert(s_.isSane()); }

       private:
        const basic_string& s_;
    };

   public:
    using ArenaManaged_ = void;
    // types
    using traits_type = T;
    using value_type = typename traits_type::char_type;
    using allocator_type = A;
    using size_type = typename std::allocator_traits<A>::size_type;
    using difference_type = typename std::allocator_traits<A>::difference_type;

    using reference = typename std::allocator_traits<A>::value_type&;
    using const_reference = const typename std::allocator_traits<A>::value_type&;
    using pointer = typename std::allocator_traits<A>::pointer;
    using const_pointer = typename std::allocator_traits<A>::const_pointer;

    using iterator = E*;
    using const_iterator = const E*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    static constexpr size_type npos = size_type(-1);
    using IsRelocatable = std::true_type;

   private:
    static void procrustes(size_type& n, size_type nmax) noexcept {
        if (n > nmax) {
            n = nmax;
        }
    }

    static auto traitsLength(const value_type* s) -> size_type;

   public:
    // C++11 21.4.2 construct/copy/destroy

    // Note: while the following two constructors can be (and previously were)
    // collapsed into one constructor written this way:
    //
    //   explicit basic_string(const A& a = A()) noexcept { }
    //
    // This can cause Clang (at least version 3.7) to fail with the error:
    //   "chosen constructor is explicit in copy-initialization ...
    //   in implicit initialization of field '(x)' with omitted initializer"
    //
    // if used in a struct which is default-initialized.  Hence the split into
    // these two separate constructors.

    basic_string() noexcept : basic_string(A()) {}

    explicit basic_string(const A& allocator) noexcept : store_(allocator) {}

    basic_string(const basic_string& str) : store_(str.store_) {}

    // Move constructor
    basic_string(basic_string&& goner) noexcept : store_(std::move(goner.store_)) {}

    // This is defined for compatibility with std::string
    template <typename A2>
    /* implicit */ basic_string(const std::basic_string<E, T, A2>& str) : store_(str.data(), str.size()) {}

    basic_string(const basic_string& str, size_type pos, size_type n = npos, const A& a = A()) : store_(a) {
        assign(str, pos, n);
    }

    /* implicit */ basic_string(const value_type* s, const A& a = A()) : store_(s, traitsLength(s), a) {}

    basic_string(const value_type* s, size_type n, const A& a = A()) : store_(s, n, a) {}

    basic_string(size_type n, value_type c, const A& a = A()) : store_(a) {
        auto const pData = store_.expandNoinit(n);
        string_detail::podFill(pData, pData + n, c);
    }

    template <class InIt>
    basic_string(InIt begin, InIt end,
                 typename std::enable_if<!std::is_same<InIt, value_type*>::value, const A>::type& a = A())
        : store_(a) {
        assign(begin, end);
    }

    // Specialization for const char*, const char*
    basic_string(const value_type* b, const value_type* e, const A& a = A()) : store_(b, size_type(e - b), a) {}

    basic_string(std::basic_string_view<value_type> view, const A& a = A()) : store_(view.data(), view.size(), a) {}

    // Construction from initialization list
    basic_string(std::initializer_list<value_type> init_list, const A& a = A()) : store_(a) {
        assign(init_list.begin(), init_list.end());
    }

    ~basic_string() noexcept = default;

    auto operator=(std::string_view lhs) -> basic_string&;

    auto operator=(const basic_string& lhs) -> basic_string&;

    // Move assignment
    auto operator=(basic_string&& goner) noexcept -> basic_string&;

    // Compatibility with std::string
    template <typename A2>
    auto operator=(const std::basic_string<E, T, A2>& rhs) -> basic_string& {
        return assign(rhs.data(), rhs.size());
    }

    // Compatibility with std::string
    [[nodiscard]] auto toStdString() const -> std::basic_string<E, T, A> {
        return std::basic_string<E, T, A>(data(), size());
    }

    auto operator=(const value_type* s) -> basic_string& { return assign(s); }

    auto operator=(value_type c) -> basic_string&;

    // This actually goes directly against the C++ spec, but the
    // value_type overload is dangerous, so we're explicitly deleting
    // any overloads of operator= that could implicitly convert to
    // value_type.
    // Note that we do need to explicitly specify the template types because
    // otherwise MSVC 2017 will aggressively pre-resolve value_type to
    // traits_type::char_type, which won't compare as equal when determining
    // which overload the implementation is referring to.
    template <typename TP>
    auto operator=(TP chr) -> typename std::enable_if<
      std::is_convertible<TP, typename basic_string<E, T, A, Storage>::value_type>::value &&
        !std::is_same<typename std::decay<TP>::type, typename basic_string<E, T, A, Storage>::value_type>::value,
      basic_string<E, T, A, Storage>&>::type = delete;

    auto operator=(std::initializer_list<value_type> init_list) -> basic_string& {
        return assign(init_list.begin(), init_list.end());
    }

    /*
     * with clone function, caller can get a truely deep copy string from the original string, default copy constructor
     * and copy assignment operator will get a CoW string if the string in the large mode(size > 255). this feature will
     * make string safe in cross cpu-core argument passing. because memory::string do not use atomic int, so the
     * refCounted ++/-- is not thread safe.
     */
    [[nodiscard]] auto clone() const -> basic_string {
#ifndef NDEBUG
        // just copy the optional<std::thread::id>
        std::optional<std::thread::id> origin_cpu = store_.cpu_;
#endif
        if (store_.category() == Storage::Category::isLarge) {
            // call copy constructor
            basic_string rst(*this);
            rst.store_.unshare();

            // restore cpu_ and new store_.cpu_, because clone is not copy.
#ifndef NDEBUG
            store_.cpu_.swap(origin_cpu);
            // new rst is a new string, so rst.store_.cpu_ should be std::nullopt;
            rst.store_.cpu_ = std::nullopt;
#endif
            return rst;
        }
#ifndef NDEBUG
        basic_string rst{*this};
        store_.cpu_.swap(origin_cpu);
        // new rst is a new string, so rst.store_.cpu_ should be kNullCPU.
        rst.store_.cpu_ = std::nullopt;
        return rst;
#else
        // directly call copy constructor.
        // and use the RVO to avoid copy or move.
        return {*this};
#endif
    }

    operator std::basic_string_view<value_type, traits_type>() const noexcept { return {data(), size()}; }

    // C++11 21.4.3 iterators:
    auto begin() noexcept -> iterator { return store_.mutableData(); }

    [[nodiscard]] auto begin() const noexcept -> const_iterator { return store_.data(); }

    [[nodiscard]] auto cbegin() const noexcept -> const_iterator { return begin(); }

    auto end() noexcept -> iterator { return store_.mutableData() + store_.size(); }

    [[nodiscard]] auto end() const noexcept -> const_iterator { return store_.data() + store_.size(); }

    [[nodiscard]] auto cend() const noexcept -> const_iterator { return end(); }

    auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }

    [[nodiscard]] auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }

    [[nodiscard]] auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

    auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }

    [[nodiscard]] auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }

    [[nodiscard]] auto crend() const noexcept -> const_reverse_iterator { return rend(); }

    // Added by C++11
    // C++11 21.4.5, element access:
    [[nodiscard]] auto front() const noexcept -> const value_type& { return *begin(); }
    [[nodiscard]] auto back() const noexcept -> const value_type& {
        Assert(!empty());
        // Should be begin()[size() - 1], but that branches twice
        return *(end() - 1);
    }
    auto front() noexcept -> value_type& { return *begin(); }
    auto back() noexcept -> value_type& {
        Assert(!empty());
        // Should be begin()[size() - 1], but that branches twice
        return *(end() - 1);
    }
    void pop_back() noexcept {
        Assert(!empty());
        store_.shrink(1);
    }

    // C++11 21.4.4 capacity:
    [[nodiscard]] auto size() const noexcept -> size_type { return store_.size(); }

    [[nodiscard]] auto length() const noexcept -> size_type { return size(); }

    [[nodiscard]] auto max_size() const noexcept -> size_type { return std::numeric_limits<size_type>::max(); }

    void resize(size_type n, value_type c = value_type());

    [[nodiscard]] auto capacity() const noexcept -> size_type { return store_.capacity(); }

    void reserve(size_type res_arg = 0) {
        enforce<std::length_error>(res_arg <= max_size(), "");
        store_.reserve(res_arg);
    }

    void shrink_to_fit() {
        // Shrink only if slack memory is sufficiently large
        if constexpr (std::same_as<Storage, string_core<E>>) {
            if (capacity() < size() * 3 / 2) {
                return;
            }
            basic_string(cbegin(), cend()).swap(*this);
        }
        // for arena_string, do not shrink at all.
    }

    void clear() { resize(0); }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    // C++11 21.4.5 element access:
    auto operator[](size_type pos) const noexcept -> const_reference { return *(begin() + pos); }

    auto operator[](size_type pos) noexcept -> reference { return *(begin() + pos); }

    [[nodiscard]] auto at(size_type n) const -> const_reference {
        enforce<std::out_of_range>(n < size(), "");
        return (*this)[n];
    }

    auto at(size_type n) -> reference {
        enforce<std::out_of_range>(n < size(), "");
        return (*this)[n];
    }

    // C++11 21.4.6 modifiers:
    auto operator+=(const basic_string& str) -> basic_string& { return append(str); }

    auto operator+=(std::basic_string_view<value_type> str) -> basic_string& { return append(str); }

    auto operator+=(const value_type* s) -> basic_string& { return append(s); }

    auto operator+=(const value_type c) -> basic_string& {
        push_back(c);
        return *this;
    }

    auto operator+=(std::initializer_list<value_type> init_list) -> basic_string& {
        append(init_list);
        return *this;
    }

    auto append(const basic_string& str) -> basic_string&;

    auto append(const basic_string& str, size_type pos, size_type n) -> basic_string&;

    auto append(const value_type* s, size_type n) -> basic_string&;

    auto append(const value_type* s) -> basic_string& { return append(s, traitsLength(s)); }

    auto append(size_type n, value_type c) -> basic_string&;

    template <class InputIterator>
    auto append(InputIterator first, InputIterator last) -> basic_string& {
        insert(end(), first, last);
        return *this;
    }

    auto append(std::initializer_list<value_type> init_list) -> basic_string& {
        return append(init_list.begin(), init_list.end());
    }

    void push_back(const value_type c) {  // primitive
        store_.push_back(c);
    }

    auto assign(const basic_string& str) -> basic_string& {
        if (&str == this) {
            return *this;
        }
        return assign(str.data(), str.size());
    }

    auto assign(basic_string&& str) noexcept -> basic_string& { return *this = std::move(str); }

    auto assign(const basic_string& str, size_type pos, size_type n) -> basic_string&;

    auto assign(const value_type* s, size_type n) -> basic_string&;

    auto assign(const value_type* s) -> basic_string& { return assign(s, traitsLength(s)); }

    auto assign(std::initializer_list<value_type> init_list) -> basic_string& {
        return assign(init_list.begin(), init_list.end());
    }

    template <class ItOrLength, class ItOrChar>
    auto assign(ItOrLength first_or_n, ItOrChar last_or_c) -> basic_string& {
        return replace(begin(), end(), first_or_n, last_or_c);
    }

    auto insert(size_type pos1, const basic_string& str) -> basic_string& {
        return insert(pos1, str.data(), str.size());
    }

    auto insert(size_type pos1, const basic_string& str, size_type pos2, size_type n) -> basic_string& {
        enforce<std::out_of_range>(pos2 <= str.length(), "");
        procrustes(n, str.length() - pos2);
        return insert(pos1, str.data() + pos2, n);
    }

    auto insert(size_type pos, const value_type* s, size_type n) -> basic_string& {
        enforce<std::out_of_range>(pos <= length(), "");
        insert(begin() + pos, s, s + n);
        return *this;
    }

    auto insert(size_type pos, const value_type* s) -> basic_string& { return insert(pos, s, traitsLength(s)); }

    auto insert(size_type pos, size_type n, value_type c) -> basic_string& {
        enforce<std::out_of_range>(pos <= length(), "");
        insert(begin() + pos, n, c);
        return *this;
    }

    auto insert(const_iterator p, const value_type c) -> iterator {
        const size_type pos = size_t(p - cbegin());
        insert(p, 1, c);
        return begin() + pos;
    }

   private:
    using istream_type = std::basic_istream<value_type, traits_type>;
    auto getlineImpl(istream_type& is, value_type delim) -> istream_type&;

   public:
    friend inline auto getline(istream_type& is, basic_string& str, value_type delim) -> istream_type& {
        return str.getlineImpl(is, delim);
    }

    friend inline auto getline(istream_type& is, basic_string& str) -> istream_type& { return getline(is, str, '\n'); }

   private:
    auto insertImplDiscr(const_iterator i, size_type n, value_type c, std::true_type) -> iterator;

    template <class InputIter>
    auto insertImplDiscr(const_iterator i, InputIter b, InputIter e, std::false_type) -> iterator;

    template <class FwdIterator>
    auto insertImpl(const_iterator i, FwdIterator s1, FwdIterator s2, std::forward_iterator_tag) -> iterator;

    template <class InputIterator>
    auto insertImpl(const_iterator i, InputIterator b, InputIterator e, std::input_iterator_tag) -> iterator;

   public:
    auto insert(const_iterator p, size_type first_or_n, value_type last_or_c) -> iterator {
        return insertImplDiscr(p, first_or_n, last_or_c, std::true_type());
    }

    template <class InputIter>
    auto insert(const_iterator p, InputIter first, InputIter last) -> iterator {
        return insertImplDiscr(p, first, last, std::false_type());
    }

    auto insert(const_iterator p, std::initializer_list<value_type> init_list) -> iterator {
        return insert(p, init_list.begin(), init_list.end());
    }

    auto erase(size_type pos = 0, size_type n = npos) -> basic_string& {
        Invariant checker(*this);

        enforce<std::out_of_range>(pos <= length(), "");
        procrustes(n, length() - pos);
        std::copy(begin() + pos + n, end(), begin() + pos);
        resize(length() - n);
        return *this;
    }

    auto erase(iterator position) -> iterator {
        const size_type pos = size_t(position - begin());
        enforce<std::out_of_range>(pos <= size(), "");
        erase(pos, 1);
        return begin() + pos;
    }

    auto erase(iterator first, iterator last) -> iterator {
        const size_type pos = size_t(first - begin());
        erase(pos, size_t(last - first));
        return begin() + pos;
    }

    // Replaces at most n1 chars of *this, starting with pos1 with the
    // content of str
    auto replace(size_type pos1, size_type n1, const basic_string& str) -> basic_string& {
        return replace(pos1, n1, str.data(), str.size());
    }

    // Replaces at most n1 chars of *this, starting with pos1,
    // with at most n2 chars of str starting with pos2
    auto replace(size_type pos1, size_type n1, const basic_string& str, size_type pos2, size_type n2) -> basic_string& {
        enforce<std::out_of_range>(pos2 <= str.length(), "");
        return replace(pos1, n1, str.data() + pos2, std::min(n2, str.size() - pos2));
    }

    // Replaces at most n1 chars of *this, starting with pos, with chars from s
    auto replace(size_type pos, size_type n1, const value_type* s) -> basic_string& {
        return replace(pos, n1, s, traitsLength(s));
    }

    // Replaces at most n1 chars of *this, starting with pos, with n2
    // occurrences of c
    //
    // consolidated with
    //
    // Replaces at most n1 chars of *this, starting with pos, with at
    // most n2 chars of str.  str must have at least n2 chars.
    template <class StrOrLength, class NumOrChar>
    auto replace(size_type pos, size_type n1, StrOrLength s_or_n2, NumOrChar n_or_c) -> basic_string& {
        Invariant checker(*this);

        enforce<std::out_of_range>(pos <= size(), "");
        procrustes(n1, length() - pos);
        const iterator b = begin() + pos;
        return replace(b, b + n1, s_or_n2, n_or_c);
    }

    auto replace(iterator i1, iterator i2, const basic_string& str) -> basic_string& {
        return replace(i1, i2, str.data(), str.length());
    }

    auto replace(iterator i1, iterator i2, const value_type* s) -> basic_string& {
        return replace(i1, i2, s, traitsLength(s));
    }

   private:
    auto replaceImplDiscr(iterator i1, iterator i2, const value_type* s, size_type n, std::integral_constant<int, 2>)
      -> basic_string&;

    auto replaceImplDiscr(iterator i1, iterator i2, size_type n2, value_type c, std::integral_constant<int, 1>)
      -> basic_string&;

    template <class InputIter>
    auto replaceImplDiscr(iterator i1, iterator i2, InputIter b, InputIter e, std::integral_constant<int, 0>)
      -> basic_string&;

   private:
    template <class FwdIterator>
    auto replaceAliased(iterator /* i1 */, iterator /* i2 */, FwdIterator /* s1 */, FwdIterator /* s2 */,
                        std::false_type /*unused*/) -> bool {
        return false;
    }

    template <class FwdIterator>
    auto replaceAliased(iterator i1, iterator i2, FwdIterator s1, FwdIterator s2, std::true_type) -> bool;

    template <class FwdIterator>
    void replaceImpl(iterator i1, iterator i2, FwdIterator s1, FwdIterator s2, std::forward_iterator_tag);

    template <class InputIterator>
    void replaceImpl(iterator i1, iterator i2, InputIterator b, InputIterator e, std::input_iterator_tag);

   public:
    template <class T1, class T2>
    auto replace(iterator i1, iterator i2, T1 first_or_n_or_s, T2 last_or_c_or_n) -> basic_string& {
        constexpr bool num1 = std::numeric_limits<T1>::is_specialized;
        constexpr bool num2 = std::numeric_limits<T2>::is_specialized;
        using Sel = std::integral_constant<int, num1 ? (num2 ? 1 : -1) : (num2 ? 2 : 0)>;
        return replaceImplDiscr(i1, i2, first_or_n_or_s, last_or_c_or_n, Sel());
    }

    auto copy(value_type* s, size_type n, size_type pos = 0) const -> size_type {
        enforce<std::out_of_range>(pos <= size(), "");
        procrustes(n, size() - pos);

        if (n != 0) {
            string_detail::podCopy(data() + pos, data() + pos + n, s);
        }
        return n;
    }

    void swap(basic_string& rhs) noexcept { store_.swap(rhs.store_); }

    [[nodiscard]] auto c_str() const noexcept -> const value_type* { return store_.c_str(); }

    [[nodiscard]] auto data() const noexcept -> const value_type* { return c_str(); }

    auto data() noexcept -> value_type* { return store_.data(); }

    [[nodiscard]] auto get_allocator() const noexcept -> allocator_type {
        if constexpr (std::same_as<Storage, string_core<E>>) {
            return allocator_type();
        } else {
            return store_.get_allocator();
        }
    }

    [[nodiscard]] auto starts_with(value_type c) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().starts_with(c);
    }

    [[nodiscard]] auto starts_with(const value_type* str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().starts_with(str);
    }

    [[nodiscard]] auto starts_with(std::basic_string_view<value_type> str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().starts_with(str);
    }

    [[nodiscard]] auto starts_with(const basic_string& str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().starts_with(str);
    }

    [[nodiscard]] auto ends_with(value_type c) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().ends_with(c);
    }

    [[nodiscard]] auto ends_with(const value_type* str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().ends_with(str);
    }

    [[nodiscard]] auto ends_with(std::basic_string_view<value_type> str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().ends_with(str);
    }

    [[nodiscard]] auto ends_with(const basic_string& str) const noexcept -> bool {
        return operator std::basic_string_view<value_type, traits_type>().ends_with(str);
    }

    [[nodiscard]] auto contains(value_type c) const noexcept -> bool { return find(c) != basic_string::npos; }

    [[nodiscard]] auto contains(const value_type* str) const noexcept -> bool {
        return find(str) != basic_string::npos;
    }

    [[nodiscard]] auto contains(std::basic_string_view<value_type> str) const noexcept -> bool {
        return find(str) != basic_string::npos;
    }

    [[nodiscard]] auto contains(const basic_string& str) const noexcept -> bool {
        return find(str) != basic_string::npos;
    }

    [[nodiscard]] auto find(const basic_string& str, size_type pos = 0) const noexcept -> size_type {
        return find(str.data(), pos, str.length());
    }

    [[nodiscard]] auto find(const value_type* needle, size_type pos, size_type nsize) const noexcept -> size_type;

    [[nodiscard]] auto find(const value_type* s, size_type pos = 0) const noexcept -> size_type {
        return find(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto find(value_type c, size_type pos = 0) const noexcept -> size_type { return find(&c, pos, 1); }

    [[nodiscard]] auto rfind(const basic_string& str, size_type pos = npos) const noexcept -> size_type {
        return rfind(str.data(), pos, str.length());
    }

    auto rfind(const value_type* s, size_type pos, size_type n) const noexcept -> size_type;

    auto rfind(const value_type* s, size_type pos = npos) const noexcept -> size_type {
        return rfind(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto rfind(value_type c, size_type pos = npos) const noexcept -> size_type {
        return rfind(&c, pos, 1);
    }

    [[nodiscard]] auto find_first_of(const basic_string& str, size_type pos = 0) const noexcept -> size_type {
        return find_first_of(str.data(), pos, str.length());
    }

    auto find_first_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type;

    auto find_first_of(const value_type* s, size_type pos = 0) const noexcept -> size_type {
        return find_first_of(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto find_first_of(value_type c, size_type pos = 0) const noexcept -> size_type {
        return find_first_of(&c, pos, 1);
    }

    [[nodiscard]] auto find_last_of(const basic_string& str, size_type pos = npos) const noexcept -> size_type {
        return find_last_of(str.data(), pos, str.length());
    }

    auto find_last_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type;

    auto find_last_of(const value_type* s, size_type pos = npos) const noexcept -> size_type {
        return find_last_of(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto find_last_of(value_type c, size_type pos = npos) const noexcept -> size_type {
        return find_last_of(&c, pos, 1);
    }

    [[nodiscard]] auto find_first_not_of(const basic_string& str, size_type pos = 0) const noexcept -> size_type {
        return find_first_not_of(str.data(), pos, str.size());
    }

    auto find_first_not_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type;

    auto find_first_not_of(const value_type* s, size_type pos = 0) const noexcept -> size_type {
        return find_first_not_of(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto find_first_not_of(value_type c, size_type pos = 0) const noexcept -> size_type {
        return find_first_not_of(&c, pos, 1);
    }

    [[nodiscard]] auto find_last_not_of(const basic_string& str, size_type pos = npos) const noexcept -> size_type {
        return find_last_not_of(str.data(), pos, str.length());
    }

    auto find_last_not_of(const value_type* s, size_type pos, size_type n) const noexcept -> size_type;

    auto find_last_not_of(const value_type* s, size_type pos = npos) const noexcept -> size_type {
        return find_last_not_of(s, pos, traitsLength(s));
    }

    [[nodiscard]] auto find_last_not_of(value_type c, size_type pos = npos) const noexcept -> size_type {
        return find_last_not_of(&c, pos, 1);
    }

    [[nodiscard]] auto substr(size_type pos = 0, size_type n = npos) const& -> basic_string {
        enforce<std::out_of_range>(pos <= size(), "");
        return basic_string(data() + pos, std::min(n, size() - pos), get_allocator());
    }

    auto substr(size_type pos = 0, size_type n = npos) && -> basic_string {
        enforce<std::out_of_range>(pos <= size(), "");
        erase(0, pos);
        if (n < size()) {
            resize(n);
        }
        return std::move(*this);
    }

    [[nodiscard]] auto compare(const basic_string& str) const noexcept -> int {
        // leo@stdb.io wrote follow code in 2023.9.6
        auto n1 = size();
        auto n2 = str.size();
        const int r = traits_type::compare(data(), str.data(), std::min(n1, n2));
        return r != 0 ? r : n1 > n2 ? 1 : n1 < n2 ? -1 : 0;
    }

    [[nodiscard]] auto compare(size_type pos1, size_type n1, const basic_string& str) const -> int {
        return compare(pos1, n1, str.data(), str.size());
    }

    auto compare(size_type pos1, size_type n1, const value_type* s) const -> int {
        return compare(pos1, n1, s, traitsLength(s));
    }

    auto compare(size_type pos1, size_type n1, const value_type* s, size_type n2) const -> int {
        enforce<std::out_of_range>(pos1 <= size(), "");
        procrustes(n1, size() - pos1);
        // The line below fixed by Jean-Francois Bastien, 04-23-2007. Thanks!
        const int r = traits_type::compare(pos1 + data(), s, std::min(n1, n2));
        return r != 0 ? r : n1 > n2 ? 1 : n1 < n2 ? -1 : 0;
    }

    [[nodiscard]] auto compare(size_type pos1, size_type n1, const basic_string& str, size_type pos2,
                               size_type n2) const -> int {
        enforce<std::out_of_range>(pos2 <= str.size(), "");
        return compare(pos1, n1, str.data() + pos2, std::min(n2, str.size() - pos2));
    }

    // Code from Jean-Francois Bastien (03/26/2007)
    auto compare(const value_type* s) const -> int {
        // Could forward to compare(0, size(), s, traitsLength(s))
        // but that does two extra checks
        const size_type n1(size());
        const size_type n2(traitsLength(s));
        const int r = traits_type::compare(data(), s, std::min(n1, n2));
        return r != 0 ? r : n1 > n2 ? 1 : n1 < n2 ? -1 : 0;
    }

   private:
    // Data
    Storage store_;
};

template <typename E, class T, class A, class S>
auto basic_string<E, T, A, S>::traitsLength(const value_type* s) -> typename basic_string<E, T, A, S>::size_type {
    return s ? traits_type::length(s) : (throw std::logic_error("basic_string: null pointer initializer not valid"), 0);
}
template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::operator=(std::string_view lhs) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);
    return assign(lhs.data(), lhs.size());
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::operator=(const basic_string& lhs) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);

    if (&lhs == this) [[unlikely]] {
        return *this;
    }

    return assign(lhs.data(), lhs.size());
}

// Move assignment
template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::operator=(basic_string&& goner) noexcept -> basic_string<E, T, A, S>& {
    if (&goner == this) [[unlikely]] {
        // Compatibility with std::basic_string<>,
        // C++11 21.4.2 [string.cons] / 23 requires self-move-assignment support.
        return *this;
    }
    // No need of this anymore
    this->~basic_string();
    // Move the goner into this
    new (&store_) S(std::move(goner.store_));
    return *this;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::operator=(value_type c) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);

    if (empty()) {
        store_.expandNoinit(1);
    } else if (store_.isShared()) {
        basic_string(1, c).swap(*this);
        return *this;
    } else {
        store_.shrink(size() - 1);
    }
    front() = c;
    return *this;
}

template <typename E, class T, class A, class S>
inline void basic_string<E, T, A, S>::resize(const size_type n, const value_type c /*= value_type()*/) {
    Invariant checker(*this);

    auto size = this->size();
    if (n <= size) {
        store_.shrink(size - n);
    } else {
        auto const delta = n - size;
        auto pData = store_.expandNoinit(delta);
        string_detail::podFill(pData, pData + delta, c);
    }
    Assert(this->size() == n);
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::append(const basic_string& str) -> basic_string<E, T, A, S>& {
#ifndef NDEBUG
    auto desiredSize = size() + str.size();
#endif
    append(str.data(), str.size());
    Assert(size() == desiredSize);
    return *this;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::append(const basic_string& str, const size_type pos, size_type n)
  -> basic_string<E, T, A, S>& {
    const size_type sz = str.size();
    enforce<std::out_of_range>(pos <= sz, "");
    procrustes(n, sz - pos);
    return append(str.data() + pos, n);
}

template <typename E, class T, class A, class S>
auto basic_string<E, T, A, S>::append(const value_type* s, size_type n) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);

    if (!n) [[unlikely]] {
        // Unlikely but must be done
        return *this;
    }
    auto const oldSize = size();
    auto const oldData = data();
    auto pData = store_.expandNoinit(n, /* expGrowth = */ true);

    // Check for aliasing (rare). We could use "<=" here but in theory
    // those do not work for pointers unless the pointers point to
    // elements in the same array. For that reason we use
    // std::less_equal, which is guaranteed to offer a total order
    // over pointers. See discussion at http://goo.gl/Cy2ya for more
    // info.
    std::less_equal<const value_type*> le;
    if (le(oldData, s) && !le(oldData + oldSize, s)) {
        Assert(le(s + n, oldData + oldSize));
        // expandNoinit() could have moved the storage, restore the source.
        s = data() + (s - oldData);
        string_detail::podMove(s, s + n, pData);
    } else {
        string_detail::podCopy(s, s + n, pData);
    }

    Assert(size() == oldSize + n);
    return *this;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::append(size_type n, value_type c) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);
    auto pData = store_.expandNoinit(n, /* expGrowth = */ true);
    string_detail::podFill(pData, pData + n, c);
    return *this;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::assign(const basic_string& str, const size_type pos, size_type n)
  -> basic_string<E, T, A, S>& {
    const size_type sz = str.size();
    enforce<std::out_of_range>(pos <= sz, "");
    procrustes(n, sz - pos);
    return assign(str.data() + pos, n);
}

template <typename E, class T, class A, class S>
auto basic_string<E, T, A, S>::assign(const value_type* s, size_type n) -> basic_string<E, T, A, S>& {
    Invariant checker(*this);

    if (n == 0) {
        resize(0);
    } else if (size() >= n) {
        // s can alias this, we need to use podMove.
        string_detail::podMove(s, s + n, store_.mutableData());
        store_.shrink(size() - n);
        Assert(size() == n);
    } else {
        // If n is larger than size(), s cannot alias this string's
        // storage.
        resize(0);
        // Do not use exponential growth here: assign() should be tight,
        // to mirror the behavior of the equivalent constructor.
        string_detail::podCopy(s, s + n, store_.expandNoinit(n));
    }

    Assert(size() == n);
    return *this;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::getlineImpl(istream_type& is, value_type delim) ->
  typename basic_string<E, T, A, S>::istream_type& {
    Invariant checker(*this);

    clear();
    size_t size = 0;
    while (true) {
        size_t avail = capacity() - size;
        // string has 1 byte extra capacity for the null terminator,
        // and getline null-terminates the read string.
        is.getline(store_.expandNoinit(avail), avail + 1, delim);
        size += is.gcount();

        if (is.bad() || is.eof() || !is.fail()) {
            // Done by either failure, end of file, or normal read.
            if (!is.bad() && !is.eof()) {
                --size;  // gcount() also accounts for the delimiter.
            }
            resize(size);
            break;
        }

        Assert(size == this->size());
        Assert(size == capacity());
        // Start at minimum allocation 63 + terminator = 64.
        reserve(std::max<size_t>(63, 3 * size / 2));
        // Clear the error so we can continue reading.
        is.clear();
    }
    return is;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::find(const value_type* needle, const size_type pos,
                                           const size_type nsize) const noexcept ->
  typename basic_string<E, T, A, S>::size_type {
    auto const size = this->size();
    // nsize + pos can overflow (eg pos == npos), guard against that by checking
    // that nsize + pos does not wrap around.
    if (nsize + pos > size || nsize + pos < pos) {
        return npos;
    }

    if (nsize == 0) {
        return pos;
    }
    // Don't use std::search, use a Boyer-Moore-like trick by comparing
    // the last characters first
    auto const haystack = data();
    auto const nsize_1 = nsize - 1;
    auto const lastNeedle = needle[nsize_1];

    // Boyer-Moore skip value for the last char in the needle. Zero is
    // not a valid value; skip will be computed the first time it's
    // needed.
    size_type skip = 0;

    const E* i = haystack + pos;
    auto iEnd = haystack + size - nsize_1;

    while (i < iEnd) {
        // Boyer-Moore: match the last element in the needle
        while (i[nsize_1] != lastNeedle) {
            if (++i == iEnd) {
                // not found
                return npos;
            }
        }
        // Here we know that the last char matches
        // Continue in pedestrian mode
        for (size_t j = 0;;) {
            Assert(j < nsize);
            if (i[j] != needle[j]) {
                // Not found, we can skip
                // Compute the skip value lazily
                if (skip == 0) {
                    skip = 1;
                    while (skip <= nsize_1 && needle[nsize_1 - skip] != lastNeedle) {
                        ++skip;
                    }
                }
                i += skip;
                break;
            }
            // Check if done searching
            if (++j == nsize) {
                // Yay
                return size_t(i - haystack);
            }
        }
    }
    return npos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::insertImplDiscr(const_iterator i, size_type n, value_type c,
                                                      std::true_type /*unused*/) ->
  typename basic_string<E, T, A, S>::iterator {
    Invariant checker(*this);

    Assert(i >= cbegin() && i <= cend());
    const size_type pos = size_t(i - cbegin());

    auto oldSize = size();
    store_.expandNoinit(n, /* expGrowth = */ true);
    auto b = begin();
    string_detail::podMove(b + pos, b + oldSize, b + pos + n);
    string_detail::podFill(b + pos, b + pos + n, c);

    return b + pos;
}

template <typename E, class T, class A, class S>
template <class InputIter>
inline auto basic_string<E, T, A, S>::insertImplDiscr(const_iterator i, InputIter b, InputIter e,
                                                      std::false_type /*unused*/) ->
  typename basic_string<E, T, A, S>::iterator {
    return insertImpl(i, b, e, typename std::iterator_traits<InputIter>::iterator_category());
}

template <typename E, class T, class A, class S>
template <class FwdIterator>
inline auto basic_string<E, T, A, S>::insertImpl(const_iterator i, FwdIterator s1, FwdIterator s2,
                                                 std::forward_iterator_tag /*unused*/) ->
  typename basic_string<E, T, A, S>::iterator {
    Invariant checker(*this);

    Assert(i >= cbegin() && i <= cend());
    const size_type pos = size_t(i - cbegin());
    auto n = std::distance(s1, s2);
    Assert(n >= 0);

    auto oldSize = size();
    store_.expandNoinit(size_t(n), /* expGrowth = */ true);
    auto b = begin();
    string_detail::podMove(b + pos, b + oldSize, b + pos + n);
    std::copy(s1, s2, b + pos);

    return b + pos;
}

template <typename E, class T, class A, class S>
template <class InputIterator>
inline auto basic_string<E, T, A, S>::insertImpl(const_iterator i, InputIterator b, InputIterator e,
                                                 std::input_iterator_tag /*unused*/) ->
  typename basic_string<E, T, A, S>::iterator {
    const auto pos = size_t(i - cbegin());
    basic_string temp(cbegin(), i, get_allocator());
    for (; b != e; ++b) {
        temp.push_back(*b);
    }
    temp.append(i, cend());
    swap(temp);
    return begin() + pos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::replaceImplDiscr(iterator i1, iterator i2, const value_type* s, size_type n,
                                                       std::integral_constant<int, 2> /*unused*/)
  -> basic_string<E, T, A, S>& {
    Assert(i1 <= i2);
    Assert(begin() <= i1 && i1 <= end());
    Assert(begin() <= i2 && i2 <= end());
    return replace(i1, i2, s, s + n);
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::replaceImplDiscr(iterator i1, iterator i2, size_type n2, value_type c,
                                                       std::integral_constant<int, 1> /*unused*/)
  -> basic_string<E, T, A, S>& {
    const size_type n1 = size_t(i2 - i1);
    if (n1 > n2) {
        std::fill(i1, i1 + n2, c);
        erase(i1 + n2, i2);
    } else {
        std::fill(i1, i2, c);
        insert(i2, n2 - n1, c);
    }
    Assert(isSane());
    return *this;
}

template <typename E, class T, class A, class S>
template <class InputIter>
inline auto basic_string<E, T, A, S>::replaceImplDiscr(iterator i1, iterator i2, InputIter b, InputIter e,
                                                       std::integral_constant<int, 0> /*unused*/)
  -> basic_string<E, T, A, S>& {
    using Cat = typename std::iterator_traits<InputIter>::iterator_category;
    replaceImpl(i1, i2, b, e, Cat());
    return *this;
}

template <typename E, class T, class A, class S>
template <class FwdIterator>
inline auto basic_string<E, T, A, S>::replaceAliased(iterator i1, iterator i2, FwdIterator s1, FwdIterator s2,
                                                     std::true_type /*unused*/) -> bool {
    std::less_equal<const value_type*> le{};
    const bool aliased = le(&*begin(), &*s1) && le(&*s1, &*end());
    if (!aliased) {
        return false;
    }
    // Aliased replace, copy to new string
    basic_string temp;
    temp.reserve(size() - size_t(i2 - i1) + size_t(std::distance(s1, s2)));
    temp.append(begin(), i1).append(s1, s2).append(i2, end());
    swap(temp);
    return true;
}

template <typename E, class T, class A, class S>
template <class FwdIterator>
inline void basic_string<E, T, A, S>::replaceImpl(iterator i1, iterator i2, FwdIterator s1, FwdIterator s2,
                                                  std::forward_iterator_tag /*unused*/) {
    Invariant checker(*this);

    // Handle aliased replace
    using Sel = std::bool_constant<std::is_same<FwdIterator, iterator>::value ||
                                   std::is_same<FwdIterator, const_iterator>::value>;
    if (replaceAliased(i1, i2, s1, s2, Sel())) {
        return;
    }

    auto const n1 = i2 - i1;
    Assert(n1 >= 0);
    auto const n2 = std::distance(s1, s2);
    Assert(n2 >= 0);

    if (n1 > n2) {
        // shrinks
        std::copy(s1, s2, i1);
        erase(i1 + n2, i2);
    } else {
        // grows
        s1 = string_detail::copy_n(s1, n1, i1).first;
        insert(i2, s1, s2);
    }
    Assert(isSane());
}

template <typename E, class T, class A, class S>
template <class InputIterator>
inline void basic_string<E, T, A, S>::replaceImpl(iterator i1, iterator i2, InputIterator b, InputIterator e,
                                                  std::input_iterator_tag /*unused*/) {
    basic_string temp(begin(), i1, get_allocator());
    temp.append(b, e).append(i2, end());
    swap(temp);
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::rfind(const value_type* s, size_type pos, size_type n) const noexcept ->
  typename basic_string<E, T, A, S>::size_type {
    if (n > length()) {
        return npos;
    }
    pos = std::min(pos, length() - n);
    if (n == 0) {
        return pos;
    }

    const_iterator i(begin() + pos);
    for (;; --i) {
        if (traits_type::eq(*i, *s) && traits_type::compare(&*i, s, n) == 0) {
            return size_t(i - begin());
        }
        if (i == begin()) {
            break;
        }
    }
    return npos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::find_first_of(const value_type* str, size_type pos, size_type n) const noexcept ->
  typename basic_string<E, T, A, S>::size_type {
    if (pos > length() || n == 0) {
        return npos;
    }
    const_iterator i(begin() + pos);
    const_iterator finish(end());
    for (; i != finish; ++i) {
        if (traits_type::find(str, n, *i) != nullptr) {
            return size_t(i - begin());
        }
    }
    return npos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::find_last_of(const value_type* str, size_type pos, size_type n) const noexcept ->
  typename basic_string<E, T, A, S>::size_type {
    if (!empty() && n > 0) {
        pos = std::min(pos, length() - 1);
        const_iterator i(begin() + pos);
        for (;; --i) {
            if (traits_type::find(str, n, *i) != nullptr) {
                return size_t(i - begin());
            }
            if (i == begin()) {
                break;
            }
        }
    }
    return npos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::find_first_not_of(const value_type* str, size_type pos,
                                                        size_type n) const noexcept ->
  typename basic_string<E, T, A, S>::size_type {
    if (pos < length()) {
        const_iterator i(begin() + pos);
        const_iterator finish(end());
        for (; i != finish; ++i) {
            if (traits_type::find(str, n, *i) == nullptr) {
                return size_t(i - begin());
            }
        }
    }
    return npos;
}

template <typename E, class T, class A, class S>
inline auto basic_string<E, T, A, S>::find_last_not_of(const value_type* str, size_type pos, size_type n) const noexcept
  -> typename basic_string<E, T, A, S>::size_type {
    if (!this->empty()) {
        pos = std::min(pos, size() - 1);
        const_iterator i(begin() + pos);
        for (;; --i) {
            if (traits_type::find(str, n, *i) == nullptr) {
                return size_t(i - begin());
            }
            if (i == begin()) {
                break;
            }
        }
    }
    return npos;
}

// non-member functions
// C++11 21.4.8.1/1
template <typename E, class T, class A, class S>
inline auto operator+(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs)
  -> basic_string<E, T, A, S> {
    basic_string<E, T, A, S> result(lhs.get_allocator());
    result.reserve(lhs.size() + rhs.size());
    result.append(lhs).append(rhs);
    return result;
}

// C++11 21.4.8.1/2
template <typename E, class T, class A, class S>
inline auto operator+(basic_string<E, T, A, S>&& lhs, const basic_string<E, T, A, S>& rhs) -> basic_string<E, T, A, S> {
    return std::move(lhs.append(rhs));
}

// C++11 21.4.8.1/3
template <typename E, class T, class A, class S>
inline auto operator+(const basic_string<E, T, A, S>& lhs, basic_string<E, T, A, S>&& rhs) -> basic_string<E, T, A, S> {
    if (rhs.capacity() >= lhs.size() + rhs.size()) {
        // Good, at least we don't need to reallocate
        return std::move(rhs.insert(0, lhs));
    }
    // Meh, no go. Forward to operator+(const&, const&).
    auto const& rhsC = rhs;
    return lhs + rhsC;
}

// C++11 21.4.8.1/4
template <typename E, class T, class A, class S>
inline auto operator+(basic_string<E, T, A, S>&& lhs, basic_string<E, T, A, S>&& rhs) -> basic_string<E, T, A, S> {
    return std::move(lhs.append(rhs));
}

// C++11 21.4.8.1/5
template <typename E, class T, class A, class S>
inline auto operator+(const E* lhs, const basic_string<E, T, A, S>& rhs) -> basic_string<E, T, A, S> {
    //
    basic_string<E, T, A, S> result(rhs.get_allocator());
    const auto len = basic_string<E, T, A, S>::traits_type::length(lhs);
    result.reserve(len + rhs.size());
    result.append(lhs, len).append(rhs);
    return result;
}

// C++11 21.4.8.1/6
template <typename E, class T, class A, class S>
inline auto operator+(const E* lhs, basic_string<E, T, A, S>&& rhs) -> basic_string<E, T, A, S> {
    //
    const auto len = basic_string<E, T, A, S>::traits_type::length(lhs);
    if (rhs.capacity() >= len + rhs.size()) {
        // Good, at least we don't need to reallocate
        rhs.insert(rhs.begin(), lhs, lhs + len);
        return std::move(rhs);
    }
    // Meh, no go. Do it by hand since we have len already.
    basic_string<E, T, A, S> result(rhs.get_allocator());
    result.reserve(len + rhs.size());
    result.append(lhs, len).append(rhs);
    return result;
}

// C++11 21.4.8.1/7
template <typename E, class T, class A, class S>
inline auto operator+(E lhs, const basic_string<E, T, A, S>& rhs) -> basic_string<E, T, A, S> {
    basic_string<E, T, A, S> result(rhs.get_allocator());
    result.reserve(1 + rhs.size());
    result.push_back(lhs);
    result.append(rhs);
    return result;
}

// C++11 21.4.8.1/8
template <typename E, class T, class A, class S>
inline auto operator+(E lhs, basic_string<E, T, A, S>&& rhs) -> basic_string<E, T, A, S> {
    //
    if (rhs.capacity() > rhs.size()) {
        // Good, at least we don't need to reallocate
        rhs.insert(rhs.begin(), lhs);
        return std::move(rhs);
    }
    // Meh, no go. Forward to operator+(E, const&).
    auto const& rhsC = rhs;
    return lhs + rhsC;
}

// C++11 21.4.8.1/9
template <typename E, class T, class A, class S>
inline auto operator+(const basic_string<E, T, A, S>& lhs, const E* rhs) -> basic_string<E, T, A, S> {
    using size_type = typename basic_string<E, T, A, S>::size_type;
    using traits_type = typename basic_string<E, T, A, S>::traits_type;

    basic_string<E, T, A, S> result(lhs.get_allocator());
    const size_type len = traits_type::length(rhs);
    result.reserve(lhs.size() + len);
    result.append(lhs).append(rhs, len);
    return result;
}

// C++11 21.4.8.1/10
template <typename E, class T, class A, class S>
inline auto operator+(basic_string<E, T, A, S>&& lhs, const E* rhs) -> basic_string<E, T, A, S> {
    //
    return std::move(lhs += rhs);
}

// C++11 21.4.8.1/11
template <typename E, class T, class A, class S>
inline auto operator+(const basic_string<E, T, A, S>& lhs, E rhs) -> basic_string<E, T, A, S> {
    basic_string<E, T, A, S> result(lhs.get_allocator());
    result.reserve(lhs.size() + 1);
    result.append(lhs);
    result.push_back(rhs);
    return result;
}

// C++11 21.4.8.1/12
template <typename E, class T, class A, class S>
inline auto operator+(basic_string<E, T, A, S>&& lhs, E rhs) -> basic_string<E, T, A, S> {
    //
    return std::move(lhs += rhs);
}

template <typename E, class T, class A, class S>
inline auto operator<=>(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept
  -> std::strong_ordering {
    // return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
    auto const cmp = lhs.compare(rhs);
    if (cmp < 0) {
        return std::strong_ordering::less;
    }
    if (cmp > 0) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

template <typename E, class T, class A, class S>
inline auto operator==(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() && lhs.compare(rhs) == 0;
}

template <typename E, class T, class A, class S>
inline auto operator==(const typename basic_string<E, T, A, S>::value_type* lhs,
                       const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs == lhs;
}

template <typename E, class T, class A, class S>
inline auto operator==(const basic_string<E, T, A, S>& lhs,
                       const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return lhs.compare(rhs) == 0;
}

template <typename E, class T, class A, class S>
inline auto operator!=(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S>
inline auto operator!=(const typename basic_string<E, T, A, S>::value_type* lhs,
                       const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S>
inline auto operator!=(const basic_string<E, T, A, S>& lhs,
                       const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S>
inline auto operator<(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

template <typename E, class T, class A, class S>
inline auto operator<(const basic_string<E, T, A, S>& lhs,
                      const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

template <typename E, class T, class A, class S>
inline auto operator<(const typename basic_string<E, T, A, S>::value_type* lhs,
                      const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

template <typename E, class T, class A, class S>
inline auto operator>(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs < lhs;
}

template <typename E, class T, class A, class S>
inline auto operator>(const basic_string<E, T, A, S>& lhs,
                      const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return rhs < lhs;
}

template <typename E, class T, class A, class S>
inline auto operator>(const typename basic_string<E, T, A, S>::value_type* lhs,
                      const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs < lhs;
}

template <typename E, class T, class A, class S>
inline auto operator<=(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(rhs < lhs);
}

template <typename E, class T, class A, class S>
inline auto operator<=(const basic_string<E, T, A, S>& lhs,
                       const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return !(rhs < lhs);
}

template <typename E, class T, class A, class S>
inline auto operator<=(const typename basic_string<E, T, A, S>::value_type* lhs,
                       const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(rhs < lhs);
}

template <typename E, class T, class A, class S>
inline auto operator>=(const basic_string<E, T, A, S>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

template <typename E, class T, class A, class S>
inline auto operator>=(const basic_string<E, T, A, S>& lhs,
                       const typename basic_string<E, T, A, S>::value_type* rhs) noexcept -> bool {
    return !(lhs < rhs);
}

template <typename E, class T, class A, class S>
inline auto operator>=(const typename basic_string<E, T, A, S>::value_type* lhs,
                       const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// C++11 21.4.8.8
template <typename E, class T, class A, class S>
void swap(basic_string<E, T, A, S>& lhs, basic_string<E, T, A, S>& rhs) noexcept {
    lhs.swap(rhs);
}

template <typename E, class T, class A, class S>
inline auto operator>>(
  std::basic_istream<typename basic_string<E, T, A, S>::value_type, typename basic_string<E, T, A, S>::traits_type>& is,
  basic_string<E, T, A, S>& str) -> std::basic_istream<typename basic_string<E, T, A, S>::value_type,
                                                       typename basic_string<E, T, A, S>::traits_type>& {
    using _istream_type =
      std::basic_istream<typename basic_string<E, T, A, S>::value_type, typename basic_string<E, T, A, S>::traits_type>;
    typename _istream_type::sentry sentry(is);
    size_t extracted = 0;
    typename _istream_type::iostate err = _istream_type::goodbit;
    if (sentry) {
        int64_t n = is.width();
        if (n <= 0) {
            n = int64_t(str.max_size());
        }
        str.erase();
        for (auto got = is.rdbuf()->sgetc(); extracted != static_cast<size_t>(n); ++extracted) {
            if (got == T::eof()) {
                err |= _istream_type::eofbit;
                is.width(0);
                break;
            }
            if (isspace(got)) {
                break;
            }
            str.push_back(got);
            got = is.rdbuf()->snextc();
        }
    }
    if (!extracted) {
        err |= _istream_type::failbit;
    }
    if (err) {
        is.setstate(err);
    }
    return is;
}

template <typename E, class T, class A, class S>
inline auto operator<<(
  std::basic_ostream<typename basic_string<E, T, A, S>::value_type, typename basic_string<E, T, A, S>::traits_type>& os,
  const basic_string<E, T, A, S>& str) -> std::basic_ostream<typename basic_string<E, T, A, S>::value_type,
                                                             typename basic_string<E, T, A, S>::traits_type>& {
#ifdef _LIBCPP_VERSION
    typedef std::basic_ostream<typename basic_string<E, T, A, S>::value_type,
                               typename basic_string<E, T, A, S>::traits_type>
      _ostream_type;
    typename _ostream_type::sentry _s(os);
    if (_s) {
        typedef std::ostreambuf_iterator<typename basic_string<E, T, A, S>::value_type,
                                         typename basic_string<E, T, A, S>::traits_type>
          _Ip;
        size_t __len = str.size();
        bool __left = (os.flags() & _ostream_type::adjustfield) == _ostream_type::left;
        if (__pad_and_output(_Ip(os), str.data(), __left ? str.data() + __len : str.data(), str.data() + __len, os,
                             os.fill())
              .failed()) {
            os.setstate(_ostream_type::badbit | _ostream_type::failbit);
        }
    }
#elif defined(_MSC_VER)
    typedef decltype(os.precision()) streamsize;
    // MSVC doesn't define __ostream_insert
    os.write(str.data(), static_cast<streamsize>(str.size()));
#else
    std::__ostream_insert(os, str.data(), int32_t(str.size()));
#endif
    return os;
}

template <typename E1, class T, class A, class S>
constexpr typename basic_string<E1, T, A, S>::size_type basic_string<E1, T, A, S>::npos;

// basic_string compatibility routines

template <typename E, class T, class A, class S, class A2>
inline auto operator<=>(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept
  -> std::strong_ordering {
    const auto cmp = lhs.compare(0, lhs.size(), rhs.data(), rhs.size());
    if (cmp < 0) {
        return std::strong_ordering::less;
    }
    if (cmp > 0) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
    // return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

// swap the lhs and rhs
template <typename E, class T, class A, class S, class A2>
inline auto operator<=>(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept
  -> std::strong_ordering {
    const auto cmp = lhs.compare(0, lhs.size(), rhs.data(), rhs.size());
    if (cmp < 0) {
        return std::strong_ordering::less;
    }
    if (cmp > 0) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator==(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator==(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs == lhs;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator!=(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator!=(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) < 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) > 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs > lhs;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return rhs < lhs;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<=(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>=(const basic_string<E, T, A, S>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<=(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>=(const std::basic_string<E, T, A2>& lhs, const basic_string<E, T, A, S>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

using string = basic_string<char>;

// Compatibility function, to make sure toStdString(s) can be called
// to convert a std::string or string variable s into type std::string
// with very little overhead if s was already std::string
inline auto toStdString(const stdb::memory::string& str) -> std::string { return std::string(str); }
inline auto toStdString(stdb::memory::string&& str) -> std::string { return std::string(str); }

inline auto toStdString(const std::string& str) -> const std::string& { return str; }

// If called with a temporary, the compiler will select this overload instead
// of the above, so we don't return a (lvalue) reference to a temporary.
inline auto toStdString(std::string&& str) -> std::string&& { return std::move(str); }

}  // namespace stdb::memory

// Hash functions to make string usable with e.g. unordered_map

/*
#define FBSTRING_HASH1(T)                                                               \
    template <>                                                                         \
    struct hash<::stdb::memory::basic_string<T>>                                      \
    {                                                                                   \
        auto operator()(const ::stdb::memory::basic_string<T>& str) const -> size_t { \
            return XXH32(str.data(), str.size(), str.size() * sizeof(T));               \
        }                                                                               \
    };

// The C++11 standard says that these four are defined for basic_string
#define FBSTRING_HASH        \
    FBSTRING_HASH1(char)     \
    FBSTRING_HASH1(char16_t) \
    FBSTRING_HASH1(char32_t) \
    FBSTRING_HASH1(wchar_t)
*/

namespace std {

template <>
struct hash<::stdb::memory::basic_string<char>>
{
    auto operator()(const ::stdb::memory::basic_string<char>& str) const noexcept -> size_t {
        if constexpr (std::is_same_v<size_t, uint64_t>) {
            return XXH3_64bits_withSeed(str.data(), str.size(), str.size() * sizeof(char));
        } else {
            return XXH32(str.data(), str.size(), str.size() * sizeof(char));
        }
    }
};
template <>
struct hash<::stdb::memory::basic_string<char16_t>>
{
    auto operator()(const ::stdb::memory::basic_string<char16_t>& str) const noexcept -> size_t {
        if constexpr (std::is_same_v<size_t, uint64_t>) {
            return XXH3_64bits_withSeed(str.data(), str.size(), str.size() * sizeof(char16_t));
        } else {
            return XXH32(str.data(), str.size(), str.size() * sizeof(char16_t));
        }
    }
};
template <>
struct hash<::stdb::memory::basic_string<char32_t>>
{
    auto operator()(const ::stdb::memory::basic_string<char32_t>& str) const noexcept -> size_t {
        if constexpr (std::is_same_v<size_t, uint64_t>) {
            return XXH3_64bits_withSeed(str.data(), str.size(), str.size() * sizeof(char32_t));
        } else {
            return XXH32(str.data(), str.size(), str.size() * sizeof(char32_t));
        }
    }
};
template <>
struct hash<::stdb::memory::basic_string<wchar_t>>
{
    auto operator()(const ::stdb::memory::basic_string<wchar_t>& str) const noexcept -> size_t {
        if constexpr (std::is_same_v<size_t, uint64_t>) {
            return XXH3_64bits_withSeed(str.data(), str.size(), str.size() * sizeof(wchar_t));
        } else {
            return XXH32(str.data(), str.size(), str.size() * sizeof(wchar_t));
        }
    }
};

}  // namespace std

// #undef FBSTRING_HASH
// #undef FBSTRING_HASH1

// #undef FBSTRING_DISABLE_SSO

namespace fmt {

template <>
struct formatter<stdb::memory::string> : formatter<string_view>
{
    using formatter<fmt::string_view>::parse;

    template <typename Context>
    auto format(const stdb::memory::string& str, Context& ctx) {
        return formatter<string_view>::format({str.data(), str.size()}, ctx);
    }
    template <typename Context>
    auto format(const stdb::memory::string& str, Context& ctx) const {
        return formatter<string_view>::format({str.data(), str.size()}, ctx);
    }
};

}  // namespace fmt

namespace std {

[[gnu::always_inline, nodiscard]] inline auto to_string(const ::stdb::memory::string& str) noexcept -> std::string {
    return {str.data()};
}

}  // namespace std
