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
#include <fmt/format.h>

#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace stdb {

template <typename T>
struct Relocatable : std::false_type
{};

template <typename T>
struct ZeroInitable : std::false_type
{};

}  // namespace stdb

namespace stdb::container {

template <typename T>
concept IsRelocatable =
  std::is_trivially_copyable_v<T> || std::is_trivially_move_constructible_v<T> || Relocatable<T>::value;

template <typename T>
concept IsZeroInitable = std::is_trivially_default_constructible_v<T> || not
std::is_class<T>::value || ZeroInitable<T>::value;

enum class Safety : bool
{
    Safe = false,
    Unsafe = true
};

// default stdb_vector capacity is 64 bytes.
constexpr std::size_t kFastVectorDefaultCapacity = 64;
constexpr std::size_t kFastVectorMaxSize = std::numeric_limits<std::ptrdiff_t>::max();

template <typename Iterator>
[[nodiscard, gnu::always_inline]] inline auto get_ptr_from_iter(Iterator& it) -> decltype(auto) {  // NOLINT
    if constexpr (std::is_pointer_v<Iterator>) {
        return it;
    } else {
        return it.operator->();
    }
}

template <typename T>
[[gnu::always_inline]] inline void construct_range(T* __restrict__ first, T* __restrict__ last) {
    assert(first != nullptr and last != nullptr);
    assert(first < last);
    if constexpr (IsZeroInitable<T>) {
        // set zero to all bytes.
        std::memset(first, 0, static_cast<size_t>(last - first) * sizeof(T));
    } else {
        for (; first != last; ++first) {
            new (first) T();
        }
    }
}

template <typename T>
    requires std::is_object_v<T>
[[gnu::always_inline]] inline void construct_range_with_cref(T* __restrict__ first, T* __restrict__ last,
                                                             const T& value) {
    assert(first != nullptr and last != nullptr);
    assert(first < last);
    static_assert(std::is_copy_constructible_v<T>);
    if constexpr (sizeof(T) == sizeof(char)) {
        std::memset(first, static_cast<int>(value), static_cast<size_t>(last - first) * sizeof(T));
    } else {
        for (; first != last; ++first) {
            new (first) T(value);
        }
    }
}

template <typename T>
[[gnu::always_inline]] inline auto copy_range(T* __restrict__ dst, const T* __restrict__ src,
                                              const T* __restrict__ src_end) -> T* {
    assert(dst != nullptr and src != nullptr);
    assert(dst != src);
    assert(src < src_end);
    // if is trivial_copyable, use memcpy is faster.
    if constexpr (IsRelocatable<T>) {
        std::memcpy(dst, src, (size_t)(src_end - src) * sizeof(T));  // NOLINT
        return dst + (src_end - src);
    } else {
        for (; src != src_end; ++src, ++dst) {
            new (dst) T(*src);
        }
        return dst;
    }
}

template <typename T>
    requires std::is_trivially_copyable_v<T> or std::is_nothrow_move_constructible_v<T>
[[gnu::always_inline]] inline void copy_value(T* __restrict__ dst, T value) {
    assert(dst != nullptr);
    if constexpr (IsRelocatable<T>) {
        *dst = value;
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        new (dst) T(std::move(value));
    }
}

template <typename T>
    requires std::is_object_v<T>
[[gnu::always_inline]] inline void copy_cref(T* __restrict__ dst, const T& value) {
    assert(dst != nullptr);
    if constexpr (IsRelocatable<T>) {
        *dst = value;
    } else {
        static_assert(std::is_copy_constructible_v<T>);
        new (dst) T(value);
    }
}

template <typename It>
concept PointerCompatibleIterator = std::is_pointer_v<It> or std::random_access_iterator<It>;

template <typename T, typename Iterator>
[[gnu::always_inline]] inline void copy_from_iterator(T* __restrict__ dst, Iterator first, Iterator last) {
    assert(dst != nullptr);
    assert(first != last);

    if constexpr (IsRelocatable<T> and PointerCompatibleIterator<Iterator>) {
        if (get_ptr_from_iter(first) < get_ptr_from_iter(last)) [[likely]] {
            std::memcpy(dst, get_ptr_from_iter(first), static_cast<size_t>(last - first) * sizeof(T));
        }
    } else {
        for (; first != last; ++first, ++dst) {
            new (dst) T(*first);
        }
    }
}

template <typename T>
[[gnu::always_inline]] inline void destroy_ptr(T* __restrict__ ptr) noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
        // do nothing.
    } else {
        ptr->~T();
    }
}

template <typename T>
[[gnu::always_inline]] inline void destroy_range(T* __restrict__ begin, T* __restrict__ end) noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
        // do nothing
    } else {
        for (; begin != end; ++begin) {
            begin->~T();
        }
    }
}

/*
 * move range [first, last) to [dst, dst + (last - first))
 * and return new_finish ptr
 */
template <typename T>
[[gnu::always_inline, nodiscard]] inline auto move_range_without_overlap(T* __restrict__ dst, T* __restrict__ src,
                                                                         T* __restrict__ src_end) noexcept -> T* {
    assert(dst != nullptr and src != nullptr);
    assert(dst != src);
    if constexpr (IsRelocatable<T>) {
        std::memcpy(dst, src, (size_t)(src_end - src) * sizeof(T));  // NOLINT
        return dst + (src_end - src);
    } else if constexpr (std::is_move_constructible_v<T>) {
        for (; src != src_end; ++src, ++dst) {
            new (dst) T(std::move(*src));
        }
        return dst;
    } else {
        static_assert(std::is_copy_constructible_v<T>);
        for (; src != src_end; ++src, ++dst) {
            new (dst) T(*src);
            src->~T();
        }
        return dst;
    }
}

