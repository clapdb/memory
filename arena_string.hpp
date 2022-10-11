/*
 * Copyright (C) 2020 hurricane <l@stdb.io>. All rights reserved.
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

#include "string.hpp"
#include "arena/arena.hpp"

namespace stdb::memory {

template<class Char>
inline auto arena_smartRealloc(std::pmr::polymorphic_allocator<Char>& allocator, void* ptr, const size_t currentSize, const size_t currentCapacity, const size_t newCapacity)
  -> void* {
    assert(ptr);
    assert(currentSize <= currentCapacity && currentCapacity < newCapacity);

    // arena do not support realloc, just allocate, and memcpy.
    if (auto* const result = allocator.allocate(newCapacity); result != nullptr) [[likely]] {
        std::memcpy(result, ptr, currentSize);
        return result;
    }
    throw std::bad_alloc();
    __builtin_unreachable();
}


// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
template <class Char>
class arena_string_core
{
    template <typename E, class T, class A, class Storage>
    friend class basic_string;

   public:
    arena_string_core(const std::pmr::polymorphic_allocator<Char>& allocator) noexcept : allocator_(allocator) { reset(); }

    arena_string_core(const arena_string_core& rhs): allocator_(rhs.allocator_) {
        assert(&rhs != this);
        switch (rhs.category()) {
            case Category::isSmall:
                copySmall(rhs);
                break;
            case Category::isMedium:
                copyMedium(rhs);
                break;
            case Category::isLarge:
                copyLarge(rhs);
                break;
            default:
                __builtin_unreachable();
        }
        assert(size() == rhs.size());
        assert(memcmp(data(), rhs.data(), size() * sizeof(Char)) == 0);
    }

    auto operator=(const arena_string_core& rhs) -> arena_string_core& = delete;

    arena_string_core(arena_string_core&& goner) noexcept : allocator_(std::move(goner.allocator_)){
        // Take goner's guts
        ml_ = goner.ml_;  // NOLINT
        // Clean goner's carcass
        goner.reset();
    }

    arena_string_core(const Char* const data, const size_t size, const std::pmr::polymorphic_allocator<Char>& allocator) : allocator_(allocator)
    {
        if (size <= maxSmallSize) {
            initSmall(data, size);
        } else if (size <= maxMediumSize) {
            initMedium(data, size);
        } else {
            initLarge(data, size);
        }
        assert(this->size() == size);
        assert(size == 0 || memcmp(this->data(), data, size * sizeof(Char)) == 0);
    }

    ~arena_string_core() noexcept = default;
    /*{
        if (category() == Category::isSmall) {
            return;
        }
        destroyMediumLarge();
    }*/

    // swap below doesn't test whether &rhs == this (and instead
    // potentially does extra work) on the premise that the rarity of
    // that situation actually makes the check more expensive than is
    // worth.
    void swap(arena_string_core& rhs) {
        auto const t = ml_;  // NOLINT
        ml_ = rhs.ml_;       // NOLINT
        rhs.ml_ = t;         // NOLINT
    }

    // In C++11 data() and c_str() are 100% equivalent.
    [[nodiscard]] auto data() const -> const Char* { return c_str(); }

    auto data() -> Char* { return c_str(); }

    [[nodiscard]] inline auto get_allocator() const -> auto {
        return allocator_;
    }

    auto mutableData() -> Char* {
        switch (category()) {
            case Category::isSmall:
                return small_;  // NOLINT
            case Category::isMedium:
                return ml_.data_;  // NOLINT
            case Category::isLarge:
                return mutableDataLarge();
        }
        __builtin_unreachable();
    }

    [[nodiscard]] auto c_str() const -> const Char* {
        const Char* ptr = ml_.data_;  // NOLINT
        // With this syntax, GCC and Clang generate a CMOV instead of a branch.
        ptr = (category() == Category::isSmall) ? small_ : ptr;  // NOLINT
        return ptr;
    }

    void shrink(const size_t delta) {
        if (category() == Category::isSmall) {
            shrinkSmall(delta);
        } else if (category() == Category::isMedium || RefCounted::refs(ml_.data_) == 1) {  // NOLINT
            shrinkMedium(delta);
        } else {
            shrinkLarge(delta);
        }
    }

    void reserve(size_t minCapacity) {
        switch (category()) {
            case Category::isSmall:
                reserveSmall(minCapacity);
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

        assert(capacity() >= minCapacity);
    }

    auto expandNoinit(size_t delta, bool expGrowth = false) -> Char*;

    void push_back(Char c) { *expandNoinit(1, /* expGrowth = */ true) = c; }  // NOLINT

    [[nodiscard]] auto size() const -> size_t {
        size_t ret = ml_.size_;  // NOLINT
        if constexpr (isLittleEndian()) {
            // We can save a couple instructions, because the category is
            // small iff the last char, as unsigned, is <= maxSmallSize.
            using UChar = typename std::make_unsigned<Char>::type;
            auto maybeSmallSize =
              size_t(maxSmallSize) - static_cast<size_t>(static_cast<UChar>(small_[maxSmallSize]));  // NOLINT
            // With this syntax, GCC and Clang generate a CMOV instead of a branch.
            ret = (static_cast<ssize_t>(maybeSmallSize) >= 0) ? maybeSmallSize : ret;
        } else {
            ret = (category() == Category::isSmall) ? smallSize() : ret;
        }
        return ret;
    }

    [[nodiscard]] auto capacity() const -> size_t {
        switch (category()) {
            case Category::isSmall:
                return maxSmallSize;
            case Category::isLarge:
                // For large-sized strings, a multi-referenced chunk has no
                // available capacity. This is because any attempt to append
                // data would trigger a new allocation.
                if (RefCounted::refs(ml_.data_) > 1) {  // NOLINT
                    return ml_.size_;                   // NOLINT
                }
                break;
            case Category::isMedium:
                break;
            default:
                __builtin_unreachable();
        }

        return ml_.capacity();  // NOLINT
    }

    [[nodiscard]] auto isShared() const -> bool {
        return category() == Category::isLarge && RefCounted::refs(ml_.data_) > 1;  // NOLINT
    }

   private:
    auto c_str() -> Char* {
        Char* ptr = ml_.data_;  // NOLINT
        // With this syntax, GCC and Clang generate a CMOV instead of a branch.
        ptr = (category() == Category::isSmall) ? small_ : ptr;  // NOLINT
        return ptr;
    }

    void reset() { setSmallSize(0); }

    /*
    void destroyMediumLarge() noexcept {
        auto const c = category();  // NOLINT
        assert(c != Category::isSmall);
        if (c == Category::isMedium) {
            free(ml_.data_);  // NOLINT
        } else {
            RefCounted::decrementRefs(ml_.data_);  // NOLINT
        }
    }
     */

    struct RefCounted
    {
        // std::atomic<size_t> refCount_;
        size_t refCount_;  // no need atomic on seastar without access cross cpu
        Char data_[1];     // NOLINT(modernize-avoid-c-arrays)

        constexpr static auto getDataOffset() -> size_t { return offsetof(RefCounted, data_); }

        static auto fromData(Char* ptr) -> RefCounted* {
            return static_cast<RefCounted*>(
              static_cast<void*>(static_cast<unsigned char*>(static_cast<void*>(ptr)) - getDataOffset()));  // NOLINT
        }

        // static size_t refs(Char* p) { return fromData(p)->refCount_.load(std::memory_order_acquire); }
        static auto refs(Char* ptr) -> size_t { return fromData(ptr)->refCount_; }

        // static void incrementRefs(Char* p) { fromData(p)->refCount_.fetch_add(1, std::memory_order_acq_rel); }
        static void incrementRefs(Char* ptr) { fromData(ptr)->refCount_++; }

        static void decrementRefs(Char* ptr) {
            auto const dis = fromData(ptr);
            // size_t oldcnt = dis->refCount_.fetch_sub(1, std::memory_order_acq_rel);
            size_t oldcnt = dis->refCount_--;
            assert(oldcnt > 0);
            /*
            if (oldcnt == 1) {
                free(dis);
            }
             */
        }

        static auto create(std::pmr::polymorphic_allocator<Char>& allocator, size_t* size) -> RefCounted* {
            size_t capacityBytes = 0;
            if (!checked_add(&capacityBytes, *size, static_cast<size_t>(1))) {
                throw(std::length_error(""));
            }
            if (!checked_muladd(&capacityBytes, capacityBytes, sizeof(Char), getDataOffset())) {
                throw(std::length_error(""));
            }
//            const size_t allocSize = goodMallocSize(capacityBytes);
            if (auto result = static_cast<RefCounted*>(static_cast<void*>(allocator.allocate(capacityBytes))); result != nullptr) {
                // result->refCount_.store(1, std::memory_order_release);
                result->refCount_ = 1;
                *size = (capacityBytes - getDataOffset()) / sizeof(Char) - 1;
                return result;
            }
            throw std::bad_alloc();
            __builtin_unreachable();
        }

        static auto create(std::pmr::polymorphic_allocator<Char>& allocator, const Char* data, size_t* size) -> RefCounted* {
            const size_t effectiveSize = *size;
            auto result = create(allocator, size);
            if (effectiveSize > 0) [[likely]] {
                string_detail::podCopy(data, data + effectiveSize, result->data_);
            }
            return result;
        }

        static auto reallocate(std::pmr::polymorphic_allocator<Char>& allocator, Char* const data, const size_t currentSize, const size_t currentCapacity,
                               size_t* newCapacity) -> RefCounted* {
            assert(*newCapacity > 0 && *newCapacity > currentSize);
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
            assert(dis->refCount_ == 1);
//            auto result = static_cast<RefCounted*>(smartRealloc(dis, getDataOffset() + (currentSize + 1) * sizeof(Char),
            auto result = static_cast<RefCounted*>(arena_smartRealloc(allocator, dis, getDataOffset() + (currentSize + 1) * sizeof(Char),
                                                                getDataOffset() + (currentCapacity + 1) * sizeof(Char),
                                                                capacityBytes));
            // assert(dis->refCount_.load(std::memory_order_acquire) == 1);
            assert(result->refCount_ == 1);
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

    [[nodiscard]] auto category() const -> Category {
        // works for both big-endian and little-endian
        return static_cast<Category>(bytes_[lastChar] & categoryExtractMask);  // NOLINT
    }

    struct MediumLarge
    {
        Char* data_;
        size_t size_;
        size_t capacity_;

        [[nodiscard]] auto capacity() const -> size_t {
            return isLittleEndian() ? capacity_ & capacityExtractMask : capacity_ >> 2U;
        }

        void setCapacity(size_t cap, Category cat) {
            capacity_ = isLittleEndian() ? cap | (static_cast<size_t>(cat) << kCategoryShift)
                                         : (cap << 2U) | static_cast<size_t>(cat);
        }
    };

    std::pmr::polymorphic_allocator<Char> allocator_;
    union
    {
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        uint8_t bytes_[sizeof(MediumLarge)]{};  // For accessing the last byte.

        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        Char small_[sizeof(MediumLarge) / sizeof(Char)];

        MediumLarge ml_;
    };

    constexpr static size_t lastChar = sizeof(MediumLarge) - 1;
    constexpr static size_t maxSmallSize = lastChar / sizeof(Char);
    constexpr static size_t maxMediumSize = 254 / sizeof(Char);
    constexpr static uint8_t categoryExtractMask = isLittleEndian() ? 0xC0 : 0x3;
    constexpr static size_t kCategoryShift = (sizeof(size_t) - 1) * 8;
    constexpr static size_t capacityExtractMask =
      isLittleEndian() ? ~(static_cast<size_t>(categoryExtractMask) << kCategoryShift) : 0x0 /* unused */;

    static_assert((sizeof(MediumLarge) % sizeof(Char)) == 0U, "Corrupt memory layout for string.");

    [[nodiscard]] auto alloc(::size_t size) -> void * {
        if (auto * rst = allocator_.allocate(size);rst != nullptr) [[likely]] {
            return rst;
        }
        throw std::bad_alloc();
        __builtin_unreachable();
    }

    [[nodiscard]] auto smallSize() const -> size_t {
        assert(category() == Category::isSmall);
        constexpr auto shift = isLittleEndian() ? 0U : 2U;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        auto smallShifted = static_cast<size_t>(small_[maxSmallSize]) >> shift;
        assert(static_cast<size_t>(maxSmallSize) >= smallShifted);
        return static_cast<size_t>(maxSmallSize) - smallShifted;
    }

    void setSmallSize(size_t newSize) {
        // Warning: this should work with uninitialized strings too,
        // so don't assume anything about the previous value of
        // small_[maxSmallSize].
        assert(newSize <= maxSmallSize);
        constexpr auto shift = isLittleEndian() ? 0U : 2U;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        small_[maxSmallSize] = Char(static_cast<char>((maxSmallSize - newSize) << shift));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        small_[newSize] = '\0';
        assert(category() == Category::isSmall && size() == newSize);
    }

    void copySmall(const arena_string_core& /*rhs*/);
    void copyMedium(const arena_string_core& /*rhs*/);
    void copyLarge(const arena_string_core& /*rhs*/);

    void initSmall(const Char* data, size_t size);
    void initMedium(const Char* data, size_t size);
    void initLarge(const Char* data, size_t size);

    void reserveSmall(size_t minCapacity);
    void reserveMedium(size_t minCapacity);
    void reserveLarge(size_t minCapacity);

    void shrinkSmall(size_t delta);
    void shrinkMedium(size_t delta);
    void shrinkLarge(size_t delta);

    void unshare(size_t minCapacity = 0);
    auto mutableDataLarge() -> Char*;
};

template <class Char>
inline void arena_string_core<Char>::copySmall(const arena_string_core& rhs) {
    static_assert(offsetof(MediumLarge, data_) == 0, "string layout failure");
    static_assert(offsetof(MediumLarge, size_) == sizeof(ml_.data_), "string layout failure");          // NOLINT
    static_assert(offsetof(MediumLarge, capacity_) == 2 * sizeof(ml_.data_), "string layout failure");  // NOLINT
    // Just write the whole thing, don't look at details. In
    // particular we need to copy capacity anyway because we want
    // to set the size (don't forget that the last character,
    // which stores a short string's length, is shared with the
    // ml_.capacity field).
    ml_ = rhs.ml_;  // NOLINT
    assert(category() == Category::isSmall && this->size() == rhs.size());
}

template <class Char>
void arena_string_core<Char>::copyMedium(const arena_string_core& rhs) {
    // Medium strings are copied eagerly. Don't forget to allocate
    // one extra Char for the null terminator.
//    auto const allocSize = goodMallocSize((1 + rhs.ml_.size_) * sizeof(Char));  // NOLINT
    auto const allocSize = (1 + rhs.ml_.size_) * sizeof(Char);  // NOLINT
//    ml_.data_ = static_cast<Char*>(checkedMalloc(allocSize));                   // NOLINT
    ml_.data_ = static_cast<Char*>(alloc(allocSize));                   // NOLINT
    // Also copies terminator.
    string_detail::podCopy(rhs.ml_.data_, rhs.ml_.data_ + rhs.ml_.size_ + 1, ml_.data_);  // NOLINT
    ml_.size_ = rhs.ml_.size_;                                                            // NOLINT
    ml_.setCapacity(allocSize / sizeof(Char) - 1, Category::isMedium);                    // NOLINT
    assert(category() == Category::isMedium);
}

template <class Char>
void arena_string_core<Char>::copyLarge(const arena_string_core& rhs) {
    // Large strings are just refcounted
    ml_ = rhs.ml_;                         // NOLINT
    RefCounted::incrementRefs(ml_.data_);  // NOLINT
    assert(category() == Category::isLarge && size() == rhs.size());
}

// Small strings are bitblitted
template <class Char>
inline void arena_string_core<Char>::initSmall(const Char* const data, const size_t size) {
    // Layout is: Char* data_, size_t size_, size_t capacity_
    static_assert(sizeof(*this) == sizeof(Char*) + 2 * sizeof(size_t) + sizeof(std::pmr::polymorphic_allocator<Char>), "string has unexpected size");
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
                ml_.capacity_ = reinterpret_cast<const size_t*>(data)[2];  // NOLINT
                [[fallthrough]];
            case 2:
                ml_.size_ = reinterpret_cast<const size_t*>(data)[1];  // NOLINT
                [[fallthrough]];
            case 1:
                ml_.data_ = *reinterpret_cast<Char**>(const_cast<Char*>(data));  // NOLINT
                [[fallthrough]];
            case 0:
                break;
        }
    } else
#endif
    {
        if (size != 0) {
            string_detail::podCopy(data, data + size, small_);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        }
    }
    setSmallSize(size);
}

