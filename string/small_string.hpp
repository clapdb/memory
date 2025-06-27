/*
 * Copyright (C) 2020 Beijing Jinyi Data Technology Co., Ltd. All rights reserved.
 *
 * This document is the property of Beijing Jinyi Data Technology Co., Ltd.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Beijing Jinyi Data Technology Co., Ltd.
 */

#pragma once
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "align/align.hpp"
#include "assert_config.hpp"

namespace stdb::memory {

enum class CoreType : uint8_t
{
    // the core type is internal
    Internal = 0,
    // the core type is short
    Short = 1,
    // the flag is for mediam buffer
    Median = 2,
    // the flag is for long buffer
    Long = 3
};

constexpr inline auto kIsInternal = static_cast<uint8_t>(CoreType::Internal);
constexpr inline auto kIsShort = static_cast<uint8_t>(CoreType::Short);
constexpr inline auto kIsMedian = static_cast<uint8_t>(CoreType::Median);
constexpr inline auto kIsLong = static_cast<uint8_t>(CoreType::Long);

template <typename S>
struct buffer_type_and_size
{
    S buffer_size;
    CoreType core_type;
};

static_assert(sizeof(buffer_type_and_size<uint32_t>) == 8);

template <typename S>
struct capacity_and_size
{
    S capacity;
    S size;
};

static_assert(sizeof(capacity_and_size<uint32_t>) == 8);

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
        uint8_t internal_size : 6;
        // 0000 , ths higher 4 bits is 0, means is_internal is 0
        uint8_t flag : 2;  // must be 00
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

        // the cap <= 32, the size <= 256
        struct cap_and_size
        {
            uint8_t cap : 5;    // the real capacity = (cap + 1) * 8, so the max(capacity) = 32 * 8 = 256
                                // the min(cap) = 0, means the min(capacity) is 8
            uint16_t size : 9;  // the size <= 256, size will never be larger than 256
            uint8_t flag : 2;   // the flag must be 01
        };

        struct idle_cap
        {
            // the idle_or_ignore is the max idle size, which is 4k - 1
            uint16_t idle_or_ignore : 14;  // max idle size = 2**14 - 1
            uint8_t flag : 2;              // the flag must be 10
        };  // struct size_and_shift

        static_assert(sizeof(idle_cap) == 2);

        union
        {
            idle_cap idle;
            cap_and_size cap_size;
            uint16_t mask;
        };  // union size_or_mask

