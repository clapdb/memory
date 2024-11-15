

#pragma once
#include <fmt/core.h>
#include <sys/types.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <type_traits>

#include "align/align.hpp"
#include "assert_config.hpp"

namespace stdb::memory {

constexpr static inline uint32_t kInvalidSize = std::numeric_limits<uint32_t>::max();
constexpr static inline uint32_t kMaxInternalBufferSize = 7;
constexpr static inline uint32_t kMaxSmallStringSize = 1024;
constexpr static inline uint32_t kMaxMediumStringSize = 4096;
constexpr static inline uint32_t kMediumStringSizeMask = kMaxMediumStringSize - 1U;
// the largest buffer is 2 ^ 32 == 4G, the header is 8 bytes, so the largest size is 2 ^ 32 - 8
constexpr static inline uint64_t kLargeStringSizeMask = (1ULL << 32ULL) - 2 * sizeof(uint32_t);
// if the delta is kDeltaMax, the real delta is >= kDeltaMax
constexpr static inline uint32_t kDeltaMax = (1U << 14U) - 1U;
constexpr static inline uint8_t kIsShift = 0;
constexpr static inline uint8_t kIsTimes = 2;
constexpr static inline uint8_t kIsDelta = 3;

/**
 * find the smallest power of 2 that is greater than size, at least return 16U
 */
[[nodiscard, gnu::always_inline]] constexpr inline auto next_power_of_2(uint16_t size) noexcept -> uint32_t {
    // if size < 8, add 8, or return the size, remove the if branch
    // make sure get 16U at least.
    return std::bit_ceil(size + ((uint16_t)(size < 9)) * 8U);
}

[[nodiscard, gnu::always_inline]] constexpr inline auto is_power_of_2(uint16_t size) noexcept -> bool {
    return bool(size) && not bool(size & (size - 1UL));
}

constexpr static inline auto next_small_size(uint32_t size) noexcept -> uint32_t {
    Assert(size <= kMaxSmallStringSize and size > 0, "size should be in (0, 1024]");
    return next_power_of_2(size);
}

constexpr static inline auto next_medium_size(uint32_t size) noexcept -> uint32_t {
    Assert(size > kMaxSmallStringSize and size <= kMaxMediumStringSize, "size should be between (1024, 4096]");
    // align up to times of 1024, just use bitwise operation
    return stdb::memory::align::AlignUpTo<1024>(size);
}

// AlignUp to the next large size
// we assume that large string usually be immutable, so we just align up to 16 bytes, to save the memory
// if you want to keep grow, you should reserve first, and then append.
// the small_string's design goal was saving memory.
constexpr static inline auto next_large_size(uint32_t size) noexcept -> uint32_t {
    // AlignUp to the next 16 bytes, the fastest way in Arm, x64 is 8
    Assert(size > kMaxMediumStringSize, "size should be bigger than 4096");
    if (size <= kLargeStringSizeMask) [[likely]] {
        return stdb::memory::align::AlignUpTo<16>(size);  // just align to 16 bytes, to save the memory
    }
    // the size is overflow, return kInvalidSize, just check the return value to handle the overflow
    return kInvalidSize;
}

// Calculate the new buffer size, the buffer size is the capacity + the size of the head
// the head is [capacity:4B, size:4B] for large buffer, [capacity:2B] for small buffer
template <bool NullTerminated = true>
constexpr static inline auto calculate_new_buffer_size(uint32_t least_new_capacity) noexcept -> uint32_t {
    if constexpr (NullTerminated) {
        // if the string is null terminated, the capacity should need 1 more.
        ++least_new_capacity;
    }
    if (least_new_capacity <= kMaxSmallStringSize) [[likely]] {
        return next_power_of_2(least_new_capacity);
    } else if (least_new_capacity <= kMaxMediumStringSize) [[likely]] {
        return next_medium_size(least_new_capacity);
    } else if (least_new_capacity <= kInvalidSize - 2 * sizeof(uint32_t))
      [[likely]] {  // the size_or_mask is not overflow
        return next_large_size(least_new_capacity + 2 * sizeof(uint32_t));
    }
    // the next_large_size always was aligned to 16 bytes, so kInvalidSize means overflow is OK
    return kInvalidSize;
}

/**
 * the struct was wrapped all of status and data / ptr.
 */
template <typename Char, bool NullTerminated>
struct malloc_core
{
    using size_type = std::uint32_t;
    using use_std_allocator = std::true_type;

    /**
     * the internal_core will hold the data and the size, for sizeof(Char) == 1, and NullTerminated is true, the
     * capacity = 6, or if NullTerminated is false, the capacity = 7 will be stored in the core, the buf_ptr ==
     * c_str_ptr == data
     *
     * TODO: if want to support sizeof(Char) != 1, internal_core maybe to be deprecated.
     */
    struct internal_core
    {
        Char data[7];
        // 0~7
        uint8_t internal_size : 4;
        // 0000 , ths higher 4 bits is 0, means is_internal is 0
        uint8_t is_internal : 4;
        [[nodiscard, gnu::always_inline]] static constexpr auto capacity() noexcept -> size_type {
            if constexpr (not NullTerminated) {
                return sizeof(data);
            } else {
                return sizeof(data) - 1;
            }
        }

        [[nodiscard, gnu::always_inline]] auto size() const noexcept -> size_type { return internal_size; }

        [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> size_type {
            if constexpr (NullTerminated) {
                return 6 - internal_size;  // the '\0' allocated 1 byte
            } else {
                return 7 - internal_size;
                // or ~internal_size & 0x7U, who is faster?
            }
        }

        [[gnu::always_inline]] void set_size(size_type new_size) noexcept { internal_size = new_size; }
    };  // struct internal_core
    static_assert(sizeof(internal_core) == 8);

    /**
     * the external_core will hold a pointer to the buffer, if the buffer size is less than 4k, the capacity and size
     * will be stored in the core, the buf_ptr == c_str_ptr
     * or store the capacity and size in the buffer head. [capacity:4B, size:4B]
     * the c_str_ptr point to the 8 bytes after the head, the c_str_ptr - 2 is the capacity, the c_str_ptr - 1 is the
     * size (char*)buf_ptr + 8 == c_str_ptr
     */
    struct external_core
    {
        // amd64 / x64 / aarch64 is all little endian, the
        int64_t c_str_ptr : 48;  // the pointer to c_str
        // 12bits: size

        // shift + is_not_shift
        // 0000 : 7
        // 0010 : 16
        // 0100 : 32
        // 0110 : 64
        // 1000 : 128
        // 1010 : 256
        // 1100 : 512
        // 1110 : 1024
        // -- if higher bit is 0, the lowerer 3 bits is shift

        // 0001 : 2048
        // 0101 : 3072
        // 1001 : 4096
        // 1101 : 4096 and 4096 size, size must be 0
        // -- if the higher 2bits is not 11, the lowerer 2bits is times

        // xx11 : if highest 2bits is 11, means capacity is over 4096, and [capacity:4B, size:4B] in the head of the
        // buffer, and the lowerer 2bits should combine the higher 12 bits, the 14bits is the delta

        // and the 14bits of the size_or_mask is the capacity - size, if the value is 1<<14 -1, the delta overflow, and
        // ignore the 14bits
        struct size_and_shift
        {
            uint16_t external_size : 12;
            uint8_t shift : 3;  // 1: 16, 2: 32, 3: 64, 4: 128, 5: 256, 6: 512, 7: 1024, shift always not be 000
            uint8_t flag : 1;   // must be 0
        };  // struct size_and_shift

        struct size_and_times
        {
            uint16_t external_size : 12;
            uint8_t times : 2;  // 0, 1, 2, if times == 3, the size is 4096
            uint8_t flag : 2;   // the flag must be 01
        };  // struct size_and_times

        struct idle_and_flag
        {
            uint16_t idle : 14;
            uint8_t flag : 2;  // must be 11
        };  // struct idle_and_flag

        union
        {
            size_and_shift size_shift;
            size_and_times size_times;
            idle_and_flag idle_flag;
        };  // union size_or_mask

       public:
        [[nodiscard]] auto capacity_fast() const noexcept -> size_type {
            Assert(is_external_valid(), "the external is not valid");
            if (size_shift.flag == kIsShift) {
                if constexpr (not NullTerminated) {
                    return 8U << size_shift.shift;
                } else {
                    return (8U << size_shift.shift) - 1;
                }
            }
            // or this branch is for times
            if (size_times.flag == kIsTimes) {
                if constexpr (not NullTerminated) {
                    return size_times.times < 3U ? ((size_times.times + 2U) << 10U) : 4096;
                } else {
                    return size_times.times < 3U ? ((size_times.times + 2U) << 10U) - 1 : 4095;
                }
            }
            // means don't know the capacity, because the size_or_mask is overflow
            return kInvalidSize;
        }