template <typename T>
[[gnu::always_inline]] inline void move_range_forward(T* __restrict__ dst, T* __restrict__ src,
                                                      T* __restrict__ src_end) {
    assert(src != nullptr and dst != nullptr);
    // src == src_end means no data to move
    // it will occur in erase the whole vector.
    assert(src_end >= src);
    assert(dst < src or dst >= src_end);
    if constexpr (IsRelocatable<T>) {
        std::memmove(dst, src, (size_t)(src_end - src) * sizeof(T));  // NOLINT
    } else if constexpr (std::is_move_constructible_v<T>) {
        // do not support throwable move constructor.
        static_assert(std::is_nothrow_move_constructible_v<T>);
        // call move constructor
        for (; src != src_end; ++src, ++dst) {
            new (dst) T(std::move(*src));
        }

    } else {
        static_assert(std::is_copy_constructible_v<T>);
        // call copy constructor and dstr
        for (; src != src_end; ++src, ++dst) {
            new (dst) T(*src);
            src->~T();
        }
    }
}

/*
 * realloc memory, if failed, throw std::bad_alloc, then move the old memory to new memory.
 * if new_size < old_size, the extra elements will be destroyed.
 *
 * not use default realloc because seastar's memory allocator does not support realloc really.
 */
template <typename T>
auto realloc_with_move(T*& __restrict__ ptr, std::size_t old_size, std::size_t new_size) -> T* {
    // default init vector or
    // after shrink_to_fit with zero size, the ptr may be nullptr.
    if (ptr == nullptr) {
        assert(old_size == 0);
        return ptr = static_cast<T*>(std::malloc(new_size * sizeof(T)));
    }
    assert(new_size > 0);
    auto new_ptr = static_cast<T*>(std::malloc(new_size * sizeof(T)));
    if (new_ptr == nullptr) [[unlikely]] {
        throw std::bad_alloc();
    }
    T* new_finish = move_range_without_overlap(new_ptr, ptr, ptr + std::min(old_size, new_size));
    std::free(ptr);
    ptr = new_ptr;
    return new_finish;
}

template <typename T>
class core
{
    using size_type = std::size_t;
    using value_type = T;
    using deference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using rvalue_reference = T&&;

   protected:
    static constexpr std::size_t kFastVectorInitCapacity =
      sizeof(T) >= kFastVectorDefaultCapacity ? 1 : kFastVectorDefaultCapacity / sizeof(T);
    T* _start;   // buffer start // NOLINT
    T* _finish;  // valid end    // NOLINT
    T* _edge;    // buffer end   // NOLINT

   private:
    [[gnu::always_inline]] void allocate(size_type cap) {
        assert(cap > 0);
        if (_start = static_cast<T*>(std::malloc(cap * sizeof(T))); _start != nullptr) [[likely]] {
            _edge = _start + cap;
        } else {
            throw std::bad_alloc();
        }
    }

   public:
    core() : _start(nullptr), _finish(nullptr), _edge(nullptr) {}

    // this function will never be called without set values/ or construct values.
    core(size_type size, size_type cap) {
        assert(size <= cap);
        allocate(cap);
        _finish = _start + size;
    }

    core(const core& rhs) {
        if (rhs.size() > 0) [[likely]] {
            allocate(rhs.size());
            auto size = rhs.size();
            copy_range(_start, rhs._start, rhs._finish);
            _finish = _start + size;

        } else {
            _start = nullptr;
            _finish = nullptr;
            _edge = nullptr;
        }
    }

    core(core&& rhs) noexcept : _start(rhs._start), _finish(rhs._finish), _edge(rhs._edge) {
        rhs._start = nullptr;
        rhs._finish = nullptr;
        rhs._edge = nullptr;
    }