        [[nodiscard, gnu::always_inline]] auto get_buffer_ptr() const noexcept -> Char* {
            Assert(idle.flag > 0, "the flag should be 01 / 10 / 11");
            if (cap_size.flag == 1) [[likely]] {
                return reinterpret_cast<Char*>(c_str_ptr);
            } else {
                return reinterpret_cast<Char*>(c_str_ptr) - sizeof(struct capacity_and_size<size_type>);
            }
        }
    };  // struct external_core

    static_assert(sizeof(external_core) == 8);
    union
    {
        int64_t body;        // use a int64 to assign or swap, for the atomic operation and best performance
        Char init_slice[8];  // just set init[7] = 0, the small_string will be initialized, and set init[0] =0, can
                             // handle NullTerminated
        internal_core internal;
        external_core external;
    };

    consteval static inline auto internal_buffer_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return sizeof(internal_core) - 2;
        } else {
            return sizeof(internal_core) - 1;
        }
    }

    consteval static inline auto median_long_buffer_header_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return sizeof(struct capacity_and_size<size_type>) + 1;
        } else {
            return sizeof(struct capacity_and_size<size_type>);
        }
    }

    consteval static inline auto max_short_buffer_size() noexcept -> size_type {
        if constexpr (NullTerminated) {
            return 255;
        } else {
            return 256;
        }
    }

    consteval static inline auto max_median_buffer_size() noexcept -> size_type {
        // the idle is a 14bits value, so the max idle is 2**14 - 1, the max size is 2**14 - 1 + 8
        return (1UL << 14UL) - 1;
    }

    consteval static inline auto max_long_buffer_size() noexcept -> size_type {
        return std::numeric_limits<size_type>::max() - median_long_buffer_header_size();
    }

    constexpr static uint8_t kInterCore = static_cast<uint8_t>(CoreType::Internal);
    constexpr static uint8_t kShortCore = static_cast<uint8_t>(CoreType::Short);
    constexpr static uint8_t kMedianCore = static_cast<uint8_t>(CoreType::Median);
    constexpr static uint8_t kLongCore = static_cast<uint8_t>(CoreType::Long);

    [[nodiscard]] auto get_core_type() const -> uint8_t { return external.idle.flag; }

    [[nodiscard, gnu::always_inline]] inline auto is_external() const noexcept -> bool { return internal.flag != 0; }

    [[nodiscard, gnu::always_inline]] constexpr auto capacity_from_buffer_header() const noexcept -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // the capacity is stored in the buffer header, the capacity is 4 bytes, and it will handle NullTerminated in
        // allocate_new_external_buffer's logic
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 2);
    }

    [[nodiscard, gnu::always_inline]] constexpr auto max_real_cap_from_buffer_header() const noexcept -> size_type {
        if constexpr (NullTerminated) {
            return capacity_from_buffer_header() - 1 - sizeof(struct capacity_and_size<size_type>);
        } else {
            return capacity_from_buffer_header() - sizeof(struct capacity_and_size<size_type>);
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto size_from_buffer_header() const noexcept -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1);
    }

    [[gnu::always_inline]] constexpr auto set_size_to_buffer_header(size_type new_size) noexcept -> void {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // check the new_size is less than the capacity
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(new_size <= max_real_cap_from_buffer_header(), "the new size should be less than the capacity");
        *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) = new_size;
    }

    [[nodiscard, gnu::always_inline]] constexpr auto increase_size_to_buffer_header(size_type size_to_increase) noexcept
      -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be no more than 32");
        Assert(size_to_increase <= get_idle_capacity_from_buffer_header(),
               "the size to increase should be less than the idle size");
        // check the new_size is less than the capacity
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) += size_to_increase;
    }

    [[nodiscard, gnu::always_inline]] constexpr auto decrease_size_to_buffer_header(size_type size_to_decrease) noexcept
      -> size_type {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        // check the new_size is less than the capacity
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(size_to_decrease <= size_from_buffer_header(),
               "the size to decrease should be less than the current size");
        return *(reinterpret_cast<size_type*>(external.c_str_ptr) - 1) -= size_to_decrease;
    }

    [[nodiscard, gnu::always_inline]] constexpr auto get_capacity_and_size_from_buffer_header() const noexcept
      -> capacity_and_size<size_type> {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        Assert(size_from_buffer_header() <= max_real_cap_from_buffer_header(),
               "the size should be less than the max real capacity");
        return *(reinterpret_cast<capacity_and_size<size_type>*>(external.c_str_ptr) - 1);
    }

    [[nodiscard, gnu::always_inline]] constexpr auto get_idle_capacity_from_buffer_header() const noexcept
      -> uint16_t {
        Assert(external.idle.flag > 1, "the flag should be 10 / 11");
        Assert(capacity_from_buffer_header() > 256, "the capacity should be more than 256");
        auto [cap, size] = get_capacity_and_size_from_buffer_header();
        if constexpr (NullTerminated) {
            return static_cast<uint16_t>(cap - size - 1 - sizeof(struct capacity_and_size<size_type>));
        } else {
            return static_cast<uint16_t>(cap - size - sizeof(struct capacity_and_size<size_type>));
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto idle_capacity() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:  // internal
                return internal_buffer_size() - internal.internal_size;
            case 1: {  // short
                Assert(external.cap_size.cap <= 32, "the cap should be no more than 32");
                Assert(external.cap_size.size <= 256, "the size should be no more than 256");
                if constexpr (NullTerminated) {
                    return (external.cap_size.cap + 1UL) * 8UL - external.cap_size.size - 1UL;
                } else {
                    return (external.cap_size.cap + 1UL) * 8UL - external.cap_size.size;
                }
            }
            case 2:  // median
                return external.idle.idle_or_ignore;
            case 3:  // long
                return get_idle_capacity_from_buffer_header();
            default:
                Assert(false, "the flag should be just 01-03");
                __builtin_unreachable();
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return internal_buffer_size();
            case 1:
                if constexpr (NullTerminated) {
                    return (external.cap_size.cap + 1UL) * 8UL - 1UL;
                } else {
                    return (external.cap_size.cap + 1UL) * 8UL;
                }
            default:
                if constexpr (NullTerminated) {
                    return capacity_from_buffer_header() - 1 - sizeof(struct capacity_and_size<size_type>);
                } else {
                    return capacity_from_buffer_header() - sizeof(struct capacity_and_size<size_type>);
                }
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto size() const noexcept -> size_type {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return internal.internal_size;
            case 1:
                return external.cap_size.size;
            default:
                return size_from_buffer_header();
        }
    }

    /**
     * set_size, whithout cap changing
     */
    constexpr void set_size_and_idle_and_set_term(size_type new_size) noexcept {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0: {
                // Internal buffer, the size is stored in the internal_core
                internal.internal_size = static_cast<uint8_t>(new_size);
                // set the terminator
                if constexpr (NullTerminated) {
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            }
            case 1: {
                // Short buffer, the size is stored in the external_core
                Assert(new_size <= (external.cap_size.cap + 1UL) * 8UL - (NullTerminated ? 1UL : 0UL),
                       "the new size should be less than the max real capacity");
                external.cap_size.size = static_cast<uint16_t>(new_size);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
                break;
            }
            case 2: {
                // Median buffer, the size is stored in the buffer header, the idle_or_ignore is the idle size
                set_size_to_buffer_header(new_size);
                external.idle.idle_or_ignore = get_idle_capacity_from_buffer_header();
                if constexpr (NullTerminated) {
                    // set the terminator
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
                break;
            }
            case 3: {
                set_size_to_buffer_header(new_size);
                if constexpr (NullTerminated) {
                    // set the terminator
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_size] = '\0';
                }
            }
        }
    }

    constexpr void increase_size_and_idle_and_set_term(size_type size_to_increase) noexcept {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                Assert(internal.internal_size + size_to_increase <= internal_buffer_size(),
                       "the new size should be less than the capacity");
                internal.internal_size += size_to_increase;
                if constexpr (NullTerminated) {
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            case 1:
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                external.cap_size.size += size_to_increase;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[external.cap_size.size] = '\0';
                }
                break;
            case 2: {
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                external.idle.idle_or_ignore -= size_to_increase;
                [[maybe_unused]] auto new_str_size = increase_size_to_buffer_header(size_to_increase);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
            case 3: {
                Assert(size_to_increase <= idle_capacity(), "the size to increase should be less than the idle size");
                [[maybe_unused]] auto new_str_size = increase_size_to_buffer_header(size_to_increase);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
        }
    }

    constexpr void decrease_size_and_idle_and_set_term(size_type size_to_decrease) noexcept {
        auto flag = external.idle.flag;
        Assert(flag > 0 and flag < 3, "the flag should be 01 or 10");
        switch (flag) {
            case 0:
                Assert(size_to_decrease <= internal.internal_size, "the size to decrease should be less than the size");
                internal.internal_size -= size_to_decrease;
                if constexpr (NullTerminated) {
                    internal.data[internal.internal_size] = '\0';
                }
                break;
            case 1:
                Assert(size_to_decrease <= external.cap_size.size,
                       "the size to decrease should be less than the current size");
                external.cap_size.size -= size_to_decrease;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[external.cap_size.size] = '\0';
                }
                break;
            case 2: {
                Assert(size_to_decrease <= size_from_buffer_header(),
                       "the size to decrease should be less than the current size");
                external.idle.idle_or_ignore += size_to_decrease;
                [[maybe_unused]] auto new_str_size = decrease_size_to_buffer_header(size_to_decrease);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
            case 3: {
                Assert(size_to_decrease <= size_from_buffer_header(),
                       "the size to decrease should be less than the idle size");
                [[maybe_unused]] auto new_str_size = decrease_size_to_buffer_header(size_to_decrease);
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(external.c_str_ptr)[new_str_size] = '\0';
                }
                break;
            }
        }
    }

    [[nodiscard]] constexpr auto get_capacity_and_size() const noexcept -> capacity_and_size<size_type> {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return {.capacity = internal_buffer_size(), .size = internal.internal_size};
            case 1: {
                if constexpr (NullTerminated) {
                    return {.capacity = static_cast<uint32_t>((external.cap_size.cap + 1) * 8UL) - 1,
                            .size = external.cap_size.size};
                } else {
                    return {.capacity = static_cast<uint32_t>((external.cap_size.cap + 1) * 8UL),
                            .size = external.cap_size.size};
                }
            }
            default: {
                auto [cap, size] = get_capacity_and_size_from_buffer_header();
                size_type real_cap;
                if constexpr (NullTerminated) {
                    real_cap = cap - 1 - sizeof(struct capacity_and_size<size_type>);
                } else {
                    real_cap = cap - sizeof(struct capacity_and_size<size_type>);
                }
                return {.capacity = real_cap, .size = size};
            }
        }
    }

    [[nodiscard, gnu::always_inline]] inline auto begin_ptr() noexcept -> Char* {
        return is_external() ? reinterpret_cast<Char*>(external.c_str_ptr) : internal.data;
    }

    [[nodiscard, gnu::always_inline]] inline auto get_string_view() const noexcept -> std::string_view {
        auto flag = external.idle.flag;
        switch (flag) {
            case 0:
                return std::string_view{internal.data, internal.internal_size};
            case 1:
                return std::string_view{reinterpret_cast<Char*>(external.c_str_ptr), external.cap_size.size};
            default:
                return std::string_view{reinterpret_cast<Char*>(external.c_str_ptr), size_from_buffer_header()};
        }
    }

    [[nodiscard, gnu::always_inline]] constexpr auto end_ptr() noexcept -> Char* {
        auto flag = internal.flag;
        switch (flag) {
            case 0:
                return &internal.data[internal.internal_size];
            case 1:
                return reinterpret_cast<Char*>(external.c_str_ptr) + external.cap_size.size;
            default:
                return reinterpret_cast<Char*>(external.c_str_ptr) + size_from_buffer_header();
        }
    }

    auto swap(malloc_core& other) noexcept -> void {
        auto temp_body = other.body;
        other.body = body;
        body = temp_body;
    }

    [[gnu::always_inline]] void fastest_zero_init() {
        if constexpr (NullTerminated) {
            init_slice[0] = 0;
        }
        init_slice[7] = 0;
    }

    // constructor
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    constexpr malloc_core([[maybe_unused]] const std::allocator<Char>& unused = std::allocator<Char>{}) noexcept {
        fastest_zero_init();
    }
    constexpr malloc_core(const malloc_core& other) noexcept : body(other.body) {}
    constexpr malloc_core(const malloc_core& other, [[maybe_unused]] const std::allocator<Char>& unused) noexcept
        : body(other.body) {}
    constexpr malloc_core(malloc_core&& gone) noexcept : body(gone.body) { gone.fastest_zero_init(); }
    ~malloc_core() = default;
    auto operator=(const malloc_core& other) -> malloc_core& = delete;
    auto operator=(malloc_core&& other) noexcept -> malloc_core& = delete;
};  // struct malloc_core

static_assert(sizeof(malloc_core<char, true>) == 8, "malloc_core should be same as a pointer");

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

    constexpr pmr_core(const pmr_core& other, const std::pmr::polymorphic_allocator<Char>& allocator) noexcept
        : malloc_core<Char, NullTerminated>(other), pmr_allocator(allocator) {}

    constexpr pmr_core(pmr_core&& gone) noexcept
        : malloc_core<Char, NullTerminated>(std::move(gone)), pmr_allocator(gone.pmr_allocator) {}

    ~pmr_core() = default;
    auto operator=(const pmr_core& other) -> pmr_core& = delete;
    auto operator=(pmr_core&& other) noexcept -> pmr_core& = delete;

};  // struct malloc_core_and_pmr_allocator