        [[nodiscard]] auto capacity() const noexcept -> size_type {
            auto cap = capacity_fast();
            // means don't know the capacity, because the size_or_mask is overflow
            if constexpr (not NullTerminated) {
                // the buffer_size - sizeof header
                return cap != kInvalidSize ? cap : *((size_type*)c_str_ptr - 2) - 2 * sizeof(size_type);
            } else {
                // the buffer_size = sizeof header and the '\0'
                return cap != kInvalidSize ? cap : *((size_type*)c_str_ptr - 2) - 2 * sizeof(size_type) - 1;
            }
        }

        [[nodiscard]] auto size_fast() const noexcept -> size_type {
            Assert(is_external_valid(), "the external is not valid");
            if (idle_flag.flag == kIsDelta) [[unlikely]] {
                return kInvalidSize;
            }
            // if external_size == 0, and the times is 3, the size is 4096
            // assert if the flag is IsTimes and the times is 3U, then the external_size must be 0
            Assert(not(size_times.flag == kIsTimes and size_times.times == 3U and size_times.external_size != 0U),
                   "if the flag is IsTimes and the times is 3U, then the external_size must be 0");

            return size_times.external_size +
                   (size_type)(size_times.flag == kIsTimes and size_times.times == 3U) * 4096;
        }

        [[nodiscard]] constexpr auto size() const noexcept -> size_type {
            auto size = size_fast();
            // if size is KInvalidSize, get the true size from the buffer head
            return size != kInvalidSize ? size : *((size_type*)c_str_ptr - 1);
        }

        // set size will be some slow, do not call it frequently
        auto set_size(size_type new_size) noexcept -> void {
            Assert(is_external_valid(), "the external is not valid");
            if (idle_flag.flag != kIsDelta) [[likely]] {
                // just set the external_size or the times
                Assert(new_size <= capacity_fast(), "the new_size is must be less or equal to the capacity_fast()");
                size_times.external_size = new_size;  // 4096 will overflow, it's nice
                if constexpr (not NullTerminated) {
                    if (new_size == 4096) [[unlikely]] {  // little probability
                        size_times.times = 3U;
                    } else if (size_times.times == 3U) [[unlikely]] {
                        // the new_size is not 4k, so the times should be 2U
                        size_times.times = 2U;
                    }
                }
                return;
            }
            // set the header of buffer the size
            auto* ptr = (size_type*)c_str_ptr - 1;
            *ptr = new_size;
            // set the delta to delta_flag.delta
            auto new_delta = *(ptr - 1) - new_size - 2 * sizeof(size_type);
            if constexpr (not NullTerminated) {
                idle_flag.idle = std::min<size_type>(new_delta, kDeltaMax);
            } else {
                idle_flag.idle = std::min<size_type>(new_delta - 1, kDeltaMax);
            }
            return;
        }

        auto increase_size(size_type delta) -> void {
            Assert(is_external_valid(), "the external is not valid");
            if (idle_flag.flag != kIsDelta) [[likely]] {
                Assert(size_times.external_size + delta <= capacity_fast(),
                       "the new_size is must be less or equal to the capacity_fast()");
                size_times.external_size += delta;  // handle the 4096 overflow is OK
                if constexpr (not NullTerminated) {
                    if (delta > 0 and size_times.external_size == 0U) [[unlikely]] {
                        // just increase the times to 3U while the overflow occurs in this increase_size function
                        // calling.
                        size_times.times = 3U;
                    }
                }
                return;
            }
            // by now the string is a large string, the size was stored in the buffer head
            auto* size_ptr = (size_type*)c_str_ptr - 1;
            size_type new_size;
            if (__builtin_uadd_overflow(*size_ptr, delta, &new_size)) [[unlikely]] {
                // the size is overflow size_type
                throw std::overflow_error("the size will overflow from size_type");
            }
            Assert(new_size <= *(size_ptr - 1), "the new_size is must be less or equal to the capacity");
            // set the size to the buffer head
            *size_ptr = new_size;
            // re-calculate the delta, because the Assert, the new_delta will never overflow or underflow
            size_type new_delta;
            if constexpr (not NullTerminated) {
                new_delta = *(size_ptr - 1) - new_size - 2 * sizeof(size_type);
            } else {
                // cap - size - size of head - size of '\0'
                new_delta = *(size_ptr - 1) - new_size - 2 * sizeof(size_type) - 1;
            }

            // set the delta to the external
            idle_flag.idle = new_delta < kDeltaMax ? new_delta : kDeltaMax;
            return;
        }

        auto decrease_size(size_type delta) -> void {
            Assert(is_external_valid(), "the external is not valid");
            // do not check in release mode, because the caller
            Assert(delta <= size(), "the delta is must be less or equal to the size");
            if (idle_flag.flag != kIsDelta) [[likely]] {
                size_times.external_size -= delta;  // handle the 4096 overflow is OK
                return;
            }
            // by now the string is a large string, the size was stored in the buffer head
            auto* size_ptr = (size_type*)c_str_ptr - 1;
            *size_ptr -= delta;

            // re-calculate the delta, because the Assert, the new_delta will never overflow or underflow
            size_type new_delta;
            if constexpr (not NullTerminated) {
                // cap - size - size of head
                new_delta = *(size_ptr - 1) - *size_ptr - 2 * sizeof(size_type);
            } else {
                // cap - size - size of head - size of '\0'
                new_delta = *(size_ptr - 1) - *size_ptr - 2 * sizeof(size_type) - 1;
            }
            // set the delta to the external
            idle_flag.idle = new_delta < kDeltaMax ? new_delta : kDeltaMax;
            return;
        }

        // capcity - size in fast way
        [[nodiscard]] auto idle_capacity_fast() const noexcept -> size_type {
            Assert(is_external_valid(), "the external is not valid");
            // capacity() - size() will be a little bit slower.
            if (idle_flag.flag == kIsDelta) [[unlikely]] {
                return idle_flag.idle;  // if the delta is kDeltaMax, the real delta is >= kDeltaMax
            }
            if (size_times.flag == kIsTimes) [[unlikely]] {
                if (size_times.times < 3U) [[likely]] {
                    if constexpr (not NullTerminated) {
                        // cap - size
                        return ((size_times.times + 1U) << 10U) - size_times.external_size;
                    } else {
                        // cap - size - sizeof('\0')
                        return ((size_times.times + 2U) << 10U) - size_times.external_size - 1;
                    }
                }
                // if the NullTerminated is false, the size is 4096, the capacity is 4096
                // or the size is 4095, the capacity is 4096, 0 = cap - size - sizeof('\0')
                return 0;  // the cap and size both are 4096
                // cap == size, the NullTerminated must is false
            }
            // by now, the flag is kIsShift
            Assert(size_shift.flag == kIsShift, "the flag is must be kIsShift");
            if constexpr (not NullTerminated) {
                // cap - size
                return (8U << size_shift.shift) - size_shift.external_size;
            } else {
                // cap - size - sizeof('\0')
                return (8U << size_shift.shift) - size_shift.external_size - 1;
            }
        }

        // capacity - size
        [[nodiscard]] auto idle_capacity() const noexcept -> size_type {
            auto delta = idle_capacity_fast();
            if (delta != kDeltaMax) [[likely]] {
                return delta;
            }
            // calculate the idle capacity with the buffer head,
            // the buffer head is [capacity, size], the external.ptr point to the 8 bytes after the head
            auto* ptr = (size_type*)c_str_ptr;
            if constexpr (not NullTerminated) {
                // cap - size - sizeof header
                return *(ptr - 2) - *(ptr - 1) - 2 * sizeof(size_type);
            } else {
                // cap - size - sizeof header - sizeof('\0')
                return *(ptr - 2) - *(ptr - 1) - 2 * sizeof(size_type) - 1;
            }
        }

        [[nodiscard, gnu::always_inline]] auto get_buffer_ptr() const noexcept -> Char* {
            Assert(is_external_valid(), "the external is not valid");
            if (idle_flag.flag != kIsDelta) [[likely]] {  // small string is designed for shorted than 4k
                return (Char*)(c_str_ptr);
            }
            return (Char*)(c_str_ptr)-2 * sizeof(size_type);
        }

        [[gnu::always_inline, nodiscard]] auto c_str() const noexcept -> Char* {
            Assert(is_external_valid(), "the external is not valid");
            return (Char*)(c_str_ptr);
        }

        [[gnu::always_inline, nodiscard]] auto is_external_valid() const noexcept -> bool {
            return (size_shift.shift | size_shift.flag) != 0 and c_str_ptr != 0;
        }
    };  // struct external_core