    auto operator=(const core& other) -> core& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        auto new_size = other.size();
        if (new_size > capacity()) {
            // if other's size is larger than current capacity, we need to reallocate memory
            // destroy old data
            destroy_range(_start, _finish);
            // free old memory
            std::free(_start);
            // allocate new memory
            allocate(new_size);
            // copy data
            _finish = copy_range(_start, other._start, other._finish);
        } else {
            // if other's size is smaller than current capacity, we can just copy data
            // destroy old data
            destroy_range(_start, _finish);
            // copy data
            if (new_size > 0) [[likely]] {
                _finish = copy_range(_start, other._start, other._finish);
            } else {
                _finish = _start;
            }
            // _edge is not changed
        }
        return *this;
    }
    auto operator=(core&& other) noexcept -> core& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        // destroy old data
        destroy_range(_start, _finish);
        // free old memory
        std::free(_start);
        // move data
        _start = std::exchange(other._start, nullptr);
        _finish = std::exchange(other._finish, nullptr);
        _edge = std::exchange(other._edge, nullptr);
        return *this;
    }
    ~core() {
        // destroy data
        destroy_range(_start, _finish);
        // free will check nullptr itself
        std::free(_start);
    }

    constexpr void swap(core<T>& rhs) noexcept {
        _start = std::exchange(rhs._start, _start);
        _finish = std::exchange(rhs._finish, _finish);
        _edge = std::exchange(rhs._edge, _edge);
    }

    [[nodiscard, gnu::always_inline]] constexpr auto size() const -> size_type {
        assert(_finish >= _start);
        return static_cast<size_type>(_finish - _start);
    }

    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const -> size_type {
        assert(_edge >= _start);
        return static_cast<size_type>(_edge - _start);
    }

    [[nodiscard, gnu::always_inline]] auto full() const -> bool {
        assert(_finish <= _edge);
        return _edge == _finish;
    }
    // data access section
    [[nodiscard, gnu::always_inline]] auto at(size_type index) const -> const_reference {
        assert(index < size());
        return _start[index];  // NOLINT
    }
    [[nodiscard, gnu::always_inline]] auto at(size_type index) -> reference {
        assert(index < size());
        return _start[index];  // NOLINT
    }

    [[gnu::always_inline]] void realloc_with_old_data(size_type new_cap) {
        // no check new_cap because it will be checked in caller.
        auto old_size = size();
        assert(new_cap >= old_size);
        _finish = realloc_with_move(_start, old_size, new_cap);
        _edge = _start + new_cap;
        return;
    }

    template <typename... Args>
    [[gnu::always_inline]] void realloc_and_emplace_back(size_type new_cap, Args&&... args) {
        // no check new_cap because it will be checked in caller.
        auto old_size = size();
        assert(new_cap > old_size);
        _finish = realloc_with_move(_start, old_size, new_cap);
        _edge = _start + new_cap;
        new (_finish++) T(std::forward<Args>(args)...);
        return;
    }

    [[gnu::always_inline]] auto realloc_drop_old_data(size_type new_cap) -> T* {
        this->~core();
        allocate(new_cap);
        return _start;
    }

    [[gnu::always_inline]] void destroy() {
        // if _start, _finish. _edge are nullptr, it was default constructed.
        // just do destroy, it will do nothing.
        // so we should allow start == end, and do a empty destroy.
        assert(_start <= _finish);
        assert(_finish <= _edge);
        destroy_range(_start, _finish);
    }

    [[gnu::always_inline, nodiscard]] auto max_size() const -> size_type { return kFastVectorMaxSize / sizeof(T); }

    // move [src, end()) to dst start range from front to end
    [[gnu::always_inline]] void move_forward(T* __restrict__ dst, T* __restrict__ src) {
        assert(dst != src);
        move_range_forward(dst, src, _finish);
    }

    // move [src, end()) to dst start range from front to end
    [[gnu::always_inline]] void move_forward(const T* __restrict__ dst, const T* __restrict__ src) {
        assert(dst != src);
        move_range_forward(const_cast<T*>(dst), const_cast<T*>(src), _finish);  // NOLINT
    }

    // move [src, end()) to dst start range from end to front
    void move_backward(T* __restrict__ dst, T* __restrict__ src) {
        assert(dst != nullptr && src != nullptr);
        // if src == _finish or src == _finish -1, just use move_forward
        assert(src < (_finish - 1));
        // if dst == src, no need to move
        assert(dst != src);

        T* dst_end = dst + (_finish - 1 - src);
        if constexpr (IsRelocatable<T>) {
            // trivially backward move
            for (T* src_end = _finish - 1; src_end >= src;) [[likely]] {
                *(dst_end--) = *(src_end--);
            }
        } else if constexpr (std::is_move_constructible_v<T>) {
            // do not support throwable move constructor.
            static_assert(std::is_nothrow_move_constructible_v<T>);
            // backward move
            for (T* src_end = _finish - 1; src_end >= src;) [[likely]] {
                new (dst_end--) T(std::move(*(src_end--)));
            }
        } else {
            // backward copy
            for (T* src_end = _finish - 1; src_end >= src; --dst_end, --src_end) [[likely]] {
                new (dst_end) T(*src_end);
                src_end->~T();
            }
        }
    }

    template <typename... Args>
    [[gnu::always_inline]] void construct_at(T* __restrict__ ptr, Args&&... args) {
        new ((void*)ptr) T(std::forward<Args>(args)...);  // NOLINT
    }
};

/*
 * stdb_vector is a vector-like container that uses a variadic size buffer to store elements.
 * it is designed to be used in the non-arena memory.
 */
template <typename T>
class stdb_vector : public core<T>
{
   public:
    using size_type = std::size_t;
    using value_type = T;
    using deference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using rvalue_reference = T&&;

    /*
     * default constructor
     * with default capacity == kFastVectorInitCapacity
     *
     * default constructor is not noexcept, because it may throw std::bad_alloc
     */
    constexpr stdb_vector() : core<T>() {}

    /*
     * constructor with capacity
     */
    constexpr explicit stdb_vector(std::size_t size) : core<T>(size, size) {
        construct_range(this->_start, this->_finish);
    }

    constexpr stdb_vector(std::size_t size, const T& value) : core<T>(size, size) {
        construct_range_with_cref(this->_start, this->_finish, value);
    }

    template <std::forward_iterator InputIt>
    constexpr stdb_vector(InputIt first, InputIt last) : core<T>() {
        int64_t size = last - first;
        // if size == 0, then do nothing.and just for caller convenience.
        assert(size >= 0);
        this->realloc_drop_old_data((size_type)size);  // NOLINT
        copy_from_iterator(this->_start, first, last);
        this->_finish = this->_start + size;
    }

