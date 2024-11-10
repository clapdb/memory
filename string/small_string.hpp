

#pragma once
#include <fmt/core.h>
#include <sys/types.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
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

// if NullTerminated is true, the string will be null terminated, and the size will be the length of the string
// if NullTerminated is false, the string will not be null terminated, and the size will still be the length of the
// string
template <typename Char, class Traits = std::char_traits<Char>, class Allocator = std::allocator<Char>,
          bool NullTerminated = true>
class basic_small_string
{
   public:
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

   private:
    struct internal_core
    {
        Char data[7] = {};
        // 0~7
        uint8_t internal_size : 4 = 0;
        // 0000 , ths higher 4 bits is 0, means is_internal is 0
        uint8_t is_internal : 4 = 0;
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
    };

    static_assert(sizeof(internal_core) == 8);

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
        };

        struct size_and_times
        {
            uint16_t external_size : 12;
            uint8_t times : 2;  // 0, 1, 2, if times == 3, the size is 4096
            uint8_t flag : 2;   // the flag must be 01
        };

        struct idle_and_flag
        {
            uint16_t idle : 14;
            uint8_t flag : 2;  // must be 11
        };

        union
        {
            size_and_shift size_shift;
            size_and_times size_times;
            idle_and_flag idle_flag;
        };

        [[nodiscard]] auto capacity_fast() const noexcept -> size_type {
            Assert(not check_if_internal(*this), "the external must be external");
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
            Assert(idle_flag.flag == kIsDelta, "the flag is must be kIsDelta");
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
            Assert(not check_if_internal(*this), "the external must be external");
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

        [[nodiscard]] auto size() const noexcept -> size_type {
            auto size = size_fast();
            // if size is KInvalidSize, get the true size from the buffer head
            return size != kInvalidSize ? size : *((size_type*)c_str_ptr - 1);
        }

        // set size will be some slow, do not call it frequently
        auto set_size(size_type new_size) noexcept -> void {
            // fmt::print("set_size: new_size: {}, old_size: {}, capacity: {}\n", new_size, size(), capacity());
            Assert(not check_if_internal(*this), "the external must be external");
            if (idle_flag.flag != kIsDelta) [[likely]] {
                // just set the external_size or the times
                Assert(new_size <= capacity_fast(), "the new_size is must be less or equal to the 4k");
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
            // fmt::print("increase_size: delta: {}, old_size: {}, capacity: {}\n", delta, size(), capacity());
            Assert(not check_if_internal(*this), "the external must be external");
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
            // do not check in release mode, because the caller
            Assert(delta <= size(), "the delta is must be less or equal to the size");
            Assert(not check_if_internal(*this), "the external must be external");
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
            Assert(not check_if_internal(*this), "the external must be external");
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

        [[gnu::always_inline]] void deallocate() noexcept {
            Assert(not check_if_internal(*this), "the external must be external");
            // check the ptr is pointed to a buffer or the after pos of the buffer head
            if (idle_flag.flag != kIsDelta) [[likely]] {
                // is not the large buffer
                std::free((Char*)(c_str_ptr));
            } else {
                // the ptr is pointed to the after pos of the buffer head
                std::free((Char*)(c_str_ptr)-2 * sizeof(size_type));
            }
        }

        [[gnu::always_inline, nodiscard]] auto c_str() const noexcept -> Char* {
            Assert(not check_if_internal(*this), "the external must be external");
            return (Char*)(c_str_ptr);
        }
    };

    // set the buf_size and str_size to the right place
    inline static auto allocate_new_external_buffer(size_type new_buffer_size,
                                                    size_type old_str_size) noexcept -> external_core {
        // make sure the old_str_size <= new_buffer_size
        if constexpr (not NullTerminated) {
            Assert(old_str_size <= new_buffer_size,
                   "if not NullTerminated,  new_str_size should be not greater than new_buffer_size");
        } else {
            if (old_str_size >= new_buffer_size) {
                fmt::print("old_str_size: {}, new_buffer_size: {}\n", old_str_size, new_buffer_size);
            }
            Assert(old_str_size < new_buffer_size,
                   "if NullTerminated str,  new_str_size should be less than new_buffer_size");
        }
        Assert(new_buffer_size != kInvalidSize, "new_buffer_size should not be kInvalidSize");
        if (new_buffer_size <= kMaxSmallStringSize) [[likely]] {
            Assert(is_power_of_2(new_buffer_size), "new_buffer_size should be a power of 2");
            // set the size_shift
            return {
              .c_str_ptr = reinterpret_cast<int64_t>(std::malloc(new_buffer_size)),
              .size_shift = {.external_size = static_cast<uint16_t>(old_str_size),
                             .shift = static_cast<uint8_t>(std::countr_zero(new_buffer_size) -
                                                           3),  // the shift will not be 000, this is internal_core
                             .flag = kIsShift}};
        } else if (new_buffer_size <= kMaxMediumStringSize) [[likely]] {
            Assert((new_buffer_size % kMaxSmallStringSize) == 0, "the medium new_buffer_size is not aligned to 1024");
            auto times = static_cast<uint8_t>((new_buffer_size >> 10U) - 2U);  // 0: 2048, 1: 3072, 2: 4096
            if constexpr (not NullTerminated) {
                return {.c_str_ptr = reinterpret_cast<int64_t>(std::malloc(new_buffer_size)),
                        .size_times = {.external_size = static_cast<uint16_t>(old_str_size),
                                       .times = static_cast<uint8_t>(times + uint8_t(bool(old_str_size == 4096))),
                                       .flag = kIsTimes}};
            } else {
                return {.c_str_ptr = reinterpret_cast<int64_t>(std::malloc(new_buffer_size)),
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
        auto* buf = std::malloc(new_buffer_size);
        auto* head = reinterpret_cast<size_type*>(buf);
        *head = new_buffer_size;
        *(head + 1) = old_str_size;
        // the ptr point to the 8 bytes after the head
        return {.c_str_ptr = reinterpret_cast<int64_t>(head + 2), .idle_flag = {.idle = idle, .flag = kIsDelta}};
    }

    [[nodiscard, gnu::always_inline]] static inline auto check_if_internal(const external_core& old_external) noexcept
      -> bool {
        return ((internal_core&)old_external).is_internal == 0;
    }

    [[nodiscard, gnu::always_inline]] static inline auto check_if_internal(const internal_core& old_internal) noexcept
      -> bool {
        return old_internal.is_internal == 0;
    }

    static inline auto print_detail_of_external(const external_core& external) -> void {
        if (check_if_internal(external)) {
            fmt::print("detail: is a internal ---- internal: size: {}, capacity: {}\n",
                       ((internal_core&)external).size(), ((const internal_core&)external).capacity());
            return;
        }
        if (external.size_shift.flag == kIsShift) {
            fmt::print("detail: is a shift external ---- external: size: {}, capacity: {}, shift: {}\n",
                       external.size_shift.external_size, external.capacity_fast(), external.size_shift.shift);
        }
        if (external.size_times.flag == kIsTimes) {
            fmt::print("detail: is a times external ---- external: size: {}, capacity: {}, times: {}\n",
                       external.size_times.external_size, external.capacity_fast(), external.size_times.times);
        }
        fmt::print("detail: is a delta external ---- external: size: {}, capacity: {}, delta: {}\n", external.size(),
                   external.capacity(), external.delta_flag.idle);
    }

    // this function handle append / push_back / operator +='s internal reallocation
    // this funciion will not change the size, but the capacity or delta
    static inline auto allocate_new_external_buffer_if_need_from_delta(external_core& old_external,
                                                                       size_type new_append_size) noexcept -> void {
        size_type old_delta_fast = check_if_internal(old_external) ? ((internal_core&)old_external).idle_capacity()
                                                                   : old_external.idle_capacity_fast();
        // print_detail_of_external(old_external);
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
            auto* cap = (size_type*)(old_external.c_str_ptr) - 2;
            auto* size = (size_type*)(old_external.c_str_ptr) - 1;
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
        if (check_if_internal(old_external)) {
            auto old_internal = (internal_core&)old_external;
            auto new_buffer_size = calculate_new_buffer_size(old_internal.size() + new_append_size);
            auto new_external = allocate_new_external_buffer(new_buffer_size, old_internal.size());
            // copy the old data to the new buffer
            std::memcpy(new_external.c_str(), old_internal.data, old_internal.size());
            if constexpr (NullTerminated) {
                new_external.c_str()[old_internal.size()] = '\0';
            }
            old_external = new_external;
            // print_detail_of_external(old_external);
            return;
        }
        // else is external
        // copy the old data to the new buffer
        auto new_buffer_size = calculate_new_buffer_size(old_external.size() + new_append_size);
        auto new_external = allocate_new_external_buffer(new_buffer_size, old_external.size());
        // print_detail_of_external(new_external);
        std::memcpy(new_external.c_str(), old_external.c_str(), old_external.size());
        if constexpr (NullTerminated) {
            new_external.c_str()[old_external.size()] = '\0';
        }
        // replace the old external with the new one
        old_external.deallocate();
        old_external = new_external;
        return;
    }

    static_assert(sizeof(external_core) == 8);

    union
    {
        internal_core internal;
        external_core external;
    };

    [[nodiscard]] auto is_external() const noexcept -> bool { return internal.is_internal != 0; }

    [[nodiscard]] auto buffer_size() const noexcept -> size_type {
        return is_external() ? external.buffer_size() : internal.buffer_size();
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> size_type {
        return is_external() ? external.idle_capacity() : internal.idle_capacity();
    }

    [[nodiscard]] auto get_capacity_and_size() const noexcept -> std::pair<size_type, size_type> {
        if (not is_external()) {
            return {internal_core::capacity(), internal.size()};
        }
        if (external.idle_flag.flag != kIsDelta) {
            return {external.capacity_fast(), external.size_fast()};
        }
        auto* buf = (size_type*)external.c_str_ptr;
        return {*(buf - 2), *(buf - 1)};
    }

    // increase the size, and won't change the capacity, so the internal/exteral'type or ptr will not change
    // you should always call the allocate_new_external_buffer first, then call this function
    inline void increase_size(size_type delta) noexcept {
        if (is_external()) [[likely]] {
            external.increase_size(delta);
        } else {
            internal.internal_size += delta;
        }
    }

    inline void set_size(size_type new_size) noexcept {
        if (is_external()) [[likely]] {
            external.set_size(new_size);
        } else {
            internal.set_size(new_size);
        }
    }

    constexpr static inline auto calc_new_buf_size_from_any_size(uint32_t size) noexcept -> uint32_t {
        if (size < internal_core::capacity()) {
            return internal_core::capacity();
        }
        return calculate_new_buffer_size(size);
    }

    constexpr inline auto check_is_internal_ptr(const Char* ptr) noexcept -> bool {
        return ptr >= c_str() and ptr < (c_str() + size());
    }

   public:
    // Member functions
    // no new or malloc needed, just noexcept
    constexpr basic_small_string() noexcept
        : internal{} {}  // the is_external is false, the size is 0, and the capacity is 7

    constexpr explicit basic_small_string([[maybe_unused]] const Allocator& /*unused*/) noexcept : internal{} {}

    constexpr basic_small_string(size_type count, Char ch, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        // the best way to initialize the string is to append the char count times
        append(count, ch);
    }

    // copy constructor
    basic_small_string(const basic_small_string& other, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) {
        if (not other.is_external()) {
            // is a internal string, just copy the internal part
            internal = other.internal;
            return;
        }
        // is a external string, check if the size is larger than the internal capacity
        auto other_size = other.size();
        if (other_size >= internal_core::capacity()) [[likely]] {
            // by now, need external buffer, allocate a new buffer
            auto new_buffer_size = calculate_new_buffer_size(other_size);
            external = allocate_new_external_buffer(new_buffer_size, other_size);
            // copy the data from the other
            std::memcpy(external.c_str(), other.c_str(), other_size);
            if constexpr (NullTerminated) {
                external.c_str()[other_size] = '\0';
            }
        } else {
            // the size is smaller than the internal capacity, do not need to allocate a new buffer
            // set the size first
            internal.set_size(other_size);
            // copy the data from the other
            std::memcpy(internal.data, other.c_str(), other_size);
            if constexpr (NullTerminated) {
                internal.data[other_size] = '\0';
            }
        }
    }

    constexpr basic_small_string(const basic_small_string& other, size_type pos,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) {
        auto new_size = other.size() - pos;
        if (new_size <= internal_core::capacity()) {
            // the size is smaller than the internal capacity, do not need to allocate a new buffer
            internal.set_size(new_size);
            std::memcpy(internal.data, other.c_str() + pos, new_size);
            if constexpr (NullTerminated) {
                internal.data[new_size] = '\0';
            }
        } else {
            // by now, need external buffer, allocate a new buffer
            external = allocate_new_external_buffer<Char>(calculate_new_buffer_size(new_size), new_size);
            // copy the data from the other
            std::memcpy((Char*)(external.ptr), other.c_str() + pos, new_size);
            if constexpr (NullTerminated) {
                ((Char*)(external.ptr))[new_size] = '\0';
            }
        }
    }

    constexpr basic_small_string(basic_small_string&& other,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) noexcept
        : external(other.external) {
        // set the other's external to 0, to avoid double free
        memset(&other.external, 0, sizeof(other.external));
    }

    constexpr basic_small_string(basic_small_string&& other, size_type pos,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos)} {}

    constexpr basic_small_string(const basic_small_string& other, size_type pos, size_type count,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    constexpr basic_small_string(basic_small_string&& other, size_type pos, size_type count,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    constexpr basic_small_string(const Char* s, size_type count,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        append(s, count);
    }

    constexpr basic_small_string(const Char* s, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string(s, std::strlen(s)) {}

    template <class InputIt>
    constexpr basic_small_string(InputIt first, InputIt last,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        append(first, last);
    }

    constexpr basic_small_string(std::initializer_list<Char> ilist,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        append(ilist.begin(), ilist.end());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr basic_small_string(const StringViewLike& s, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        append(s.begin(), s.end());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    constexpr basic_small_string(const StringViewLike& s, size_type pos, size_type n,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : internal{} {
        append(s, pos, n);
    }

    basic_small_string(std::nullptr_t) = delete;

    ~basic_small_string() noexcept {
        if (is_external()) {
            external.deallocate();
        }
    }

    // copy assignment
    auto operator=(const basic_small_string& other) -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        if (is_external()) {
            external.deallocate();
        }
        // call the copy constructor
        new (this) basic_small_string(other);
        return *this;
    }

    // move assignment
    auto operator=(basic_small_string&& other) noexcept -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        if (is_external()) {
            external.deallocate();
        }
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
        if (is_external()) {
            external.deallocate();
        }
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

    auto get_allocator() const -> Allocator { return Allocator(); }

    // element access
    template <bool Safe = true>
    constexpr auto at(size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    template <bool Safe = true>
    constexpr auto at(size_type pos) const noexcept(not Safe) -> const_reference {
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

    auto front() noexcept -> reference { return *(Char*)data(); }

    auto front() const noexcept -> const_reference { return *c_str(); }

    auto back() noexcept -> reference {
        // do not use c_str() and size() to avoid extra if check
        if (is_external()) {
            return *(reinterpret_cast<Char*>(external.c_str_ptr) + external.size() - 1);
        }
        // else is internal
        return internal.data[internal.internal_size - 1];
    }

    auto back() const noexcept -> const_reference {
        // do not use c_str() and size() to avoid extra if check
        if (is_external()) {
            return *(reinterpret_cast<Char*>(external.c_str_ptr) + external.size() - 1);
        }
        // else is internal
        return internal.data[internal.size - 1];
    }

    [[nodiscard, gnu::always_inline]] auto c_str() const noexcept -> const Char* {
        return is_external() ? reinterpret_cast<const Char*>(external.c_str_ptr) : internal.data;
    }

    [[nodiscard, gnu::always_inline]] auto data() const noexcept -> const Char* { return c_str(); }

    [[nodiscard, gnu::always_inline]] auto data() noexcept -> Char* {
        return is_external() ? reinterpret_cast<Char*>(external.c_str_ptr) : internal.data;
    }

    // Iterators
    auto begin() noexcept -> iterator { return data(); }

    auto begin() const noexcept -> const_iterator { return c_str(); }

    auto cbegin() const noexcept -> const_iterator { return c_str(); }

    auto end() noexcept -> iterator {
        if (is_external()) {
            return (Char*)external.c_str_ptr + external.size();
        }
        return internal.data + internal.internal_size;
    }

    auto end() const noexcept -> const_iterator {
        if (is_external()) {
            return external.c_str() + external.size();
        }
        return internal.data + internal.internal_size;
    }

    auto cend() const noexcept -> const_iterator {
        if (is_external()) {
            return external.c_str() + external.size();
        }
        return internal.data + internal.internal_size;
    }

    auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }

    auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }

    auto crbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }

    auto rend() noexcept -> reverse_iterator { return reverse_iterator{begin()}; }

    auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }

    auto crend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }

    // capacity
    [[nodiscard, gnu::always_inline]] auto empty() const noexcept -> bool { return size() == 0; }

    [[nodiscard, gnu::always_inline]] auto size() const noexcept -> size_type {
        return is_external() ? external.size() : internal.size();
    }

    [[nodiscard, gnu::always_inline]] auto length() const noexcept -> size_type { return size(); }

    /**
     * @brief The maximum number of elements that can be stored in the string.
     * the buffer largest size is 1 << 15, the cap occupy 2 bytes, so the max size is 1 << 15 - 2, is 65534
     */
    [[nodiscard]] constexpr auto max_size() const noexcept -> size_type { return kInvalidSize - 1; }

    constexpr auto reserve(size_type new_cap) -> void {
        // check the new_cap is larger than the internal capacity, and larger than current cap
        if (new_cap > capacity()) [[likely]] {
            // still be a small string
            auto new_buffer_size = calculate_new_buffer_size(new_cap);
            auto old_size = size();
            // allocate a new buffer
            auto new_external = allocate_new_external_buffer(new_buffer_size, old_size);
            // copy the old data to the new buffer
            std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), c_str(), old_size);
            if constexpr (NullTerminated) {
                reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
            }
            // copy the old size to tmp
            if (is_external())
              [[likely]] {  // we do not wish reserve a internal string to a external str, it will be a little slower.
                // if the old string is a external string, deallocate the old buffer
                external.deallocate();
            }
            // replace the old external with the new one
            external = new_external;
        }
    }

    // it was a just exported function, should not be called frequently, it was a little bit slow in some case.
    [[nodiscard, gnu::always_inline]] auto capacity() const noexcept -> size_type {
        return is_external() ? external.capacity() : internal_core::capacity();
    }

    auto shrink_to_fit() -> void {
        auto [cap, size] = get_capacity_and_size();
        Assert(cap >= size, "cap should always be greater or equal to size");
        auto best_cap = calc_new_buf_size_from_any_size(size);
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
    auto clear() noexcept -> void {
        if (is_external()) {
            external.set_size(0);
            if constexpr (NullTerminated) {
                *((Char*)external.c_str_ptr) = '\0';
            }
        } else {
            internal.set_size(0);
            if constexpr (NullTerminated) {
                internal.data[0] = '\0';
            }
        }
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, size_type count, Char ch) -> basic_small_string& {
        if (index > size()) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        auto old_size = size();
        std::memmove(data() + index + count, c_str() + index, old_size - index);
        std::memset(data() + index, ch, count);
        increase_size(count);
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
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        auto old_size = size();
        std::memmove(data() + index + count, c_str() + index, old_size - index);
        // set the new data
        std::memcpy(data() + index, str, count);
        increase_size(count);
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
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        std::memmove(const_cast<Char*>(pos) + count, pos,
                     static_cast<size_type>(end() - pos));  // end() - pos is faster than size() - (pos - begin())
        // set the new data
        std::memset(const_cast<Char*>(pos), ch, count);
        increase_size(count);
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
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // move the data to the new position
        std::memmove(data() + index + count, data() + index, static_cast<size_type>(size() - index));
        // copy the new data
        std::copy(first, last, data() + index);
        increase_size(count);
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
        // memmove the data to the new position
        std::memmove(data() + index, c_str() + index + real_count, old_size - index - real_count);
        // set the new size
        if (is_external()) [[likely]] {
            external.set_size(old_size - real_count);
            if constexpr (NullTerminated) {
                *(external.c_str() + old_size - real_count) = '\0';
            }
        } else {
            internal.internal_size -= real_count;
            if constexpr (NullTerminated) {
                internal.data[internal.internal_size] = '\0';
            }
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
        if (is_external()) [[likely]] {
            // the size maybe was stored in the head of the external buffer
            external.decrease_size(1);
        } else {
            internal.internal_size--;
        }
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
    }
    template <bool Safe = true>
    constexpr auto append(size_type count, Char c) -> basic_small_string& {
        if constexpr (Safe) {
            // allocate a new external buffer if need
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        std::memset(data() + size(), c, count);
        increase_size(count);
        if constexpr (NullTerminated) {
            data()[size()] = '\0';
        }
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other) -> basic_small_string& {
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, other.size());
        }
        // by now, the capacity is enough
        // size() function maybe slower than while the size is larger than 4k, so store it.
        auto other_size = other.size();
        std::memcpy(end(), other.c_str(), other_size);
        increase_size(other_size);
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
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }

        std::memcpy(data() + size(), s, count);
        increase_size(count);
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
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        std::copy(first, last, data() + size());
        increase_size(count);
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
    auto replace(size_type pos, size_type count, size_type count2, Char ch) -> basic_small_string& {
        // check the pos is not out of range
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        // then check if pos+count is the out of range
        if (pos + count >= size()) {
            // no right part to move
            uint64_t new_size = pos + count2;  // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            if (new_size > capacity()) {
                reserve(new_size);
            }
            // by now, the capacity is enough
            std::memset(data() + pos, ch, count2);
            set_size(new_size);
            if constexpr (NullTerminated) {
                data()[new_size] = '\0';
            }
            return *this;
        }
        // else, there is right part, maybe need to move
        // check the size, if the pos + count is greater than the cap, we need to reserve
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

            // the memmove will handle the overlap automatically
            std::memmove(data() + pos + count, c_str() + pos + count2, size() - pos - count);
        }
        // by now, the buffer is ready
        std::memset(data() + pos, ch, count2);
        set_size(new_size);
        if constexpr (NullTerminated) {
            data()[new_size] = '\0';
        }
        return *this;
    }

    // replace [pos, pos+count] with the range [cstr, cstr + count2]
    auto replace(size_type pos, size_type count, const Char* cstr, size_type count2) -> basic_small_string& {
        // check the pos is not out of range
        auto old_size = size();
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }

        if (pos + count >= old_size) {
            // no right part to move
            uint64_t new_size = pos + count2;  // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            auto old_cap = capacity();
            if (new_size > old_cap) {
                reserve(new_size);
            }
            // by now, the capacity is enough
            std::memcpy(data() + pos, cstr, count2);
            set_size(new_size);
            if constexpr (NullTerminated) {
                data()[new_size] = '\0';
            }
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
            reserve(new_size);
        }
        // by now, the capacity is enough
        // check if need do some memmove
        if (count != count2) {
            // move the right part to the new position, and the size will not be zero

            std::memmove(data() + pos + count, c_str() + pos + count2, old_size - pos - count);
            // the memmove will handle the overlap automatically
        }
        // by now, the buffer is ready
        std::memcpy(data() + pos, cstr, count2);
        set_size(new_size);
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
        Assert(first2 < last2, "the range is valid");
        auto count = last - first;
        if (count > 0) [[unlikely]] {
            throw std::invalid_argument("the range is invalid");
        }
        auto count2 = std::distance(first2, last2);
        if (last == end()) {
            // no right part to move
            uint64_t new_size = first - begin() + count2;  // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            if (new_size > capacity()) {
                reserve(new_size);
            }
            // by now, the capacity is enough
            // if first2 and last2 is ptr, will use memcpy?
            std::copy(first2, last2, first);
            set_size(new_size);
            if constexpr (NullTerminated) {
                data()[new_size] = '\0';
            }
            return *this;
        }
        // else, there is right part, maybe need to move
        uint64_t new_size = size() - (last - first) + count2;  // to avoid overflow from size_type
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
        set_size(new_size);
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
        if (count <= size()) {
            set_size(count);
            if constexpr (NullTerminated) {
                data()[count] = '\0';
            }
            return;
        }
        if (count > capacity()) {
            reserve(count);
        }
        std::memset(data() + size(), '\0', count - size());
        set_size(count);
        if constexpr (NullTerminated) {
            data()[count] = '\0';
        }
        return;
    }

    auto resize(size_type count, Char ch) -> void {
        // if the count is less than the size, just set the size
        if (count <= size()) {
            set_size(count);
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
        set_size(count);
        if constexpr (NullTerminated) {
            data()[count] = '\0';
        }
        return;
    }

    void swap(basic_small_string& other) noexcept {
        // union, just swap the memory
        std::swap(internal, other.internal);
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

    constexpr auto find(const Char* needle, size_type pos = 0) const -> size_type {
        return find(needle, pos, traits_type::length(needle));
    }

    constexpr auto find(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return find(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    constexpr auto find(Char ch, size_type pos = 0) const -> size_type {
        auto* found = traits_type::find(c_str() + pos, size() - pos, ch);
        return found == nullptr ? npos : size_type(found - c_str());
    }

    constexpr auto rfind(const Char* needle, size_type pos, size_type other_size) const -> size_type {
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

    constexpr auto rfind(const Char* needle, size_type pos = 0) const -> size_type {
        return rfind(needle, pos, traits_type::length(needle));
    }

    constexpr auto rfind(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return rfind(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto rfind(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    constexpr auto rfind(Char ch, size_type pos = npos) const -> size_type {
        auto current_size = size();
        if (current_size == 0) [[unlikely]] return npos;
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

    constexpr auto find_first_of(const Char* str, size_type pos, size_type count) const -> size_type {
        return find(str, pos, count);
    }

    constexpr auto find_first_of(const Char* str, size_type pos = 0) const -> size_type {
        return find(str, pos, traits_type::length(str));
    }

    constexpr auto find_first_of(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return find(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_first_of(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    constexpr auto find_first_of(Char ch, size_type pos = 0) const -> size_type { return find(ch, pos); }

    constexpr auto find_first_not_of(const Char* str, size_type pos, size_type count) const -> size_type {
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

    constexpr auto find_first_not_of(const Char* str, size_type pos = 0) const -> size_type {
        return find_first_not_of(str, pos, traits_type::length(str));
    }

    constexpr auto find_first_not_of(const basic_small_string& other, size_type pos = 0) const -> size_type {
        return find_first_not_of(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_first_not_of(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find_first_not_of(view.data(), pos, view.size());
    }

    constexpr auto find_first_not_of(Char ch, size_type pos = 0) const -> size_type {
        return find_first_not_of(&ch, pos, 1);
    }

    constexpr auto find_last_of(const Char* str, size_type pos, size_type count) const -> size_type {
        return rfind(str, pos, count);
    }

    constexpr auto find_last_of(const Char* str, size_type pos = npos) const -> size_type {
        return rfind(str, pos, traits_type::length(str));
    }

    constexpr auto find_last_of(const basic_small_string& other, size_type pos = npos) const -> size_type {
        return rfind(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_last_of(const StringViewLike& view, size_type pos = npos) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    constexpr auto find_last_of(Char ch, size_type pos = npos) const -> size_type { return rfind(ch, pos); }

    constexpr auto find_last_not_of(const Char* str, size_type pos, size_type count) const -> size_type {
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

    constexpr auto find_last_not_of(const Char* str, size_type pos = npos) const -> size_type {
        return find_last_not_of(str, pos, traits_type::length(str));
    }

    constexpr auto find_last_not_of(const basic_small_string& other, size_type pos = npos) const -> size_type {
        return find_last_not_of(other.c_str(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_last_not_of(const StringViewLike& view, size_type pos = npos) const -> size_type {
        return find_last_not_of(view.data(), pos, view.size());
    }

    constexpr auto find_last_not_of(Char ch, size_type pos = npos) const -> size_type {
        return find_last_not_of(&ch, pos, 1);
    }

    // Operations
    constexpr auto compare(const basic_small_string& other) const noexcept -> int {
        return traits_type::compare(c_str(), other.c_str(), std::min(size(), other.size()));
    }
    constexpr auto compare(size_type pos, size_type count, const basic_small_string& other) const noexcept -> int {
        return traits_type::compare(c_str() + pos, other.c_str(), std::min(count, other.size()));
    }
    constexpr auto compare(size_type pos1, size_type count1, const basic_small_string& str, size_type pos2,
                           size_type count2 = npos) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, str.c_str() + pos2,
                                    std::min(count1, count2 == npos ? str.size() - pos2 : count2));
    }
    constexpr auto compare(const Char* str) const noexcept -> int {
        return traits_type::compare(c_str(), str, std::min<size_type>(size(), traits_type::length(str)));
    }
    constexpr auto compare(size_type pos, size_type count, const Char* str) const noexcept -> int {
        return traits_type::compare(c_str() + pos, str, std::min<size_type>(count, traits_type::length(str)));
    }
    constexpr auto compare(size_type pos1, size_type count1, const Char* str, size_type count2) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, str, std::min<size_type>(count1, count2));
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(const StringViewLike& view) const noexcept -> int {
        return traits_type::compare(c_str(), view.data(), std::min(size(), view.size()));
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(size_type pos, size_type count, const StringViewLike& view) const noexcept -> int {
        return traits_type::compare(c_str() + pos, view.data(), std::min(count, view.size()));
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(size_type pos1, size_type count1, const StringViewLike& view, size_type pos2,
                           size_type count2 = npos) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, view.data() + pos2,
                                    std::min(count1, count2 == npos ? view.size() - pos2 : count2));
    }

    constexpr auto starts_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(0, view.size(), view) == 0;
    }

    constexpr auto starts_with(Char ch) const noexcept -> bool { return not empty() && traits_type::eq(front(), ch); }

    constexpr auto starts_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(0, len, str) == 0;
    }

    constexpr auto ends_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(size() - view.size(), view.size(), view) == 0;
    }

    constexpr auto ends_with(Char ch) const noexcept -> bool { return not empty() && traits_type::eq(back(), ch); }

    constexpr auto ends_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(size() - len, len, str) == 0;
    }

    constexpr auto contains(std::basic_string_view<Char> view) const noexcept -> bool { return find(view) != npos; }

    constexpr auto contains(Char ch) const noexcept -> bool { return find(ch) != npos; }

    constexpr auto contains(const Char* str) const noexcept -> bool { return find(str) != npos; }

    constexpr auto substr(size_type pos = 0, size_type count = npos) const& -> basic_small_string {
        auto size = this->size();
        if (pos > size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }

        return basic_small_string(c_str() + pos, (count == npos || count + pos >= size) ? size - pos : count);
    }

    constexpr auto substr(size_type pos = 0, size_type count = npos) && -> basic_small_string {
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }

        if (pos == 0) {
            // no need to generate a new string
            resize(count);
            return std::move(*this);
        }
        auto real_count = (count == npos || count + pos >= size()) ? size() - pos : count;
        // do replace first
        std::memmove(data(), c_str() + pos, real_count);
        resize(real_count);
        return std::move(*this);
    }
};  // class basic_small_string

// input/output
template <typename C, class T, class A>
inline auto operator<<(std::basic_ostream<C, T>& os,
                       const basic_small_string<C, T, A>& str) -> std::basic_ostream<C, T>& {
    return os.write(str.data(), str.size());
}

template <typename C, class T, class A>
inline auto operator>>(std::basic_istream<C, T>& is, basic_small_string<C, T, A>& str) -> std::basic_istream<C, T>& {
    return is >> std::basic_string_view<C, T>(str.data(), str.size());
}

// operator +
// 1
template <typename C, class T, class A>
inline auto operator+(const basic_small_string<C, T, A>& lhs,
                      const basic_small_string<C, T, A>& rhs) -> basic_small_string<C, T, A> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 2
template <typename C, class T, class A>
inline auto operator+(const basic_small_string<C, T, A>& lhs, const C* rhs) -> basic_small_string<C, T, A> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 3
template <typename C, class T, class A>
inline auto operator+(const basic_small_string<C, T, A>& lhs, C rhs) -> basic_small_string<C, T, A> {
    auto result = lhs;
    result.push_back(rhs);
    return result;
}

// 5
template <typename C, class T, class A>
inline auto operator+(const C* lhs, const basic_small_string<C, T, A>& rhs) -> basic_small_string<C, T, A> {
    return basic_small_string<C, T, A>(1, lhs) + rhs;
}

// 6
template <typename C, class T, class A>
inline auto operator+(C lhs, const basic_small_string<C, T, A>& rhs) -> basic_small_string<C, T, A> {
    return basic_small_string<C, T, A>(1, lhs) + rhs;
}

// 8
template <typename C, class T, class A>
inline auto operator+(basic_small_string<C, T, A>&& lhs,
                      basic_small_string<C, T, A>&& rhs) -> basic_small_string<C, T, A> {
    basic_small_string<C, T, A> result(std::move(lhs));
    result.append(std::move(rhs));
    return result;
}

// 9
template <typename C, class T, class A>
inline auto operator+(basic_small_string<C, T, A>&& lhs,
                      const basic_small_string<C, T, A>& rhs) -> basic_small_string<C, T, A> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 10
template <typename C, class T, class A>
inline auto operator+(basic_small_string<C, T, A>&& lhs, const C* rhs) -> basic_small_string<C, T, A> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 11
template <typename C, class T, class A>
inline auto operator+(basic_small_string<C, T, A>&& lhs, C rhs) -> basic_small_string<C, T, A> {
    auto result = std::move(lhs);
    result.push_back(rhs);
    return result;
}

// 13
template <typename C, class T, class A>
inline auto operator+(const basic_small_string<C, T, A>& lhs,
                      basic_small_string<C, T, A>&& rhs) -> basic_small_string<C, T, A> {
    auto result = lhs;
    result.append(std::move(rhs));
    return result;
}

// 14
template <typename C, class T, class A>
inline auto operator+(const C* lhs, basic_small_string<C, T, A>&& rhs) -> basic_small_string<C, T, A> {
    return basic_small_string<C, T, A>(lhs) + std::move(rhs);
}

// 15
template <typename C, class T, class A>
inline auto operator+(C lhs, basic_small_string<C, T, A>&& rhs) -> basic_small_string<C, T, A> {
    return basic_small_string<C, T, A>(1, lhs) + std::move(rhs);
}

// comparison operators
template <typename C, class T, class A>
inline auto operator<=>(const basic_small_string<C, T, A>& lhs,
                        const basic_small_string<C, T, A>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(rhs) <=> 0;
}

template <typename C, class T, class A>
inline auto operator==(const basic_small_string<C, T, A>& lhs,
                       const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and lhs.compare(rhs) == 0;
}

template <typename C, class T, class A>
inline auto operator!=(const basic_small_string<C, T, A>& lhs,
                       const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return not(lhs == rhs);
}

template <typename C, class T, class A>
inline auto operator>(const basic_small_string<C, T, A>& lhs, const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

template <typename C, class T, class A>
inline auto operator<(const basic_small_string<C, T, A>& lhs, const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

template <typename C, class T, class A>
inline auto operator>=(const basic_small_string<C, T, A>& lhs,
                       const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return not(lhs < rhs);
}

template <typename C, class T, class A>
inline auto operator<=(const basic_small_string<C, T, A>& lhs,
                       const basic_small_string<C, T, A>& rhs) noexcept -> bool {
    return not(lhs > rhs);
}

// basic_string compatibility routines
template <typename E, class T, class A, class S, class A2>
inline auto operator<=>(const basic_small_string<E, T, A>& lhs,
                        const std::basic_string<E, T, A2>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

// swap the lhs and rhs
template <typename C, class T, class A, class S, class A2>
inline auto operator<=>(const std::basic_string<C, T, A2>& lhs,
                        const basic_small_string<C, T, A>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

template <typename C, class T, class A, class A2>
inline auto operator==(const basic_small_string<C, T, A>& lhs,
                       const std::basic_string<C, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator==(const std::basic_string<E, T, A2>& lhs,
                       const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) == 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator!=(const basic_small_string<E, T, A>& lhs,
                       const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator!=(const std::basic_string<E, T, A2>& lhs,
                       const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<(const basic_small_string<E, T, A>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) < 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>(const basic_small_string<E, T, A>& lhs, const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) > 0;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<(const std::basic_string<E, T, A2>& lhs, const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return rhs > lhs;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>(const std::basic_string<E, T, A2>& lhs, const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return rhs < lhs;
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<=(const basic_small_string<E, T, A>& lhs,
                       const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>=(const basic_small_string<E, T, A>& lhs,
                       const std::basic_string<E, T, A2>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator<=(const std::basic_string<E, T, A2>& lhs,
                       const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

template <typename E, class T, class A, class S, class A2>
inline auto operator>=(const std::basic_string<E, T, A2>& lhs,
                       const basic_small_string<E, T, A>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

using small_string = basic_small_string<char>;

}  // namespace stdb::memory