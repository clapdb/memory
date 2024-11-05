

#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include "assert_config.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include "align/align.hpp"

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

[[nodiscard, gnu::always_inline]] constexpr inline auto next_power_of_2(uint16_t size) noexcept -> uint32_t {
    // find the smallest power of 2 that is greater than size
    // at least return 16U
    // because the size of internal buffer is 7
    Assert(size > kMaxInternalBufferSize, "size should be greater than 7");
    // make sure get 16U at least.
    constexpr uint16_t kMediumMask = 0xF8U;
    // if size < 8, add 8, or return the size, remove any if branch
    return std::bit_ceil(size + (uint16_t)(not (bool)(size & kMediumMask)) * 8U);
}

[[nodiscard, gnu::always_inline]] constexpr inline auto is_power_of_2(uint16_t size) noexcept -> bool {
    return bool(size) && not bool(size & (size - 1UL));
}

constexpr static inline auto next_small_size(uint32_t size) noexcept -> uint32_t {
    Assert(size <= 1024, "size should be not more than 1024");
    return next_power_of_2(size + sizeof(uint16_t));
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
        return stdb::memory::align::AlignUpTo<16>(size); // just align to 16 bytes, to save the memory
    }
    // the size is overflow, return 0.
    return 0;
}


// Calculate the new buffer size, the buffer size is the capacity + the size of the head
// the head is [capacity:4B, size:4B] for large buffer, [capacity:2B] for small buffer
constexpr static inline auto calculate_new_buffer_size(uint32_t least_new_capacity) noexcept -> uint32_t {
    if (least_new_capacity <= kMaxSmallStringSize) [[likely]] {
        return next_power_of_2(least_new_capacity);
    } else if (least_new_capacity <= kMaxMediumStringSize) [[likely]] {
        return next_medium_size(least_new_capacity);
    } else if (least_new_capacity <= kInvalidSize - 2 * sizeof(uint32_t)) [[likely]] {
        // the size_or_mask is not overflow
        return next_large_size(least_new_capacity + 2 * sizeof(uint32_t));
    }
    // the next_large_size always was aligned to 16 bytes, so kInvalidSize means overflow is OK
    return kInvalidSize;
}


template <typename Char>
struct internal_core {
    // 0000
    uint8_t is_internal: 4;
    // 0~7
    uint8_t internal_size : 4;
    Char data[7];
    [[nodiscard, gnu::always_inline]] static constexpr auto capacity() noexcept -> uint32_t {
        return sizeof(data);
    }

    [[nodiscard, gnu::always_inline]] auto size() const noexcept -> uint32_t {
        return internal_size;
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> uint32_t {
        return 7 - internal_size;
        // or ~internal_size & 0x7U, who is faster?
    }

    [[gnu::always_inline]] void set_size(uint32_t new_size) noexcept {
        internal_size = new_size;
    }
};

static_assert(sizeof(internal_core<char>) == 8);

template <typename Char>
struct external_core {
    // 0000 : 7
    // 0001 : 16
    // 0010 : 32
    // 0011 : 64
    // 0100 : 128
    // 0101 : 256
    // 0110 : 512
    // 0111 : 1024
    // 1000 : 2048
    // 1001 : 3072
    // 1010 : 4096
    // 1011 : 4096 and 4096 size, size must be 0
    // 11xx : over 4096, and [capacity:4B, size:4B] in the head of the buffer,
    // and the 14bits of the size_or_mask is the capacity - size, if the value is 1<<14 -1, the delta overflow, and
    // ignore the 14bits
    struct cap_and_size {
        uint8_t shift_or_times : 4; // 0x0U ~ 0xBU
        uint16_t external_size : 12;  
    };

    struct flag_and_delta {
        uint8_t flag : 2;  // 0x3U
        uint16_t delta: 14;
    };

    union {
        cap_and_size cap_size;
        flag_and_delta flag_delta;
    };
    // TODO(leo): move the c_str_ptr to begining of 
    int64_t c_str_ptr: 48; // the pointer to c_str

    [[nodiscard]] auto capacity_fast() const noexcept -> uint32_t {
        if (cap_size.shift_or_times <= 8U) [[likely]] {
            return 8U << cap_size.shift_or_times;
        } else if (cap_size.shift_or_times < 12U) [[likely]] {  // < 1100
            return ((cap_size.shift_or_times - 8U + 2U) << 10U);
        } else {
            // means don't know the capacity, because the size_or_mask is overflow
            // return *((uint32_t*)c_str_ptr - 2);
            return kInvalidSize;
        }
    }

    [[nodiscard]] auto capacity() const noexcept -> uint32_t {
        auto cap = capacity_fast();
        // means don't know the capacity, because the size_or_mask is overflow
        return cap != kInvalidSize ? cap : *((uint32_t*)c_str_ptr - 2);
    }