template <typename Char, template <typename, bool> typename Core, class Traits, class Allocator, bool NullTerminated,
          float Growth = 1.5F>
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
    constexpr static size_t npos = std::numeric_limits<size_type>::max();

    using core_type = Core<Char, NullTerminated>;

    enum class Need0 : bool
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

    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_median_long_real_idle_capacity(
      size_type buffer_size, size_type old_size) noexcept -> size_type {
        if constexpr (NullTerminated) {
            Assert(buffer_size >= sizeof(struct capacity_and_size<size_type>) + 1 + old_size,
                   "the buffer_size should be no less than the size of the buffer header and the old size");
            return buffer_size - sizeof(struct capacity_and_size<size_type>) - 1 - old_size;
        } else {
            Assert(buffer_size >= sizeof(struct capacity_and_size<size_type>) + old_size,
                   "the buffer_size should be no less than the size of the buffer header and the old size");
            return buffer_size - sizeof(struct capacity_and_size<size_type>) - old_size;
        }
    }

    // set the buf_size and str_size to the right place
    inline static auto allocate_new_external_buffer(
      struct buffer_type_and_size<size_type> type_and_size, size_type old_str_size,
      std::pmr::polymorphic_allocator<Char>* allocator_ptr = nullptr) noexcept -> typename core_type::external_core {
        // make sure the old_str_size <= new_buffer_size
        auto type = type_and_size.core_type;
        auto new_buffer_size = type_and_size.buffer_size;

        switch (type) {
            case CoreType::Internal: {
                Assert(false, "the internal buffer should not be allocated");
                __builtin_unreachable();
            }
            case CoreType::Short: {
                Assert(type_and_size.buffer_size % 8 == 0, "the buffer_size should be aligned to 8");
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                return {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                        .cap_size = {.cap = static_cast<uint8_t>(type_and_size.buffer_size / 8 - 1),
                                     .size = static_cast<uint16_t>(old_str_size),
                                     .flag = kIsShort}};
            }
            case CoreType::Median: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = old_str_size;
                return {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                        .idle = {.idle_or_ignore = static_cast<uint16_t>(
                                   calculate_median_long_real_idle_capacity(new_buffer_size, old_str_size)),
                                 .flag = kIsMedian}};
            }
            case CoreType::Long: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = allocator_ptr->allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = old_str_size;
                return {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                        .idle = {.idle_or_ignore = 0, .flag = kIsLong}};
            }
            default:
                __builtin_unreachable();
        }
    }

   protected:
    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_new_buffer_size(size_t size) noexcept
      -> buffer_type_and_size<size_type> {
        if (size <= core_type::internal_buffer_size()) [[unlikely]] {
            return {.buffer_size = core_type::internal_buffer_size(), .core_type = CoreType::Internal};
        }
        if (size <= core_type::max_short_buffer_size()) [[likely]] {
            if constexpr (NullTerminated) {
                return {.buffer_size = static_cast<size_type>(align::AlignUpTo<8>(size + 1)), .core_type = CoreType::Short};
            } else {
                return {.buffer_size = static_cast<size_type>(align::AlignUpTo<8>(size)), .core_type = CoreType::Short};
            }
        }
        if (size <= core_type::max_median_buffer_size()) [[likely]] {  // faster than 3-way compare
            return {.buffer_size =
                      static_cast<size_type>(align::AlignUpTo<8>(size + core_type::median_long_buffer_header_size())),
                    .core_type = CoreType::Median};
        }
        Assert(size <= core_type::max_long_buffer_size(),
               "the buffer size should be less than the max value of size_type");
        return {.buffer_size = static_cast<size_type>(align::AlignUpTo<8>(size + core_type::median_long_buffer_header_size())),
                .core_type = CoreType::Long};
    }

    [[nodiscard, gnu::always_inline]] constexpr static inline auto calculate_new_buffer_size(size_t size,
                                                                                             float growth) noexcept
      -> buffer_type_and_size<size_type> {
        return calculate_new_buffer_size(size * growth);
    }

    [[nodiscard]] auto get_core_type() const -> uint8_t { return _core.get_core_type(); }

    // the fastest initial_allocate, do not calculate the type or size, just allocate the buffer
    // caller should make sure the type_and_size is correct
    constexpr void initial_allocate(buffer_type_and_size<size_type> cap_and_type, size_type size) noexcept {
        auto type = cap_and_type.core_type;
        auto new_buffer_size = cap_and_type.buffer_size;
        switch (type) {
            case CoreType::Internal:
                _core.internal.flag = kIsInternal;
                _core.internal.internal_size = static_cast<uint8_t>(size);
                if constexpr (NullTerminated) {
                    _core.internal.data[size] = '\0';
                }
                break;
            case CoreType::Short: {
                Assert(new_buffer_size % 8 == 0, "the buffer_size should be aligned to 8");
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(buf),
                                  .cap_size = {.cap = static_cast<uint8_t>(new_buffer_size / 8 - 1),
                                               .size = static_cast<uint16_t>(size),
                                               .flag = kIsShort}};
                break;
            }
            case CoreType::Median: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = size;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size + sizeof(struct capacity_and_size<size_type>)] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                                  .idle = {.idle_or_ignore = static_cast<uint16_t>(
                                             calculate_median_long_real_idle_capacity(new_buffer_size, size)),
                                           .flag = kIsMedian}};
                break;
            }
            case CoreType::Long: {
                void* buf = nullptr;
                if constexpr (core_type::use_std_allocator::value) {
                    buf = std::malloc(new_buffer_size);
                } else {
                    buf = _core.pmr_allocator.allocate(new_buffer_size);
                }
                auto* head = reinterpret_cast<capacity_and_size<size_type>*>(buf);
                head->capacity = new_buffer_size;
                head->size = size;
                if constexpr (NullTerminated) {
                    reinterpret_cast<Char*>(buf)[size + sizeof(struct capacity_and_size<size_type>)] = '\0';
                }
                _core.external = {.c_str_ptr = reinterpret_cast<int64_t>(head + 1),
                                  .idle = {.idle_or_ignore = 0, .flag = kIsLong}};
                break;
            }
        }
        return;
    }

    [[gnu::always_inline]] constexpr inline void initial_allocate(size_t new_string_size) noexcept {
        // Assert(new_string_size <= std::numeric_limits<size_type>::max(),
        Assert(new_string_size < core_type::max_long_buffer_size(),
               "the new_string_size should be less than the max value of size_type");
        auto cap_and_type = calculate_new_buffer_size(new_string_size);
        initial_allocate(cap_and_type, static_cast<size_type>(new_string_size));
    }

    // this funciion will not change the size, but the capacity or delta
    template <Need0 Term = Need0::Yes>
    void allocate_more(size_type new_append_size) noexcept {
        size_type old_delta = _core.idle_capacity();

        // if no need, do nothing, just update the size or delta
        if (old_delta >= new_append_size) [[likely]] {
            // just return and do nothing
            return;
        }
        auto old_size = size();
        // if need allocate a new buffer, always a external_buffer
        // do the allocation
        typename core_type::external_core new_external;
        if constexpr (core_type::use_std_allocator::value) {
            new_external =
              allocate_new_external_buffer(calculate_new_buffer_size(old_size + new_append_size, Growth), old_size);

        } else {
            new_external = allocate_new_external_buffer(calculate_new_buffer_size(old_size + new_append_size, Growth),
                                                        old_size, &_core.pmr_allocator);
        }

        // copy the old data to the new buffer
        std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), get_buffer(), old_size);
        // set the '\0' at the end of the buffer if needed;
        if constexpr (NullTerminated and Term == Need0::Yes) {
            reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
        }
        // deallocate the old buffer
        if (_core.is_external()) [[likely]] {
            if constexpr (core_type::use_std_allocator::value) {
                std::free(_core.external.get_buffer_ptr());
            } else {
                // the size is not used, so just set it to 0
                _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
            }
        }
        // replace the old external with the new one
        _core.external = new_external;
    }

   public:
    template <Need0 Term, bool NeedCopy>
    constexpr auto buffer_reserve(size_type new_cap) -> void {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        // check the new_cap is larger than the internal capacity, and larger than current cap
        auto [old_cap, old_size] = get_capacity_and_size();
        if (new_cap > old_cap) [[likely]] {
            // no growth was needed
            typename core_type::external_core new_external;

            auto new_buffer_type_and_size = calculate_new_buffer_size(new_cap);
            // allocate a new buffer
            if constexpr (core_type::use_std_allocator::value) {
                new_external = allocate_new_external_buffer(new_buffer_type_and_size, old_size);
            } else {
                new_external = allocate_new_external_buffer(new_buffer_type_and_size, old_size, &_core.pmr_allocator);
            }
            if constexpr (NeedCopy) {
                // copy the old data to the new buffer
                std::memcpy(reinterpret_cast<Char*>(new_external.c_str_ptr), get_buffer(), old_size);
            }
            if constexpr (NullTerminated and Term == Need0::Yes and NeedCopy) {
                reinterpret_cast<Char*>(new_external.c_str_ptr)[old_size] = '\0';
            }
            // deallocate the old buffer
            if (_core.is_external()) [[likely]] {
                if constexpr (core_type::use_std_allocator::value) {
                    std::free(_core.external.get_buffer_ptr());
                } else {
                    _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
                }
            }
            // replace the old external with the new one
            _core.external = new_external;
        }
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type <= new_core_type, "the core type should stay or grow");
#endif
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> size_type { return _core.idle_capacity(); }

    [[nodiscard]] auto get_capacity_and_size() const noexcept -> capacity_and_size<size_type> {
        return _core.get_capacity_and_size();
    }

    // increase the size, and won't change the capacity, so the internal/exteral'type or ptr will not change
    // you should always call the allocate_new_external_buffer first, then call this function
    [[gnu::always_inline]] inline void increase_size(size_type delta) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.increase_size_and_idle_and_set_term(delta);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

    [[gnu::always_inline]] inline void set_size(size_type new_size) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.set_size_and_idle_and_set_term(new_size);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

    [[gnu::always_inline]] inline void decrease_size(size_type delta) noexcept {
#ifndef NDEBUG
        auto origin_core_type = _core.get_core_type();
#endif
        _core.decrease_size_and_idle_and_set_term(delta);
#ifndef NDEBUG
        auto new_core_type = _core.get_core_type();
        Assert(origin_core_type == new_core_type, "the core type should not change");
#endif
    }

   public:
    constexpr small_string_buffer([[maybe_unused]] const Allocator& allocator) noexcept : _core{allocator} {
        if constexpr (not core_type::use_std_allocator::value) {
            Assert(check_the_allocator(), "the pmr default allocator is not allowed to be used in small_string");
        }
    }

    constexpr small_string_buffer(const small_string_buffer& other) = delete;
    constexpr small_string_buffer(const small_string_buffer& other,
                                  [[maybe_unused]] const Allocator& allocator) = delete;

    constexpr small_string_buffer(small_string_buffer&& other) noexcept = delete;

    // move constructor
    constexpr small_string_buffer(small_string_buffer&& other, [[maybe_unused]] const Allocator& /*unused*/) noexcept
        : _core(std::move(other._core)) {
        Assert(
          check_the_allocator(),
          "the pmr default allocator is not allowed to be used in small_string");  // very important, check the
                                                                                   // allocator incorrect injected into.
    }

    ~small_string_buffer() noexcept {
        if constexpr (core_type::use_std_allocator::value) {
            if (_core.is_external()) [[likely]] {
                std::free(reinterpret_cast<void*>(_core.external.get_buffer_ptr()));
            }
        } else {
            if (_core.is_external()) [[likely]] {
                // the size is not used, so just set it to 0
                _core.pmr_allocator.deallocate(_core.external.get_buffer_ptr(), 0);
            }
        }
    }

    constexpr auto swap(small_string_buffer& other) noexcept -> void { _core.swap(other._core); }

    constexpr auto operator=(const small_string_buffer& other) noexcept = delete;
    constexpr auto operator=(small_string_buffer&& other) noexcept = delete;

    [[nodiscard]] constexpr auto get_buffer() noexcept -> Char* { return _core.begin_ptr(); }

    [[nodiscard]] constexpr auto get_buffer() const noexcept -> const Char* {
        return const_cast<small_string_buffer*>(this)->_core.begin_ptr();
    }

    // will be fast than call get_buffer() + size(), it will waste many times for if checking
    [[nodiscard]] constexpr auto end() noexcept -> Char* { return _core.end_ptr(); }

    [[nodiscard]] constexpr auto size() const noexcept -> size_t { return _core.size(); }

    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type { return _core.capacity(); }

    [[nodiscard]] constexpr auto get_allocator() const -> Allocator {
        if constexpr (core_type::use_std_allocator::value) {
            return Allocator();
        } else {
            return _core.pmr_allocator;
        }
    }

    template <size_type Size>
    constexpr void static_assign(Char ch) {
        static_assert(Size > 0 and Size <= core_type::internal_buffer_size(), "the size should be greater than 0");
        // TODO(leo): combine the memset and set_size in same switch case
        memset(get_buffer(), ch, Size);
        set_size(Size);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto get_string_view() const noexcept -> std::string_view {
        return _core.get_string_view();
    }

};  // class small_string_buffer

