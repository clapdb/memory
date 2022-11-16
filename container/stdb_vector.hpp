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
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <concepts>
#include <new>
#include <limits>
#include <stdexcept>
#include <span>
#include <iterator>
#include <utility>
#include <cassert>

namespace stdb::container {

enum class Safety : bool{
    Safe = false,
    Unsafe = true
};

// default stdb_vector capacity is 64 bytes.
constexpr std::size_t kFastVectorDefaultCapacity = 64;
constexpr std::size_t kFastVectorMaxSize= std::numeric_limits<std::ptrdiff_t>::max();

template<typename Iterator>
[[nodiscard, gnu::always_inline]] auto get_ptr_from_iter(Iterator& it) -> decltype(auto) {
    return it.operator->();
}

template<typename T>
void construct_range(T* __restrict__ first, T* __restrict__ last) {
    assert(first != nullptr and last != nullptr);
    assert(first < last);
    if constexpr (std::is_trivially_constructible_v<T> && std::is_standard_layout_v<T>) {
        // set zero to all bytes.
        std::memset(first, 0, static_cast<size_t>(last - first) * sizeof(T));
    } else {
        for (; first != last; ++first) {
            new (first) T();
        }
    }
}

template<typename T> requires std::is_object_v<T>
void construct_range_with_cref(T* __restrict__ first, T* __restrict__ last, const T& value) {
    assert(first != nullptr and last != nullptr);
    assert(first < last);
    static_assert(std::is_copy_constructible_v<T>);
    if constexpr (sizeof (T) == sizeof (char)) {
        std::memset(first, static_cast<int>(value), (last - first) * sizeof(T));
    } else {
        for (; first != last; ++first) {
            new (first) T(value);
        }
    }

}

template<typename T>
void copy_range(T* __restrict__ dst, const T* __restrict__ src, std::size_t n)
{
    assert(dst != nullptr and src != nullptr);
    assert(n > 0);
    assert(dst != src);
    // if is trivial_copyable, use memcpy is faster.
    if constexpr (std::is_trivially_copyable_v<T>) {
        std::memcpy(dst, src, n * sizeof(T));
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            new (dst + i) T(src[i]);
        }
    }
}

template<typename T> requires std::is_trivially_copyable_v<T> or std::is_nothrow_move_constructible_v<T>
void copy_value(T* __restrict__ dst, T value) {
    assert(dst != nullptr);
    if constexpr (std::is_trivially_copyable_v<T>) {
        *dst = value;
    } else {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        new (dst) T(std::move(value));
    }
}

template<typename T> requires std::is_object_v<T>
void copy_cref(T* __restrict__ dst, const T& value) {
    assert(dst != nullptr);
    if constexpr (std::is_trivially_copyable_v<T>) {
        *dst = value;
    } else {
        static_assert(std::is_copy_constructible_v<T>);
        new (dst) T(value);
    }
}

template <typename It>
constexpr auto check_iterator_is_random() -> bool {
    // check the It is not ptr, and is a random access iterator
    return !std::is_pointer_v<It> and std::random_access_iterator<It>;
}

template<typename T, typename Iterator>
void copy_from_iterator(T* __restrict__ dst, Iterator first, Iterator last)
{
    assert(dst != nullptr);
    assert(first != last);

    if constexpr (std::is_trivially_copyable_v<T> and check_iterator_is_random<Iterator>()) {
        if (get_ptr_from_iter(first) < get_ptr_from_iter(last)) [[likely]] {
            std::memcpy(dst, get_ptr_from_iter(first), static_cast<size_t>(last - first) * sizeof(T));
        }
    } else {
        for (; first != last; ++first, ++dst) {
            new (dst) T(*first);
        }
    }
}

template<typename T>
[[gnu::always_inline]] void destroy_ptr(T*  __restrict__ ptr) noexcept {
    if constexpr (std::is_trivially_destructible_v<T>) {
        // do nothing.
    } else {
        ptr->~T();
    }
}

template<typename T>
[[gnu::always_inline]] void destroy_range(T* __restrict__ begin, T* __restrict__ end) noexcept
{
    if constexpr (std::is_trivially_destructible_v<T>) {
        // do nothing
    } else {
        for (; begin != end; ++begin) {
            begin->~T();
        }
    }
}

template<typename T>
void move_range_without_overlap(T* __restrict__ dst, T* __restrict__ src, std::size_t n)
{
    if (dst == src) [[unlikely]] {
        return;
    }
    assert(dst != nullptr and src != nullptr);
    assert(dst != src);
    if constexpr (std::is_trivial_v<T> and std::is_standard_layout_v<T>) {
        std::memcpy(dst, src, n * sizeof(T));
    } else if constexpr (std::is_move_constructible_v<T>) {
        for (std::size_t i = 0; i < n; ++i) {
            new (dst + i) T(std::move(src[i]));
        }
    } else {
        static_assert(std::is_copy_constructible_v<T>);
        for (std::size_t i = 0; i < n; ++i) {
            new (dst + i) T(src[i]);
            src[i].~T();
        }
    }
}