    [[nodiscard]] auto size_fast() const noexcept -> uint32_t {
        // assert if shift_or_times_or_delta is 11U, the external_size must be 0
        Assert(cap_size.shift_or_times != 11U or cap_size.external_size == 0U, "if size_or_mask is '1011', the external_size must be 0");
        return cap_size.shift_or_times < 11U   ? cap_size.external_size // 0000 ~ 1010
               : cap_size.shift_or_times > 11U ? kInvalidSize // 1100 ~ 1111
                                                        : 4096; // 1011
    }

    [[nodiscard]] auto size() const noexcept -> uint32_t {
        auto size = size_fast();
        return size != kInvalidSize ? size : *((uint32_t*)c_str_ptr - 1);
    }


    // just work for the case that just change the size, do not change the capacity
    auto set_size(uint32_t new_size) noexcept -> void {
        if (flag_delta.flag != 3U) [[likely]] {
            Assert(new_size <= kMaxMediumStringSize, "the new_size is must be less or equal to the 4k");
            cap_size.external_size = new_size;
        } else {
            // set to the buffer head
            auto* ptr = (uint32_t*)c_str_ptr;
            // set the size value to buffer header
            *(ptr - 1) = new_size;
            // set the delta
            flag_delta.delta = std::min<uint32_t>(*((uint32_t*)c_str_ptr - 2) - new_size, kDeltaMax);
        }
    }

    auto increase_size(uint32_t delta) noexcept -> void {
        if (flag_delta.flag != 3U) [[likely]] {
            Assert(cap_size.external_size + delta <= kMaxMediumStringSize, "the new_size is must be less or equal to the 4k");
            cap_size.external_size += delta;
        } else { // not optimized for the large buffer
            // set to the buffer head
            auto* ptr = (uint32_t*)c_str_ptr;
            // set the size value to buffer header
            *(ptr - 1) += delta; // do not check overflow
            // set the delta
            flag_delta.delta -= delta;
        }
    }

    // just change the external_core, do not change the buffer
    auto set_delta_fast(uint32_t new_delta) noexcept {
        if (cap_size.shift_or_times < 11U) [[likely]] {
            if (new_delta < kDeltaMax) [[likely]] {
                flag_delta.flag = 11U;
                flag_delta.delta = new_delta;
            }
            // set the all bit of external_core to 1
            memset(this, 0xFF, sizeof(*this));
            // and do not save the delta in anywhere.
        }
    }

    // capcity - size in fast way
    [[nodiscard, gnu::always_inline]] auto idle_capacity_fast() const noexcept -> uint32_t {
        // capacity() - size() will be a little bit slower.
        if (cap_size.shift_or_times < 8U) [[likely]] { // 0000 ~ 0111
            Assert(cap_size.external_size < (8U << cap_size.shift_or_times), "the external_size is must be less than the capacity");
            return (8U << cap_size.shift_or_times) - cap_size.external_size;
        } else if (cap_size.shift_or_times < 11U) [[likely]] { // 1000 ~ 1010
            Assert(cap_size.external_size < ((cap_size.shift_or_times - 8U + 2U) << 10U), "the external_size is must be less than the capacity");
            return ((cap_size.shift_or_times - 8U + 2U) << 10U) - cap_size.external_size;
        } else if (cap_size.shift_or_times == 11U) [[likely]] { // 1011
            return 0;
        }
        Assert(flag_delta.flag == 3U, "if the shift_or_times_or_delta is 11, the flag must be 11");
        return flag_delta.delta;
    }

    // capacity - size
    [[nodiscard]] auto idle_capacity() const noexcept -> uint32_t {
        auto delta = idle_capacity_fast();
        if (delta != kDeltaMax) [[likely]] {
            return delta;
        }
        // calculate the idle capacity with the buffer head,
        // the buffer head is [capacity, size], the external.ptr point to the 8 bytes after the head
        auto* ptr =(uint32_t*)c_str_ptr;
        return *(ptr - 2) - *(ptr - 1);
    }

    [[gnu::always_inline]] void deallocate() noexcept {
        // check the ptr is pointed to a buffer or the after pos of the buffer head
        if (flag_delta.flag != 3U) [[likely]] {
            // is not the large buffer
            std::free((Char*)(c_str_ptr));
        } else {
            // the ptr is pointed to the after pos of the buffer head
            std::free((Char*)(c_str_ptr) - 2 * sizeof(uint32_t));
        }
    }