    static_assert(sizeof(external_core) == 8);
    union
    {
        int64_t zero_init = 0;  // make sure the struct be zero initialized
        internal_core internal;
        external_core external;
    };

    [[nodiscard, gnu::always_inline]] inline auto is_external() const noexcept -> bool {
        return internal.is_internal != 0;
    }

    [[nodiscard, gnu::always_inline]] constexpr auto get_capacity() const noexcept -> size_type {
        return is_external() ? external.capacity() : internal_core::capacity();
    }

    [[nodiscard, gnu::always_inline]] inline auto get_c_str() const noexcept -> Char* {
        return is_external() ? external.c_str() : internal_core::c_str();
    }

    auto swap(malloc_core& other) noexcept -> void {
        auto temp = other.zero_init;
        other.zero_init = zero_init;
        zero_init = temp;
    }

    // constructor
    constexpr malloc_core([[maybe_unused]] const std::allocator<Char>& unused = std::allocator<Char>{}) noexcept {}
};  // struct malloc_core

template <typename Char, bool NullTerminated>
struct pmr_core : public malloc_core<Char, NullTerminated>
{
    std::pmr::polymorphic_allocator<Char> pmr_allocator = {};

    using use_std_allocator = std::false_type;
    auto swap(pmr_core& other) noexcept -> void {
        Assert(pmr_allocator.resource() == other.pmr_allocator.resource(), "the swap's 2 allocator is not the same");
        malloc_core<Char, NullTerminated>::swap(other);
        // the buffer and the allocator_ptr belongs to same Arena or Object, so the swap both is OK
    }
    // constructor
    constexpr pmr_core(const std::pmr::polymorphic_allocator<Char>& allocator) noexcept
        : malloc_core<Char, NullTerminated>(), pmr_allocator(allocator) {}

    constexpr pmr_core() noexcept = delete;

    constexpr pmr_core(const pmr_core& other) noexcept
        : malloc_core<Char, NullTerminated>(other), pmr_allocator(other.pmr_allocator) {}
};  // struct malloc_core_and_pmr_allocator

template <typename Char, template <typename, bool> typename Core, class Traits, class Allocator, bool NullTerminated>
class small_string_buffer
{
   protected:
    // types
    using value_type = typename Traits::char_type;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using size_type = std::uint32_t;
    using difference_type = typename std::allocator_traits<Allocator>::difference_type;

    using reference = typename std::allocator_traits<Allocator>::value_type&;
    using const_reference = const typename std::allocator_traits<Allocator>::value_type&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = const typename std::allocator_traits<Allocator>::const_pointer;

    using iterator = Char*;
    using const_iterator = const Char*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // npos is the largest value of size_type, is the max value of size_type
    constexpr static size_type npos = std::numeric_limits<size_type>::max();

    using core_type = Core<Char, NullTerminated>;

    enum class NeedTerminated : bool
    {
        Yes = true,
        No = false
    };

   private:
    core_type _core;

    [[nodiscard]] auto check_the_allocator() const -> bool {
        if constexpr (core_type::use_std_allocator::value) {
            return true;
        } else {
            return _core.pmr_allocator != std::pmr::polymorphic_allocator<Char>{};
        }
    }

    inline static auto calculate_buffer_real_capacity(size_type new_buffer_size) noexcept -> size_type {
        auto real_capacity = new_buffer_size - (new_buffer_size > kMaxMediumStringSize ? 2 * sizeof(size_type) : 0) -
                             (NullTerminated ? 1 : 0);
        return real_capacity;
    }

    // set the buf_size and str_size to the right place
    inline static auto allocate_new_external_buffer(
      size_type new_buffer_size, size_type old_str_size,
      std::pmr::polymorphic_allocator<Char>* allocator_ptr = nullptr) noexcept -> core_type::external_core {
        // make sure the new_buffer_size is not too large
        Assert(new_buffer_size != kInvalidSize, "new_buffer_size should not be kInvalidSize");
        // make sure the old_str_size <= new_buffer_size
        Assert(old_str_size <= calculate_buffer_real_capacity(new_buffer_size),
               "old_str_size should be less than the real capacity of the new buffer");

        if (new_buffer_size <= kMaxSmallStringSize) [[likely]] {
            Assert(is_power_of_2(new_buffer_size), "new_buffer_size should be a power of 2");
            // set the size_shift
            void* buf = nullptr;
            if constexpr (core_type::use_std_allocator::value) {
                buf = std::malloc(new_buffer_size);
            } else {
                buf = allocator_ptr->allocate(new_buffer_size);
            }
            return {
              .c_str_ptr = reinterpret_cast<int64_t>(buf),
              .size_shift = {.external_size = static_cast<uint16_t>(old_str_size),
                             .shift = static_cast<uint8_t>(std::countr_zero(new_buffer_size) -
                                                           3),  // the shift will not be 000, this is internal_core
                             .flag = kIsShift}};
        } else if (new_buffer_size <= kMaxMediumStringSize) [[likely]] {
            Assert((new_buffer_size % kMaxSmallStringSize) == 0, "the medium new_buffer_size is not aligned to 1024");
            auto times = static_cast<uint8_t>((new_buffer_size >> 10U) - 2U);  // 0: 2048, 1: 3072, 2: 4096
            void* buf = nullptr;
            if constexpr (core_type::use_std_allocator::value) {
                buf = std::malloc(new_buffer_size);
            } else {
                buf = allocator_ptr->allocate(new_buffer_size);
            }
            if constexpr (not NullTerminated) {
                return {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                        .size_times = {.external_size = static_cast<uint16_t>(old_str_size),
                                       .times = static_cast<uint8_t>(times + uint8_t(bool(old_str_size == 4096))),
                                       .flag = kIsTimes}};
            } else {
                return {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                        .size_times = {
                          .external_size = static_cast<uint16_t>(old_str_size), .times = times, .flag = kIsTimes}};
            }
        }
        uint16_t idle;
        // the large buffer
        if constexpr (not NullTerminated) {
            idle = std::min<size_type>(new_buffer_size - old_str_size - 2 * sizeof(size_type), kDeltaMax);
        } else {
            idle = std::min<size_type>(new_buffer_size - old_str_size - 2 * sizeof(size_type) - 1, kDeltaMax);
        }
        void* buf = nullptr;
        if constexpr (core_type::use_std_allocator::value) {
            buf = std::malloc(new_buffer_size);
        } else {
            buf = allocator_ptr->allocate(new_buffer_size);
        }
        auto* head = reinterpret_cast<size_type*>(buf);
        *head = new_buffer_size;
        *(head + 1) = old_str_size;
        // the ptr point to the 8 bytes after the head
        return {.c_str_ptr = reinterpret_cast<int64_t>(head + 2), .idle_flag = {.idle = idle, .flag = kIsDelta}};
    }

   protected:
    // this function handle append / push_back / operator +='s internal reallocation
    // this funciion will not change the size, but the capacity or delta
    template <NeedTerminated NeedTerminated = NeedTerminated::Yes>
    void allocate_more(size_type new_append_size) noexcept {
        size_type old_delta_fast =
          _core.is_external() ? _core.external.idle_capacity_fast() : _core.internal.idle_capacity();

        // if no need, do nothing, just update the size or delta
        if (old_delta_fast >= new_append_size) {
            // just return and do nothing
            return;
        }
        // by now, new_append_size > old_delta
        // check the delta is overflow
        if (old_delta_fast == kDeltaMax)
          [[unlikely]] {  // small_string was designed for small string, so large string is not optimized
            // the delta is overflow, re-calc the delta
            auto* cap = (size_type*)(_core.external.c_str_ptr) - 2;
            auto* size = (size_type*)(_core.external.c_str_ptr) - 1;
            if constexpr (not NullTerminated) {
                // cap - size - size of head
                if ((*cap - *size - 2 * sizeof(size_type)) > new_append_size) {
                    // no need to allocate a new buffer, just return
                    return;
                }
            } else {
                if ((*cap - *size - 2 * sizeof(size_type) - 1) > new_append_size) {
                    // no need to allocate a new buffer, just return
                    return;
                }
            }
        }
        // by now, the old delta is not enough, have to allocate a new buffer, to save the new_append_size
        // check if the old_external is internal
        if (not _core.is_external()) {
            auto& old_internal = _core.internal;
            auto new_buffer_size = calculate_new_buffer_size(old_internal.size() + new_append_size);
            typename core_type::external_core new_external;
            if constexpr (core_type::use_std_allocator::value) {
                new_external = allocate_new_external_buffer(new_buffer_size, old_internal.size());
            } else {
                new_external = allocate_new_external_buffer(new_buffer_size, old_internal.size(), &_core.pmr_allocator);
            }
            // copy the old data to the new buffer
            std::memcpy(new_external.c_str(), old_internal.data, old_internal.size());
            if constexpr (NullTerminated and NeedTerminated == NeedTerminated::Yes) {
                new_external.c_str()[old_internal.size()] = '\0';
            }
            _core.external = new_external;
            return;
        }
        // else is external
        // copy the old data to the new buffer
        auto new_buffer_size = calculate_new_buffer_size(_core.external.size() + new_append_size);
        typename core_type::external_core new_external;
        if constexpr (core_type::use_std_allocator::value) {
            new_external = allocate_new_external_buffer(new_buffer_size, _core.external.size());
        } else {
            new_external = allocate_new_external_buffer(new_buffer_size, _core.external.size(), &_core.pmr_allocator);
        }
        std::memcpy(new_external.c_str(), _core.external.c_str(), _core.external.size());

        if constexpr (NullTerminated and NeedTerminated == NeedTerminated::Yes) {
            new_external.c_str()[_core.external.size()] = '\0';
        }
        // deallocate the old buffer
        if constexpr (core_type::use_std_allocator::value) {
            std::free(_core.external.get_buffer_ptr());
        } else {
            _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), _core.external.capacity());
        }
        _core.external = new_external;
        return;
    }