    constexpr stdb_vector(std::initializer_list<T> init) : core<T>() {
        assert((init.size()) > 0 and (init.size() <= this->max_size()));
        auto size = init.size();
        this->realloc_drop_old_data(size);
        copy_range(this->_start, init.begin(), init.end());
        this->_finish = this->_start + size;
    }

    /*
     * copy constructor of stdb_vector
     */
    constexpr stdb_vector(const stdb_vector& other) = default;

    /*
     * copy assignment operator of stdb_vector
     */
    constexpr auto operator=(const stdb_vector& other) -> stdb_vector& = default;

    /*
     * move constructor of stdb_vector
     */
    constexpr stdb_vector(stdb_vector&&) noexcept = default;

    /*
     * move assignment operator of stdb_vector
     */
    auto operator=(stdb_vector&&) noexcept -> stdb_vector& = default;

    ~stdb_vector() = default;

    constexpr void assign(std::size_t count, const T& value) {
        // cleanup old data
        if (count > this->capacity()) {
            // if count is larger than current capacity, we need to reallocate memory
            this->realloc_drop_old_data(count);
        } else {
            // if count is smaller than current capacity, we can just copy data
            // clear old data
            this->destroy();
        }
        this->_finish = this->_start + count;
        // construct data with value
        construct_range_with_cref(this->_start, this->_finish, value);
    }

    template <std::forward_iterator Iterator>
    constexpr void assign(Iterator first, Iterator last) {
        int64_t size_to_assign = last - first;
        assert(size_to_assign >= 0);
        auto count = (size_type)size_to_assign;  // NOLINT
        if (count > this->capacity()) {
            // if count is larger than current capacity, we need to reallocate memory
            this->realloc_drop_old_data(count);
        } else {
            // if count is smaller than current capacity, we can just copy data
            // clear old data
            this->destroy();
        }
        copy_from_iterator(this->_start, first, last);
        this->_finish = this->_start + count;
    }

    /*
     * assign from initializer list
     */
    [[gnu::always_inline]] constexpr inline void assign(std::initializer_list<T> list) {
        assign(list.begin(), list.end());
    }