    [[gnu::always_inline]] auto c_str() noexcept -> Char* { return (Char*)(c_str_ptr); }
};

// set the buf_size and str_size to the right place
template <typename Char>
auto allocate_new_external_buffer(uint32_t new_buffer_size, uint32_t old_str_size) noexcept -> external_core<Char> {
    static_assert(sizeof(Char) == 1);
    Assert(old_str_size <= new_buffer_size, "new_str_size should be less than new_buffer_size");
    Assert(new_buffer_size > 7, "new_buffer_size should be greater than 7, or it should not be an external buffer");
    Assert(new_buffer_size != kInvalidSize, "new_buffer_size should not be kInvalidSize");
    if (new_buffer_size <= kMaxSmallStringSize) [[likely]] {
        Assert(is_power_of_2(new_buffer_size), "new_buffer_size should be a power of 2");
        // the shift_or_times_or_delta is the (bit_width of the new_buffer_size) - 4
        return {.cap_size = {.shift_or_times =
                               static_cast<uint8_t>(static_cast<uint8_t>(std::bit_width(new_buffer_size)) - 4U),
                             .external_size = static_cast<uint16_t>(old_str_size)},
                .c_str_ptr = reinterpret_cast<int64_t>(std::malloc(new_buffer_size))};
    } else if (new_buffer_size <= kMaxMediumStringSize) [[likely]] {
        Assert((new_buffer_size & kMaxMediumStringSize) == 0, "the medium new_buffer_size is not aligned to 1024");
        return {.cap_size = {.shift_or_times = static_cast<uint8_t>(8U & ((new_buffer_size >> 10U) - 2U)),
                             .external_size = static_cast<uint16_t>(old_str_size)},
                .c_str_ptr = reinterpret_cast<int64_t>(std::malloc(new_buffer_size))};
    }
    // the large buffer
    uint16_t delta = std::min<uint32_t>(new_buffer_size - old_str_size, kDeltaMax);
    auto* buf = std::malloc(new_buffer_size);
    auto* head = reinterpret_cast<uint32_t*>(buf);
    *head = new_buffer_size;
    *(head + 1) = old_str_size;
    // the ptr point to the 8 bytes after the head
    return {.flag_delta = {.flag = 3U, .delta = delta}, .c_str_ptr = reinterpret_cast<int64_t>(head + 2)};
}

template <typename Char>
[[nodiscard, gnu::always_inline]] auto check_if_internal(const external_core<Char>& old_external) noexcept -> bool {
    return ((internal_core<Char>&)(old_external)).is_internal == 0;
}

template <typename Char>
[[nodiscard, gnu::always_inline]] auto check_if_internal(const internal_core<Char>& old_internal) noexcept -> bool {
    return old_internal.is_internal == 0;
}

// this function handle append / push_back / operator +='s internal reallocation
// this funciion will not change the size, but the capacity or delta
template<typename Char>
auto allocate_new_external_buffer_if_need_from_delta(external_core<Char>& old_external, uint32_t new_append_size) noexcept -> void {
    uint32_t old_delta = old_external.cap_size.shift_or_times == 0? ((internal_core<Char>&)old_external).idle_capacity() : old_external.idle_capacity_fast();
    // if no need, do nothing, just update the size or delta
    if (old_delta >= new_append_size) {
        // just return and do nothing
        return;
    }
    // by now, new_append_size > old_delta
    // check the delta is overflow
    if (old_delta == kDeltaMax) [[unlikely]] { // small_string was designed for small string, so large string is not optimized
        // the delta is overflow, re-calc the delta
        auto* cap = (uint32_t*)(old_external.c_str_ptr) - 2;
        auto* size = (uint32_t*)(old_external.c_str_ptr) - 1;
        if ((*cap - *size) > new_append_size) {
            // no need to allocate a new buffer, just return
            return;
        }
    }
    // by now, the old delta is not enough, have to allocate a new buffer, to save the new_append_size
    // have to allocate a new buffer, to save the new_append_size
    auto new_buffer_size = calculate_new_buffer_size(old_external.size() + new_append_size);
    auto new_external = allocate_new_external_buffer<Char>(new_buffer_size, old_external.size());
    if (check_if_internal(old_external)) {
        auto old_internal = (internal_core<Char>&)old_external;
        // copy the old data to the new buffer
        std::memcpy(new_external.c_str(), old_internal.data, old_internal.size());
        old_external = new_external;
        return;
    }
    // copy the old data to the new buffer
    std::memcpy(new_external.c_str(), old_external.c_str(), old_external.size());
    // replace the old external with the new one
    old_external.deallocate();
    old_external = new_external;
    return;
}

static_assert(sizeof(external_core<char>) == 8);

template <typename Char, class Traits = std::char_traits<Char>, class Allocator = std::allocator<Char>>
class basic_small_string {

    union {
        internal_core<Char> internal;
        external_core<Char> external;
    };


   private:
    [[nodiscard]] auto is_external() const noexcept -> bool {
        return internal.is_internal != 0;
    }

    [[nodiscard]] auto buffer_size() const noexcept -> uint16_t {
        return is_external() ? external.buffer_size() : internal.buffer_size();
    }

    [[nodiscard, gnu::always_inline]] auto idle_capacity() const noexcept -> uint32_t {
        return is_external() ? external.idle_capacity() : internal.idle_capacity();
    }

    [[nodiscard]] auto get_capacity_and_size() const noexcept -> std::pair<uint32_t, uint32_t> {
        if (not is_external()) {
            return {internal_core<Char>::capacity(), internal.size()};
        }
        if (external.flag_delta.flag != 3U) {
            return {external.capacity_fast(), external.size_fast()};
        }
        auto* buf = (uint32_t*)external.c_str_ptr;
        return {*(buf - 2), *(buf - 1)};
    }