template <class Char>
void arena_string_core<Char>::initMedium(const Char* const data, const size_t size) {
    // Medium strings are allocated normally. Don't forget to
    // allocate one extra Char for the terminating null.
//    auto const allocSize = goodMallocSize((1 + size) * sizeof(Char));
    auto const allocSize = (1 + size) * sizeof(Char);
//    ml_.data_ = static_cast<Char*>(checkedMalloc(allocSize));  // NOLINT
    ml_.data_ = static_cast<Char*>(alloc(allocSize));  // NOLINT
    if (size > 0) [[likely]] {
        string_detail::podCopy(data, data + size, ml_.data_);  // NOLINT
    }
    ml_.size_ = size;                                                   // NOLINT
    ml_.setCapacity(allocSize / sizeof(Char) - 1, Category::isMedium);  // NOLINT
    ml_.data_[size] = '\0';                                             // NOLINT
}

template <class Char>
void arena_string_core<Char>::initLarge(const Char* const data, const size_t size) {
    // Large strings are allocated differently
    size_t effectiveCapacity = size;
    auto const newRC = RefCounted::create(allocator_, data, &effectiveCapacity);
    ml_.data_ = newRC->data_;                               // NOLINT
    ml_.size_ = size;                                       // NOLINT
    ml_.setCapacity(effectiveCapacity, Category::isLarge);  // NOLINT
    ml_.data_[size] = '\0';                                 // NOLINT
}