// if NullTerminated is true, the string will be null terminated, and the size will be the length of the string
// if NullTerminated is false, the string will not be null terminated, and the size will still be the length of the
// string
template <typename Char,
          template <typename, template <typename, bool> class, class, class, bool, float> class Buffer =
            small_string_buffer,
          template <typename, bool> class Core = malloc_core, class Traits = std::char_traits<Char>,
          class Allocator = std::allocator<Char>, bool NullTerminated = true, float Growth = 1.5F>
class basic_small_string : private Buffer<Char, Core, Traits, Allocator, NullTerminated, Growth>
{
   public:
    // types
    using buffer_type = Buffer<Char, Core, Traits, Allocator, NullTerminated, Growth>;
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

    struct initialized_later
    {};

   public:
    constexpr basic_small_string(initialized_later, size_t new_string_size, const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        Assert((new_string_size + Core<Char, NullTerminated>::median_long_buffer_header_size()) <= std::numeric_limits<size_type>::max(),
               "the new_string_size should be less than the max value of size_type");
        buffer_type::initial_allocate(static_cast<size_type>(new_string_size));
    }

    constexpr basic_small_string(initialized_later, buffer_type_and_size<size_type> type_and_size,
                                 size_t new_string_size, const Allocator& allocator = Allocator())
        : buffer_type(allocator) {
        buffer_type::initial_allocate(type_and_size, new_string_size);
    }

    static constexpr auto create_uninitialized_string(size_t new_string_size, const Allocator& allocator = Allocator())
      -> basic_small_string {
        return basic_small_string(initialized_later{}, new_string_size, allocator);
    }

    constexpr basic_small_string([[maybe_unused]] const Allocator& allocator = Allocator()) noexcept
        : buffer_type(allocator) {}

    constexpr basic_small_string(size_t count, Char ch, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, count, allocator) {
        memset(data(), ch, count);
    }

    // copy constructor

    constexpr basic_small_string(const basic_small_string& other)
        : basic_small_string(initialized_later{}, other.size(), other.get_allocator()) {
        std::memcpy(data(), other.data(), other.size());
    }

    constexpr basic_small_string(const basic_small_string& other, [[maybe_unused]] const Allocator& allocator)
        : basic_small_string(initialized_later{}, other.size(), other.get_allocator()) {
        std::memcpy(data(), other.data(), other.size());
    }