    // increase the size, and won't change the capacity, so the internal/exteral'type or ptr will not change
    // you should always call the allocate_new_external_buffer first, then call this function
    inline void increase_size(uint32_t delta) noexcept {
        if (is_external()) [[likely]] {
            external.increase_size(delta);
        } else {
            internal.internal_size += delta;
        }
    }

    inline void set_size(uint32_t new_size) noexcept {
        if (is_external()) [[likely]] {
            external.set_size(new_size);
        } else {
            internal.set_size(new_size);
        }
    }

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
    // npos is the largest value of uint16_t, is 65535, which is max_size() + 1
    constexpr static size_type npos = std::numeric_limits<uint16_t>::max();


    // Member functions
    // no new or malloc needed, just noexcept
    constexpr basic_small_string() noexcept : internal{} {} // the is_external is false, the size is 0, and the capacity is 7

    constexpr explicit basic_small_string([[maybe_unused]] const Allocator& /*unused*/) noexcept : internal{} {}

    constexpr basic_small_string(size_type count, Char ch, [[maybe_unused]] const Allocator& /*unused*/= Allocator()) : internal{} {
        // the best way to initialize the string is to append the char count times
        append(count, ch);
    }

    // copy constructor
    basic_small_string(const basic_small_string& other, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) : external{other.external} {
        if (not is_external()) {
            // is a internal string, just copy the internal part
            return;
        }
        // is a external string, check if the size is larger than the internal capacity
        auto other_size = other.size();
        if (other_size <= internal_core<Char>::capacity()) {
            // the size is smaller than the internal capacity, do not need to allocate a new buffer

            // set the size first
            internal.set_size(other_size);
            // copy the data from the other
            std::memcpy(internal.data, other.c_str(), other_size);
        }
        // by now, need external buffer, allocate a new buffer
        external = allocate_new_external_buffer<Char>(calculate_new_buffer_size(other_size), other_size);
        // copy the data from the other
        std::memcpy((Char*)(external.ptr), other.c_str(), other_size);
    }

    constexpr basic_small_string(const basic_small_string& other, size_type pos, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) {
        auto new_size = other.size() - pos;
        if (new_size <= internal_core<Char>::capacity()) {
            // the size is smaller than the internal capacity, do not need to allocate a new buffer
            internal.set_size(new_size);
            std::memcpy(internal.data, other.c_str() + pos, new_size);
        } else {
            // by now, need external buffer, allocate a new buffer
            external = allocate_new_external_buffer<Char>(calculate_new_buffer_size(new_size), new_size);
            // copy the data from the other
            std::memcpy((Char*)(external.ptr), other.c_str() + pos, new_size);
        }
    }

    constexpr basic_small_string(basic_small_string&& other, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) noexcept : external(other.external) {
        // set the other's external to 0, to avoid double free
        memset(&other.external, 0, sizeof(other.external));
    }

    constexpr basic_small_string(basic_small_string&& other, size_type pos,
                                 [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos)} {}

    constexpr basic_small_string(const basic_small_string& other, size_type pos, size_type count, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    constexpr basic_small_string(basic_small_string&& other, size_type pos, size_type count, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
        : basic_small_string{other.substr(pos, count)} {}

    constexpr basic_small_string(const Char* s, size_type count, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) : internal{} {
        append(s, count);
    }

    constexpr basic_small_string(const Char* s, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) : basic_small_string(s, std::strlen(s)) { }

    template <class InputIt>
    constexpr basic_small_string(InputIt first, InputIt last, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) : internal{} {
        append(first, last);
    }

    constexpr basic_small_string(std::initializer_list<Char> ilist, [[maybe_unused]] const Allocator& /*unused*/ = Allocator()) : internal{} {
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
    constexpr basic_small_string(const StringViewLike& s, size_type pos, size_type n, [[maybe_unused]] const Allocator& /*unused*/ = Allocator())
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
        append(ilist);
        return *this;
    }

    template<class StringViewLike> requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and not std::is_convertible_v<const StringViewLike&, const Char*>)
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
        clear();
        append(str);
        return *this;
    }