   public:
    template <NeedTerminated NeedTerminated>
    constexpr auto buffer_reserve(size_type new_cap) -> void {
        // check the new_cap is larger than the internal capacity, and larger than current cap
        auto [old_cap, old_size] = get_capacity_and_size();
        if (new_cap > old_cap) [[likely]] {
            // still be a small string
            auto new_buffer_size = calculate_new_buffer_size(new_cap);
            // allocate a new buffer
            typename core_type::external_core new_external;
            if constexpr (core_type::use_std_allocator::value) {
                new_external = allocate_new_external_buffer(new_buffer_size, old_size);
            } else {
                new_external = allocate_new_external_buffer(new_buffer_size, old_size, &_core.pmr_allocator);
            }
            // copy the old data to the new buffer
            std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), get_buffer(), old_size);
            if constexpr (NullTerminated and NeedTerminated == NeedTerminated::Yes) {
                reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
            }
            // deallocate the old buffer
            if constexpr (core_type::use_std_allocator::value) {
                if (_core.is_external()) [[likely]] {
                    std::free(_core.external.get_buffer_ptr());
                }
            } else {
                if (_core.is_external()) [[likely]] {
                    _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), _core.external.capacity());
                }
            }
            // replace the old external with the new one
            _core.external = new_external;
        }
    }

    [[nodiscard]] auto buffer_size() const noexcept -> size_type {
        return _core.is_external() ? _core.external.buffer_size() : _core.internal.buffer_size();
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> size_type {
        return _core.is_external() ? _core.external.idle_capacity() : _core.internal.idle_capacity();
    }

    [[nodiscard]] auto get_capacity_and_size() const noexcept -> std::pair<size_type, size_type> {
        if (not _core.is_external()) {
            return {core_type::internal_core::capacity(), _core.internal.size()};
        }
        if (_core.external.idle_flag.flag != kIsDelta) {
            return {_core.external.capacity_fast(), _core.external.size_fast()};
        }
        auto* buf = (size_type*)_core.external.c_str_ptr;
        if constexpr (NullTerminated) {
            return {*(buf - 2) - 2 * sizeof(size_type) - 1, *(buf - 1)};
        } else {
            return {*(buf - 2) - 2 * sizeof(size_type), *(buf - 1)};
        }
    }

    // increase the size, and won't change the capacity, so the internal/exteral'type or ptr will not change
    // you should always call the allocate_new_external_buffer first, then call this function
    void increase_size(size_type delta) noexcept {
        if (_core.is_external()) [[likely]] {
            _core.external.increase_size(delta);
        } else {
            _core.internal.internal_size += delta;
        }
    }

    void set_size(size_type new_size) noexcept {
        if (_core.is_external()) [[likely]] {
            _core.external.set_size(new_size);
        } else {
            _core.internal.set_size(new_size);
        }
    }

    void decrease_size(size_type delta) noexcept {
        if (_core.is_external()) [[likely]] {
            _core.external.decrease_size(delta);
        } else {
            _core.internal.internal_size -= delta;
        }
    }

    constexpr static inline auto calculate_buffer_size(uint32_t size) noexcept -> uint32_t {
        if (size < core_type::internal_core::capacity()) [[unlikely]] {
            return core_type::internal_core::capacity();
        }
        return calculate_new_buffer_size(size);
    }

   public:
    constexpr small_string_buffer([[maybe_unused]] const Allocator& allocator) noexcept : _core{allocator} {
        if constexpr (not core_type::use_std_allocator::value) {
            Assert(check_the_allocator(), "the pmr default allocator is not allowed to be used in small_string");
        }
    }
    // delete the copy and move constructor
    constexpr small_string_buffer(const small_string_buffer& other) = delete;
    constexpr small_string_buffer(small_string_buffer&& other) noexcept = delete;

    // copy constructor is deleted
    constexpr small_string_buffer(const small_string_buffer& other,
                                  [[maybe_unused]] const Allocator& /*unused*/) noexcept = delete;
    // move constructor
    constexpr small_string_buffer(small_string_buffer&& other, [[maybe_unused]] const Allocator& /*unused*/) noexcept
        : _core(other._core) {
        // set the other's external to 0, to avoid double free
        other._core.zero_init = 0;
        Assert(check_the_allocator(), "the pmr default allocator is not allowed to be used in small_string");
    }

    constexpr ~small_string_buffer() noexcept {
        if constexpr (core_type::use_std_allocator::value) {
            if (_core.is_external()) [[likely]] {
                std::free(_core.external.get_buffer_ptr());
            }
        } else {
            if (_core.is_external()) [[likely]] {
                _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), _core.external.capacity());
            }
        }
    }

    constexpr auto swap(small_string_buffer& other) noexcept -> void { _core.swap(other._core); }

    constexpr auto operator=(const small_string_buffer& other) noexcept = delete;
    constexpr auto operator=(small_string_buffer&& other) noexcept = delete;

    [[nodiscard]] constexpr auto get_buffer() noexcept -> Char* {
        return _core.is_external() ? _core.external.c_str() : _core.internal.data;
    }

    [[nodiscard]] constexpr auto get_buffer() const noexcept -> const Char* {
        return _core.is_external() ? _core.external.c_str() : _core.internal.data;
    }

    [[nodiscard]] constexpr auto size() const noexcept -> size_type {
        return _core.is_external() ? _core.external.size() : _core.internal.size();
    }

    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
        return _core.is_external() ? _core.external.capacity() : core_type::internal_core::capacity();
    }

    [[nodiscard]] constexpr auto get_allocator() const -> Allocator {
        if constexpr (core_type::use_std_allocator::value) {
            return Allocator();
        } else {
            return _core.pmr_allocator;
        }
    }
};  // class small_string_buffer

// if NullTerminated is true, the string will be null terminated, and the size will be the length of the string
// if NullTerminated is false, the string will not be null terminated, and the size will still be the length of the
// string
template <typename Char,
          template <typename, template <typename, bool> class, class, class, bool> class Buffer = small_string_buffer,
          template <typename, bool> class Core = malloc_core, class Traits = std::char_traits<Char>,
          class Allocator = std::allocator<Char>, bool NullTerminated = true>
class basic_small_string : private Buffer<Char, Core, Traits, Allocator, NullTerminated>
{
   public:
    // types
    using buffer_type = Buffer<Char, Core, Traits, Allocator, NullTerminated>;
    using value_type = typename Traits::char_type;
    using traits_type = Traits;
    using allocator_type = Allocator;
    using size_type = std::uint32_t;
    using difference_type = typename std::allocator_traits<Allocator>::difference_type;

    using reference = typename std::allocator_traits<Allocator>::value_type&;
    using const_reference = const typename std::allocator_traits<Allocator>::value_type&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = const typename std::allocator_traits<Allocator>::const_pointer;

    using iterator = Char*;
    using const_iterator = const Char*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // npos is the largest value of size_type, is the max value of size_type
    constexpr static size_type npos = std::numeric_limits<size_type>::max();

   public:
    constexpr explicit basic_small_string([[maybe_unused]] const Allocator& allocator = Allocator()) noexcept
        : buffer_type(allocator) { }

    constexpr basic_small_string(size_type count, Char ch, [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(count, ch);
    }

    // copy constructor
    basic_small_string(const basic_small_string& other)
        : buffer_type(other.get_allocator()) {
        append(other);
    }

    constexpr basic_small_string(const basic_small_string& other, size_type pos,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(other, pos);
    }

    constexpr basic_small_string(basic_small_string&& other,
                                 [[maybe_unused]] const Allocator& allocator = Allocator()) noexcept
        : buffer_type(std::move(other), allocator) {}

    constexpr basic_small_string(basic_small_string&& other, size_type pos,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos)} {}