template <class Char>
void arena_string_core<Char>::unshare(size_t minCapacity) {
    assert(category() == Category::isLarge);
    size_t effectiveCapacity = std::max(minCapacity, ml_.capacity());  // NOLINT
    auto const newRC = RefCounted::create(allocator_, &effectiveCapacity);
    // If this fails, someone placed the wrong capacity in an
    // string.
    assert(effectiveCapacity >= ml_.capacity());  // NOLINT
    // Also copies terminator.
    string_detail::podCopy(ml_.data_, ml_.data_ + ml_.size_ + 1, newRC->data_);  // NOLINT
    RefCounted::decrementRefs(ml_.data_);                                        // NOLINT
    ml_.data_ = newRC->data_;                                                    // NOLINT
    ml_.setCapacity(effectiveCapacity, Category::isLarge);                       // NOLINT
    // size_ remains unchanged.
}

template <class Char>
inline auto arena_string_core<Char>::mutableDataLarge() -> Char* {
    assert(category() == Category::isLarge);
    if (RefCounted::refs(ml_.data_) > 1) {  // Ensure unique. // NOLINT
        unshare();
    }
    return ml_.data_;  // NOLINT
}

template <class Char>
void arena_string_core<Char>::reserveLarge(size_t minCapacity) {
    assert(category() == Category::isLarge);
    if (RefCounted::refs(ml_.data_) > 1) {  // Ensure unique // NOLINT
        // We must make it unique regardless; in-place reallocation is
        // useless if the string is shared. In order to not surprise
        // people, reserve the new block at current capacity or
        // more. That way, a string's capacity never shrinks after a
        // call to reserve.
        unshare(minCapacity);
    } else {
        // String is not shared, so let's try to realloc (if needed)
        if (minCapacity > ml_.capacity()) {  // NOLINT
            // Asking for more memory
            auto const newRC = RefCounted::reallocate(allocator_, ml_.data_, ml_.size_, ml_.capacity(), &minCapacity);  // NOLINT
            ml_.data_ = newRC->data_;                                                                       // NOLINT
            ml_.setCapacity(minCapacity, Category::isLarge);                                                // NOLINT
        }
        assert(capacity() >= minCapacity);
    }
}