    /*
     * Capacity section
     */
    [[nodiscard, gnu::always_inline]] constexpr inline auto size() const noexcept -> size_type {
        return core<T>::size();
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto capacity() const noexcept -> size_type {
        return core<T>::capacity();
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto empty() const noexcept -> bool { return this->size() == 0; }

    [[nodiscard, gnu::always_inline]] constexpr inline auto max_size() const noexcept -> size_type {
        return core<T>::max_size();
    }

    /*
     * shrink_to_fit will cause memory reallocation
     * if the size is smaller than the capacity
     * otherwise, it will do nothing
     *
     * shrink_to_fit is not noexcept, because it may throw std::bad_alloc
     *
     * it is very heavy.
     */
    constexpr void shrink_to_fit() {
        auto size = this->size();
        if (size == capacity()) [[unlikely]] {
            return;
        }

        if (size == 0) [[unlikely]] {
            this->~core<T>();
            new (this) core<T>();
            return;
        }
        this->realloc_with_old_data(size);
        return;
    }

    /*
     * reserve will cause memory reallocation
     * reserve has no effect if the capacity is larger than the new capacity
     * reserve is not noexcept, because it may throw std::bad_alloc
     * reserve has no computing for new capacity, just do reallocation and data move.
     */
    [[gnu::always_inline]] constexpr inline void reserve(size_type new_capacity) {
        if (new_capacity > capacity()) [[likely]] {
            this->realloc_with_old_data(new_capacity);
        }
        return;
    }

    /*
     * Element access section
     */
    [[nodiscard, gnu::always_inline]] constexpr inline auto operator[](size_type index) noexcept -> reference {
        return this->at(index);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto operator[](size_type index) const noexcept
      -> const_reference {
        return this->at(index);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto at(std::size_t index) -> reference {
        return core<T>::at(index);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto at(size_type index) const -> const_reference {
        return core<T>::at(index);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto data() noexcept -> pointer { return this->_start; }

    [[nodiscard, gnu::always_inline]] constexpr inline auto data() const noexcept -> const_pointer {
        return this->_start;
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto front() const noexcept -> const_reference {
        assert(size() > 0);
        return *this->_start;
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto front() noexcept -> reference {
        assert(size() > 0);
        return *this->_start;
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto back() const noexcept -> const_reference {
        assert(size() > 0);
        return *(this->_finish - 1);
    }

    [[nodiscard, gnu::always_inline]] constexpr inline auto back() noexcept -> reference {
        assert(size() > 0);
        return *(this->_finish - 1);
    }

    /*
     * IteratorT section
     */
    // forward iterator
    template <bool Const>
    struct IteratorT
    {
        using iterator_category = std::contiguous_iterator_tag;
        using deference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<Const, const T, T>;
        using pointer = std::conditional_t<Const, const T*, T*>;
        using reference = std::conditional_t<Const, const T&, T&>;

       private:
        pointer _ptr;

       public:
        IteratorT() : _ptr(nullptr) {}
        ~IteratorT() = default;
        explicit IteratorT(pointer ptr) : _ptr(ptr) {}
        IteratorT(IteratorT&& rhs) noexcept : _ptr(std::exchange(rhs._ptr, nullptr)) {}
        IteratorT(const IteratorT&) noexcept = default;

        template<bool OtherIterator> requires(!OtherIterator && Const)
        IteratorT(IteratorT<OtherIterator> rhs) noexcept : _ptr(rhs.operator->()) {}
        // assignment operators
        auto operator=(IteratorT&& rhs) noexcept -> IteratorT& {
            _ptr = std::exchange(rhs._ptr, nullptr);
            return *this;
        }
        auto operator=(const IteratorT&) noexcept -> IteratorT& = default;

        [[gnu::always_inline]] constexpr inline auto operator++() noexcept -> IteratorT& {
            ++_ptr;
            return *this;
        }

        [[gnu::always_inline, nodiscard]] constexpr inline auto operator++(int) noexcept -> IteratorT {
            auto tmp = *this;
            ++_ptr;
            return tmp;
        }

        [[gnu::always_inline]] constexpr inline auto operator--() noexcept -> IteratorT& {
            --_ptr;
            return *this;
        }

        [[gnu::always_inline, nodiscard]] constexpr inline auto operator--(int) noexcept -> IteratorT {
            auto tmp = *this;
            --_ptr;
            return tmp;
        }

        [[gnu::always_inline]] constexpr inline auto operator+=(deference_type n) noexcept -> IteratorT& {
            _ptr += n;
            return *this;
        }

        [[gnu::always_inline]] constexpr inline auto operator-=(deference_type n) noexcept -> IteratorT& {
            _ptr -= n;
            return *this;
        }

        [[nodiscard, gnu::always_inline]] constexpr inline auto operator*() const noexcept -> reference {
            return *_ptr;
        }

        [[nodiscard, gnu::always_inline]] constexpr inline auto operator->() const noexcept -> pointer { return _ptr; }

        [[nodiscard, gnu::always_inline]] constexpr inline auto operator[](std::size_t pos) const noexcept
          -> reference {
            return *(_ptr + pos);
        }

        friend auto operator<=>(const IteratorT& lhs, const IteratorT& rhs) noexcept -> std::strong_ordering = default;

        friend auto operator-(const IteratorT& lhs, const IteratorT& rhs) noexcept -> deference_type {
            return lhs._ptr - rhs._ptr;
        }

        friend auto operator+(const IteratorT& lhs, deference_type offset) noexcept -> IteratorT {
            return IteratorT{lhs._ptr + offset};
        }

        friend auto operator-(const IteratorT& lhs, deference_type offset) noexcept -> IteratorT {
            return IteratorT{lhs._ptr - offset};
        }

        friend auto operator+(deference_type offset, const IteratorT& rhs) noexcept -> IteratorT {
            return IteratorT{rhs._ptr + offset};
        }

    };  // class IteratorT
    using Iterator = IteratorT<false>;
    using ConstIterator = IteratorT<true>;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    [[nodiscard, gnu::always_inline]] inline auto begin() noexcept -> Iterator { return Iterator(this->_start); }

    [[nodiscard, gnu::always_inline]] inline auto begin() const noexcept -> ConstIterator {
        return ConstIterator(this->_start);
    }

    [[nodiscard, gnu::always_inline]] auto cbegin() const noexcept -> ConstIterator {
        return ConstIterator(this->_start);
    }

    [[nodiscard, gnu::always_inline]] inline auto end() noexcept -> Iterator { return Iterator{this->_finish}; }

    [[nodiscard, gnu::always_inline]] inline auto end() const noexcept -> ConstIterator {
        return ConstIterator{this->_finish};
    }

    [[nodiscard, gnu::always_inline]] inline auto cend() const noexcept -> ConstIterator {
        return ConstIterator{this->_finish};
    }

    [[nodiscard, gnu::always_inline]] inline auto rbegin() noexcept -> ReverseIterator {
        return std::make_reverse_iterator(end());
    }

    [[nodiscard, gnu::always_inline]] inline auto rbegin() const noexcept -> ConstReverseIterator {
        return std::make_reverse_iterator(end());
    }

    [[nodiscard, gnu::always_inline]] inline auto crbegin() const noexcept -> ConstReverseIterator {
        return std::make_reverse_iterator(cend());
    }

    [[nodiscard, gnu::always_inline]] inline auto rend() noexcept -> ReverseIterator {
        return std::make_reverse_iterator(begin());
    }

    [[nodiscard, gnu::always_inline]] inline auto rend() const noexcept -> ConstReverseIterator {
        return std::make_reverse_iterator(begin());
    }

    [[nodiscard, gnu::always_inline]] inline auto crend() const noexcept -> ConstReverseIterator {
        return std::make_reverse_iterator(cbegin());
    }

    template <Safety safety = Safety::Safe>
    void fill(size_type (*filler)(T*)) {
        if constexpr (safety == Safety::Safe) {
            auto to_fill = filler(nullptr);
            if (to_fill + this->size() > this->capacity()) [[unlikely]] {
                this->realloc_with_old_data(compute_new_capacity(to_fill + size()));
            }
            this->_finish += filler(this->_finish);
        } else {
            this->_finish += filler(this->_finish);
        }
    }

    /*
     * get an alloced buffer for writing.
     */
    template <Safety safety = Safety::Safe>
    [[nodiscard, gnu::always_inline]] auto get_writebuffer(size_type buf_size) -> std::span<T> {
        if constexpr (safety == Safety::Safe) {
            if (buf_size + this->_finish > this->_edge) {
                this->realloc_with_old_data(compute_new_capacity(buf_size));
            }
        }
        auto buf = std::span<T>(this->_finish, buf_size);
        this->_finish += buf_size;
        return buf;
    }

    /*
     * Modifiers sections
     */
    template <Safety safety = Safety::Safe, typename U = T>
        requires std::is_object_v<U>
    void push_back(const value_type& value) {
        if constexpr (safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                copy_cref(this->_finish++, std::forward<const value_type&>(value));
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<const_reference>(value));
            }
        } else {
            assert(not this->full());
            copy_cref(this->_finish++, std::forward<const_reference&>(value));
        }
    }

    template <Safety safety = Safety::Safe, typename U = T>
        requires std::is_trivial_v<U> || std::is_move_constructible_v<U>
    void push_back(value_type&& value) {
        if constexpr (safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                copy_value(this->_finish++, std::forward<value_type&&>(value));
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<rvalue_reference>(value));
            }
        } else {
            assert(not this->full());
            copy_value(this->_finish++, std::forward<rvalue_reference>(value));
        }
    }

    template <Safety safety = Safety::Safe, typename... Args>
    auto emplace_back(Args&&... args) -> Iterator {
        if constexpr (safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                this->construct_at(this->_finish++, std::forward<Args>(args)...);
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<Args>(args)...);
            }
            return Iterator{this->_finish - 1};
        } else {
            assert(not this->full());
            this->construct_at(this->_finish++, std::forward<Args>(args)...);
            return Iterator{this->_finish - 1};
        }
    }

    [[gnu::always_inline]] inline void clear() noexcept {
        this->destroy();
        this->_finish = this->_start;
    }

    constexpr auto erase(ConstIterator pos) -> Iterator {
        assert(pos >= cbegin() and pos < cend());

        auto pos_ptr = get_ptr_from_iter(pos);
        destroy_ptr(pos_ptr);
        this->move_forward(pos_ptr, pos_ptr + 1);
        --this->_finish;
        return Iterator{(T*)pos_ptr};  // NOLINT
    }

    constexpr auto erase(Iterator pos) -> Iterator {
        assert(pos >= begin() and pos < end());
        T* ptr = get_ptr_from_iter(pos);
        destroy_ptr(ptr);
        this->move_forward(ptr, ptr + 1);
        --this->_finish;
        return pos;
    }

    constexpr auto erase(ConstIterator first, ConstIterator last) -> Iterator {
        assert(first >= cbegin() and last <= cend());
        assert(last > first);
        auto first_ptr = get_ptr_from_iter(first);
        auto last_ptr = get_ptr_from_iter(last);
        destroy_range(first_ptr, last_ptr);
        this->move_forward(first_ptr, last_ptr);
        this->_finish -= (last_ptr - first_ptr);
        return Iterator{(T*)last_ptr};  // NOLINT
    }

    auto erase(Iterator first, Iterator last) -> Iterator {
        assert(first >= begin() and last <= end());
        assert(last > first);
        T* first_ptr = get_ptr_from_iter(first);
        T* last_ptr = get_ptr_from_iter(last);
        destroy_range(first_ptr, last_ptr);
        this->move_forward(first_ptr, last_ptr);
        this->_finish -= (last_ptr - first_ptr);
        return Iterator{last_ptr};
    }

    auto erase(const value_type& value) -> size_type {  // NOLINT
        if constexpr (IsRelocatable<T>) {
            size_t erased = 0;
            for (auto *src = this->_start, *dst = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (*src != value) {
                    if (dst != src) {
                        *dst = *src;
                    }
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    ++erased;
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        } else if constexpr (std::is_move_constructible_v<T>) {
            size_t erased = 0;
            T* dst = this->_start;
            for (T* src = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (*src != value) {
                    if (dst != src) {
                        new (dst) T(std::move(*src));
                    }
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    ++erased;
                    src->~T();
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        } else {
            static_assert(std::is_copy_constructible_v<T>, "Cannot erase from a vector of non-copyable types");
            size_t erased = 0;
            T* dst = this->_start;
            for (T* src = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (*src != value) {
                    if (dst != src) {
                        new (dst) T(*src);
                        src->~T();
                    }
                    // do nothing, just move forward dst
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    ++erased;
                    src->~T();
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        }
    }

    /*
     * just like erase, but use a Pred function to test if the element should be erased
     */
    template <class Pred>
        requires std::predicate<Pred, const T&> || std::predicate<Pred, T&> ||
                 std::predicate<Pred, T>
                 auto erase_if(Pred pred) -> size_type {  // NOLINT
        if constexpr (IsRelocatable<T>) {
            size_t erased = 0;
            for (auto *src = this->_start, *dst = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (!pred(*src)) {
                    if (dst != src) {
                        *dst = *src;
                    }
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    ++erased;
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        } else if constexpr (std::is_move_constructible_v<T>) {
            size_t erased = 0;
            T* dst = this->_start;
            for (T* src = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (!pred(*src)) {
                    if (dst != src) {
                        new (dst) T(std::move(*src));
                    }
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    src->~T();
                    ++erased;
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        } else {
            static_assert(std::is_copy_constructible_v<T>, "Cannot erase from a vector of non-copyable types");
            size_t erased = 0;
            T* dst = this->_start;
            for (T* src = this->_start; src != this->_finish;) {
                // if the *src is not equal to the value we want to erase and dst == src, move forward both
                if (!pred(*src)) {
                    if (dst != src) {
                        new (dst) T(*src);
                        src->~T();
                    }
                    ++dst;
                }
                // if the *src is equal to the value we want to erase, move forward src only
                else {
                    ++erased;
                    src->~T();
                }
                ++src;
            }
            this->_finish -= erased;
            return erased;
        }
    }

    [[gnu::always_inline]] inline void pop_back() { destroy_ptr(this->_finish-- - 1); }

    template <Safety safety = Safety::Safe>
    constexpr void resize(size_type count) {
        if (count > this->size()) {
            if (count > this->capacity()) {
                this->realloc_with_old_data(count);
            }
            if constexpr (safety == Safety::Safe) {
                // construct the new elements
                auto* old_end = this->_finish;
                this->_finish = this->_start + count;
                construct_range(old_end, this->_finish);
            } else {
                this->_finish = this->_start + count;
            }

        } else {
            // destroy the elements that are not needed anymore
            auto* old_end = this->_finish;
            this->_finish = this->_start + count;
            destroy_range(this->_finish, old_end);
        }
    }

    constexpr void resize(size_type count, const_reference value) {
        // no checking count == _size, because very unlikely, and it's not a big deal
        if (count > this->size()) {
            if (count > this->capacity()) {
                this->realloc_with_old_data(count);
            }
            // destroy the elements that are not needed anymore
            auto* old_end = this->_finish;
            this->_finish = this->_start + count;
            construct_range_with_cref(old_end, this->_finish, value);
        } else {
            // destroy the elements that are not needed anymore
            auto* old_end = this->_finish;
            this->_finish = this->_start + count;
            destroy_range(this->_finish, old_end);
        }
    }

    [[gnu::always_inline]] constexpr inline void swap(stdb_vector& other) noexcept { core<T>::swap(other); }

    template <Safety safety = Safety::Safe>
    constexpr auto insert(ConstIterator pos, const_reference value) -> Iterator {
        assert(pos >= cbegin() && pos <= cend());
        T* pos_ptr = (T*)get_ptr_from_iter(pos);  // NOLINT
        if constexpr (safety == Safety::Safe) {
            if (this->full()) [[unlikely]] {
                std::ptrdiff_t pos_index = pos_ptr - this->_start;
                reserve(compute_next_capacity());
                pos_ptr = this->_start + pos_index;
            }
        }
        assert(not this->full());
        // insert value to the end pos
        if (pos_ptr == this->_finish) [[unlikely]] {
            copy_cref(this->_finish++, value);
            return Iterator(pos_ptr);
        }
        // move all elements after pos to the right, with backward direction, because have overlap
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);
        } else {
            this->move_forward(pos_ptr + 1, pos_ptr);
        }
        copy_cref(pos_ptr, value);
        ++this->_finish;
        return Iterator(pos_ptr);
    }

    template <Safety safety = Safety::Safe>
    constexpr auto insert(ConstIterator pos, rvalue_reference value) -> Iterator {
        assert((pos >= cbegin()) && (pos <= cend()));
        T* pos_ptr = (T*)get_ptr_from_iter(pos);  // NOLINT
        if constexpr (safety == Safety::Safe) {
            if (this->full()) [[unlikely]] {
                std::ptrdiff_t pos_index = pos_ptr - this->_start;
                reserve(compute_next_capacity());
                pos_ptr = this->_start + pos_index;
            }
        }
        assert(not this->full());
        // insert value to the end pos
        if (pos_ptr == this->_finish) [[unlikely]] {
            copy_value(this->_finish++, std::move(value));
            return Iterator((T*)(pos_ptr));  // NOLINT
        }
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);

        } else {
            this->move_forward(pos_ptr + 1, pos_ptr);
        }
        ++this->_finish;
        copy_value(pos_ptr, std::move(value));
        return Iterator(pos_ptr);
    }

    template <Safety safety = Safety::Safe>
    constexpr auto insert(ConstIterator pos, size_type count, const_reference value) -> Iterator {
        assert(count > 0);
        assert(pos >= cbegin() && pos <= cend());
        auto size = this->size();
        T* pos_ptr = (T*)get_ptr_from_iter(pos);  // NOLINT
        if constexpr (safety == Safety::Safe) {
            if (size + count > this->capacity()) [[unlikely]] {
                std::ptrdiff_t pos_index = pos_ptr - this->_start;
                reserve(compute_new_capacity(size + count));
                pos_ptr = this->_start + pos_index;
            }
        }
        assert((size + count) <= this->capacity());
        // new elements are inserted after the end pos
        if (pos_ptr + count >= this->_finish) {
            this->move_forward(pos_ptr + count, pos_ptr);
        } else {
            this->move_backward(pos_ptr + count, pos_ptr);
        }
        construct_range_with_cref(pos_ptr, pos_ptr + count, value);
        this->_finish += count;
        return Iterator(pos_ptr);
    }

    template <Safety safety = Safety::Safe, class InputIt>
        requires std::input_iterator<InputIt>
    constexpr auto insert(ConstIterator pos, InputIt first, InputIt last) -> Iterator {
        int64_t count = last - first;
        if (count == 0) [[unlikely]] {
            return Iterator((T*)get_ptr_from_iter(pos));  // NOLINT
        }
        auto size_to_insert = (size_type)count;   // NOLINT
        T* pos_ptr = (T*)get_ptr_from_iter(pos);  // NOLINT

        if constexpr (safety == Safety::Safe) {
            auto size = this->size();
            if ((size + size_to_insert) > capacity()) [[unlikely]] {
                std::ptrdiff_t pos_index = pos_ptr - this->_start;
                reserve(compute_new_capacity(size + size_to_insert));
                // calculate the new pos_ptr
                pos_ptr = this->_start + pos_index;
            }
        }

        assert((this->size() + size_to_insert) <= this->capacity());
        if (pos_ptr + count >= this->_finish) {
            this->move_forward(pos_ptr + count, pos_ptr);
        } else {
            this->move_backward(pos_ptr + count, pos_ptr);
        }
        copy_from_iterator(pos_ptr, first, last);
        this->_finish += size_to_insert;
        return Iterator(pos_ptr);
    }

    template <Safety safety = Safety::Safe>
    [[gnu::always_inline]] constexpr inline auto insert(ConstIterator pos, std::initializer_list<T> ilist) -> Iterator {
        return insert<safety>(pos, ilist.begin(), ilist.end());
    }

    template <Safety safety = Safety::Safe, typename... Args>
    constexpr auto emplace(Iterator pos, Args&&... args) -> Iterator {
        T* pos_ptr = get_ptr_from_iter(pos);
        if constexpr (safety == Safety::Safe) {
            if (this->full()) [[unlikely]] {
                std::ptrdiff_t pos_offset = pos_ptr - this->_start;
                reserve(compute_next_capacity());
                // calculate the new pos_ptr
                pos_ptr = this->_start + pos_offset;
            }
        }
        assert(not this->full());
        if (pos_ptr == this->_finish) [[unlikely]] {
            new (this->_finish++) T(std::forward<Args>(args)...);
            return Iterator(pos_ptr);
        }
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);
        } else {
            this->move_forward(pos_ptr + 1, pos_ptr);
        }
        new (pos_ptr) T(std::forward<Args>(args)...);
        ++this->_finish;
        return Iterator(pos_ptr);
    }

    template <Safety safety = Safety::Safe, typename... Args>
    constexpr auto emplace(size_type pos, Args&&... args) -> Iterator {
        if constexpr (safety == Safety::Safe) {
            if (this->full()) [[unlikely]] {
                reserve(compute_next_capacity());
            }
        }
        assert(not this->full());
        T* pos_ptr = this->_start + pos;

        if (pos_ptr == this->_finish) [[unlikely]] {
            new (this->_finish++) T(std::forward<Args>(args)...);
            return Iterator(pos_ptr);
        }
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);
        } else {
            this->move_forward(pos_ptr + 1, pos_ptr);
        }
        new (pos_ptr) T(std::forward<Args>(args)...);
        ++this->_finish;
        return Iterator(pos_ptr);
    }

   private:
    [[nodiscard, gnu::always_inline]] inline auto compute_new_capacity(size_type new_size) const -> size_type {
        assert(new_size > capacity());
        if (auto next_capacity = compute_next_capacity(); next_capacity > new_size) {
            return next_capacity;
        }
        return new_size;
    }
    [[nodiscard]] auto compute_next_capacity() const -> size_type {
        auto cap = capacity();
        // NOLINTNEXTLINE
        if (cap < 4096 * 32 / sizeof(T) and cap >= core<T>::kFastVectorInitCapacity) [[likely]] {
            // the capacity is smaller than a page,
            // use 1.5 but not 2 to reuse memory objects.
            return (cap * 3 + 1) / 2;
        }
        // the capacity is larger than a page,
        // so we can just double it to use whole pages.
        // NOLINTNEXTLINE
        if (cap >= 4096 * 32 / sizeof(T)) [[likely]] {
            return cap * 2;
        }
        return core<T>::kFastVectorInitCapacity;
    }
};  // class stdb_vector

template <typename T>
auto operator==(const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

template <typename T>
auto operator<=>(const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> std::strong_ordering {
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if (lhs[i] < rhs[i]) {
            return std::strong_ordering::less;
        }
        if (lhs[i] > rhs[i]) {
            return std::strong_ordering::greater;
        }
    }
    if (lhs.size() < rhs.size()) {
        return std::strong_ordering::less;
    }
    if (lhs.size() > rhs.size()) {
        return std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

}  // namespace stdb::container

namespace std {

template <typename T>
constexpr void swap(stdb::container::stdb_vector<T>& lhs, stdb::container::stdb_vector<T>& rhs) {
    lhs.swap(rhs);
}

template <class T, class U>
constexpr auto erase(stdb::container::stdb_vector<T>& vec, const U& value) -> std::size_t {
    return vec.erase(value);
}

template <class T, class Predicate>
constexpr auto erase_if(stdb::container::stdb_vector<T>& vec, Predicate pred) -> std::size_t {
    return vec.erase_if(pred);
}

}  // namespace std

namespace fmt {
template <typename T> struct formatter<stdb::container::stdb_vector<T>> : formatter<T> {
    template <typename FormatContext>
    auto format(const stdb::container::stdb_vector<T>& vec, FormatContext& ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(), "[{}]", fmt::join(vec, ", "));
    }
};
}  // namespace fmt