    constexpr basic_small_string(const basic_small_string& other, size_type pos, size_type count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    constexpr basic_small_string(basic_small_string&& other, size_type pos, size_type count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string{other.substr(pos, count), allocator} {}

    constexpr basic_small_string(const Char* s, size_type count,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(s, count);
    }

    constexpr basic_small_string(const Char* s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(s, std::strlen(s), allocator) {}

    template <class InputIt>
    constexpr basic_small_string(InputIt first, InputIt last, [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(first, last);
    }

    constexpr basic_small_string(std::initializer_list<Char> ilist,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(ilist.begin(), ilist.end());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr basic_small_string(const StringViewLike& s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(s.begin(), s.end());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    constexpr basic_small_string(const StringViewLike& s, size_type pos, size_type n,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        append(s, pos, n);
    }

    basic_small_string(std::nullptr_t) = delete;

    ~basic_small_string() noexcept = default;
    
    // get_allocator
    [[nodiscard]] constexpr auto get_allocator() const -> Allocator { return buffer_type::get_allocator(); }

    // copy assignment
    auto operator=(const basic_small_string& other) -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        // assign the other to this
        assign(other.data(), other.size());
        return *this;
    }

    // move assignment
    auto operator=(basic_small_string&& other) noexcept -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        this->~basic_small_string();
        // call the move constructor
        new (this) basic_small_string(std::move(other));
        return *this;
    }

    auto operator=(const Char* s) -> basic_small_string& {
        clear();
        append(s);
        return *this;
    }

    auto operator=(Char ch) -> basic_small_string& {
        clear();
        append(1, ch);
        return *this;
    }

    auto operator=(std::initializer_list<Char> ilist) -> basic_small_string& {
        clear();
        append(ilist.begin(), ilist.size());
        return *this;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator=(const StringViewLike& s) -> basic_small_string& {
        clear();
        append(s);
        return *this;
    }

    auto operator=(std::nullptr_t) -> basic_small_string& = delete;

    // assign
    auto assign(size_type count, Char ch) -> basic_small_string& {
        clear();
        append(count, ch);
        return *this;
    }

    auto assign(const basic_small_string& str) -> basic_small_string& {
        if (this == &str) [[unlikely]] {
            return *this;
        }
        clear();
        append(str);
        return *this;
    }

    auto assign(const basic_small_string& str, size_type pos, size_type count = npos) -> basic_small_string& {
        auto sub_str = str.substr(pos, count);
        return assign(std::move(sub_str));
    }

    auto assign(basic_small_string&& gone) noexcept -> basic_small_string& {
        if (this == &gone) [[unlikely]] {
            return *this;
        }
        this->~basic_small_string();
        // call the move constructor
        new (this) basic_small_string(std::move(gone));
        return *this;
    }

    auto assign(const Char* s, size_type count) -> basic_small_string& {
        clear();
        append(s, count);
        return *this;
    }

    auto assign(const Char* s) -> basic_small_string& {
        clear();
        append(s);
        return *this;
    }

    template <class InputIt>
    auto assign(InputIt first, InputIt last) -> basic_small_string& {
        clear();
        append(first, last);
        return *this;
    }

    auto assign(std::initializer_list<Char> ilist) -> basic_small_string& {
        clear();
        append(ilist);
        return *this;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto assign(const StringViewLike& s) -> basic_small_string& {
        clear();
        append(s);
        return *this;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    auto assign(const StringViewLike& s, size_type pos, size_type n = npos) -> basic_small_string& {
        clear();
        append(s, pos, n);
        return *this;
    }

    // element access
    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    template <bool Safe = true>
    constexpr auto operator[](size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    template <bool Safe = true>
    constexpr auto operator[](size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    [[nodiscard]] constexpr auto front() noexcept -> reference { return *(Char*)data(); }

    [[nodiscard]] constexpr auto front() const noexcept -> const_reference { return *c_str(); }

    [[nodiscard]] constexpr auto back() noexcept -> reference {
        auto size = buffer_type::size();
        auto buffer = buffer_type::get_buffer();
        return buffer[size - 1];
    }

    [[nodiscard]] constexpr auto back() const noexcept -> const_reference {
        auto [buffer, size] = buffer_type::get_buffer_and_size();
        return buffer[size - 1];
    }

    [[nodiscard, gnu::always_inline]] auto c_str() const noexcept -> const Char* { return buffer_type::get_buffer(); }

    [[nodiscard, gnu::always_inline]] auto data() const noexcept -> const Char* { return c_str(); }

    [[nodiscard, gnu::always_inline]] auto data() noexcept -> Char* { return buffer_type::get_buffer(); }

    // Iterators
    [[nodiscard]] constexpr auto begin() noexcept -> iterator { return data(); }

    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator { return c_str(); }

    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return c_str(); }

    [[nodiscard]] constexpr auto end() noexcept -> iterator {
        auto* buffer = buffer_type::get_buffer();
        auto size = buffer_type::size();
        return buffer + size;
    }

    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        auto* buffer = buffer_type::get_buffer();
        auto size = buffer_type::size();
        return buffer + size;
    }

    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
        auto* buffer = buffer_type::get_buffer();
        auto size = buffer_type::size();
        return buffer + size;
    }

    [[nodiscard]] constexpr auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }

    [[nodiscard]] constexpr auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    [[nodiscard]] constexpr auto rend() noexcept -> reverse_iterator { return reverse_iterator{begin()}; }

    [[nodiscard]] constexpr auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    // capacity
    [[nodiscard, gnu::always_inline]] constexpr auto empty() const noexcept -> bool { return size() == 0; }

    [[nodiscard, gnu::always_inline]] constexpr auto size() const noexcept -> size_type { return buffer_type::size(); }

    [[nodiscard, gnu::always_inline]] constexpr auto length() const noexcept -> size_type {
        return buffer_type::size();
    }

    /**
     * @brief The maximum number of elements that can be stored in the string.
     * the buffer largest size is 1 << 15, the cap occupy 2 bytes, so the max size is 1 << 15 - 2, is 65534
     */
    [[nodiscard, gnu::always_inline]] constexpr auto max_size() const noexcept -> size_type { return kInvalidSize - 1; }

    constexpr auto reserve(size_type new_cap) -> void {
        this->template buffer_reserve<buffer_type::NeedTerminated::Yes>(new_cap);
    }

    // it was a just exported function, should not be called frequently, it was a little bit slow in some case.
    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const noexcept -> size_type {
        return buffer_type::capacity();
    }

    constexpr auto shrink_to_fit() -> void {
        auto [cap, size] = buffer_type::get_capacity_and_size();
        Assert(cap >= size, "cap should always be greater or equal to size");
        auto best_cap = buffer_type::calculate_buffer_size(size);
        if (cap > best_cap) {  // the cap is larger than the best cap, so need to shrink
            auto new_str(*this);
            swap(new_str);
        }
    }

    // modifiers
    /*
     * clear the string, do not erase the memory, just set the size to 0, and set the first char to '\0' if
     * NullTerminated is true
     * @detail : do not use set_size(0), it will be a do some job for some other sizes.
     */
    constexpr auto clear() noexcept -> void {
        buffer_type::set_size(0);
        if constexpr (NullTerminated) {
            data()[0] = '\0';
        }
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, size_type count, Char ch) -> basic_small_string& {
        if (index > size()) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(count);
        }
        auto old_size = size();
        std::memmove(data() + index + count, c_str() + index, old_size - index);
        std::memset(data() + index, ch, count);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[old_size + count] = '\0';
        }
        return *this;
    }

    // just support zero-terminated input string, if you want to insert a string without zero-terminated, use
    // insert(size_type index, const Char* str, size_type count)
    template <bool Safe = true>
    constexpr auto insert(size_type index, const Char* str) -> basic_small_string& {
        return insert<Safe>(index, str, std::strlen(str));
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, const Char* str, size_type count) -> basic_small_string& {
        if (index > size()) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }
        // check if the capacity is enough
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        auto old_size = size();
        std::memmove(data() + index + count, c_str() + index, old_size - index);
        // set the new data
        std::memcpy(data() + index, str, count);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[old_size + count] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& other) -> basic_small_string& {
        return insert<Safe>(index, other.data(), other.size());
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& str, size_type other_index,
                          size_type count = npos) -> basic_small_string& {
        return insert<Safe>(index, str.substr(other_index, count));
    }

    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, Char ch) -> iterator {
        return insert<Safe>(pos, 1, ch);
    }

    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, size_type count, Char ch) -> iterator {
        // check if the capacity is enough
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        std::memmove(const_cast<Char*>(pos) + count, pos,
                     static_cast<size_type>(end() - pos));  // end() - pos is faster than size() - (pos - begin())
        // set the new data
        std::memset(const_cast<Char*>(pos), ch, count);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return const_cast<iterator>(pos);
    }