template <class Char>
void arena_string_core<Char>::reserveMedium(const size_t minCapacity) {
    assert(category() == Category::isMedium);
    // String is not shared
    if (minCapacity <= ml_.capacity()) {  // NOLINT
        return;                           // nothing to do, there's enough room
    }
    if (minCapacity <= maxMediumSize) {
        // Keep the string at medium size. Don't forget to allocate
        // one extra Char for the terminating null.
//        size_t capacityBytes = goodMallocSize((1 + minCapacity) * sizeof(Char));
        size_t capacityBytes = (1 + minCapacity) * sizeof(Char);
        // Also copies terminator.
        ml_.data_ = static_cast<Char*>(  // NOLINT
                                         //  NOLINTNEXTLINE
          arena_smartRealloc(allocator_, ml_.data_, (ml_.size_ + 1) * sizeof(Char), (ml_.capacity() + 1) * sizeof(Char), capacityBytes));
        ml_.setCapacity(capacityBytes / sizeof(Char) - 1, Category::isMedium);  // NOLINT
    } else {
        // Conversion from medium to large string
        arena_string_core nascent(allocator_);
        // Will recurse to another branch of this function
        nascent.reserve(minCapacity);
        nascent.ml_.size_ = ml_.size_;  // NOLINT
        // Also copies terminator.
        string_detail::podCopy(ml_.data_, ml_.data_ + ml_.size_ + 1, nascent.ml_.data_);  // NOLINT
        nascent.swap(*this);
        assert(capacity() >= minCapacity);
    }
}