    auto assign(const basic_small_string& str, size_type pos, size_type count = npos) -> basic_small_string& {
        clear();
        append(str, pos, count);
        return *this;
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


    template<class InputIt>
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

    template<class StringViewLike> requires(std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> and not std::is_convertible_v<const StringViewLike&, const Char*>)
    auto assign(const StringViewLike& s) -> basic_small_string& {
        clear();
        append(s);
        return *this;
    }

    template<class StringViewLike> requires(std::is_convertible_v<const StringViewLike&, const Char*> and not std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>>)
    auto assign(const StringViewLike& s, size_type pos, size_type n = npos) -> basic_small_string& {
        clear();
        append(s, pos, n);
        return *this;
    }

    auto get_allocator() const -> Allocator {
        return Allocator();
    }

    // element access
    template<bool Safe = true>
    constexpr auto at(size_type pos) noexcept(Safe)-> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    template<bool Safe = true>
    constexpr auto at(size_type pos) const noexcept(Safe)-> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    template<bool Safe = true>
    constexpr auto operator[](size_type pos) noexcept(Safe)-> reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    template<bool Safe = true>
    constexpr auto operator[](size_type pos) const noexcept(Safe)-> const_reference {
        if constexpr (Safe) {
            if (pos >= size()) [[unlikely]] {
                throw std::out_of_range("pos is out of range");
            }
        }
        return *(c_str() + pos);
    }

    auto front() noexcept -> reference {
        return *c_str();
    }

    auto front() const noexcept -> const_reference {
        return *c_str();
    }

    auto back() noexcept -> reference {
        // do not use c_str() and size() to avoid extra if check
        if (is_external()) {
            return *(reinterpret_cast<Char*>(external.ptr) + external.size - 1);
        }
        // else is internal
        return internal.data[internal.size - 1];
    }

    auto back() const noexcept -> const_reference {
        // do not use c_str() and size() to avoid extra if check
        if (is_external()) {
            return *(reinterpret_cast<Char*>(external.ptr) + external.size - 1);
        }
        // else is internal
        return internal.data[internal.size - 1];
    }

    [[nodiscard, gnu::always_inline]] auto c_str() noexcept -> Char* {
        return is_external() ? reinterpret_cast<Char*>(external.c_str_ptr) : internal.data;
    }

    [[nodiscard, gnu::always_inline]] auto data() noexcept -> Char* {
        return c_str();
    }

    // Iterators
    auto begin() noexcept -> iterator {
        return c_str();
    }

    auto begin() const noexcept -> const_iterator {
        return c_str();
    }

    auto cbegin() const noexcept -> const_iterator {
        return c_str();
    }

    auto end() noexcept -> iterator {
        if (is_external()) {
            return reinterpret_cast<Char*>(external.ptr) + external.size;
        }
        return internal.data + internal.size;
    }

    auto end() const noexcept -> const_iterator {
        if (is_external()) {
            return reinterpret_cast<const Char*>(external.ptr) + external.size;
        }
        return internal.data + internal.size;
    }

    auto cend() const noexcept -> const_iterator {
        if (is_external()) {
            return reinterpret_cast<const Char*>(external.ptr) + external.size;
        }
        return internal.data + internal.size;
    }

    auto rbegin() noexcept -> reverse_iterator {
        return reverse_iterator(end());
    }

    auto rbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    auto crbegin() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(end());
    }

    auto rend() noexcept -> reverse_iterator {
        return reverse_iterator(begin());
    }

    auto rend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }

    auto crend() const noexcept -> const_reverse_iterator {
        return const_reverse_iterator(begin());
    }   

    // capacity
    [[nodiscard, gnu::always_inline]] auto empty() const noexcept -> bool {
        return size() == 0;
    }

    [[nodiscard, gnu::always_inline]] auto size() const noexcept -> size_type {
        return is_external() ? external.size() : internal.size();
    }

    [[nodiscard, gnu::always_inline]] auto length() const noexcept -> size_type {
        return size();
    }

    /**
     * @brief The maximum number of elements that can be stored in the string.
     * the buffer largest size is 1 << 15, the cap occupy 2 bytes, so the max size is 1 << 15 - 2, is 65534
     */
    [[nodiscard]] constexpr auto max_size() const noexcept -> uint32_t {
        return kInvalidSize - 1;
    }


    constexpr auto reserve(size_type new_cap) -> void {
        // check the new_cap is larger than the internal capacity, and larger than current cap
        if (new_cap > capacity()) [[likely]] {
            // still be a small string
            auto new_buffer_size = calculate_new_buffer_size(new_cap);
            // allocate a new buffer
            auto new_external = allocate_new_external_buffer<Char>(new_buffer_size, size());
            // copy the old data to the new buffer
            std::memcpy(reinterpret_cast<Char*>(new_external.ptr), c_str(), size());
            // copy the old size to tmp
            if (is_external())
              [[likely]] {  // we do not wish reserve a internal string to a external str, it will be a little slower.
                // if the old string is a external string, deallocate the old buffer
                external.deallocate();
            }
            // replace the old external with the new one
            external = new_external;
        }
        // else is internal, then do nothing
    }

    // it was a just exported function, should not be called frequently, it was a little bit slow in some case.
    [[nodiscard, gnu::always_inline]] auto capacity() const noexcept -> size_type {
        return is_external() ? external.capacity() : internal_core<Char>::capacity();
    }

    auto shrink_to_fit() -> void {
        auto [cap, size] = get_capacity_and_size();
        Assert(cap >= size, "cap should always be greater or equal to size");
        auto best_cap = calculate_new_buffer_size(size);
        if (cap > best_cap) {
            auto new_str(*this);
            *this = std::move(new_str);
        }
    }

    // modifiers
    auto clear() noexcept -> void {
        // do not erase the memory, just set the size to 0
        if (is_external()) [[likely]] {
            external.set_size(0);
            // if you want to reduce the capacity, use shrink_to_fit()
        } else {
            internal.size = 0;
        }
    }