    template <class InputIt, bool Safe = true>
    constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        // static check the type of the input iterator
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        // calculate the count
        size_type count = std::distance(first, last);
        size_type index = pos - begin();
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(count);
        }
        // by now, the capacity is enough
        // move the data to the new position
        std::memmove(data() + index + count, data() + index, static_cast<size_type>(size() - index));
        // copy the new data
        std::copy(first, last, data() + index);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return const_cast<iterator>(pos);
    }

    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, std::initializer_list<Char> ilist) -> iterator {
        // the template function do not support partial specialization, so use the decltype to get the type of the
        // ilist.begin()
        return insert<decltype(ilist.begin()), Safe>(pos, ilist.begin(), ilist.end());
    }

    template <class StringViewLike, bool Safe = true>
    constexpr auto insert(const_iterator pos, const StringViewLike& t) -> iterator {
        return insert<Safe>(pos, t.data(), t.size());
    }

    template <class StringViewLike, bool Safe = true>
    constexpr auto insert(const_iterator pos, const StringViewLike& t, size_type pos2,
                          size_type count = npos) -> iterator {
        return insert<Safe>(pos, t.substr(pos2, count));
    }

    // erase the data from the index to the end
    // NOTICE: the erase function will never change the capacity of the string or the external flag
    auto erase(size_type index = 0, size_type count = npos) -> basic_small_string& {
        auto old_size = this->size();
        if (index > old_size) [[unlikely]] {
            throw std::out_of_range("erase: index is out of range");
        }
        // calc the real count
        size_type real_count = std::min(count, old_size - index);
        // if the count is greater than 0, then move right part of  the data to the new position
        if (real_count > 0) {
            // memmove the data to the new position
            std::memmove(data() + index, c_str() + index + real_count, old_size - index - real_count);
        }
        // set the new size
        buffer_type::set_size(old_size - real_count);
        if constexpr (NullTerminated) {
            data()[old_size - real_count] = '\0';
        }
        return *this;
    }

    constexpr auto erase(const_iterator first) -> iterator {
        // the first must be valid, and of the string.
        auto index = first - begin();
        return erase(index, 1).begin() + index;
    }

    constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
        auto index = first - begin();
        return erase(index, last - first).begin() + index;
    }

    template <bool Safe = true>
    [[gnu::always_inline]] inline void push_back(Char c) {
        append<Safe>(1, c);
    }

    void pop_back() {
        buffer_type::decrease_size(1);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
    }
    template <bool Safe = true>
    constexpr auto append(size_type count, Char c) -> basic_small_string& {
        if constexpr (Safe) {
            buffer_type::allocate_more(count);
        }
        // by now, the capacity is enough
        std::memset(data() + size(), c, count);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other) -> basic_small_string& {
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(other.size());
        }
        // by now, the capacity is enough
        // size() function maybe slower than while the size is larger than 4k, so store it.
        auto other_size = other.size();
        std::memcpy(end(), other.c_str(), other_size);
        buffer_type::increase_size(other_size);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other, size_type pos,
                          size_type count = npos) -> basic_small_string& {
        if (count == npos) {
            count = other.size() - pos;
        }
        return append<Safe>(other.substr(pos, count));
    }

    template <bool Safe = true>
    constexpr auto append(const Char* s, size_type count) -> basic_small_string& {
        if constexpr (Safe) {
            buffer_type::allocate_more(count);
        }

        std::memcpy(data() + size(), s, count);
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const Char* s) -> basic_small_string& {
        return append<Safe>(s, std::strlen(s));
    }

    template <class InputIt, bool Safe = true>
    constexpr auto append(InputIt first, InputIt last) -> basic_small_string& {
        // first, and last is the range of the input string or string_view or vector
        // calculate the count

        // static check the type of the input iterator
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        size_type count = std::distance(first, last);
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::NeedTerminated::No>(count);
        }
        std::copy(first, last, data() + size());
        buffer_type::increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist.begin(), ilist.size());
    }

    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto append(const StringViewLike& t) -> basic_small_string& {
        return append<Safe>(t.data(), t.size());
    }

    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto append(const StringViewLike& s, size_type pos, size_type count = npos) -> basic_small_string& {
        if (count == npos) {
            count = s.size() - pos;
        }
        return append<Safe>(s.substr(pos, count));
    }

    template <bool Safe = true>
    auto operator+=(const basic_small_string& other) -> basic_small_string& {
        return append<Safe>(other);
    }

    template <bool Safe = true>
    auto operator+=(Char ch) -> basic_small_string& {
        push_back<Safe>(ch);
        return *this;
    }

    template <bool Safe = true>
    auto operator+=(const Char* s) -> basic_small_string& {
        return append<Safe>(s);
    }

    template <bool Safe = true>
    auto operator+=(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist);
    }

    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator+=(const StringViewLike& t) -> basic_small_string& {
        return append<Safe>(t);
    }

    // replace [pos, pos+count] with count2 times of ch
    template <bool Safe = true>
    auto replace(size_type pos, size_type count, size_type count2, Char ch) -> basic_small_string& {
        // check the pos is not out of range
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        // then check if pos+count is the out of range
        if (count >= size() or pos + count >= size()) { // to avoid count overflow from size_type
            // no right part to move, just append the new string
            append<Safe>(count2, ch);
            return *this;
        }

        if (count2 == 0) [[unlikely]] {
            // just delete the right part
            return erase(pos, count);
        }
        // else, there is right part, maybe need to move
        // check the size, if the pos + count is greater than the cap, we need to reserve
        uint64_t new_size = size() - count + count2;  // to avoid overflow from size_type
        if (new_size > max_size()) [[unlikely]] {
            throw std::length_error("the new capacity is too large");
        }
        // to make sure the capacity is enough
        if (new_size > capacity()) {
            this->template buffer_reserve<buffer_type::NeedTerminated::No>(new_size);
        }
        // by now, the capacity is enough
        // check if need do some memmove
        if (count != count2) {
            // move the right part to the new position, and the size will not be zero

            // the memmove will handle the overlap automatically
            std::memmove(data() + pos + count2, c_str() + pos + count, size() - pos - count);
        }
        // by now, the buffer is ready
        std::memset(data() + pos, ch, count2);
        buffer_type::set_size(new_size);
        if constexpr (NullTerminated) {
            data()[new_size] = '\0';
        }
        return *this;
    }

    // replace [pos, pos+count] with the range [cstr, cstr + count2]
    template <bool Safe = true>
    auto replace(size_type pos, size_type count, const Char* cstr, size_type count2) -> basic_small_string& {
        
        // check the pos is not out of range
        auto old_size = size();
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        if (count2 == 0) [[unlikely]] {
            // just delete the right part
            return erase(pos, count);
        }

        if (pos + count >= old_size) {
            // no right part to move
            uint64_t new_size = pos + count2;  // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            // just append the new string
            append<Safe>(cstr, count2);
            return *this;
        }
        // else, there is right part, maybe need to move
        // check the size, if the pos + count is greater than the cap, we need to reserve
        uint64_t new_size = old_size - count + count2;  // to avoid overflow from size_type
        if (new_size > max_size()) [[unlikely]] {
            throw std::length_error("the new capacity is too large");
        }
        auto cap = capacity();
        if (new_size > cap) {
            // because count2 is not 0, so 
            this->template buffer_reserve<buffer_type::NeedTerminated::No>(new_size);
        }
        // by now, the capacity is enough
        // check if need do some memmove
        if (count != count2) {
            // move the right part to the new position, and the size will not be zero
            std::memmove(data() + pos + count2, c_str() + pos + count, old_size - pos - count);
            // the memmove will handle the overlap automatically
        }
        // by now, the buffer is ready
        std::memcpy(data() + pos, cstr, count2);
        buffer_type::set_size(new_size);
        if constexpr (NullTerminated) {
            data()[new_size] = '\0';
        }
        return *this;
    }

    auto replace(size_type pos, size_type count, const basic_small_string& other) -> basic_small_string& {
        return replace(pos, count, other.c_str(), other.size());
    }

    auto replace(const_iterator first, const_iterator last, const basic_small_string& other) -> basic_small_string& {
        return replace(first - begin(), last - first, other.c_str(), other.size());
    }

    auto replace(size_type pos, size_type count, const basic_small_string& other, size_type pos2,
                 size_type count2 = npos) -> basic_small_string& {
        if (count2 == npos) {
            count2 = other.size() - pos2;
        }
        return replace(pos, count, other.substr(pos2, count2));
    }

    auto replace(const_iterator first, const_iterator last, const Char* cstr, size_type count2) -> basic_small_string& {
        return replace(first - begin(), last - first, cstr, count2);
    }

    auto replace(size_type pos, size_type count, const Char* cstr) -> basic_small_string& {
        return replace(pos, count, cstr, traits_type::length(cstr));
    }

    auto replace(const_iterator first, const_iterator last, const Char* cstr) -> basic_small_string& {
        return replace(first - begin(), last - first, cstr, traits_type::length(cstr));
    }

    auto replace(const_iterator first, const_iterator last, size_type count, Char ch) -> basic_small_string& {
        return replace(first - begin(), last - first, count, ch);
    }

    template <class InputIt>
    auto replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2) -> basic_small_string& {
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        Assert(first2 <= last2, "the range is valid");
        auto count = std::distance(first, last);
        if (count < 0) [[unlikely]] {
            throw std::invalid_argument("the range is invalid");
        }
        auto count2 = std::distance(first2, last2);
        if (last == end()) {
            // no right part to move, just append the new string
            append(first2, last2);
            return *this;
        }

        if (count2 == 0) [[unlikely]] {
            // just delete the right part
            return erase(begin(), first);
        }
        // else, there is right part, maybe need to move
        uint64_t new_size = size() - count + count2;  // to avoid overflow from size_type
        if (new_size > max_size()) [[unlikely]] {
            throw std::length_error("the new capacity is too large");
        }
        if (new_size > capacity()) {
            reserve(new_size);
        }
        // by now, the capacity is enough
        // check if need do some memmove
        if (count != count2) {
            // move the right part to the new position, and the size will not be zero

            // move the [last, end] -> [first + count2, end() - last + count2]
            // the memmove will handle the overlap automatically
            std::memmove(first + count2, last, end() - last);
        }
        // the buffer is ready
        std::copy(first2, last2, first);
        buffer_type::set_size(new_size);
        if constexpr (NullTerminated) {
            data()[new_size] = '\0';
        }
        return *this;
    }

    auto replace(const_iterator first, const_iterator last, std::initializer_list<Char> ilist) -> basic_small_string& {
        return replace(first, last, ilist.begin(), ilist.end());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(const_iterator first, const_iterator last, const StringViewLike& view) -> basic_small_string& {
        return replace(first, last, view.data(), view.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(size_type pos, size_type count, const StringViewLike& view, size_type pos2,
                 size_type count2 = npos) -> basic_small_string& {
        if (count2 == npos) {
            count2 = view.size() - pos2;
        }
        return replace(pos, count, view.data() + pos, count2);
    }

    auto copy(Char* dest, size_type count = npos, size_type pos = 0) const -> size_type {
        if (count == npos) {
            count = size() - pos;
        }
        std::memcpy(dest, c_str() + pos, count);
        return count;
    }

    auto resize(size_type count) -> void {
        fmt::print("resize: count = {}, size = {}, capacity = {}\n", count, size(), capacity());
        if (count <= size()) {
            buffer_type::set_size(count);
            if constexpr (NullTerminated) {
                data()[count] = '\0';
            }
            return;
        }
        if (count > capacity()) {
            reserve(count);
        }
        std::memset(data() + size(), '\0', count - size());
        buffer_type::set_size(count);
        if constexpr (NullTerminated) {
            data()[count] = '\0';
        }
        return;
    }

    auto resize(size_type count, Char ch) -> void {
        // if the count is less than the size, just set the size
        if (count <= size()) {
            buffer_type::set_size(count);
            if constexpr (NullTerminated) {
                data()[count] = '\0';
            }
            return;
        }
        if (count > capacity()) {
            // if the count is greater than the size, need to reserve
            reserve(count);
        }
        // by now, the capacity is enough
        std::memset(data() + size(), ch, count - size());
        buffer_type::set_size(count);
        if constexpr (NullTerminated) {
            data()[count] = '\0';
        }
        return;
    }

    void swap(basic_small_string& other) noexcept {
        // just a temp variable to avoid self-assignment, std::swap will do the same thing, maybe faster?
        buffer_type::swap(other);
    }

    // search
    constexpr auto find(const Char* needle, size_type pos, size_type other_size) const -> size_type {
        auto const size = this->size();
        if (pos + other_size > size || other_size + pos < pos) {
            // check overflow
            return npos;
        }
        if (other_size == 0) {
            return pos;
        }

        // Don't use std::search, use a Boyer-Moore-like trick by comparing
        // the last characters first
        auto const haystack = data();
        auto const nsize_1 = other_size - 1;
        auto const lastNeedle = needle[nsize_1];

        // Boyer-Moore skip value for the last char in the needle. Zero is
        // not a valid value; skip will be computed the first time it's
        // needed.
        size_type skip = 0;
        const Char* i = haystack + pos;
        auto iEnd = haystack + size - nsize_1;

        while (i < iEnd) {
            // Boyer-Moore: match the last element in the needle
            while (i[nsize_1] != lastNeedle) {
                if (++i == iEnd) {
                    return npos;
                }
            }
            // Here we know that the last char matches
            // Continue in pedestrian mode
            for (size_type j = 0;;) {
                Assert(j < other_size, "find index can not overflow the size");
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
                if (++j == other_size) {
                    // Yay
                    return size_type(i - haystack);
                }
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find(const Char* needle, size_type pos = 0) const -> size_type {
        return find(needle, pos, traits_type::length(needle));
    }

    [[nodiscard]] constexpr auto find(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return find(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find(Char ch, size_type pos = 0) const -> size_type {
        auto* found = traits_type::find(c_str() + pos, size() - pos, ch);
        return found == nullptr ? npos : size_type(found - c_str());
    }

    [[nodiscard]] constexpr auto rfind(const Char* needle, size_type pos, size_type other_size) const -> size_type {
        if (other_size > size()) [[unlikely]] {
            return npos;
        }
        pos = std::min(pos, size() - other_size);
        if (other_size == 0) [[unlikely]] {
            return pos;
        }
        const_iterator i{begin() + pos};
        for (;; --i) {
            if (traits_type::eq(*i, *needle) && traits_type::compare(&*i, needle, other_size) == 0) {
                return size_type(i - begin());
            }
            if (i == begin()) {
                break;
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto rfind(const Char* needle, size_type pos = 0) const -> size_type {
        return rfind(needle, pos, traits_type::length(needle));
    }

    [[nodiscard]] constexpr auto rfind(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return rfind(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto rfind(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto rfind(Char ch, size_type pos = npos) const -> size_type {
        auto current_size = size();
        if (current_size == 0) [[unlikely]]
            return npos;
        pos = std::min(pos, current_size - 1);
        while (pos > 0) {
            if (traits_type::eq(at(pos), ch)) {
                return pos;
            }
            --pos;
        }
        // pos == 0
        return traits_type::eq(at(0), ch) ? 0 : npos;
    }

    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_type pos, size_type count) const -> size_type {
        return find(str, pos, count);
    }

    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_type pos = 0) const -> size_type {
        return find(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_first_of(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return find(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_of(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_first_of(Char ch, size_type pos = 0) const -> size_type { return find(ch, pos); }

    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_type pos, size_type count) const -> size_type {
        if (pos < size()) {
            const_iterator i(begin() + pos);
            const_iterator finish(end());
            for (; i != finish; ++i) {
                if (traits_type::find(str, count, *i) == nullptr) {
                    return size_type(i - begin());
                }
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_type pos = 0) const -> size_type {
        return find_first_not_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_first_not_of(const basic_small_string& other,
                                                   size_type pos = 0) const -> size_type {
        return find_first_not_of(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_not_of(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find_first_not_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_first_not_of(Char ch, size_type pos = 0) const -> size_type {
        return find_first_not_of(&ch, pos, 1);
    }

    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_type pos, size_type count) const -> size_type {
        return rfind(str, pos, count);
    }

    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_type pos = npos) const -> size_type {
        return rfind(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_last_of(const basic_small_string& other,
                                              size_type pos = npos) const -> size_type {
        return rfind(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_last_of(const StringViewLike& view, size_type pos = npos) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_last_of(Char ch, size_type pos = npos) const -> size_type {
        return rfind(ch, pos);
    }

    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_type pos, size_type count) const -> size_type {
        if (not empty()) [[likely]] {
            pos = std::min(pos, size() - 1);
            const_iterator i(begin() + pos);
            for (;; --i) {
                if (traits_type::find(str, count, *i) == nullptr) {
                    return size_type(i - begin());
                }
                if (i == begin()) {
                    break;
                }
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_type pos = npos) const -> size_type {
        return find_last_not_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_last_not_of(const basic_small_string& other,
                                                  size_type pos = npos) const -> size_type {
        return find_last_not_of(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_last_not_of(const StringViewLike& view, size_type pos = npos) const -> size_type {
        return find_last_not_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_last_not_of(Char ch, size_type pos = npos) const -> size_type {
        return find_last_not_of(&ch, pos, 1);
    }

    // Operations
    [[nodiscard]] constexpr auto compare(const basic_small_string& other) const noexcept -> int {
        auto this_size = size();
        auto other_size = other.size();
        auto r = traits_type::compare(c_str(), other.c_str(), std::min(this_size, other_size));
        return r != 0 ? r : this_size > other_size ? 1 : this_size < other_size ? -1 : 0;
    }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const basic_small_string& other) const -> int {
        return compare(pos, count, other.c_str(), other.size());
    }
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const basic_small_string& other,
                                         size_type pos2, size_type count2 = npos) const -> int {
        if (pos2 > other.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min(count2, other.size() - pos2);
        return compare(pos1, count1, other.c_str() + pos2, count2);
    }
    [[nodiscard]] constexpr auto compare(const Char* str) const noexcept -> int {
        return compare(0, size(), str, traits_type::length(str));
    }
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const Char* str) const -> int {
        return compare(pos, count, str, traits_type::length(str));
    }
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const Char* str,
                                         size_type count2) const -> int {
        // make sure the pos1 is valid
        if (pos1 > size()) [[unlikely]] {
            throw std::out_of_range("compare: pos1 is out of range");
        }
        // make sure the count1 is valid, if count1 > size() - pos1, set count1 = size() - pos1
        count1 = std::min(count1, size() - pos1);
        auto r = traits_type::compare(c_str() + pos1, str, std::min(count1, count2));
        return r != 0 ? r : count1 > count2 ? 1 : count1 < count2 ? -1 : 0;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(const StringViewLike& view) const noexcept -> int {
        auto this_size = size();
        auto view_size = view.size();
        auto r = traits_type::compare(c_str(), view.data(), std::min(this_size, view_size));
        return r != 0 ? r : this_size > view_size ? 1 : this_size < view_size ? -1 : 0;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(size_type pos, size_type count, const StringViewLike& view) const -> int {
        return compare(pos, count, view.data(), view.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(size_type pos1, size_type count1, const StringViewLike& view, size_type pos2,
                                         size_type count2 = npos) const -> int {
        if (pos2 > view.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min(count2, view.size() - pos2);
        return compare(pos1, count1, view.data() + pos2, count2);
    }

    [[nodiscard]] constexpr auto starts_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(0, view.size(), view) == 0;
    }

    [[nodiscard]] constexpr auto starts_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(front(), ch);
    }

    [[nodiscard]] constexpr auto starts_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(0, len, str) == 0;
    }

    [[nodiscard]] constexpr auto ends_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(size() - view.size(), view.size(), view) == 0;
    }

    [[nodiscard]] constexpr auto ends_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(back(), ch);
    }

    [[nodiscard]] constexpr auto ends_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(size() - len, len, str) == 0;
    }

    [[nodiscard]] constexpr auto contains(std::basic_string_view<Char> view) const noexcept -> bool {
        return find(view) != npos;
    }

    [[nodiscard]] constexpr auto contains(Char ch) const noexcept -> bool { return find(ch) != npos; }

    [[nodiscard]] constexpr auto contains(const Char* str) const noexcept -> bool { return find(str) != npos; }

    [[nodiscard]] constexpr auto substr(size_type pos = 0, size_type count = npos) const& -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }

        return basic_small_string{data() + pos, std::min(count, current_size - pos), get_allocator()};
    }

    [[nodiscard]] constexpr auto substr(size_type pos = 0, size_type count = npos) && -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }
        // fmt::print("erase with pos: {}, current size : {} \n", pos, current_size);
        erase(0, pos);
        if (count < current_size) {
            resize(count);
        }
        return std::move(*this);
    }

    // convert to std::basic_string_view, to support C++11 compatibility. and it's noexcept.
    // and small_string can be converted to std::basic_string_view implicity, so third party String can be converted from small_string.
    operator std::basic_string_view<Char, Traits>() const noexcept {
        return std::basic_string_view<Char, Traits>(data(), size());
    }
};  // class basic_small_string

// input/output
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<<(std::basic_ostream<Char, Traits>& os,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& str)
  -> std::basic_ostream<Char, Traits>& {
    return os.write(str.data(), str.size());
}

template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>>(std::basic_istream<Char, Traits>& is,
                       basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& str)
  -> std::basic_istream<Char, Traits>& {
    using _istream_type = std::basic_istream<
      typename basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>::value_type,
      typename basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>::traits_type>;
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
            if (got == Traits::eof()) {
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

// operator +
// 1
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 2
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const Char* rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 3
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      Char rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.push_back(rhs);
    return result;
}

// 5
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const Char* lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) + rhs;
}

// 6
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(Char lhs, const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) + rhs;
}

// 8
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> result(std::move(lhs));
    result.append(std::move(rhs));
    return result;
}

// 9
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 10
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& lhs,
                      const Char* rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 11
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& lhs,
                      Char rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = std::move(lhs);
    result.push_back(rhs);
    return result;
}

// 13
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.append(std::move(rhs));
    return result;
}

// 14
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(const Char* lhs, basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) + std::move(rhs);
}

// 15
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator+(Char lhs, basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) + std::move(rhs);
}

// comparison operators
// small_string <=> small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> std::strong_ordering {
    return lhs.compare(rhs) <=> 0;
}

// small_string == small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return lhs.size() == rhs.size() and lhs.compare(rhs) == 0;
}

// small_string != small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return not(lhs == rhs);
}

// small_string > small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return lhs.compare(rhs) > 0;
}

// small_string < small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return lhs.compare(rhs) < 0;
}

// small_string >= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return not(lhs < rhs);
}

// small_string <= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return not(lhs > rhs);
}

// basic_string compatibility routines
// small_string <=> basic_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                        const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

// basic_string <=> small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<=>(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

// small_string <=> Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs, Traits::length(rhs)) <=> 0;
}

// Char* <=> small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=>(const Char* lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> std::strong_ordering {
    auto r = Traits::compare(lhs, rhs.data(), std::min(Traits::length(lhs), rhs.size()));
    return r != 0 ? r <=> 0 : rhs.size() <=> Traits::length(lhs);
}

// small_string == basic_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

// basic_string == small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator==(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs == lhs;
}

// small_string == Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator==(const Char* lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs.size() == Traits::length(lhs) and rhs.compare(lhs) == 0;
}

// small_string == Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return rhs == lhs;
}

// small_string != basic_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// basic_string != small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator!=(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs == rhs);
}