template <class Char>
void arena_string_core<Char>::reserveSmall(size_t minCapacity) {
    assert(category() == Category::isSmall);
    if (minCapacity <= maxSmallSize) {
        // small
        // Nothing to do, everything stays put
    } else if (minCapacity <= maxMediumSize) {
        // medium
        // Don't forget to allocate one extra Char for the terminating null
//        auto const allocSizeBytes = goodMallocSize((1 + minCapacity) * sizeof(Char));
        auto const allocSizeBytes = (1 + minCapacity) * sizeof(Char);
//        auto const pData = static_cast<Char*>(checkedMalloc(allocSizeBytes));
        auto const pData = static_cast<Char*>(alloc(allocSizeBytes));
        auto const size = smallSize();
        // Also copies terminator.
        string_detail::podCopy(small_, small_ + size + 1, pData);                // NOLINT
        ml_.data_ = pData;                                                       // NOLINT
        ml_.size_ = size;                                                        // NOLINT
        ml_.setCapacity(allocSizeBytes / sizeof(Char) - 1, Category::isMedium);  // NOLINT
    } else {
        // large
        auto const newRC = RefCounted::create(allocator_, &minCapacity);
        auto const size = smallSize();
        // Also copies terminator.
        string_detail::podCopy(small_, small_ + size + 1, newRC->data_);  // NOLINT
        ml_.data_ = newRC->data_;                                         // NOLINT
        ml_.size_ = size;                                                 // NOLINT
        ml_.setCapacity(minCapacity, Category::isLarge);                  // NOLINT
        assert(capacity() >= minCapacity);
    }
}