    constexpr basic_small_string(const basic_small_string& other, size_type pos,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(other.substr(pos)) {}

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

    constexpr basic_small_string(const Char* s, size_t count, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, count, allocator) {
        std::memcpy(data(), s, count);
    }

    constexpr basic_small_string(const Char* s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(s, Traits::length(s), allocator) {}

    template <class InputIt>
    constexpr basic_small_string(InputIt first, InputIt last, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, static_cast<size_t>(std::distance(first, last)), allocator) {
        std::copy(first, last, begin());
    }

    constexpr basic_small_string(std::initializer_list<Char> ilist,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, ilist.size(), allocator) {
        std::copy(ilist.begin(), ilist.end(), begin());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr basic_small_string(const StringViewLike& s, [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, s.size(), allocator) {
        std::copy(s.begin(), s.end(), begin());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    constexpr basic_small_string(const StringViewLike& s, size_type pos, size_type n,
                                 [[maybe_unused]] const Allocator& allocator = Allocator())
        : basic_small_string(initialized_later{}, n, allocator) {
        std::copy(s.begin() + pos, s.begin() + pos + n, begin());
    }

    basic_small_string(std::nullptr_t) = delete;

    ~basic_small_string() noexcept = default;

    // get_allocator
    [[nodiscard]] constexpr auto get_allocator() const -> Allocator { return buffer_type::get_allocator(); }

    // get_core_type
    [[nodiscard]] constexpr auto get_core_type() const -> size_type { return buffer_type::get_core_type(); }

    // copy assignment
    auto operator=(const basic_small_string& other) -> basic_small_string& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        // assign the other to this
        return assign(other.data(), other.size());
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

    auto operator=(const Char* s) -> basic_small_string& { return assign(s, Traits::length(s)); }

    auto operator=(Char ch) -> basic_small_string& {
        this->template static_assign<1>(ch);
        return *this;
    }

    auto operator=(std::initializer_list<Char> ilist) -> basic_small_string& {
        return assign(ilist.begin(), ilist.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator=(const StringViewLike& s) -> basic_small_string& {
        return assign(s.data(), s.size());
    }

    auto operator=(std::nullptr_t) -> basic_small_string& = delete;

    // assign
    template <bool Safe = true>
    auto assign(size_type count, Char ch) -> basic_small_string& {
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        auto* buffer = buffer_type::get_buffer();
        memset(buffer, ch, count);
        // do not use resize, to avoid the if checking
        buffer_type::set_size(count);

        return *this;
    }

    template <bool Safe = true>
    [[gnu::always_inline]] auto assign(const basic_small_string& str) -> basic_small_string& {
        if (this == &str) [[unlikely]] {
            return *this;
        }
        return assign<Safe>(str.data(), str.size());
    }

    template <bool Safe = true>
    auto assign(const basic_small_string& str, size_type pos, size_type count = npos) -> basic_small_string& {
        auto other_size = str.size();
        if (pos > other_size) [[unlikely]] {
            throw std::out_of_range("assign: input pos is out of range");
        }
        if (this == &str) [[unlikely]] {
            // erase the other part
            count = std::min<size_type>(count, other_size - pos);
            auto* buffer = buffer_type::get_buffer();
            std::memmove(buffer, buffer + pos, count);
            buffer_type::set_size(count);
            return *this;
        }
        return assign<Safe>(str.data() + pos, std::min<size_type>(count, other_size - pos));
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

    template <bool Safe = true>
    auto assign(const Char* s, size_type count) -> basic_small_string& {
        Assert(s != nullptr, "assign: s should not be nullptr");
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        auto* buffer = buffer_type::get_buffer();
        // use memmove to handle the overlap
        std::memmove(buffer, s, count);
        buffer_type::set_size(count);
        return *this;
    }

    template <bool Safe = true>
    auto assign(const Char* s, size_t count) -> basic_small_string& {
        Assert((count <= Core<Char, NullTerminated>::max_long_buffer_size()),
               "assign: count should be less than the max value of size_type");
        return assign(s, static_cast<size_type>(count));
    }

    template <bool Safe = true>
    auto assign(const Char* s) -> basic_small_string& {
        return assign<Safe>(s, Traits::length(s));
    }

    template <class InputIt, bool Safe = true>
    auto assign(InputIt first, InputIt last) -> basic_small_string& {
        auto count = std::distance(first, last);
        if constexpr (Safe) {
            this->template buffer_reserve<buffer_type::Need0::No, false>(count);
        }
        std::copy(first, last, begin());
        buffer_type::set_size(count);
        return *this;
    }

    template <bool Safe = true>
    auto assign(std::initializer_list<Char> ilist) -> basic_small_string& {
        return assign<decltype(ilist.begin()), Safe>(ilist.begin(), ilist.end());
    }

    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and
                 not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto assign(const StringViewLike& s) -> basic_small_string& {
        return assign<Safe>(s.data(), s.size());
    }

    template <class StringViewLike, bool Safe = true>
        requires(std::is_convertible_v<const StringViewLike&, const Char*> and
                 not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    auto assign(const StringViewLike& s, size_type pos, size_type n = npos) -> basic_small_string& {
        return assign<Safe>(s + pos, std::min<size_type>(n, s.size() - pos));
    }

    // element access
    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(buffer_type::get_buffer() + pos);
    }

    template <bool Safe = true>
    [[nodiscard]] constexpr auto at(size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("at: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    template <bool Safe = true>
    constexpr auto operator[](size_type pos) noexcept(not Safe) -> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(buffer_type::get_buffer() + pos);
    }

    template <bool Safe = true>
    constexpr auto operator[](size_type pos) const noexcept(not Safe) -> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("operator []: pos is out of range");
            }
        }
        return *(data() + pos);
    }

    [[nodiscard]] constexpr auto front() noexcept -> reference { return *reinterpret_cast<Char*>(data()); }

    [[nodiscard]] constexpr auto front() const noexcept -> const_reference { return *data(); }

    [[nodiscard]] constexpr auto back() noexcept -> reference {
        auto size = buffer_type::size();
        auto buffer = buffer_type::get_buffer();
        return buffer[size - 1];
    }

    [[nodiscard]] constexpr auto back() const noexcept -> const_reference {
        auto [buffer, size] = buffer_type::get_buffer_and_size();
        return buffer[size - 1];
    }

    // enable if the NullTerminated is true
    // make c_str() only available when NullTerminated is true
    template <bool U = NullTerminated, typename = std::enable_if_t<U, std::true_type>>
    [[nodiscard, gnu::always_inline]] auto c_str() const noexcept -> const Char* {
        return buffer_type::get_buffer();
    }

    [[nodiscard, gnu::always_inline]] auto data() const noexcept -> const Char* { return buffer_type::get_buffer(); }
    [[nodiscard, gnu::always_inline]] auto data() noexcept -> Char* { return buffer_type::get_buffer(); }

    // Iterators
    [[nodiscard]] constexpr auto begin() noexcept -> iterator { return data(); }

    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator { return data(); }

    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return data(); }

    [[nodiscard]] constexpr auto end() noexcept -> iterator { return buffer_type::end(); }

    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        return const_cast<basic_small_string*>(this)->end();
    }

    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
        return const_cast<basic_small_string*>(this)->end();
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

    [[nodiscard, gnu::always_inline]] constexpr auto size() const noexcept -> size_t { return buffer_type::size(); }

    [[nodiscard, gnu::always_inline]] constexpr auto length() const noexcept -> size_type {
        return buffer_type::size();
    }

    /**
     * @brief The maximum number of elements that can be stored in the string.
     * the buffer largest size is 1 << 15, the cap occupy 2 bytes, so the max size is 1 << 15 - 2, is 65534
     */
    [[nodiscard, gnu::always_inline]] constexpr auto max_size() const noexcept -> size_type {
        if constexpr (NullTerminated) {
            return std::numeric_limits<size_type>::max() - sizeof(struct buffer_type_and_size<size_type>) - 1;
        } else {
            return std::numeric_limits<size_type>::max() - sizeof(struct buffer_type_and_size<size_type>);
        }
    }

    constexpr auto reserve(size_type new_cap) -> void {
        this->template buffer_reserve<buffer_type::Need0::Yes, true>(new_cap);
    }

    // it was a just exported function, should not be called frequently, it was a little bit slow in some case.
    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const noexcept -> size_type {
        return buffer_type::capacity();
    }

    constexpr auto shrink_to_fit() -> void {
#ifndef NDEBUG
        auto origin_core_type = buffer_type::get_core_type();
#endif

        auto [cap, size] = buffer_type::get_capacity_and_size();
        Assert(cap >= size, "cap should always be greater or equal to size");
        auto cap_and_type = buffer_type::calculate_new_buffer_size(size);
        if (cap > cap_and_type.buffer_size) {  // the cap is larger than the best cap, so need to shrink
            basic_small_string new_str{initialized_later{}, cap_and_type, size, buffer_type::get_allocator()};
            std::memcpy(new_str.data(), data(), size);
            swap(new_str);
        }

#ifndef NDEBUG
        auto new_core_type = buffer_type::get_core_type();
        Assert(origin_core_type >= new_core_type, "the core type should stay or shrink");
#endif
    }

    // modifiers
    /*
     * clear the string, do not erase the memory, just set the size to 0, and set the first char to '\0' if
     * NullTerminated is true
     * @detail : do not use set_size(0), it will be a do some job for some other sizes.
     */
    constexpr auto clear() noexcept -> void { buffer_type::set_size(0); }

    // 1
    template <bool Safe = true>
    constexpr auto insert(size_type index, size_type count, Char ch) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if (index > size()) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }

        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        auto old_size = size();
        auto* buffer = buffer_type::get_buffer();
        if (index < old_size) [[likely]] {
            // if index == old_size, do not need memmove
            std::memmove(buffer + index + count, buffer + index, old_size - index);
        }
        std::memset(buffer + index, ch, count);
        buffer_type::increase_size(count);
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
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        auto old_size = size();
        if (index > old_size) [[unlikely]] {
            throw std::out_of_range("index is out of range");
        }
        // check if the capacity is enough
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        auto* buffer = buffer_type::get_buffer();
        if (index < old_size) [[likely]] {
            std::memmove(buffer + index + count, buffer + index, old_size - index);
        }
        // set the new data
        std::memcpy(buffer + index, str, count);
        buffer_type::increase_size(count);
        return *this;
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& other) -> basic_small_string& {
        return insert<Safe>(index, other.data(), other.size());
    }

    template <bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& str, size_type other_index, size_type count = npos)
      -> basic_small_string& {
        return insert<Safe>(index, str.substr(other_index, count));
    }

    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, Char ch) -> iterator {
        return insert<Safe>(pos, 1, ch);
    }

    template <bool Safe = true>
    constexpr auto insert(const_iterator pos, size_type count, Char ch) -> iterator {
        size_type index = pos - begin();
        insert(index, count, ch);
        return begin() + index;
    }

    template <class InputIt, bool Safe = true>
    constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        // static check the type of the input iterator
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        // calculate the count
        size_type count = std::distance(first, last);
        if (count == 0) [[unlikely]] {
            // do nothing
            return const_cast<iterator>(pos);
        }
        size_type index = pos - begin();
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        if (index < size()) [[likely]] {
            // move the data to the new position
            std::memmove(data() + index + count, data() + index, static_cast<size_type>(size() - index));
        }
        // copy the new data
        std::copy(first, last, data() + index);
        buffer_type::increase_size(count);
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
    constexpr auto insert(const_iterator pos, const StringViewLike& t, size_type pos2, size_type count = npos)
      -> iterator {
        return insert<Safe>(pos, t.substr(pos2, count));
    }