template<typename T>
void move_range_forward(T* __restrict__ dst, T* __restrict__ src, std::size_t n)
{
    if (dst == src) [[unlikely]] {
        return;
    }
    assert(src != nullptr and dst != nullptr);
    assert(dst < src or dst >= src + n);
    if constexpr (std::is_trivial_v<T> and std::is_standard_layout_v<T>) {
        std::memmove(dst, src, n * sizeof(T));
    } else if constexpr (std::is_move_constructible_v<T>) {
        // do not support throwable move constructor.
        static_assert(std::is_nothrow_move_constructible_v<T>);
        // call move constructor
        for (std::size_t i = 0; i < n; ++i) {
            new (dst + i) T(std::move(src[i]));
        }
    } else {
        static_assert(std::is_copy_constructible_v<T>);
        // call copy constructor
        for (std::size_t i = 0; i < n; ++i) {
            new (dst + i) T(src[i]);
            src[i].~T();
        }
    }
}

template<typename T>
void move_range_backward(T* __restrict__ dst, T* __restrict__ src_start, T* __restrict__ src_end)
{
    assert(dst != nullptr and src_start != nullptr and src_end != nullptr);
    if (dst == src_start or src_start == src_end) [[unlikely]] {
        return;
    }
    T* dst_end = dst + (src_end - src_start);
    assert(dst > src_start and dst <= src_end);
    if constexpr (std::is_trivially_move_constructible_v<T>) {
        // backward move
        for (T* pilot = src_end; pilot >= src_start ; ) {
            *(dst_end--) = *(pilot--);
        }
    } else if constexpr (std::is_move_constructible_v<T>) {
        // do not support throwable move constructor.
        static_assert(std::is_nothrow_move_constructible_v<T>);
        // call move constructor
        for (T* pilot = src_end; pilot >= src_start ; ) {
            new (dst_end--) T(std::move(*(pilot--)));
        }
    } else {
        // call copy constructor
        for (T* pilot = src_end; pilot >= src_start ; --dst_end, --pilot) {
            new (dst_end) T(*pilot);
            pilot->~T();
        }
    }
}

/*
 * realloc memory, if failed, throw std::bad_alloc, then move the old memory to new memory.
 * if new_size < old_size, the extra elements will be destroyed.
 *
 * not use default realloc because seastar's memory allocator does not support realloc really.
 */
template<typename T>
auto realloc_with_move(T* __restrict__ ptr, std::size_t old_size, std::size_t new_size) -> T* {
    // just after shrink_to_fit with zero size, the ptr may be nullptr.
    if (ptr == nullptr) {
        assert(old_size == 0);
        return static_cast<T*>(std::malloc(new_size * sizeof(T)));
    }
    assert(new_size > 0);
    auto new_ptr = static_cast<T*>(std::malloc(new_size * sizeof(T)));
    if (new_ptr == nullptr) [[unlikely]] {
        throw std::bad_alloc();
    }
    // TODO: handle exceptions, if move_range_forward throws exception, memory leak.
    // and should undo the move_range_forward and free new_ptr.
    move_range_without_overlap(new_ptr, ptr, std::min(old_size, new_size));
    std::free(ptr);
    return new_ptr;
}


template<typename T>
class core {
    using size_type = std::size_t;
    using value_type = T;
    using deference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using rvalue_reference = T&&;
   protected:
    static constexpr std::size_t kFastVectorInitCapacity = sizeof (T) >= kFastVectorDefaultCapacity? 1 : kFastVectorDefaultCapacity / sizeof (T);
    T* _start;    // buffer start
    T* _finish;      // valid end
    T* _edge;   // buffer end

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
    core() : _start(nullptr), _finish(nullptr), _edge(nullptr) { }

    // this function will never be called without set values/ or construct values.
    core(size_type size, size_type cap) {
        assert(size <= cap);
        allocate(cap);
        _finish = _start + size;
    }

    core(const core& rhs) {
        allocate(rhs.capacity());
        auto size = rhs.size();
        copy_range(_start, rhs._start, size);
        _finish = _start + size;
    }

    core(core&& rhs): _start(rhs._start), _finish(rhs._finish), _edge(rhs._edge) {
        rhs._start = nullptr;
        rhs._finish = nullptr;
        rhs._edge = nullptr;
    }