// small_string != Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// Char* != small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator!=(const Char* lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs == rhs);
}

// small_string < basic_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// basic_string < small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs.compare(lhs) > 0;
}

// small_string < Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                      const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// Char* < small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<(const Char* lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs.compare(lhs) > 0;
}

// basic_string > small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator>(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs.compare(lhs) < 0;
}

// Char* > small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>(const Char* lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return rhs.compare(lhs) < 0;
}

// small_string > Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

// small_string <= basic_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// basic_string <= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator<=(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs > rhs);
}

// small_string <= Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// Char* <= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=(const Char* lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs > rhs);
}

// basic_string >= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// basic_string >= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated>
inline auto operator>=(const std::basic_string<Char, Traits, STDAllocator>& lhs,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs < rhs);
}

// small_string >= Char*
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// Char* >= small_string
template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator>=(const Char* lhs,
                        const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& rhs) noexcept
  -> bool {
    return !(lhs < rhs);
}

// to std::string


using small_string = basic_small_string<char>;
using small_byte_string =
  basic_small_string<char, small_string_buffer, malloc_core, std::char_traits<char>, std::allocator<char>, false>;

static_assert(sizeof(small_string) == 8, "small_string should be same as a pointer");

}  // namespace stdb::memory

// decl the formatter of small_string
namespace fmt {

template <typename Char, template <typename, template <class, bool> class, class T, class A, bool N> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
struct formatter<stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>>
    : formatter<string_view>
{
    using formatter<fmt::string_view>::parse;

    template <typename Context>
    auto format(const stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& str,
                Context& ctx) const noexcept {
        return formatter<string_view>::format({str.data(), str.size()}, ctx);
    }
};
}  // namespace fmt

namespace stdb::memory::pmr {
using small_string = basic_small_string<char, small_string_buffer, pmr_core,
                                        std::char_traits<char>, std::pmr::polymorphic_allocator<char>, true>;
using small_byte_string = basic_small_string<char, small_string_buffer, pmr_core,
                                             std::char_traits<char>, std::allocator<char>, false>;


static_assert(sizeof(small_string) == 16, "small_string should be same as a pointer");

}  // namespace stdb::memory::pmr