    template<bool Safe = true>
    constexpr auto insert(size_type index, size_type count, Char ch) -> basic_small_string& {
        return insert<Safe>(begin() + index, count, ch);
    }

    // just support zero-terminated input string, if you want to insert a string without zero-terminated, use
    // insert(size_type index, const Char* str, size_type count)
    template<bool Safe = true>
    constexpr auto insert(size_type index, const Char* str) -> basic_small_string& {
        return insert<Safe>(begin() + index, str, std::strlen(str));
    }

    template<bool Safe = true>
    constexpr auto insert(size_type index, const Char* str, size_type count) -> basic_small_string& {
        // check if the capacity is enough
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        std::memmove(c_str() + index + count, c_str() + index, size() - index);
        // set the new data
        std::memcpy(c_str() + index, str, count);
        return *this;
    }

    template<bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& other) -> basic_small_string& {
        return insert<Safe>(begin() + index, other.data(), other.size());
    }

    template<bool Safe = true>
    constexpr auto insert(size_type index, const basic_small_string& str, size_type other_index, size_type count = npos) -> basic_small_string& {
        return insert<Safe>(begin() + index, str.substr(other_index, count));
    }

    template<bool Safe = true>
    constexpr auto insert(const_iterator pos, Char ch) -> iterator {
        return insert<Safe>(pos, 1, ch);
    }

    template<bool Safe = true>
    constexpr auto insert(const_iterator pos, size_type count, Char ch) -> iterator {
        // check if the capacity is enough
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // memmove the data to the new position
        std::memmove(pos + count, pos, end() - pos); // end() - pos is faster than size() - (pos - begin())
        // set the new data
        std::memset(pos, ch, count);
        return pos;
    }

    template<class InputIt, bool Safe = true>
    constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
        // static check the type of the input iterator
        static_assert(std::is_same_v<typename std::iterator_traits<InputIt>::value_type, Char>,
                      "the value type of the input iterator is not the same as the char type");
        // calculate the count
        uint16_t count = std::distance(first, last);
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        // move the data to the new position
        std::memmove(pos + count, pos, end() - pos);
        // copy the new data
        std::copy(first, last, pos);
        return pos;
    }

    template<bool Safe = true>
    constexpr auto insert(const_iterator pos, std::initializer_list<Char> ilist) -> iterator {
        return insert<Safe>(pos, ilist.begin(), ilist.end());
    }

    template<class StringViewLike, bool Safe = true>
    constexpr auto insert(const_iterator pos, const StringViewLike& t) -> iterator {
        return insert<Safe>(pos, t.data(), t.size());
    }

    template<class StringViewLike, bool Safe = true>
    constexpr auto insert(const_iterator pos, const StringViewLike& t, size_type pos2, size_type count = npos) -> iterator {
        return insert<Safe>(pos, t.substr(pos2, count));
    }

    // erase the data from the index to the end
    // NOTICE: the erase function will never change the capacity of the string or the external flag
    auto erase(size_type index = 0, size_type count = npos) -> basic_small_string& {
        // check if the count is out of range
        if (index + count > size()) [[unlikely]] {
            // if count is zero, do nothing
            // so index even is end() is ok
            throw std::out_of_range("count is out of range");
        }
        // calc the real count
        uint16_t real_count = std::min(count, size() - index);
        // memmove the data to the new position
        std::memmove(c_str() + index, c_str() + index + real_count, size() - index - real_count);
        // set the new size
        if (is_external()) [[likely]] {
            external.size -= real_count;
        } else {
            internal.size -= real_count;
        }
        return *this;
    }

    constexpr auto erase(const_iterator first) -> iterator {
        // the first must be valid, and of the string.
        return erase(first - begin(), 1);
    }

    constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
        return erase(first - begin(), last - first);
    }

    template <bool Safe = true>
    [[gnu::always_inline]] inline void push_back(Char c) {
        append<Safe>(1, c);
    }

    void pop_back() {
        if (is_external()) [[likely]] {
            external.size--;
        } else {
            internal.size--;
        }
    }
    // constexpr auto insert(uint16_t index, uint16_t count, Char c) -> small_string& {
    // }
    template<bool Safe = true>
    constexpr auto append(uint16_t count, Char c) -> basic_small_string& {
        if constexpr (Safe) {
            // allocate a new external buffer if need
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        // by now, the capacity is enough
        std::memset(c_str() + size(), c, count);
        increase_size(count);
        return *this;
    }

    template<bool Safe = true>
    constexpr auto append(const basic_small_string& other) -> basic_small_string& {
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, other.size());
        }
        // by now, the capacity is enough
        // size() function maybe slower than while the size is larger than 4k, so store it.
        auto other_size = other.size();
        std::memcpy(c_str() + size(), other.c_str(), other_size);
        increase_size(other_size);
        return *this;
    }

    template<bool Safe = true>
    constexpr auto append(const basic_small_string& other, uint16_t pos, uint16_t count = npos) -> basic_small_string& {
        if (count == npos) {
            count = other.size() - pos;
        }
        return append<Safe>(other.substr(pos, count));
    }

    template<bool Safe = true>
    constexpr auto append(const Char* s, uint32_t count) -> basic_small_string& {
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        
        auto tsize = size();
        auto* dest = c_str();
        dest+= tsize;
        // std::memcpy(c_str() + size(), s, count);
        std::memcpy(dest, s, count);
        increase_size(count);
        return *this;
    }

    template<bool Safe = true>
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
        uint32_t count = std::distance(first, last);
        if constexpr (Safe) {
            allocate_new_external_buffer_if_need_from_delta(external, count);
        }
        std::copy(first, last, c_str() + size());
        increase_size(count);
        return *this;
    }

    template<bool Safe = true>
    constexpr auto append(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist.begin(), ilist.end());
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
    constexpr auto append(const StringViewLike& s, uint16_t pos, uint16_t count = npos) -> basic_small_string& {
        if (count == npos) {
            count = s.size() - pos;
        }
        return append<Safe>(s.substr(pos, count));
    }

    template<bool Safe = true>
    auto operator+=(const basic_small_string& other) -> basic_small_string& {
        return append<Safe>(other);
    }

    
    template<bool Safe = true>
    auto operator+=(Char ch) -> basic_small_string& {
        return push_back<Safe>(ch);
    }

    template<bool Safe = true>
    auto operator+=(const Char* s) -> basic_small_string& {
        return append<Safe>(s);
    }

    template<bool Safe = true>
    auto operator+=(std::initializer_list<Char> ilist) -> basic_small_string& {
        return append<Safe>(ilist);
    }

    template<class StringViewLike, bool Safe = true>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto operator+=(const StringViewLike& t) -> basic_small_string& {
        return append<Safe>(t);
    }

    // replace [pos, pos+count] with count2 times of ch
    auto replace(size_type pos, size_type count, size_type count2, Char ch) -> basic_small_string& {
        // check the pos is not out of range
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("pos is out of range");
        }
        // then check if pos+count is the out of range
        if (pos + count >= size()) {
            // no right part to move
            uint64_t new_size = pos + count2; // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            if (new_size > capacity()) {
                reserve(new_size);
            }
            // by now, the capacity is enough
            std::memset(c_str() + pos, ch, count2);
            set_size(new_size);
            return *this;
        }
        // else, there is right part, maybe need to move
        // check the size, if the pos + count is greater than the cap, we need to reserve
        uint64_t new_size = size() - count + count2; // to avoid overflow from size_type
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
            std::memmove(c_str() + pos + count, c_str() + pos + count2, size() - pos - count);
        }
        // by now, the buffer is ready
        std::memset(c_str() + pos, ch, count2);
        set_size(new_size);
        return *this;
    }

    // replace [pos, pos+count] with the range [cstr, cstr + count2]
    auto replace(size_type pos, size_type count, const Char* cstr, size_type count2) -> basic_small_string& {
        // check the pos is not out of range
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("pos is out of range");
        }

        if (pos + count >= size()) {
            // no right part to move
            uint64_t new_size = pos + count2; // to avoid overflow from size_type
            if (new_size > max_size()) [[unlikely]] {
                throw std::length_error("the new capacity is too large");
            }
            if (new_size > capacity()) {
                reserve(new_size);
            }
            // by now, the capacity is enough
            std::memcpy(c_str() + pos, cstr, count2);
            set_size(new_size);
            return *this;
        }
        // else, there is right part, maybe need to move
        // check the size, if the pos + count is greater than the cap, we need to reserve
        uint64_t new_size = size() - count + count2; // to avoid overflow from size_type
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
            
            std::memmove(c_str() + pos + count, c_str() + pos + count2, size() - pos - count);
            // the memmove will handle the overlap automatically
        }
        // by now, the buffer is ready
        std::memcpy(c_str() + pos, cstr, count2);
        set_size(new_size);
        return *this;
    }

    auto replace(size_type pos, size_type count, const basic_small_string& other) -> basic_small_string& {
        return replace(pos, count, other.c_str(), other.size());
    }

    auto replace(const_iterator first, const_iterator last, const basic_small_string& other) -> basic_small_string& {
        return replace(first - begin(), last - first, other.c_str(), other.size());
    }

    auto replace(size_type pos, size_type count, const basic_small_string& other, size_type pos2, size_type count2 = npos) -> basic_small_string& {
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

    template<class InputIt>
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
            uint64_t new_size = first - begin() + count2; // to avoid overflow from size_type
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
            return *this;
        }
        // else, there is right part, maybe need to move
        uint64_t new_size = size() - (last - first) + count2; // to avoid overflow from size_type
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
        return *this;
    }

    auto replace(const_iterator first, const_iterator last, std::initializer_list<Char> ilist) -> basic_small_string& {
        return replace(first, last, ilist.begin(), ilist.end());
    }

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(const_iterator first, const_iterator last, const StringViewLike& view) -> basic_small_string& {
        return replace(first, last, view.data(), view.size());
    }

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    auto replace(size_type pos, size_type count, const StringViewLike& view, size_type pos2, size_type count2 = npos) -> basic_small_string& {
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

    auto resize(size_type count, Char ch = Char{}) -> void {
        // if the count is less than the size, just set the size
        if (count < size()) {
            set_size(count);
            return;
        }
        if (count > capacity()) {
            // if the count is greater than the size, need to reserve
            reserve(count);
        }
        // by now, the capacity is enough
        std::memset(c_str() + size(), ch, count - size());
        set_size(count);
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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    constexpr auto find(Char ch, size_type pos = 0) const -> size_type {
        return traits_type::find(c_str() + pos, size() - pos, ch);
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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto rfind(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    constexpr auto rfind(Char ch, size_type pos = npos) const -> size_type {
        pos = pos == npos ? size() - 1 : pos;
        while (pos >= 0) {
            if (traits_type::eq(at(pos), ch)) {
                return pos;
            }
            --pos;
        }
        // not found
        return npos;
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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_first_of(const StringViewLike& view, size_type pos = 0) const -> size_type {
        return find(view.data(), pos, view.size());
    }

    constexpr auto find_first_of(Char ch, size_type pos = 0) const -> size_type {
        return find(ch, pos);
    }

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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto find_last_of(const StringViewLike& view, size_type pos = npos) const -> size_type {
        return rfind(view.data(), pos, view.size());
    }

    constexpr auto find_last_of(Char ch, size_type pos = npos) const -> size_type {
        return rfind(ch, pos);
    }

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

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
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
    constexpr auto compare(size_type pos1, size_type count1, const basic_small_string& str, size_type pos2, size_type count2 = npos) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, str.c_str() + pos2,
                               std::min(count1, count2 == npos ? str.size() - pos2 : count2));
    }
    constexpr auto compare(const Char* str) const noexcept -> int {
        return traits_type::compare(c_str(), str, std::min(size(), traits_type::length(str)));
    }
    constexpr auto compare(size_type pos, size_type count, const Char* str) const noexcept -> int {
        return traits_type::compare(c_str() + pos, str, std::min(count, traits_type::length(str)));
    }
    constexpr auto compare(size_type pos1, size_type count1, const Char* str, size_type count2) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, str, std::min(count1, count2));
    }

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(const StringViewLike& view) const noexcept -> int {
        return traits_type::compare(c_str(), view.data(), std::min(size(), view.size()));
    }

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(size_type pos, size_type count, const StringViewLike& view) const noexcept -> int {
        return traits_type::compare(c_str() + pos, view.data(), std::min(count, view.size()));
    }

    template<class StringViewLike>
    requires (std::is_convertible_v<const StringViewLike&, std::basic_string_view<Char>> &&
              !std::is_convertible_v<const StringViewLike&, const Char*>)
    constexpr auto compare(size_type pos1, size_type count1, const StringViewLike& view, size_type pos2, size_type count2 = npos) const noexcept -> int {
        return traits_type::compare(c_str() + pos1, view.data() + pos2,
                               std::min(count1, count2 == npos ? view.size() - pos2 : count2));
    }

    constexpr auto starts_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(0, view.size(), view) == 0;
    }

    constexpr auto starts_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(front(), ch);
    }

    constexpr auto starts_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(0, len, str) == 0;
    }

    constexpr auto ends_with(std::basic_string_view<Char> view) const noexcept -> bool {
        return size() >= view.size() && compare(size() - view.size(), view.size(), view) == 0;
    }

    constexpr auto ends_with(Char ch) const noexcept -> bool {
        return not empty() && traits_type::eq(back(), ch);
    }

    constexpr auto ends_with(const Char* str) const noexcept -> bool {
        auto len = traits_type::length(str);
        return size() >= len && compare(size() - len, len, str) == 0;
    }

    constexpr auto contains(std::basic_string_view<Char> view) const noexcept -> bool {
        return find(view) != npos;
    }

    constexpr auto contains(Char ch) const noexcept -> bool {
        return find(ch) != npos;
    }

    constexpr auto contains(const Char* str) const noexcept -> bool {
        return find(str) != npos;
    }

    constexpr auto substr(size_type pos = 0, size_type count = npos) const& -> basic_small_string {
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("pos is out of range");
        }
        
        return basic_small_string(c_str() + pos, count == npos ? size() - pos : count);
    }

    constexpr auto substr(size_type pos = 0, size_type count = npos) && -> basic_small_string {
        if (pos > size()) [[unlikely]] {
            throw std::out_of_range("pos is out of range");
        }
        
        if (pos == 0) {
            // no need to generate a new string
            resize(count);
            return std::move(*this);
        }
        auto real_count = count == npos ? size() - pos : count;
        // do replace first
        std::memmove(c_str(), c_str() + pos, real_count);
        resize(real_count);
        return std::move(*this);
    }
};

}  // namespace stdb::memory