    auto operator=(const core& other) -> core& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        auto new_size = other.size();
        if (other.size() > capacity()) {
            // if other's size is larger than current capacity, we need to reallocate memory
            // destroy old data
            destroy_range(_start, _finish);
            // free old memory
            std::free(_start);
            // allocate new memory
            allocate(new_size);
            // copy data
            copy_range(_start, other._start, other.size());
            _finish = _start + new_size;
        }
        else {
            // if other's size is smaller than current capacity, we can just copy data
            // destroy old data
            destroy_range(_start, _finish);
            // copy data
            copy_range(_start, other._start, new_size);
            _finish = _start + new_size;
        }
        return *this;
    }
    auto operator=(core&& other) -> core& {
        if (this == &other) [[unlikely]] {
            return *this;
        }
        // destroy old data
        destroy_range(_start, _finish);
        // free old memory
        std::free(_start);
        // move data
        _start = std::exchange(other._data, nullptr);
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

    [[nodiscard, gnu::always_inline]] constexpr auto capacity() const-> size_type {
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
        return _start[index];
    }
    [[nodiscard, gnu::always_inline]] auto at(size_type index) -> reference {
        assert(index < size());
        return _start[index];
    }

    [[gnu::always_inline]] void realloc_with_old_data(size_type new_cap) {
        // no check new_cap because it will be checked in caller.
        auto old_size = size();
        assert(new_cap > old_size);
        _start = realloc_with_move(_start, old_size, new_cap);
        _finish = _start + old_size;
        _edge = _start + new_cap;
        return;
     }

     template <typename ...Args>
     [[gnu::always_inline]] void realloc_and_emplace_back(size_type new_cap, Args&&... args) {
        // no check new_cap because it will be checked in caller.
        auto old_size = size();
        assert(new_cap > old_size);
        _start = realloc_with_move(_start, old_size, new_cap);
        _finish = _start + old_size;
        _edge = _start + new_cap;
        new (_finish) T(std::forward<Args>(args)...);
        _finish++;
        return;
     }

    [[gnu::always_inline]] auto realloc_without_old_data(size_type new_cap) -> T* {
        this->~core();
        allocate(new_cap);
        return _start;
    }

    [[gnu::always_inline]] void destroy() {
        assert(_start != nullptr);
        assert(_finish != nullptr);
        assert(_edge != nullptr);
        // should allow start == end, and do a empty destroy.
        assert(_start <= _finish);
        assert(_finish <= _edge);
        destroy_range(_start, _finish);
    }

    [[gnu::always_inline, nodiscard]] auto max_size() const -> size_type {
        return kFastVectorMaxSize / sizeof(T);
    }

    // move [src, end()) to dst start range from front to end
    [[gnu::always_inline]] void move_forward(T* dst, T* src) {
        auto size_to_move = _finish - src;
        move_range_forward(dst, src, static_cast<size_type >(size_to_move));
    }

    // move [src, end()) to dst start range from end to front
    [[gnu::always_inline]] void move_backward(T* dst, T* src) {
        move_range_backward(dst, src, _finish - 1);
    }

    template <typename ...Args>
    [[gnu::always_inline]] void construct_at(T* ptr, Args&&... args) {
        ::new ((void*)ptr) T(std::forward<Args>(args)...);
    }
};

/*
 * stdb_vector is a vector-like container that uses a variadic size buffer to store elements.
 * it is designed to be used in the non-arena memory.
 */
template <typename T>
class stdb_vector  : public core<T> {
    using size_type = std::size_t;
    using value_type = T;
    using deference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using rvalue_reference = T&&;
   public:
    /*
     * default constructor
     * with default capacity == kFastVectorInitCapacity
     *
     * default constructor is not noexcept, because it may throw std::bad_alloc
     */
    constexpr stdb_vector() : core<T>() { }

    /*
     * constructor with capacity
     */
    constexpr explicit stdb_vector(std::size_t size) : core<T>(size, size) {
        construct_range(this->_start, this->_finish);
    }


    constexpr stdb_vector(std::size_t size, const T& value): core<T>(size, size) {
        construct_range_with_cref(this->_start, this->_finish, value);
    }

    template<std::forward_iterator InputIt>
    constexpr stdb_vector(InputIt first, InputIt last): core<T>() {
        long size = last - first;
        // if size == 0, then do nothing.and just for caller convenience.
        assert(size >= 0);
        this->realloc_without_old_data(static_cast<size_type>(size));
        copy_from_iterator(this->_start, first, last);
        this->_finish = this->_start + size;
    }

    constexpr stdb_vector(std::initializer_list<T> init) : core<T>() {
        assert((init.size()) > 0 and (init.size() <= this->max_size()));
        auto size = init.size();
        this->realloc_without_old_data(size);
        copy_range(this->_start, init.begin(), size);
        this->_finish = this->_start + size;
    }

    /*
     * copy constructor of stdb_vector
     */
    constexpr stdb_vector(const stdb_vector& other) = default;

    /*
     * copy assignment operator of stdb_vector
     */
    constexpr auto operator = (const stdb_vector& other) -> stdb_vector& = default;

    /*
     * move constructor of stdb_vector
     */
    constexpr stdb_vector(stdb_vector&&) noexcept = default;

    /*
     * move assignment operator of stdb_vector
     */
    auto operator = (stdb_vector&&) noexcept -> stdb_vector& = default;

    ~stdb_vector() = default;