    // erase the data from the index to the end
    // NOTICE: the erase function will never change the capacity of the string or the external flag
    auto erase(size_type index = 0, size_t count = npos) -> basic_small_string& {
        auto old_size = this->size();
        if (index > old_size) [[unlikely]] {
            throw std::out_of_range("erase: index is out of range");
        }
        // calc the real count
        size_type real_count = std::min(count, old_size - index);
        if (real_count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        auto new_size = old_size - real_count;
        Assert(old_size >= index + real_count, "old size should be greater or equal than the index + real count");
        auto right_size = old_size - index - real_count;
        // if the count is greater than 0, then move right part of  the data to the new position
        char* buffer_ptr = buffer_type::get_buffer();
        if (right_size > 0) {
            // memmove the data to the new position
            std::memmove(buffer_ptr + index, buffer_ptr + index + real_count, static_cast<size_type>(right_size));
        }
        // set the new size
        buffer_type::set_size(new_size);
        return *this;
    }

    constexpr auto erase(const_iterator first) -> iterator {
        // the first must be valid, and of the string.
        auto index = first - begin();
        return erase(index, 1).begin() + index;
    }

    constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
        auto index = first - begin();
        return erase(index, static_cast<size_t>(last - first)).begin() + index;
    }

    template <bool Safe = true>
    [[gnu::always_inline]] inline void push_back(Char c) {
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(1UL);
        }
        data()[size()] = c;
        buffer_type::increase_size(1);
    }

    void pop_back() { buffer_type::decrease_size(1); }

    template <bool Safe = true>
    constexpr auto append(size_t count, Char c) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        // by now, the capacity is enough
        std::memset(end(), c, count);
        buffer_type::increase_size(count);
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other) -> basic_small_string& {
        if (other.empty()) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(other.size());
        }
        // by now, the capacity is enough
        // size() function maybe slower than while the size is larger than 4k, so store it.
        auto other_size = other.size();
        std::memcpy(end(), other.data(), other_size);
        buffer_type::increase_size(other_size);
        return *this;
    }

    template <bool Safe = true>
    constexpr auto append(const basic_small_string& other, size_type pos, size_type count = npos)
      -> basic_small_string& {
        if (pos > other.size()) [[unlikely]] {
            throw std::out_of_range("append: pos is out of range");
        }
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        count = std::min(count, other.size() - pos);
        return append<Safe>(other.data() + pos, count);
    }

    template <bool Safe = true>
    constexpr auto append(const Char* s, size_t count) -> basic_small_string& {
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }

        std::memcpy(end(), s, count);
        buffer_type::increase_size(count);
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
        if (count == 0) [[unlikely]] {
            // do nothing
            return *this;
        }
        if constexpr (Safe) {
            this->template allocate_more<buffer_type::Need0::No>(count);
        }
        std::copy(first, last, end());
        buffer_type::increase_size(count);
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
    constexpr auto append(const StringViewLike& s, size_t pos, size_t count = npos) -> basic_small_string& {
        if (count == npos or count + pos > s.size()) {
            count = s.size() - pos;
        }
        return append<Safe>(s.data() + pos, count);
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
    auto replace(size_t pos, size_t count, size_t count2, Char ch) -> basic_small_string& {
        auto [cap, old_size] = buffer_type::get_capacity_and_size();
        // check the pos is not out of range
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        if (count2 == 0) [[unlikely]] {
            // just erase the data
            return erase(pos, count);
        }
        count = std::min(count, old_size - pos);

        if ((count >= old_size - pos) and count2 <= (cap - pos)) {
            // copy the data to pos, and no need to move the right part
            std::memset(buffer_type::get_buffer() + pos, ch, count2);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }

        if (count == count2) {
            // just replace
            std::memset(buffer_type::get_buffer() + pos, ch, count2);
        }

        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, old_size - count + count2, buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::memcpy(p, data(), pos);
        p += pos;
        std::memset(p, ch, count2);
        p += count2;
        std::copy(begin() + pos + count, end(), p);
        return *this;
    }

    // replace [pos, pos+count] with the range [cstr, cstr + count2]
    template <bool Safe = true>
    auto replace(size_t pos, size_t count, const Char* str, size_t count2) -> basic_small_string& {
        auto [cap, old_size] = buffer_type::get_capacity_and_size();
        if (pos > old_size) [[unlikely]] {
            throw std::out_of_range("replace: pos is out of range");
        }
        if (count2 == 0) [[unlikely]] {
            return erase(pos, count);
        }
        count = std::min(count, old_size - pos);

        if ((count >= old_size - pos) and count2 <= (cap - pos)) {  // count == npos still >= old_size - pos.
            // copy the data to pos, and no need to move the right part
            std::memcpy(buffer_type::get_buffer() + pos, str, count2);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }
        if (count == count2) {
            // just replace
            std::memcpy(buffer_type::get_buffer() + pos, str, count2);
        }
        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, old_size - count + count2, buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::memcpy(p, data(), pos);
        p += pos;
        // copy the new data
        std::memcpy(p, str, count2);
        p += count2;
        std::copy(begin() + pos + count, end(), p);
        *this = std::move(ret);
        return *this;
    }

    auto replace(size_t pos, size_t count, const basic_small_string& other) -> basic_small_string& {
        return replace(pos, count, other.data(), other.size());
    }

    auto replace(const_iterator first, const_iterator last, const basic_small_string& other) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), other.data(),
                       other.size());
    }

    auto replace(size_t pos, size_t count, const basic_small_string& other, size_t pos2, size_t count2 = npos)
      -> basic_small_string& {
        if (count2 == npos) {
            count2 = other.size() - pos2;
        }
        return replace(pos, count, other.substr(pos2, count2));
    }

    auto replace(const_iterator first, const_iterator last, const Char* cstr, size_t count2) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), cstr, count2);
    }

    auto replace(size_t pos, size_t count, const Char* cstr) -> basic_small_string& {
        return replace(pos, count, cstr, traits_type::length(cstr));
    }

    auto replace(const_iterator first, const_iterator last, const Char* cstr) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), cstr,
                       traits_type::length(cstr));
    }

    auto replace(const_iterator first, const_iterator last, size_t count, Char ch) -> basic_small_string& {
        return replace(static_cast<size_t>(first - begin()), static_cast<size_t>(last - first), count, ch);
    }

    template <class InputIt>
    auto replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2) -> basic_small_string& {
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        Assert(first2 <= last2, "the range is valid");
        auto pos = std::distance(begin(), first);
        auto count = std::distance(first, last);
        if (count < 0) [[unlikely]] {
            throw std::invalid_argument("the range is invalid");
        }
        auto count2 = std::distance(first2, last2);
        if (count2 == 0) [[unlikely]] {
            // just delete the right part
            return erase(first, last);
        }

        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (last == end() and count2 <= (cap - pos)) {
            // copy the data to pos, and no need to move the right part
            std::copy(first2, last2, data() + pos);
            auto new_size = pos + count2;
            buffer_type::set_size(new_size);
            return *this;
        }

        if (count == count2) {
            // just replace
            std::copy(first2, last2, data() + pos);
        }

        // else, the count != count2, need to move the right part
        // init a new string
        basic_small_string ret{initialized_later{}, old_size - count + count2, buffer_type::get_allocator()};
        Char* p = ret.data();
        // copy the left party
        std::copy(begin(), first, p);
        p += pos;
        // copy the new data
        std::copy(first2, last2, p);
        p += count2;
        std::copy(last, end(), p);
        *this = std::move(ret);
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
    auto replace(size_type pos, size_type count, const StringViewLike& view, size_type pos2, size_type count2 = npos)
      -> basic_small_string& {
        if (count2 == npos) {
            count2 = view.size() - pos2;
        }
        return replace(pos, count, view.data() + pos, count2);
    }

    auto copy(Char* dest, size_t count = npos, size_t pos = 0) const -> size_type {
        auto current_size = size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("copy's pos > size()");
        }
        if ((count == npos) or (pos + count > current_size)) {
            count = current_size - pos;
        }
        std::memcpy(dest, data() + pos, count);
        return count;
    }

    auto resize(size_t count) -> void {
        Assert(count < std::numeric_limits<size_type>::max(), "count is too large");
        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (count <= old_size) [[likely]] {
            // if count == old_size, just set the size
            buffer_type::set_size(count);
            return;
        }
        if (count > cap) {
            this->template buffer_reserve<buffer_type::Need0::No, true>(count);
        }
        std::memset(data() + old_size, '\0', count - size());
        buffer_type::set_size(count);
        return;
    }

    auto resize(size_t count, Char ch) -> void {
        // if the count is less than the size, just set the size
        Assert(count < std::numeric_limits<size_type>::max(), "count is too large");
        auto [cap, old_size] = buffer_type::get_capacity_and_size();

        if (count <= old_size) [[likely]] {
            // if count == old_size, just set the size
            buffer_type::set_size(count);
            return;
        }
        if (count > cap) {
            // if the count is greater than the size, need to reserve
            this->template buffer_reserve<buffer_type::Need0::No, true>(count);
        }
        // by now, the capacity is enough
        std::memset(data() + old_size, ch, count - old_size);
        buffer_type::set_size(count);
        return;
    }

    void swap(basic_small_string& other) noexcept {
        // just a temp variable to avoid self-assignment, std::swap will do the same thing, maybe faster?
        buffer_type::swap(other);
    }

    // search
    constexpr auto find(const Char* str, size_t pos, size_t count) const -> size_t {
        auto current_size = buffer_type::size();
        if (count == 0) [[unlikely]] {
            return pos <= current_size ? pos : npos;
        }
        if (pos >= current_size) [[unlikely]] {
            return npos;
        }

        const auto elem0 = str[0];
        const auto* data_ptr = buffer_type::get_buffer();
        const auto* first_ptr = data_ptr + pos;
        const auto* const last_ptr = data_ptr + current_size;
        auto len = current_size - pos;

        while (len >= count) {
            first_ptr = traits_type::find(first_ptr, len - count + 1, elem0);
            if (first_ptr == nullptr) {
                return npos;
            }
            if (traits_type::compare(first_ptr, str, count) == 0) {
                return size_t(first_ptr - data_ptr);
            }
            len = static_cast<size_type>(last_ptr - ++first_ptr);
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find(const Char* needle, size_t pos = 0) const -> size_t {
        return find(needle, pos, traits_type::length(needle));
    }

    [[nodiscard]] constexpr auto find(const basic_small_string& other, size_t pos = 0) const -> size_t {
        return find(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find(const StringViewLike& view, size_t pos = 0) const -> size_t {
        return find(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find(Char ch, size_t pos = 0) const -> size_t {
        auto* found = traits_type::find(data() + pos, size() - pos, ch);
        return found == nullptr ? npos : size_t(found - data());
    }

    [[nodiscard]] constexpr auto rfind(const Char* str, size_t pos, size_t str_length) const -> size_t {
        auto current_size = buffer_type::size();
        if (str_length <= current_size) [[likely]] {
            pos = std::min(pos, current_size - str_length);
            const auto* buffer_ptr = buffer_type::get_buffer();
            do {
                if (traits_type::compare(buffer_ptr + pos, str, str_length) == 0) {
                    return pos;
                }
            } while (pos-- > 0);
        }
        return npos;
    }

    [[nodiscard]] constexpr auto rfind(const Char* needle, size_t pos = 0) const -> size_t {
        return rfind(needle, pos, traits_type::length(needle));
    }

    [[nodiscard]] constexpr auto rfind(const basic_small_string& other, size_t pos = 0) const -> size_t {
        return rfind(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto rfind(const StringViewLike& view, size_t pos = 0) const -> size_t {
        return rfind(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto rfind(Char ch, size_t pos = npos) const -> size_t {
        auto current_size = size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        if (current_size > 0) [[likely]] {
            if (--current_size > pos) {
                current_size = pos;
            }
            for (++current_size; current_size-- > 0;) {
                if (traits_type::eq(buffer_ptr[current_size], ch)) {
                    return current_size;
                }
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_t pos, size_t count) const -> size_t {
        auto current_size = this->size();
        auto buffer_ptr = buffer_type::get_buffer();
        for (; count > 0 && pos < current_size; ++pos) {
            if (traits_type::find(str, count, buffer_ptr[pos]) != nullptr) {
                return pos;
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_first_of(const Char* str, size_t pos = 0) const -> size_t {
        return find_first_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_first_of(const basic_small_string& other, size_t pos = 0) const -> size_t {
        return find_first_of(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_of(const StringViewLike& view, size_t pos = 0) const -> size_t {
        return find_first_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_first_of(Char ch, size_t pos = 0) const -> size_t {
        return find_first_of(&ch, pos, 1);
    }

    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_t pos, size_t count) const -> size_t {
        auto current_size = this->size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        for (; pos < current_size; ++pos) {
            if (traits_type::find(str, count, buffer_ptr[pos]) == nullptr) {
                return pos;
            }
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_first_not_of(const Char* str, size_t pos = 0) const -> size_t {
        return find_first_not_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_first_not_of(const basic_small_string& other, size_t pos = 0) const -> size_t {
        return find_first_not_of(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_first_not_of(const StringViewLike& view, size_t pos = 0) const -> size_t {
        return find_first_not_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_first_not_of(Char ch, size_t pos = 0) const -> size_t {
        return find_first_not_of(&ch, pos, 1);
    }

    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_t pos, size_t count) const -> size_t {
        auto current_size = this->size();
        const auto* buffer_ptr = buffer_type::get_buffer();
        if (current_size && count) [[likely]] {
            if (--current_size > pos) {
                current_size = pos;
            }
            do {
                if (traits_type::find(str, count, buffer_ptr[current_size]) != nullptr) {
                    return current_size;
                }
            } while (current_size-- != 0);
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_last_of(const Char* str, size_t pos = npos) const -> size_t {
        return find_last_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_last_of(const basic_small_string& other, size_t pos = npos) const -> size_t {
        return find_last_of(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_last_of(const StringViewLike& view, size_t pos = npos) const -> size_t {
        return find_last_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_last_of(Char ch, size_t pos = npos) const -> size_t {
        return find_last_of(&ch, pos, 1);
    }

    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_t pos, size_t count) const -> size_t {
        auto current_size = buffer_type::size();
        if (current_size > 0) {
            if (--current_size > pos) {
                current_size = pos;
            }
            do {
                if (traits_type::find(str, count, at(current_size)) == nullptr) {
                    return current_size;
                }
            } while (current_size-- != 0);
        }
        return npos;
    }

    [[nodiscard]] constexpr auto find_last_not_of(const Char* str, size_t pos = npos) const -> size_t {
        return find_last_not_of(str, pos, traits_type::length(str));
    }

    [[nodiscard]] constexpr auto find_last_not_of(const basic_small_string& other, size_t pos = npos) const -> size_t {
        return find_last_not_of(other.data(), pos, other.size());
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto find_last_not_of(const StringViewLike& view, size_t pos = npos) const -> size_t {
        return find_last_not_of(view.data(), pos, view.size());
    }

    [[nodiscard]] constexpr auto find_last_not_of(Char ch, size_t pos = npos) const -> size_t {
        return find_last_not_of(&ch, pos, 1);
    }

    // Operations
    [[nodiscard]] constexpr auto compare(const basic_small_string& other) const noexcept -> int {
        auto this_size = size();
        auto other_size = other.size();
        auto r = traits_type::compare(data(), other.data(), std::min(this_size, other_size));
        return r != 0 ? r : this_size > other_size ? 1 : this_size < other_size ? -1 : 0;
    }
    [[nodiscard]] constexpr auto compare(size_t pos, size_t count, const basic_small_string& other) const -> int {
        return compare(pos, count, other.data(), other.size());
    }
    [[nodiscard]] constexpr auto compare(size_t pos1, size_t count1, const basic_small_string& other, size_t pos2,
                                         size_t count2 = npos) const -> int {
        if (pos2 > other.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min(count2, other.size() - pos2);
        return compare(pos1, count1, other.data() + pos2, count2);
    }
    [[nodiscard]] constexpr auto compare(const Char* str) const noexcept -> int {
        return compare(0, size(), str, traits_type::length(str));
    }
    [[nodiscard]] constexpr auto compare(size_t pos, size_t count, const Char* str) const -> int {
        return compare(pos, count, str, traits_type::length(str));
    }
    [[nodiscard]] constexpr auto compare(size_t pos1, size_t count1, const Char* str, size_t count2) const -> int {
        // make sure the pos1 is valid
        if (pos1 > size()) [[unlikely]] {
            throw std::out_of_range("compare: pos1 is out of range");
        }
        // make sure the count1 is valid, if count1 > size() - pos1, set count1 = size() - pos1
        count1 = std::min<size_t>(count1, size() - pos1);
        auto r = traits_type::compare(data() + pos1, str, std::min<size_t>(count1, count2));
        return r != 0 ? r : count1 > count2 ? 1 : count1 < count2 ? -1 : 0;
    }

    template <class StringViewLike>
        requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
                 !std::is_convertible_v<const StringViewLike&, const Char*>)
    [[nodiscard]] constexpr auto compare(const StringViewLike& view) const noexcept -> int {
        auto this_size = size();
        auto view_size = view.size();
        auto r = traits_type::compare(data(), view.data(), std::min<size_t>(this_size, view_size));
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
    [[nodiscard]] constexpr auto compare(size_t pos1, size_t count1, const StringViewLike& view, size_t pos2,
                                         size_t count2 = npos) const -> int {
        if (pos2 > view.size()) [[unlikely]] {
            throw std::out_of_range("compare: pos2 is out of range");
        }
        count2 = std::min<size_type>(count2, view.size() - pos2);
        return compare(pos1, count1, view.data() + pos2, count2);
    }

    [[nodiscard]] constexpr auto starts_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(0, view.size(), view.data(), view.size()) == 0;
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

    [[nodiscard]] constexpr auto substr(size_t pos = 0, size_t count = npos) const& -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }

        return basic_small_string{data() + pos, std::min<size_t>(count, current_size - pos), get_allocator()};
    }

    [[nodiscard]] constexpr auto substr(size_t pos = 0, size_t count = npos) && -> basic_small_string {
        auto current_size = this->size();
        if (pos > current_size) [[unlikely]] {
            throw std::out_of_range("substr: pos is out of range");
        }
        erase(0, pos);
        if (count < current_size - pos) {
            resize(count);
        }
        // or just return *this
        return std::move(*this);
    }

    // convert to std::basic_string_view, to support C++11 compatibility. and it's noexcept.
    // and small_string can be converted to std::basic_string_view implicity, so third party String can be converted
    // from small_string.
    [[nodiscard, gnu::always_inline]] inline operator std::basic_string_view<Char, Traits>() const noexcept {
        return buffer_type::get_string_view();
    }
};  // class basic_small_string

// input/output
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<<(std::basic_ostream<Char, Traits>& os,
                       const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& str)
  -> std::basic_ostream<Char, Traits>& {
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}

template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>>(std::basic_istream<Char, Traits>& is,
                       basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& str)
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
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 2
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = lhs;
    result.append(rhs);
    return result;
}

// 3
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      Char rhs) -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.push_back(rhs);
    return result;
}

// 5
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const Char* lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) + rhs;
}

// 6
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(Char lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) + rhs;
}

// 8
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> result(std::move(lhs));
    result.append(std::move(rhs));
    return result;
}

// 9
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 10
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs,
                      const Char* rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.append(rhs);
    return result;
}

// 11
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& lhs, Char rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth> {
    auto result = std::move(lhs);
    result.push_back(rhs);
    return result;
}

// 13
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    auto result = lhs;
    result.append(std::move(rhs));
    return result;
}

// 14
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(const Char* lhs,
                      basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(lhs, rhs.get_allocator()) +
           std::move(rhs);
}

// 15
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator+(Char lhs, basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>&& rhs)
  -> basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated> {
    return basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>(1, lhs, rhs.get_allocator()) +
           std::move(rhs);
}

// comparison operators
// small_string <=> small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return lhs.compare(rhs) <=> 0;
}

// small_string == small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// small_string != small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs == rhs);
}

// small_string > small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

// small_string < small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// small_string >= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs < rhs);
}

// small_string <= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return not(lhs > rhs);
}

// basic_string compatibility routines
// small_string <=> basic_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

// basic_string <=> small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=>(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return 0 <=> rhs.compare(0, rhs.size(), lhs.data(), lhs.size());
}

// small_string <=> Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        const Char* rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs, Traits::length(rhs)) <=> 0;
}

// Char* <=> small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    auto rhs_size = rhs.size();
    auto lhs_size = Traits::length(lhs);
    auto r = Traits::compare(lhs, rhs.data(), std::min<decltype(rhs_size)>(lhs_size, rhs_size));
    return r != 0 ? r <=> 0 : lhs_size <=> rhs_size;
}

// small_string <=> std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                        std::string_view rhs) noexcept -> std::strong_ordering {
    return lhs.compare(0, lhs.size(), rhs.data(), rhs.size()) <=> 0;
}

// std::string_view <=> small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=>(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept
  -> std::strong_ordering {
    return 0 <=> rhs.compare(0, rhs.size(), lhs.data(), lhs.size());
}

// small_string == basic_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// basic_string == small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator==(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// small_string == Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.size() == Traits::length(lhs) and std::equal(rhs.begin(), rhs.end(), lhs);
}

// small_string == Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return lhs.size() == Traits::length(rhs) and std::equal(lhs.begin(), lhs.end(), rhs);
}

// small_string == std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// std::string_view == small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator==(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return lhs.size() == rhs.size() and std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

// small_string != basic_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// basic_string != small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator!=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// small_string != Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// Char* != small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// small_string != std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// std::string_view != small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator!=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs == rhs);
}

// small_string < basic_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// basic_string < small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

// small_string < Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// Char* < small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

// small_string < std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      std::string_view rhs) noexcept -> bool {
    return lhs.compare(rhs) < 0;
}

// std::string_view < small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) > 0;
}

// basic_string > small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

// Char* > small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

// small_string > Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      const Char* rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

// small_string > std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                      std::string_view rhs) noexcept -> bool {
    return lhs.compare(rhs) > 0;
}

// std::string_view > small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return rhs.compare(lhs) < 0;
}

// small_string <= basic_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// basic_string <= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator<=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// small_string <= Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// Char* <= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// small_string <= std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// std::string_view <= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator<=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs > rhs);
}

// basic_string >= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const std::basic_string<Char, Traits, STDAllocator>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// basic_string >= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, class STDAllocator, bool NullTerminated,
          float Growth>
inline auto operator>=(
  const std::basic_string<Char, Traits, STDAllocator>& lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// small_string >= Char*
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       const Char* rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// Char* >= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  const Char* lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// small_string >= std::string_view
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& lhs,
                       std::string_view rhs) noexcept -> bool {
    return !(lhs < rhs);
}

// std::string_view >= small_string
template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
inline auto operator>=(
  std::string_view lhs,
  const basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>& rhs) noexcept -> bool {
    return !(lhs < rhs);
}

using small_string = basic_small_string<char>;
using small_byte_string =
  basic_small_string<char, small_string_buffer, malloc_core, std::char_traits<char>, std::allocator<char>, false>;

static_assert(sizeof(small_string) == 8, "small_string should be same as a pointer");
static_assert(sizeof(small_byte_string) == 8, "small_byte_string should be same as a pointer");

template <typename String, typename T>
auto to_small_string(T value) -> String {
    auto size = std::formatted_size("{}", value);
    auto formatted = String::create_uninitialized_string(size);
    std::format_to(formatted.data(), "{}", value);
    return formatted;
}

template <typename String>
auto to_small_string(const char* value) -> String {
    return String{value};
}

template <typename String>
auto to_small_string(const std::string& value) -> String {
    return String{value};
}

template <typename String>
auto to_small_string(std::string_view view) -> String {
    return String{view};
}

}  // namespace stdb::memory

// decl the formatter of small_string

template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
struct std::formatter<stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>>
    : formatter<string_view>
{
    using formatter<std::string_view>::parse;

    auto format(const stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated>& str,
                std::format_context& ctx) const noexcept {
        return formatter<string_view>::format({str.data(), str.size()}, ctx);
    }
};

namespace stdb::memory::pmr {
using small_string = basic_small_string<char, small_string_buffer, pmr_core, std::char_traits<char>,
                                        std::pmr::polymorphic_allocator<char>, true>;
using small_byte_string = basic_small_string<char, small_string_buffer, pmr_core, std::char_traits<char>,
                                             std::pmr::polymorphic_allocator<char>, false>;

static_assert(sizeof(small_string) == 16, "small_string should be same as a pointer");

template <typename String, typename T>
auto to_small_string(T value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    auto size = std::formatted_size("{}", value);
    auto formatted = String::create_uninitialized_string(size, allocator);
    std::format_to(formatted.data(), "{}", value);
    return formatted;
}

template <typename String>
auto to_small_string(const char* value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{value, std::strlen(value), allocator};
}

template <typename String>
auto to_small_string(const std::string& value, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{value, allocator};
}

template <typename String>
auto to_small_string(std::string_view view, std::pmr::polymorphic_allocator<char> allocator) -> String {
    return String{view, allocator};
}

}  // namespace stdb::memory::pmr

namespace std {

template <typename Char,
          template <typename, template <class, bool> class, class T, class A, bool N, float G> class Buffer,
          template <typename, bool> class Core, class Traits, class Allocator, bool NullTerminated, float Growth>
struct hash<stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>>
{
    using argument_type =
      stdb::memory::basic_small_string<Char, Buffer, Core, Traits, Allocator, NullTerminated, Growth>;
    using result_type = std::size_t;

    auto operator()(const argument_type& str) const noexcept -> result_type {
        return std::hash<std::basic_string_view<Char, Traits>>{}(str);
    }
};

}  // namespace std