template <class Char>
inline auto arena_string_core<Char>::expandNoinit(const size_t delta, bool expGrowth) -> Char* {
    // Strategy is simple: make room, then change size
    assert(capacity() >= size());
    size_t oldSz = 0;
    size_t newSz = 0;
    if (category() == Category::isSmall) {
        oldSz = smallSize();
        newSz = oldSz + delta;
        if (newSz <= maxSmallSize) {
            setSmallSize(newSz);
            return small_ + oldSz;  // NOLINT
        }
        reserveSmall(expGrowth ? std::max(newSz, 2 * maxSmallSize) : newSz);
    } else {
        oldSz = ml_.size_;  // NOLINT
        newSz = oldSz + delta;
        if (newSz > capacity()) [[unlikely]] {
            // ensures not shared
            reserve(expGrowth ? std::max(newSz, 1 + capacity() * 3 / 2) : newSz);
        }
    }
    assert(capacity() >= newSz);
    // Category can't be small - we took care of that above
    assert(category() == Category::isMedium || category() == Category::isLarge);
    ml_.size_ = newSz;        // NOLINT
    ml_.data_[newSz] = '\0';  // NOLINT
    assert(size() == newSz);
    return ml_.data_ + oldSz;  // NOLINT
}

template <class Char>
inline void arena_string_core<Char>::shrinkSmall(const size_t delta) {
    // Check for underflow
    assert(delta <= smallSize());
    setSmallSize(smallSize() - delta);
}