    constexpr void assign(std::size_t count, const T& value) {
        // cleanup old data
        if (count > this->capacity()) {
            // if count is larger than current capacity, we need to reallocate memory
            this->realloc_without_old_data(count);
        }
        else {
            // if count is smaller than current capacity, we can just copy data
            // clear old data
            this->destroy();
        }
        this->_finish = this->_start + count;
        // construct data with value
        construct_range_with_cref(this->_start, this->_finish, value);
    }

    template<std::forward_iterator Iterator>
    constexpr void assign(Iterator first, Iterator last) {
        long size_to_assign = last - first;
        assert(size_to_assign >= 0);
        size_type count = static_cast<size_type>(size_to_assign);
        if (count > this->capacity()) {
            // if count is larger than current capacity, we need to reallocate memory
            this->realloc_without_old_data(count);
        }
        else {
            // if count is smaller than current capacity, we can just copy data
            // clear old data
            this->destroy();
        }
        copy_from_iterator(this->_start, first, last);
        this->_finish  = this->_start + count;
    }

    /*
     * assign from initializer list
     */
    constexpr void assign(std::initializer_list<T> list) {
        assign(list.begin(), list.end());
    }

    /*
     * Capacity section
     */
    [[nodiscard]] constexpr auto size() const noexcept -> size_type {
        return core<T>::size();
    }

    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
        return core<T>::capacity();
    }

    [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return this->size() == 0;
    }

    [[nodiscard]] constexpr auto max_size() const noexcept -> size_type {
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
        if (size == capacity()) [[unlikely]]{
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
    constexpr void reserve(size_type new_capacity) {
        if (new_capacity > capacity()) [[likely]] {
            this->realloc_with_old_data(new_capacity);
        }
        return ;
    }

    /*
     * Element access section
     */
    [[nodiscard]] constexpr auto operator [] (size_type index) noexcept -> reference {
        return this->at(index);
    }

    [[nodiscard]] constexpr auto operator [] (size_type index) const noexcept -> const_reference {
        return this->at(index);
    }

    constexpr auto at(std::size_t index) -> reference {
        return core<T>::at(index);
    }

    constexpr auto at(size_type index) const-> const_reference {
        return core<T>::at(index);
    }

    [[nodiscard]] constexpr auto data() noexcept -> pointer {
        return this->_start;
    }

    [[nodiscard]] constexpr auto data() const noexcept -> const_pointer {
        return this->_start;
    }

    [[nodiscard]] constexpr auto front() const noexcept -> const_reference {
        assert(size() > 0);
        return *this->_start;
    }

    [[nodiscard]] constexpr auto front() noexcept -> reference {
        assert(size() > 0);
        return *this->_start;
    }

    [[nodiscard]] constexpr auto back() const noexcept -> const_reference {
        assert(size() > 0);
        return *(this->_finish - 1);
    }

    [[nodiscard]] constexpr auto back() noexcept -> reference {
        assert(size() > 0);
        return *(this->_finish - 1);
    }

    /*
     * Iterator section
     */
    // forward iterator
    struct Iterator {
       private:
        pointer _ptr;
       public:
        using iterator_category = std::random_access_iterator_tag;
        using deference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        Iterator(): _ptr(nullptr) {}
        Iterator(pointer ptr): _ptr(ptr) {}
        Iterator(Iterator&& rhs) noexcept: _ptr(std::exchange(rhs._ptr, nullptr)) {}
        Iterator(const Iterator&) noexcept = default;
        // assignment operators
        Iterator& operator = (Iterator&& rhs) noexcept {
            _ptr = std::exchange(rhs._ptr, nullptr);
            return *this;
        }
        Iterator& operator = (const Iterator&) noexcept = default;

        constexpr auto operator ++ () noexcept -> Iterator& {
            ++_ptr;
            return *this;
        }

        constexpr auto operator ++ (int) noexcept -> Iterator {
            auto tmp = *this;
            ++_ptr;
            return tmp;
        }

        constexpr auto operator -- () noexcept -> Iterator& {
            --_ptr;
            return *this;
        }

        constexpr auto operator -- (int) noexcept -> Iterator {
            auto tmp = *this;
            --_ptr;
            return tmp;
        }

        constexpr auto operator + (deference_type n) const noexcept -> Iterator {
            return Iterator(_ptr + n);
        }

        constexpr auto operator - (deference_type n) const noexcept -> Iterator {
            return Iterator(_ptr - n);
        }

        constexpr auto operator += (deference_type n) noexcept -> Iterator& {
            _ptr += n;
            return *this;
        }

        constexpr auto operator -= (deference_type n) noexcept -> Iterator& {
            _ptr -= n;
            return *this;
        }

        constexpr auto operator - (const Iterator& other) const noexcept -> deference_type {
            return _ptr - other._ptr;
        }

        constexpr auto operator == (const Iterator& other) const noexcept -> bool {
            return _ptr == other._ptr;
        }

        constexpr auto operator != (const Iterator& other) const noexcept -> bool {
            return _ptr != other._ptr;
        }

        constexpr auto operator < (const Iterator& other) const noexcept -> bool {
            return _ptr < other._ptr;
        }

        constexpr auto operator > (const Iterator& other) const noexcept -> bool {
            return _ptr > other._ptr;
        }

        constexpr auto operator <= (const Iterator& other) const noexcept -> bool {
            return _ptr <= other._ptr;
        }

        constexpr auto operator >= (const Iterator& other) const noexcept -> bool {
            return _ptr >= other._ptr;
        }

        constexpr auto operator * () const noexcept -> reference {
            return *_ptr;
        }

        constexpr auto operator -> () const noexcept -> pointer {
            return _ptr;
        }

    }; // class Iterator

    struct ConstIterator {
        private:
          const_pointer _ptr;
        public:
          using iterator_category = std::random_access_iterator_tag;
          using deference_type = std::ptrdiff_t;
          using value_type = T;
          using pointer = const T*;
          using reference = const T&;

          ConstIterator(): _ptr(nullptr) {}
          ConstIterator(pointer ptr): _ptr(ptr) {}
          // copy and move constructor
          ConstIterator(ConstIterator&& rhs) noexcept: _ptr(std::exchange(rhs._ptr, nullptr)) {}
          ConstIterator(const ConstIterator&) noexcept= default;
          // assignment operators
          ConstIterator& operator = (ConstIterator&& rhs) noexcept {
              _ptr = std::exchange(rhs._ptr, nullptr);
              return *this;
          }
          ConstIterator& operator = (const ConstIterator&) noexcept = default;

          constexpr auto operator ++ () noexcept -> ConstIterator& {
              ++_ptr;
              return *this;
          }

          constexpr auto operator ++ (int) noexcept -> ConstIterator {
              auto tmp = *this;
              ++_ptr;
              return tmp;
          }

          constexpr auto operator -- () noexcept -> ConstIterator& {
              --_ptr;
              return *this;
          }

          constexpr auto operator -- (int) noexcept -> ConstIterator {
              auto tmp = *this;
              --_ptr;
              return tmp;
          }

          constexpr auto operator + (deference_type n) const noexcept -> ConstIterator {
              return ConstIterator(_ptr + n);
          }

          constexpr auto operator - (deference_type n) const noexcept -> ConstIterator {
              return ConstIterator(_ptr - n);
          }

          constexpr auto operator += (deference_type n) noexcept -> ConstIterator& {
              _ptr += n;
              return *this;
          }

          constexpr auto operator -= (deference_type n) noexcept -> ConstIterator& {
              _ptr -= n;
              return *this;
          }

          constexpr auto operator - (const ConstIterator& other) const noexcept -> deference_type {
              return _ptr - other._ptr;
          }

          constexpr auto operator == (const ConstIterator& other) const noexcept -> bool {
              return _ptr == other._ptr;
          }

          constexpr auto operator != (const ConstIterator& other) const noexcept -> bool {
              return _ptr != other._ptr;
          }

          constexpr auto operator < (const ConstIterator& other) const noexcept -> bool {
              return _ptr < other._ptr;
          }

          constexpr auto operator > (const ConstIterator& other) const noexcept -> bool {
              return _ptr > other._ptr;
          }

          constexpr auto operator <= (const ConstIterator& other) const noexcept -> bool {
              return _ptr <= other._ptr;
          }

          constexpr auto operator >= (const ConstIterator& other) const noexcept -> bool {
              return _ptr >= other._ptr;
          }

          constexpr auto operator * () const noexcept -> const_reference {
              return *_ptr;
          }

          constexpr auto operator -> () const noexcept -> const_pointer { return _ptr; }

    }; // class ConstIterator

    // reverse iterator
    struct ReverseIterator
    {
       private:
        pointer _ptr;

       public:
        using iterator_category = std::random_access_iterator_tag;
        using deference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        ReverseIterator(): _ptr(nullptr) {}
        explicit ReverseIterator(pointer ptr) : _ptr(ptr) {}
        // copy and move constructor
        ReverseIterator(ReverseIterator&& rhs) noexcept: _ptr(std::exchange(rhs._ptr, nullptr)) {}
        ReverseIterator(const ReverseIterator&) noexcept = default;
        // assignment operators
        ReverseIterator& operator = (ReverseIterator&& rhs) noexcept {
            _ptr = std::exchange(rhs._ptr, nullptr);
            return *this;
        }
        ReverseIterator& operator = (const ReverseIterator&) noexcept = default;

        auto operator++() -> ReverseIterator& {
            --_ptr;
            return *this;
        }

        auto operator++(int) -> ReverseIterator {
            auto tmp = *this;
            --_ptr;
            return tmp;
        }

        auto operator--() -> ReverseIterator& {
            ++_ptr;
            return *this;
        }

        auto operator--(int) -> ReverseIterator {
            auto tmp = *this;
            ++_ptr;
            return tmp;
        }

        auto operator+=(deference_type n) -> ReverseIterator& {
            _ptr -= n;
            return *this;
        }

        auto operator-=(deference_type n) -> ReverseIterator& {
            _ptr += n;
            return *this;
        }

        auto operator+(deference_type n) const -> ReverseIterator {
            return ReverseIterator(_ptr - n);
        }

        auto operator-(deference_type n) const -> ReverseIterator {
            return ReverseIterator(_ptr + n);
        }

        auto operator-(const ReverseIterator& other) const -> deference_type {
            return other._ptr - _ptr;
        }

        auto operator==(const ReverseIterator& other) const -> bool {
            return _ptr == other._ptr;
        }

        auto operator!=(const ReverseIterator& other) const -> bool {
            return _ptr != other._ptr;
        }

        auto operator<(const ReverseIterator& other) const -> bool {
            return _ptr > other._ptr;
        }

        auto operator>(const ReverseIterator& other) const -> bool {
            return _ptr < other._ptr;
        }

        auto operator<=(const ReverseIterator& other) const -> bool {
            return _ptr >= other._ptr;
        }

        auto operator>=(const ReverseIterator& other) const -> bool {
            return _ptr <= other._ptr;
        }

        auto operator*() const -> reference {
            return *_ptr;
        }

        auto operator->() const -> pointer {
            return _ptr;
        }

    }; // class ReserveIterator

    struct ConstReverseIterator
    {
       private:
        const_pointer _ptr;

       public:
        using iterator_category = std::random_access_iterator_tag;
        using deference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = const T*;
        using reference = const T&;

        ConstReverseIterator(): _ptr(nullptr) {}
        explicit ConstReverseIterator(const_pointer ptr) : _ptr(ptr) {}
        // copy and move constructor
        ConstReverseIterator(ConstReverseIterator&& rhs) noexcept : _ptr(std::exchange(rhs._ptr, nullptr)) {}
        ConstReverseIterator(const ConstReverseIterator&) noexcept = default;
        // assignment operators
        ConstReverseIterator& operator = (ConstReverseIterator&& rhs) noexcept {
            _ptr = std::exchange(rhs._ptr, nullptr);
            return *this;
        }
        ConstReverseIterator& operator = (const ConstReverseIterator&) noexcept = default;

        auto operator++() -> ConstReverseIterator& {
            --_ptr;
            return *this;
        }

        auto operator++(int) -> ConstReverseIterator {
            auto tmp = *this;
            --_ptr;
            return tmp;
        }

        auto operator--() -> ConstReverseIterator& {
            ++_ptr;
            return *this;
        }

        auto operator--(int) -> ConstReverseIterator {
            auto tmp = *this;
            ++_ptr;
            return tmp;
        }

        auto operator+=(deference_type n) -> ConstReverseIterator& {
            _ptr -= n;
            return *this;
        }

        auto operator-=(deference_type n) -> ConstReverseIterator& {
            _ptr += n;
            return *this;
        }

        auto operator+(deference_type n) const -> ConstReverseIterator {
            return ConstReverseIterator(_ptr - n);
        }

        auto operator-(deference_type n) const -> ConstReverseIterator {
            return ConstReverseIterator(_ptr + n);
        }

        auto operator-(const ConstReverseIterator& other) const -> deference_type {
            return other._ptr - _ptr;
        }

        auto operator==(const ConstReverseIterator& other) const -> bool {
            return _ptr == other._ptr;
        }

        auto operator!=(const ConstReverseIterator& other) const -> bool {
            return _ptr != other._ptr;
        }

        auto operator<(const ConstReverseIterator& other) const -> bool {
            return _ptr > other._ptr;
        }

        auto operator>(const ConstReverseIterator& other) const -> bool {
            return _ptr < other._ptr;
        }

        auto operator<=(const ConstReverseIterator& other) const -> bool {
            return _ptr >= other._ptr;
        }

        auto operator>=(const ConstReverseIterator& other) const -> bool {
            return _ptr <= other._ptr;
        }

        auto operator*() const -> const_reference {
            return *_ptr;
        }

        auto operator->() const -> const_pointer {
            return _ptr;
        }

    };  // class ConstReverseIterator

    auto begin() noexcept -> Iterator {
        return Iterator(this->_start);
    }

    auto begin() const noexcept -> ConstIterator {
        return ConstIterator(this->_start);
    }

    auto cbegin() const noexcept -> ConstIterator {
        return ConstIterator(this->_start);
    }

    auto end() noexcept -> Iterator {
        return Iterator(this->_finish);
    }

    auto end() const noexcept -> ConstIterator {
        return ConstIterator(this->_finish);
    }

    auto cend() const noexcept -> ConstIterator {
        return ConstIterator(this->_finish);
    }

    auto rbegin() noexcept -> ReverseIterator {
        return ReverseIterator(this->_finish - 1);
    }

    auto rbegin() const noexcept -> ConstReverseIterator {
        return ConstReverseIterator(this->_finish - 1);
    }

    auto crbegin() const noexcept -> ConstReverseIterator {
        return ConstReverseIterator(this->_finish - 1);
    }

    auto rend() noexcept -> ReverseIterator {
        return ReverseIterator(this->_start - 1);
    }

    auto rend() const noexcept -> ConstReverseIterator {
        return ConstReverseIterator(this->_start - 1);
    }

    auto crend() const noexcept -> ConstReverseIterator {
        return ConstReverseIterator(this->_start - 1);
    }

    template<Safety safety = Safety::Safe>
    void fill(size_type(*filler)(T*)) {
        if constexpr (safety == Safety::Safe) {
            auto to_fill = filler(nullptr);
            if (to_fill + this->size() > this->capacity()) [[unlikely]]{
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
    [[nodiscard, gnu::always_inline]] auto get_buffer(size_type buf_size) -> std::span<T> {
        if (buf_size + this->_finish > this->_edge) {
            this->realloc_with_old_data(compute_new_capacity(buf_size));
        }
        auto buf = std::span<T>(this->_finish, buf_size);
        this->_finish += buf_size;
        return buf;
    }

    /*
     * Modifiers sections
     */
    template<Safety safety = Safety::Safe, typename U = T> requires std::is_object_v<U>
    void push_back(const value_type& value) {
        if constexpr (safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                copy_cref(this->_finish++, std::forward<const value_type&>(value));
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<const_reference>(value));
            }
        } else {
            assert(not this->full());
            copy_cref(this->_finish++, std::forward<const value_type &>(value));
        }
    }

    template<Safety safety = Safety::Safe, typename U = T> requires std::is_trivial_v<U> || std::is_move_constructible_v<U>
    void push_back(value_type&& value) {
        if constexpr (safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                copy_value(this->_finish++, std::forward<value_type &&>(value));
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<rvalue_reference>(value));
            }
        } else {
            assert(not this->full());
            copy_value(this->_finish++, std::forward<value_type &&>(value));
        }
    }

    template<Safety safety = Safety::Safe, typename ...Args>
    auto emplace_back(Args&&... args) -> Iterator {
        static_assert(not std::is_trivial_v<T>, "Use push_back() instead of emplace_back() with trivial types");
        if constexpr(safety == Safety::Safe) {
            if (!this->full()) [[likely]] {
                this->construct_at(this->_finish++, args...);
            } else {
                this->realloc_and_emplace_back(compute_next_capacity(), std::forward<Args...>(args...));
            }
            return Iterator(this->_finish - 1);
        } else {
            assert(not this->full());
            this->construct_at(this->_finish++, args...);
            return Iterator(this->_finish - 1);
        }

    }

    void clear() noexcept {
        this->destroy();
        this->_finish = this->_start;
    }

    void erase(ConstIterator pos) {
        assert(pos >= begin() and pos < end());

        auto pos_ptr = get_ptr_from_iter(pos);
        destroy_ptr(pos_ptr);
        this->move_forward(pos_ptr, pos_ptr + 1);
        --this->_finish;
    }

    void erase(Iterator pos) {
        assert(pos >= begin() and pos < end());
        T* ptr = get_ptr_from_iter(pos);
        destroy_ptr(ptr);
        this->move_forward(ptr, ptr + 1);
        --this->_finish;
    }

    void erase(ConstIterator first, ConstIterator last) {
        assert(first >= begin() and last < end());
        assert(last > first);
        auto first_ptr = get_ptr_from_iter(first);
        auto last_ptr = get_ptr_from_iter(last);
        destroy_range(first_ptr, last_ptr - first_ptr);
        this->move_forward(first_ptr, last_ptr);
        --this->_finish;
    }

    void erase(Iterator first, Iterator last) {
        assert(last > first);
        T* first_ptr = get_ptr_from_iter(first);
        T* last_ptr = get_ptr_from_iter(last);
        destroy_range(first_ptr, last_ptr);
        this->move_forward(first_ptr, last_ptr);
        this->_finish -= (last_ptr - first_ptr);
    }

    void pop_back() {
        destroy_ptr(this->_finish-- - 1);
    }

    template<Safety safety = Safety::Safe>
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
            auto * old_end = this->_finish;
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

    constexpr void swap(stdb_vector& other) noexcept {
        core<T>::swap(other);
    }

    template<Safety safety = Safety::Safe>
    constexpr auto insert(Iterator pos, const_reference value) -> Iterator {
        assert(pos >= begin() && pos <= end());
        T* pos_ptr = get_ptr_from_iter(pos);
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

    template<Safety safety = Safety::Safe>
    constexpr auto insert(Iterator pos, rvalue_reference value) -> Iterator {
        assert((pos >= begin()) && (pos <= end()));
        T* pos_ptr = get_ptr_from_iter(pos);
        if constexpr(safety == Safety::Safe) {
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
            return Iterator(pos_ptr);
        }
        if (pos_ptr < this->_finish - 1) [[likely]]{
            this->move_backward(pos_ptr + 1, pos_ptr);

        } else {
            this->move_forward(pos_ptr + 1, pos_ptr);
        }
        ++this->_finish;
        copy_value(pos_ptr, std::move(value));
        return Iterator(pos_ptr);
    }

    template<Safety safety = Safety::Safe>
    constexpr auto insert(Iterator pos, size_type count, const_reference value) -> Iterator {
        assert(pos >= begin() && pos <= end());
        auto size = this->size();
        T* pos_ptr = get_ptr_from_iter(pos);
        if constexpr( safety == Safety::Safe) {
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

    template<Safety safety = Safety::Safe, class InputIt> requires std::input_iterator<InputIt>
    constexpr auto insert(Iterator pos, InputIt first, InputIt last) -> Iterator {
        long count = last - first;
        if (count == 0) [[unlikely]] {
            return pos;
        }
        size_type size_to_insert = static_cast<size_type>(count);
        T* pos_ptr = get_ptr_from_iter(pos);

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

    template<Safety safety = Safety::Safe>
    constexpr auto insert(Iterator pos, std::initializer_list<T> ilist) -> Iterator {
        return insert<safety>(pos, ilist.begin(), ilist.end());
    }

    template<Safety safety = Safety::Safe, typename... Args>
    constexpr auto emplace(Iterator pos, Args&&... args) -> Iterator {
        T* pos_ptr = get_ptr_from_iter(pos);
        if constexpr(safety == Safety::Safe)  {
            if (this->full()) [[unlikely]] {
                std::ptrdiff_t pos_offset = pos_ptr - this->_start;
                reserve(compute_next_capacity());
                // calculate the new pos_ptr
                pos_ptr = this->_start + pos_offset;
            }
        }
        assert(not this->full());
        if (pos_ptr == this->_finish) [[unlikely]]{
            new (this->_finish++) T (std::forward<Args>(args)...);
            return Iterator(pos_ptr);
        }
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);
        } else {
            core<T>::move_forward(pos_ptr + 1, pos_ptr);
        }
        new (pos_ptr) T(std::forward<Args>(args)...);
        ++this->_finish;
        return Iterator(pos_ptr);
    }
    template<Safety safety = Safety::Safe, typename... Args>
    constexpr auto emplace(size_type pos, Args&&... args) -> Iterator {
        if constexpr(safety == Safety::Safe)  {
            if (this->full()) [[unlikely]] {
                reserve(compute_next_capacity());
            }
        }
        assert(not this->full());
        T* pos_ptr = this->_start + pos;

        if (pos_ptr == this->_finish) [[unlikely]]{
            new (this->_finish++) T (std::forward<Args>(args)...);
            return Iterator(pos_ptr);
        }
        if (pos_ptr < this->_finish - 1) [[likely]] {
            this->move_backward(pos_ptr + 1, pos_ptr);
        } else {
            core<T>::move_forward(pos_ptr + 1, pos_ptr);
        }
        new (pos_ptr) T(std::forward<Args>(args)...);
        ++this->_finish;
        return Iterator(pos_ptr);
    }

   private:
    [[nodiscard]] auto compute_new_capacity(size_type new_size) const -> size_type {
        assert(new_size > capacity());
        if (auto next_capacity = compute_next_capacity(); next_capacity > new_size) {
            return next_capacity;
        }
        return new_size;
    }
    [[nodiscard]] auto compute_next_capacity() const -> size_type {
        auto cap = capacity();
        if (cap < 4096 * 32 / sizeof(T) and cap > 0) [[likely]] {
            // the capacity is smaller than a page,
            // use 1.5 but not 2 to reuse memory objects.
            return (cap * 3 + 1) / 2;
        }
        // the capacity is larger than a page,
        // so we can just double it to use whole pages.
        if (cap > 4096 * 32 / sizeof(T)) [[likely]] {
            return cap * 2;
        }
        return core<T>::kFastVectorInitCapacity;
    }
}; // class stdb_vector

template<typename T>
auto operator == (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
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

template<typename T>
auto operator != (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    return !(lhs == rhs);
}

template<typename T>
auto operator >= (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if (lhs[i] < rhs[i]) {
            return false;
        }
    }
    return true;
}

template<typename T>
auto operator <= (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return true;
}

template<typename T>
auto operator > (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    if (lhs.empty() or rhs.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if (lhs[i] <= rhs[i]) {
            return false;
        }
    }
    return true;
}

template<typename T>
auto operator < (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> bool {
    if (lhs.empty() or rhs.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < std::min(lhs.size(), rhs.size()); ++i) {
        if (lhs[i] >= rhs[i]) {
            return false;
        }
    }
    return true;
}

template<typename T>
auto operator <=> (const stdb_vector<T>& lhs, const stdb_vector<T>& rhs) -> std::strong_ordering {
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

} // namespace stdb::container