template <class Char>
inline void arena_string_core<Char>::shrinkMedium(const size_t delta) {
    // Medium strings and unique large strings need no special
    // handling.
    assert(ml_.size_ >= delta);   // NOLINT
    ml_.size_ -= delta;           // NOLINT
    ml_.data_[ml_.size_] = '\0';  // NOLINT
}

template <class Char>
inline void arena_string_core<Char>::shrinkLarge(const size_t delta) {
    assert(ml_.size_ >= delta);  // NOLINT
    // Shared large string, must make unique. This is because of the
    // durn terminator must be written, which may trample the shared
    // data.
    if (delta != 0U) {
        arena_string_core(ml_.data_, ml_.size_ - delta, allocator_).swap(*this);  // NOLINT
    }
    // No need to write the terminator.
}

static_assert(sizeof (arena_string_core<char>) == 32);

template<class Char>
using arena_basic_string = basic_string<Char, std::char_traits<Char>, std::pmr::polymorphic_allocator<Char>, arena_string_core<Char>>;

using arena_string = basic_string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>, arena_string_core<char>>;

} // namespace stdb::memory

namespace fmt {
template <>
struct ::fmt::formatter<stdb::memory::arena_string> : private formatter<fmt::string_view>
{
    using formatter<fmt::string_view>::parse;

    template <typename Context>
    auto format(const stdb::memory::string& str, Context& ctx) const -> typename Context::iterator {
        return formatter<fmt::string_view>::format({str.data(), str.size()}, ctx);
    }
};
}   // namespace fmt


namespace std {
// for unordered_map
template <>
struct hash<::stdb::memory::arena_basic_string<char>>
{
    auto operator()(const ::stdb::memory::arena_basic_string<char>& str) const -> size_t {
        return XXH32(str.data(), str.size(), str.size() * sizeof(char));
    }
};
template <>
struct hash<::stdb::memory::arena_basic_string<char16_t>>
{
    auto operator()(const ::stdb::memory::arena_basic_string<char16_t>& str) const -> size_t {
        return XXH32(str.data(), str.size(), str.size() * sizeof(char16_t));
    }
};
template <>
struct hash<::stdb::memory::arena_basic_string<char32_t>>
{
    auto operator()(const ::stdb::memory::arena_basic_string<char32_t>& str) const -> size_t {
        return XXH32(str.data(), str.size(), str.size() * sizeof(char32_t));
    }
};
template <>
struct hash<::stdb::memory::arena_basic_string<wchar_t>>
{
    auto operator()(const ::stdb::memory::arena_basic_string<wchar_t>& str) const -> size_t {
        return XXH32(str.data(), str.size(), str.size() * sizeof(wchar_t));
    }
};

}  // namespace std
