#include <cstdint>
#include <string/small_string.hpp>
#include <tuple>
#include <type_traits>
#include <utility>

#include "fmt/core.h"
#if __has_include(<ankerl/unordered_dense.hpp>)
#include <ankerl/unordered_dense.hpp>
#define HAS_ANKERL_UNORDERED_DENSE
#else
#include <xxhash.h>
#endif

namespace stdb::container {

enum class layout_type : uint8_t
{
    K = 0,
    KV = 1,
    VK = 2,
};

namespace detail {

struct nonesuch
{};

template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
struct detector
{
    using value_t = std::false_type;
    using type = Default;
};

template <class Default, template <class...> class Op, class... Args>
struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
    using type = Op<Args...>;
};

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<detail::nonesuch, void, Op, Args...>::value_t;

template <template <class...> class Op, class... Args>
constexpr bool is_detected_v = is_detected<Op, Args...>::value;

template <typename T>
using detect_is_transparent = typename T::is_transparent;

template <typename T>
using detect_avalanching = typename T::is_avalanching;

template <typename Hash, typename KeyEqual>
constexpr bool is_transparent_v =
  is_detected_v<detect_is_transparent, Hash> && is_detected_v<detect_is_transparent, KeyEqual>;
}  // namespace detail

template <typename K, typename V>
    requires(sizeof(K) == 8 && sizeof(V) == 4)
struct k8_v4_bucket
{
    static constexpr uint32_t kFingerWidth = 8U;
    static constexpr uint32_t kDistInc = 1U << kFingerWidth;
    static constexpr uint32_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor =
      not(std::is_trivially_destructible_v<K> and std::is_trivially_destructible_v<V>);
    constexpr static layout_type layout = layout_type::VK;
    uint32_t dist_and_fingerprint;
    V value;
    K key;
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k8v4 bucket: dist_and_fingerprint: {}, key: {}, value: {}\n", dist_and_fingerprint, value, key);
        } else {
            fmt::print("k8v4 bucket: empty\n");
        }
    }
    struct view
    {
        uint32_t _;
        V second;
        K first;
    };
    k8_v4_bucket() = default;
    k8_v4_bucket(uint32_t d_f, K&& k, V&& v) : dist_and_fingerprint(d_f), value(std::move(v)), key(std::move(k)) {}
    // move constructor
    k8_v4_bucket(k8_v4_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)),
          value(std::move(other.value)),
          key(std::move(other.key)) {}
    // copy constructor
    k8_v4_bucket(const k8_v4_bucket& other)
        : dist_and_fingerprint(other.dist_and_fingerprint), value(other.value), key(other.key) {}

};  // struct k8_v4_bucket

static_assert(sizeof(k8_v4_bucket<uint64_t, uint32_t>) == 2 * sizeof(uint64_t));

template <typename K>
    requires(sizeof(K) == 4)
struct k4_bucket
{
    static constexpr uint32_t kFingerWidth = 8U;
    static constexpr uint32_t kDistInc = 1U << kFingerWidth;
    static constexpr uint32_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor = not std::is_trivially_destructible_v<K>;
    constexpr static layout_type layout = layout_type::K;
    uint32_t dist_and_fingerprint;
    K key;
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k4 bucket: dist_and_fingerprint: {}, key: {}\n", dist_and_fingerprint, key);
        } else {
            fmt::print("k4 bucket: empty\n");
        }
    }
    struct view
    {
        uint32_t _;
        K value;
    };
    k4_bucket() = default;
    k4_bucket(uint32_t d_f, K&& k) : dist_and_fingerprint(d_f), key(std::move(k)) {}
    k4_bucket(const k4_bucket& other) : dist_and_fingerprint(other.dist_and_fingerprint), key(other.key) {}
    k4_bucket(k4_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)), key(std::move(other.key)) {}
};  // struct k4_bucket

static_assert(sizeof(k4_bucket<uint32_t>) == sizeof(uint64_t));

template <typename K>
    requires(sizeof(K) == sizeof(uint64_t))
struct k8_bucket
{
    static constexpr uint64_t kFingerWidth = 32U;
    static constexpr uint64_t kDistInc = 1UL << kFingerWidth;
    static constexpr uint64_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor = not std::is_trivially_destructible_v<K>;
    constexpr static layout_type layout = layout_type::K;
    uint64_t dist_and_fingerprint;
    K key;
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k8 bucket: dist_and_fingerprint: {}, key: {}\n", dist_and_fingerprint, key);
        } else {
            fmt::print("k8 bucket: empty\n");
        }
    }
    struct view
    {
        uint64_t _;
        K value;
    };
    k8_bucket() = default;
    k8_bucket(uint64_t d_f, K&& k) : dist_and_fingerprint(d_f), key(std::move(k)) {}
    k8_bucket(const k8_bucket& other) : dist_and_fingerprint(other.dist_and_fingerprint), key(other.key) {}
    k8_bucket(k8_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)), key(std::move(other.key)) {}
};  // struct k8_bucket

static_assert(sizeof(k8_bucket<uint64_t>) == 2 * sizeof(uint64_t));

template <typename K>
    requires(sizeof(K) == 3 * sizeof(uint32_t))
struct k12_bucket
{
    static constexpr uint32_t kFingerWidth = 8U;
    static constexpr uint32_t kDistInc = 1U << kFingerWidth;
    static constexpr uint32_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor = not std::is_trivially_destructible_v<K>;
    constexpr static layout_type layout = layout_type::K;
    uint32_t dist_and_fingerprint;
    K key;
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k12 bucket: dist_and_fingerprint: {}, key: {}\n", dist_and_fingerprint, key);
        } else {
            fmt::print("k12 bucket: empty\n");
        }
    }
    struct view
    {
        uint32_t _;
        K value;
    };
    k12_bucket() = default;
    k12_bucket(uint32_t d_f, K&& k) : dist_and_fingerprint(d_f), key(std::move(k)) {}
    k12_bucket(const k12_bucket& other) : dist_and_fingerprint(other.dist_and_fingerprint), key(other.key) {}
    k12_bucket(k12_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)), key(std::move(other.key)) {}
};  // struct k12_bucket

template <typename K, typename V>
    requires(sizeof(K) == 4 && sizeof(V) == 8 && std::is_trivially_copyable_v<K> && std::is_trivially_copyable_v<V>)
struct k4_v8_bucket
{
    static constexpr uint32_t kFingerWidth = 8U;
    static constexpr uint32_t kDistInc = 1U << kFingerWidth;
    static constexpr uint32_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor =
      not(std::is_trivially_destructible_v<K> and std::is_trivially_destructible_v<V>);
    constexpr static layout_type layout = layout_type::KV;
    uint32_t dist_and_fingerprint;
    K key;
    V value;
    struct view
    {
        uint32_t _;
        K first;
        V second;
    };
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k4v8 bucket: dist_and_fingerprint: {}, key: {}, value: {}\n", dist_and_fingerprint, key, value);
        } else {
            fmt::print("k4v8 bucket: empty\n");
        }
    }
    k4_v8_bucket() = default;
    k4_v8_bucket(uint32_t d_f, K&& k, V&& v) : dist_and_fingerprint(d_f), key(std::move(k)), value(std::move(v)) {}
    k4_v8_bucket(const k4_v8_bucket& other)
        : dist_and_fingerprint(other.dist_and_fingerprint), key(other.key), value(other.value) {}
    k4_v8_bucket(k4_v8_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)),
          key(std::move(other.key)),
          value(std::move(other.value)) {}
};  // struct k4_v8_bucket

static_assert(sizeof(k4_v8_bucket<uint32_t, uint64_t>) == 2 * sizeof(uint64_t));

template <typename K, typename V>
    requires(sizeof(K) == sizeof(uint64_t) and sizeof(V) == sizeof(uint64_t))
struct k8_v8_bucket
{
    static constexpr uint32_t kFingerWidth = 32U;
    static constexpr uint64_t kDistInc = 1UL << kFingerWidth;
    static constexpr uint64_t kFingerprintMask = kDistInc - 1;
    constexpr static bool need_destructor =
      not(std::is_trivially_destructible_v<K> and std::is_trivially_destructible_v<V>);
    constexpr static layout_type layout = layout_type::KV;
    uint64_t dist_and_fingerprint;
    K key;
    V value;
    void print() const {
        if (dist_and_fingerprint != 0) {
            fmt::print("k8v8 bucket: dist_and_fingerprint: {}, key: {}, value: {}\n", dist_and_fingerprint, key, value);
        } else {
            fmt::print("k8v8 bucket: empty\n");
        }
    }
    struct view
    {
        uint64_t _;
        K first;
        V second;
    };
    k8_v8_bucket() = default;
    k8_v8_bucket(uint64_t d_f, K&& k, V&& v) : dist_and_fingerprint(d_f), key(std::move(k)), value(std::move(v)) {}
    k8_v8_bucket(const k8_v8_bucket& other)
        : dist_and_fingerprint(other.dist_and_fingerprint), key(other.key), value(other.value) {}
    k8_v8_bucket(k8_v8_bucket&& other) noexcept
        : dist_and_fingerprint(std::exchange(other.dist_and_fingerprint, 0)),
          key(std::move(other.key)),
          value(std::move(other.value)) {}
};  // struct k8_v8_bucket

static_assert(sizeof(k8_v8_bucket<uint64_t, uint64_t>) == 3 * sizeof(uint64_t));

template <typename Mapped>
constexpr bool is_map_v = not std::is_void_v<Mapped>;

template <typename K, typename V>
consteval auto BucketChooser() {
    if constexpr (sizeof(K) == sizeof(uint64_t) and std::is_void_v<V>) {
        return k8_bucket<K>{};
    } else if constexpr (sizeof(K) == sizeof(uint32_t) and std::is_void_v<V>) {
        return k4_bucket<K>{};
    } else if constexpr (sizeof(K) == sizeof(uint64_t) and sizeof(V) == 4) {
        return k8_v4_bucket<K, V>{};
    } else if constexpr (sizeof(K) == sizeof(uint32_t) and sizeof(V) == sizeof(uint64_t)) {
        return k4_v8_bucket<K, V>{};
    } else if constexpr (sizeof(K) == sizeof(uint64_t) and sizeof(V) == sizeof(uint64_t)) {
        return k8_v8_bucket<K, V>{};
    } else {
        static_assert(false, "Invalid key and value size was not supported");
    }
}

[[nodiscard, gnu::always_inline]] constexpr auto calculate_shifts(uint32_t bucket_count) -> uint8_t {
    Assert(bucket_count > 1, "bucket_count must be greater than 0");
    return 64 - std::bit_width(bucket_count - 1);
}
template <typename T>
[[gnu::always_inline, nodiscard]] static inline auto calc_num_buckets_by_shift(uint8_t shift) -> T {
    return T{1} << (64U - shift);
}

template <typename Key, typename Value, class Hash, class KeyEqual,
          typename Layout = decltype(BucketChooser<Key, Value>()), typename BucketContainer = void,
          float MaxLoadFactor = 0.8F>
class inplace_table
{
    using distance_and_fingerprint_t = decltype(Layout::dist_and_fingerprint);
    // no memory enough for uint64_t, all of buckets number will be less than 2^32
    using bucket_index_t = uint32_t;
    using node_type = std::conditional_t<is_map_v<Value>, std::pair<Key, Value>, Key>;

    struct bucket_view
    {
        distance_and_fingerprint_t _;
    };

    union bucket_t
    {
        Layout layout;
        Layout::view view;
        std::array<distance_and_fingerprint_t, sizeof(Layout) / sizeof(distance_and_fingerprint_t)> data;
        bucket_t() : data() {};
        explicit bucket_t(Layout&& lay) : layout(std::move(lay)) {}
        bucket_t(bucket_t&& other) noexcept : data(std::exchange(other.data, {})) {}
        bucket_t(const bucket_t& other) : data(other.data) {}
        auto operator=(bucket_t&& other) noexcept -> bucket_t& {
            data = std::exchange(other.data, {});
            return *this;
        }
        ~bucket_t() {
            if constexpr (Layout::need_destructor) {
                layout.~Layout();
            }
        }
    };
    static_assert(sizeof(bucket_t) == sizeof(Layout));

    struct default_bucket_container_t
    {
        bucket_t* begin_bucket;
        bucket_t* end_bucket;
        auto begin() const -> bucket_t* { return begin_bucket; }
        auto end() const -> bucket_t* { return end_bucket; }
        auto begin() -> bucket_t* { return begin_bucket; }
        auto end() -> bucket_t* { return end_bucket; }

        auto at(bucket_index_t idx) -> bucket_t* {
            Assert(idx < capacity(), "idx is out of range");
            return begin_bucket + idx;
        }

        auto at(bucket_index_t idx) const -> const bucket_t* {
            Assert(idx < capacity(), "idx is out of range");
            return begin_bucket + idx;
        }

        [[gnu::always_inline, nodiscard]] auto capacity() const -> bucket_index_t { return end_bucket - begin_bucket; }
    };

    using bucket_container_t =
      std::conditional_t<std::is_same_v<BucketContainer, void>, default_bucket_container_t, BucketContainer>;

   public:
    template <bool IsConst>
    struct Iterator
    {
        using slot_t = std::conditional_t<IsConst, const bucket_t, bucket_t>;
        mutable slot_t* current;
        const slot_t* end;

        Iterator() = delete;

        template <bool OtherIsConst, std::enable_if_t<IsConst and not OtherIsConst, bool> = true>
        Iterator(const Iterator<OtherIsConst>& other) : current(other.current), end(other.end) {}

        Iterator(slot_t* current_pos, const slot_t* end_pos) : current(current_pos), end(end_pos) {}
        Iterator(const Iterator& other) : current(other.current), end(other.end) {}
        Iterator(Iterator&& other) noexcept : current(other.current), end(other.end) {}
        auto operator=(const Iterator& other) -> Iterator& = default;
        auto operator=(Iterator&& other) noexcept -> Iterator& = default;
        // add functions of iterator
        ~Iterator() = default;
        template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
        [[nodiscard]] constexpr auto operator*() const -> Layout::view& {
            Assert(current != nullptr, "current must be not nullptr");
            return current->view;
        }

        template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
        [[nodiscard]] constexpr auto operator->() const -> Layout::view* {
            Assert(current != nullptr, "current must be not nullptr");
            return &current->view;
        }
        template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
        [[nodiscard]] constexpr auto operator*() const -> Key& {
            Assert(current != nullptr, "current must be not nullptr");
            return current->view.value;
        }

        template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
        [[nodiscard]] constexpr auto operator->() const -> Key* {
            Assert(current != nullptr, "current must be not nullptr");
            return &current->view.value;
        }

        [[nodiscard]] constexpr auto operator==(const Iterator& other) const -> bool {
            return current == other.current;
        }

        [[nodiscard]] constexpr auto operator!=(const Iterator& other) const -> bool {
            return current != other.current;
        }
        [[nodiscard]] constexpr auto operator++() -> Iterator& {
            do {
                ++current;
            } while (current != end and current->layout.dist_and_fingerprint == 0);
            return *this;
        }

        [[nodiscard]] constexpr auto operator++(int) -> Iterator {
            auto ret = *this;
            do {
                ++current;
            } while (current != end and current->layout.dist_and_fingerprint == 0);
            return ret;
        }

        [[nodiscard]] constexpr auto operator+(bucket_index_t n) const -> Iterator {
            auto new_ptr = current + n;
            while (new_ptr != end and new_ptr->layout.dist_and_fingerprint == 0) {
                ++new_ptr;
            }
            return Iterator{new_ptr, end};
        }

        [[nodiscard]] constexpr auto operator-(bucket_index_t n) const -> Iterator {
            return Iterator{current - n, end};
        }

        [[nodiscard]] constexpr auto operator-(const Iterator& other) const -> ptrdiff_t {
            return current - other.current;
        }
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

   private:
    constexpr static uint8_t kInitialShifts = 64 - 2;
    uint8_t _shifts;
    float _max_load_factor = MaxLoadFactor;
    bucket_container_t _buckets{};
    bucket_index_t _max_bucket_capacity =
      0;  // TODO(leo): maybe use calc_num_buckets_by_shift to calculate the max_bucket_capacity
    bucket_index_t _size = 0;
    Hash _hasher = {};
    KeyEqual _key_eq;

    [[gnu::always_inline]] static inline void copy_bucket_and_change_fingerprint(
      const bucket_t* src, bucket_t* dst, distance_and_fingerprint_t dist_and_fingerprint) __attribute__((nonnull)) {
        dst->data[0] = dist_and_fingerprint;

#if defined(__clang__)
#pragma clang loop vectorize(enable)
#endif
        for (size_t i = 1; i < sizeof(bucket_t) / sizeof(distance_and_fingerprint_t); ++i) {
            dst->data[i] = src->data[i];
        }
        return;
    }

    static auto new_bucket_with_new_dist_and_fingerprint(const bucket_t* original,
                                                         distance_and_fingerprint_t dist_and_fingerprint) -> bucket_t {
        // just copy the data, avoid to use copy or move constructor
        bucket_t new_bucket;
        copy_bucket_and_change_fingerprint(original, &new_bucket, dist_and_fingerprint);
        return new_bucket;
    }

    // create the buckets with new_capacity and new_size, and return the bucket_container_t
    // NOTE: the new_size is the size of the filled buckets , the new_capacity is the total size of the buckets
    // and the size will just be set, the content of the buckets will be filled later in eeeell_buckets function
    [[nodiscard, gnu::always_inline]] static auto create_buckets(bucket_index_t new_capacity) -> bucket_container_t {
        if constexpr (std::is_same_v<BucketContainer, void>) {
            auto* buf = reinterpret_cast<bucket_t*>(std::malloc(sizeof(bucket_t) * new_capacity));
            if (buf == nullptr) [[unlikely]] {
                throw std::bad_alloc();
            }
#if defined(__clang__)
#pragma clang loop vectorize(enable)
#endif
            for (bucket_index_t i = 0; i < new_capacity; ++i) {
                buf[i].layout.dist_and_fingerprint = 0;
            }
            return {.begin_bucket = buf, .end_bucket = buf + new_capacity};
        } else {
            Assert(false, "not implemented");
        }
    }

    void init_buckets_from_shift() {
        auto num_buckets = calc_num_buckets_by_shift<bucket_index_t>(_shifts);
        if constexpr (std::is_same_v<BucketContainer, void>) {
            _buckets = create_buckets(num_buckets);
        } else {
            Assert(false, "not implemented");
        }

        if (num_buckets == max_bucket_count()) {
            _max_bucket_capacity = max_bucket_count();
        } else {
            _max_bucket_capacity = static_cast<bucket_index_t>(static_cast<float>(num_buckets) * _max_load_factor);
        }
    }

    auto do_rehash_with_new_shift(uint8_t new_shifts) -> void {
        // assume that the _shifts is set correctly
        Assert(_shifts != new_shifts, "the new shifts was not changed!");
        _shifts = new_shifts;
        auto num_of_new_buckets = calc_num_buckets_by_shift<bucket_index_t>(new_shifts);
        Assert(num_of_new_buckets * _max_load_factor >= _size, "the new bucket capacity is not enough");

        if constexpr (std::is_same_v<BucketContainer, void>) {
            auto new_buckets = create_buckets(num_of_new_buckets);
            auto fill_count = _size;
            // TODO(leo): maybe check the size will be slower then just use check with end().
            // benchmark it later.
            for (auto iter = _buckets.begin(); fill_count > 0 and iter != _buckets.end(); ++iter) {
                auto [dist_and_fingerprint, bucket_ptr] = next_while_less(new_buckets, iter->layout.key);
                place_and_shift_up<false>(new_bucket_with_new_dist_and_fingerprint(iter, dist_and_fingerprint),
                                          bucket_ptr);
                --fill_count;
            }
            // free the old buckets's memory
            std::free(_buckets.begin_bucket);
            // assign the new buckets to the _buckets
            _buckets = new_buckets;
            _max_bucket_capacity =
              static_cast<bucket_index_t>(static_cast<float>(num_of_new_buckets) * _max_load_factor);
            return;
        } else {
            Assert(false, "not implemented");
        }
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_rehash_and_emplace_one(uint8_t new_shifts, Key&& new_key, Q&& new_value) -> bucket_t* {
        do_rehash_with_new_shift(new_shifts);
        auto [dist_and_fingerprint, bucket_ptr] = next_while_less(_buckets, new_key);
        return place_and_shift_up(
          bucket_t{Layout{dist_and_fingerprint, std::forward<Key>(new_key), std::forward<Q>(new_value)}}, bucket_ptr);
    }

    auto do_rehash_and_emplace_one(uint8_t new_shifts, Key&& new_key) -> bucket_t* {
        do_rehash_with_new_shift(new_shifts);
        auto [dist_and_fingerprint, bucket_ptr] = next_while_less(_buckets, new_key);
        if constexpr (Layout::layout == layout_type::K) {
            return do_place_element(dist_and_fingerprint, bucket_ptr, std::forward<Key>(new_key));
        } else {
            __builtin_unreachable();
        }
    }

    // static functions
    [[nodiscard]] static constexpr auto at(bucket_container_t& bucket, bucket_index_t offset) -> bucket_t& {
        return *bucket.at(offset);
    }

    [[nodiscard]] static constexpr auto at(const bucket_container_t& bucket, bucket_index_t offset) -> const bucket_t& {
        return *bucket.at(offset);
    }

    [[nodiscard]] static constexpr auto distance_increase(distance_and_fingerprint_t dist_and_finger)
      -> distance_and_fingerprint_t {
        return static_cast<distance_and_fingerprint_t>(dist_and_finger + Layout::kDistInc);
    }

    [[nodiscard]] static constexpr auto distance_decrease(distance_and_fingerprint_t dist_and_finger)
      -> distance_and_fingerprint_t {
        return static_cast<distance_and_fingerprint_t>(dist_and_finger - Layout::kDistInc);
    }

    // functions
    [[nodiscard]] constexpr auto next(bucket_t* bucket_ptr) const -> bucket_t* {
        if (++bucket_ptr == _buckets.end_bucket) [[unlikely]] {
            return _buckets.begin_bucket;
        }
        return bucket_ptr;
    }

    [[nodiscard]] constexpr auto next(bucket_index_t bucket_idx) const -> bucket_index_t {
        if (++bucket_idx == _buckets.capacity()) [[unlikely]] {
            return 0;
        }
        return bucket_idx;
    }

    [[nodiscard]] constexpr auto is_full() const -> bool {
        Assert(_max_bucket_capacity > 0, "max_bucket_capacity must be set");
        Assert(_size <= _max_bucket_capacity, "size must be not greater than max_bucket_capacity");
        return _size == _max_bucket_capacity;  // TODO(leo): maybe use 1U << _shifts to calculate the
                                               // max_bucket_capacity in runtime.
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto increase_size(Key&& new_key, Q&& new_value) -> bucket_t* {
        // do not check the size is overflow of uint64_t, because the OOM will come first

        // TODO(leo): allocate the new buckets, and copy the old buckets to the new buckets
        return do_rehash_and_emplace_one(_shifts - 1, std::forward<Key>(new_key), std::forward<Value>(new_value));
    }

    template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
    auto increase_size(Key&& new_key) -> bucket_t* {
        return do_rehash_and_emplace_one(_shifts - 1, std::forward<Key>(new_key));
    }

    template <typename K>
    [[nodiscard]] constexpr auto well_hash(K const& key) const -> uint64_t {
        // TODO(leo): just use the hash function by now, optimize later
        if constexpr (detail::is_detected_v<detail::detect_avalanching, Hash>) {
            // the hash function is avalanching, so we can use it directly
            if constexpr (sizeof(decltype(_hasher(key))) < sizeof(uint64_t)) {
                // 32bit hash and is_avalanching => multiply with a constant to avalanche bits upwards
                return _hasher(key) * UINT64_C(0x9ddfea08eb382d69);
            } else {
                // 64bit and is_avalanching => only use the hash itself.
                return _hasher(key);
            }
        } else {
            // not is_avalanching => apply wyhash or use xxhash
#if defined(HAS_ANKERL_UNORDERED_DENSE)
            return wyhash::hash(_hasher(key));
#else
            return XXH3_64bits_withSeed(&key, sizeof(key), 0);
#endif
        }

    }

    [[nodiscard]] constexpr auto extract_distance_and_fingerprint(uint64_t hash) const -> distance_and_fingerprint_t {
        return Layout::kDistInc | (static_cast<distance_and_fingerprint_t>(hash) & Layout::kFingerprintMask);
    }

    [[nodiscard]] constexpr auto calculate_bucket_idx_from_hash(uint64_t hash) const -> bucket_index_t {
        return static_cast<bucket_index_t>(hash >> _shifts);
    }

    [[nodiscard]] constexpr auto calculate_bucket_idx_from_distance_and_fingerprint(distance_and_fingerprint_t x) const
      -> bucket_index_t {
        return static_cast<bucket_index_t>(x >> Layout::kFingerWidth);
    }

    struct bucket_slot
    {
        distance_and_fingerprint_t dist_and_fingerprint;
        bucket_t* bucket;
    };

    template <typename K>
    [[nodiscard]] auto next_while_less(bucket_container_t& new_buckets, K const& key) const -> bucket_slot {
        auto hash = well_hash(key);
        auto dist_and_fingerprint = extract_distance_and_fingerprint(hash);
        auto bucket_idx = calculate_bucket_idx_from_hash(hash);
        auto* current_bucket = new_buckets.at(bucket_idx);
        while (dist_and_fingerprint < current_bucket->layout.dist_and_fingerprint) {
            dist_and_fingerprint = distance_increase(dist_and_fingerprint);
            current_bucket = next(current_bucket);
        }
        return {dist_and_fingerprint, current_bucket};
    }

    template <bool IncreaseSize = true>
    auto place_and_shift_up(bucket_t&& bucket, bucket_t* place) -> bucket_t* {
        auto* old_place = place;
        while (0 != place->layout.dist_and_fingerprint) {
            // if not empty, shift the bucket to the next place
            // std::exchange will handle internal ptr of bucket
            // TODO(leo): maybe use std::move will be faster.
            bucket = std::exchange(*place, std::move(bucket));
            bucket.layout.dist_and_fingerprint = distance_increase(bucket.layout.dist_and_fingerprint);
            place = next(place);
        }
        // found the empty place, place the bucket there
        *place = std::move(bucket);
        if constexpr (IncreaseSize) {
            ++_size;
        }
        return old_place;
    }

   private:
    // do_place_element function
    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_place_element(distance_and_fingerprint_t dist_and_fingerprint, bucket_t* place, Key&& key, Q&& value)
      -> bucket_t* {
        if (is_full()) [[unlikely]] {
            // ignore the place, just place all elements to the new buckets
            return increase_size(std::forward<Key>(key), std::forward<Q>(value));
        } else {
            // capacity is enough, place the element and shift up
            auto bucket = bucket_t{Layout{dist_and_fingerprint, std::forward<Key>(key), std::forward<Q>(value)}};
            return place_and_shift_up(std::move(bucket), place);
            // return place_and_shift_up(bucket_t{Layout{dist_and_fingerprint, std::forward<Key>(key),
            // std::forward<Q>(value)}}, place);
        }
    }

    // do_place_element just with key
    template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
    auto do_place_element(distance_and_fingerprint_t dist_and_fingerprint, bucket_t* place, Key&& key) -> bucket_t* {
        if (is_full()) [[unlikely]] {
            // ignore the place, just place all elements to the new buckets
            return increase_size(std::forward<Key>(key));
        } else {
            // capacity is enough, place the element and shift up
            if constexpr (Layout::layout == layout_type::K) {
                return place_and_shift_up(bucket_t{Layout{dist_and_fingerprint, std::forward<Key>(key)}}, place);
            } else {
                __builtin_unreachable();
            }
        }
    }

    // do_find function
    template <typename K>
    [[nodiscard]] auto do_find(K const& key) -> iterator {
        if (empty()) [[unlikely]] {
            return end();
        }
        auto hash = well_hash(key);
        // original distance and fingerprint
        auto dist_and_fingerprint = extract_distance_and_fingerprint(hash);
        // original bucket index while if best match
        auto bucket_idx = calculate_bucket_idx_from_hash(hash);
        auto* current_bucket = _buckets.at(bucket_idx);

        // unrolled loop. *Always* check a few directly, then enter the loop. This is faster.
        // and do not check the dist_and_fingerprint > current_bucket->layout.dist_and_fingerprint, because the
        // dist_and_fingerprint is always increasing. we **hopefully** find the key in the first few buckets.
        if (dist_and_fingerprint == current_bucket->layout.dist_and_fingerprint &&
            _key_eq(key, current_bucket->layout.key)) {
            return iterator{current_bucket, _buckets.end_bucket};
        }
        dist_and_fingerprint = distance_increase(dist_and_fingerprint);
        current_bucket = next(current_bucket);

        if (dist_and_fingerprint == current_bucket->layout.dist_and_fingerprint &&
            _key_eq(key, current_bucket->layout.key)) {
            return iterator{current_bucket, _buckets.end_bucket};
        }
        dist_and_fingerprint = distance_increase(dist_and_fingerprint);
        current_bucket = next(current_bucket);

        while (true) {
            // fmt::print("bucket id = {}, dist_and_fingerprint = {}\n", bucket_idx, dist_and_fingerprint);
            if (dist_and_fingerprint == current_bucket->layout.dist_and_fingerprint) {
                if (_key_eq(key, current_bucket->layout.key)) [[likely]] {
                    return iterator{current_bucket, _buckets.end_bucket};
                }
            } else if (dist_and_fingerprint > current_bucket->layout.dist_and_fingerprint) {
                return end();
            }
            dist_and_fingerprint = distance_increase(dist_and_fingerprint);
            current_bucket = next(current_bucket);
        }
        return end();
    }

    template <typename K>
    auto do_find(K const& key) const -> const_iterator {
        return const_cast<inplace_table*>(this)->do_find(key);
    }

    // do_at function
    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_at(Key const& key) -> Q& {
        if (auto pos = find(key); pos != end()) {
            return pos->second;
        }
        throw std::out_of_range("at: key not found");
    }

    template <typename K, typename Q = Value, typename H = Hash, typename KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && detail::is_transparent_v<H, KE>, bool> = true>
    auto do_at(K const& key) -> Q& {
        if (auto pos = find(key); pos != end()) {
            return pos->second;
        }
        throw std::out_of_range("at: key not found");
    }

    // do_try_emplace function
    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_try_emplace(Key&& key, Q&& value) -> std::pair<iterator, bool> {
        // use the bucket.key
        auto hash = well_hash(key);
        auto dist_and_fingerprint = extract_distance_and_fingerprint(hash);
        auto bucket_idx = calculate_bucket_idx_from_hash(hash);

        auto* current_bucket_ptr = _buckets.at(bucket_idx);
        while (dist_and_fingerprint <= current_bucket_ptr->layout.dist_and_fingerprint) {
            if (dist_and_fingerprint == current_bucket_ptr->layout.dist_and_fingerprint &&
                _key_eq(key, current_bucket_ptr->layout.key)) {
                return {iterator{current_bucket_ptr, _buckets.end_bucket}, false};
            }
            dist_and_fingerprint = distance_increase(dist_and_fingerprint);
            current_bucket_ptr = next(current_bucket_ptr);
        }

        // no found, place the element to the right place
        auto* new_bucket_ptr =
          do_place_element(dist_and_fingerprint, current_bucket_ptr, std::forward<Key>(key), std::forward<Q>(value));
        return {iterator{new_bucket_ptr, _buckets.end_bucket}, true};
    }

    // do_try_emplace just with key
    template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
    auto do_try_emplace(Key&& key) -> std::pair<iterator, bool> {
        auto hash = well_hash(key);
        auto dist_and_fingerprint = extract_distance_and_fingerprint(hash);
        auto bucket_idx = calculate_bucket_idx_from_hash(hash);

        auto* current_bucket_ptr = _buckets.at(bucket_idx);
        while (dist_and_fingerprint <= current_bucket_ptr->layout.dist_and_fingerprint) {
            if (dist_and_fingerprint == current_bucket_ptr->layout.dist_and_fingerprint &&
                _key_eq(key, current_bucket_ptr->layout.key)) {
                return {iterator{current_bucket_ptr, _buckets.end_bucket}, false};
            }
            dist_and_fingerprint = distance_increase(dist_and_fingerprint);
            current_bucket_ptr = next(current_bucket_ptr);
        }

        // no found, place the element to the right place
        auto* new_bucket_ptr = do_place_element(dist_and_fingerprint, current_bucket_ptr, std::forward<Key>(key));
        return {iterator{new_bucket_ptr, _buckets.end_bucket}, true};
    }

    // do_erase function
    template <typename Op>
    void __attribute__((nonnull)) do_erase(bucket_t* bucket, Op op) {
        // operate the bucket's content by handle_erased_value
        op(bucket->view);
        auto next_bucket = next(bucket);
        // shift down until either empty or an element with correct spot is found
        while (next_bucket->layout.dist_and_fingerprint >= Layout::kDistInc * 2) {
            // move the bucket to previous bucket
            copy_bucket_and_change_fingerprint(next_bucket, bucket,
                                               distance_decrease(next_bucket->layout.dist_and_fingerprint));
            bucket = std::exchange(next_bucket, next(next_bucket));
        }
        // set the bucket to empty, the bucket is empty now.
        bucket->layout.dist_and_fingerprint = 0;
        --_size;
        return;
    }

    auto do_erase_key(const Key& key) -> uint64_t {
        if (empty()) [[unlikely]] {
            return 0;
        }

        auto [dist_and_fingerprint, bucket_ptr] = next_while_less(_buckets, key);

        while (dist_and_fingerprint == bucket_ptr->layout.dist_and_fingerprint and
               not _key_eq(key, bucket_ptr->layout.key)) {
            dist_and_fingerprint = distance_increase(dist_and_fingerprint);
            bucket_ptr = next(bucket_ptr);
        }
        // quit the loop, if fingerprint is not same, then not found.
        if (dist_and_fingerprint != bucket_ptr->layout.dist_and_fingerprint) {
            return 0;
        }
        do_erase(bucket_ptr, [](auto&&) {});
        return 1;
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto do_insert_or_assign(Key&& key, Q&& value) -> std::pair<iterator, bool> {
        auto [it, success] = try_emplace(std::forward<Key>(key), std::forward<Q>(value));
        if (!success) {
            it->second = std::forward<Value>(value);
        }
        return {it, success};
    }

   public:
    // constructors
    inplace_table() : _shifts(kInitialShifts) { init_buckets_from_shift(); }

    explicit inplace_table(size_t bucket_count) : _shifts(calculate_shifts(bucket_count)) { init_buckets_from_shift(); }

    template <typename InputIt>
    inplace_table(InputIt first, InputIt last, size_t bucket_count = 0) : inplace_table(bucket_count) {
        insert(first, last);
    }

    inplace_table(const inplace_table& other)
        : _shifts(other._shifts),
          _max_load_factor(other._max_load_factor),
          _buckets(),
          _max_bucket_capacity(other._max_bucket_capacity),
          _size(other._size),
          _hasher(other._hasher),
          _key_eq(other._key_eq) {
        init_buckets_from_shift();
        if constexpr (not Layout::need_destructor) {
            // just memcpy the buckets
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
            std::memcpy(_buckets.begin_bucket, other._buckets.begin_bucket, _max_bucket_capacity * sizeof(bucket_t));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
        } else {
            for (auto *other_bucket = other._buckets.begin_bucket, *bucket = _buckets.begin_bucket;
                 _size > 0 and other_bucket != other._buckets.end_bucket; ++other_bucket, ++bucket) {
                if (other_bucket->layout.dist_and_fingerprint != 0) {
                    new (bucket) bucket_t{*other_bucket};
                }
            }
        }
    }

    inplace_table(inplace_table&& other) noexcept
        : _shifts(std::exchange(other._shifts, 0)),
          _max_load_factor(std::exchange(other._max_load_factor, 0.0F)),
          _buckets(std::exchange(other._buckets, {})),
          _max_bucket_capacity(std::exchange(other._max_bucket_capacity, 0)),
          _size(std::exchange(other._size, 0)),
          _hasher(std::exchange(other._hasher, {})),
          _key_eq(std::exchange(other._key_eq, {})) {}

    ~inplace_table() {
        if constexpr (Layout::need_destructor) {
            for (auto* bucket = _buckets.begin_bucket; _size > 0 and bucket != _buckets.end_bucket; ++bucket) {
                if (bucket->layout.dist_and_fingerprint != 0) {
                    bucket->layout.~Layout();
                }
            }
        }
        std::free(_buckets.begin_bucket);
    }

    auto operator=(const inplace_table& other) -> inplace_table& {
        if (this == &other) {
            return *this;
        }
        this->~inplace_table();
        new (this) inplace_table(other);
        return *this;
    }

    auto operator=(inplace_table&& other) noexcept -> inplace_table& {
        if (this == &other) {
            return *this;
        }
        if constexpr (Layout::need_destructor) {
            for (auto* bucket = _buckets.begin_bucket; _size > 0 and bucket != _buckets.end_bucket; ++bucket) {
                if (bucket->layout.dist_and_fingerprint != 0) {
                    bucket->layout.~Layout();
                }
            }
        }
        std::free(_buckets.begin_bucket);
        new (this) inplace_table(std::move(other));
        return *this;
    }

    // hash policy
    [[nodiscard]] auto load_factor() const -> float {
        return bucket_count() > 0 ? static_cast<float>(size()) / static_cast<float>(bucket_count()) : 0.0F;
    }

    [[nodiscard]] auto max_load_factor() const -> float { return _max_load_factor; }

    void max_load_factor(float ml) {
        _max_load_factor = ml;
        if (bucket_count() != max_bucket_count()) {
            _max_bucket_capacity = static_cast<bucket_index_t>(static_cast<float>(bucket_count()) * max_load_factor());
        }
    }

    void rehash(bucket_index_t new_capacity) {
        new_capacity = new_capacity / _max_load_factor;
        auto new_shifts = std::min(calculate_shifts(new_capacity), calculate_shifts(_size));
        do_rehash_with_new_shift(new_shifts);
    }

    void reserve(size_t new_capacity) {
        new_capacity = new_capacity / _max_load_factor;

        if (new_capacity > max_bucket_count()) [[unlikely]] {
            Assert(false, "inplace_table::reserve: new_capacity just support uint32_t type capacity");
            throw std::bad_alloc();
        }
        if (new_capacity > _buckets.capacity()) {
            do_rehash_with_new_shift(calculate_shifts(new_capacity));
        }
        // else do nothing
    }

    // observers
    [[nodiscard]] auto hash_function() const -> Hash { return _hasher; }

    [[nodiscard]] auto key_eq() const -> KeyEqual { return _key_eq; }

    // iterator functions
    auto begin() noexcept -> iterator {
        for (auto bucket : _buckets) {
            if (bucket.dist_and_fingerprint != 0) {
                return iterator{bucket};
            }
        }
        return end();
    }

    auto begin() const noexcept -> const_iterator {
        for (auto bucket : _buckets) {
            if (bucket.dist_and_fingerprint != 0) {
                return const_iterator{bucket};
            }
        }
        return end();
    }

    auto cbegin() const noexcept -> const_iterator { return begin(); }

    auto end() noexcept -> iterator { return iterator{_buckets.end_bucket, _buckets.end_bucket}; }

    auto end() const noexcept -> const_iterator { return const_iterator{_buckets.end_bucket, _buckets.end_bucket}; }

    auto cend() const noexcept -> const_iterator { return const_iterator{_buckets.end_bucket, _buckets.end_bucket}; }

    // capacity functions
    [[nodiscard, gnu::always_inline]] auto empty() const noexcept -> bool { return _size == 0; }

    [[nodiscard, gnu::always_inline]] auto size() const noexcept -> size_t { return _size; }

    [[nodiscard, gnu::always_inline]] static consteval auto max_size() -> bucket_index_t {
        // TODO(leo): maybe not right
        return std::numeric_limits<bucket_index_t>::max();
    }

    // bucket interface
    [[nodiscard, gnu::always_inline]] auto max_bucket_count() const noexcept -> bucket_index_t { return max_size(); }

    [[nodiscard, gnu::always_inline]] auto bucket_count() const noexcept -> bucket_index_t {
        return _buckets.capacity();
    }

    // modifiers
    void clear() {
        _size = 0;
        if constexpr (Layout::need_destructor) {
            for (auto* bucket = _buckets.begin_bucket; _size > 0 and bucket != _buckets.end_bucket; ++bucket) {
                if (bucket->layout.dist_and_fingerprint != 0) {
                    if constexpr (Layout::need_destructor) {
                        bucket->layout.~Layout();
                    }
                }
            }
        } else {
#if defined(__clang__)
#pragma clang loop vectorize(enable)
#endif
            for (auto* bucket = _buckets.begin_bucket; bucket != _buckets.end_bucket; ++bucket) {
                bucket->layout.dist_and_fingerprint = 0;
            }
        }
    }

    void swap(inplace_table& other) noexcept {
        using std::swap;
        swap(other, *this);
    }

    template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
    auto emplace(Key&& key) -> std::pair<iterator, bool> {
        return do_try_emplace(std::forward<Key>(key));
    }

    /**
     * @brief emplace the element to the right place
     *
     * @param key
     * @param value
     * @return std::pair<iterator, bool>
     * @note keep same behavior with try_emplace
     */
    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto emplace(Key&& key, Q&& value) -> std::pair<iterator, bool> {
        return do_try_emplace(std::forward<Key>(key), std::forward<Q>(value));
    }

    auto emplace_hint(const_iterator /*hint*/, Key&& key) -> iterator {
        return do_try_emplace(std::forward<Key>(key)).first;
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto emplace_hint(const_iterator /*hint*/, Key&& key, Q&& value) -> iterator {
        return do_try_emplace(std::forward<Key>(key), std::forward<Q>(value)).first;
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(Key const& key, Q&& value) -> std::pair<iterator, bool> {
        return do_try_emplace(key, std::forward<Q>(value));
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto try_emplace(Key&& key, Q&& value) -> std::pair<iterator, bool> {
        return do_try_emplace(std::forward<Key>(key), std::forward<Q>(value));
    }

    template <typename Q = Value, std::enable_if_t<std::is_void_v<Q>, bool> = true>
    auto try_emplace(Key&& key) -> std::pair<iterator, bool> {
        return do_try_emplace(std::forward<Key>(key));
    }

    auto insert(node_type const& value) -> std::pair<iterator, bool> {
        if constexpr (is_map_v<Value>) {
            auto [k, v] = value;
            return emplace(std::move(k), std::move(v));
        } else {
            return emplace(std::move(value));
        }
    }

    auto insert(node_type&& value) -> std::pair<iterator, bool> {
        if constexpr (is_map_v<Value>) {
            return emplace(std::move(value.first), std::move(value.second));
        } else {
            return emplace(std::move(value));
        }
    }

    auto insert(const_iterator /*hint*/, node_type const& value) -> iterator {
        if constexpr (is_map_v<Value>) {
            return emplace(value.first, value.second).first;
        } else {
            return emplace(value).first;
        }
    }

    auto insert(const_iterator /*hint*/, node_type&& value) -> iterator {
        if constexpr (is_map_v<Value>) {
            return emplace(std::move(value.first), std::move(value.second)).first;
        } else {
            return emplace(std::move(value.value)).first;
        }
    }

    template <class InputIt>
    void insert(InputIt first, InputIt last) {
        while (first != last) {
            insert(*first);
            ++first;
        }
    }

    void insert(std::initializer_list<node_type> ilist) { insert(ilist.begin(), ilist.end()); }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(Key const& key, Q&& value) -> std::pair<iterator, bool> {
        return do_insert_or_assign(key, std::forward<Q>(value));
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    auto insert_or_assign(Key&& key, Q&& value) -> std::pair<iterator, bool> {
        return do_insert_or_assign(std::forward<Key>(key), std::forward<Q>(value));
    }

    auto erase(iterator pos) -> iterator {
        do_erase(pos.current, [](auto&&) {});
        return ++pos;
    }

    auto erase(const_iterator pos) -> iterator {
        do_erase(pos.current, [](auto&&) {});
        return ++pos;
    }

    auto erase(const_iterator first, const_iterator last) -> iterator {
        for (auto iter = first; iter != last; ++iter) {
            do_erase(iter.current, [](auto&&) {});
        }
        return ++last;
    }

    auto erase(Key const& key) -> size_t { return do_erase_key(key); }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    auto erase(Key&& key) -> size_t {
        const auto local_key(key);
        return do_erase_key(local_key);
    }

    auto extract(iterator pos) -> node_type {
        node_type node;
        do_erase(pos.current, [&node](auto&& view) {
            if constexpr (is_map_v<Value>) {
                node.first = std::move(view.first);
                node.second = std::move(view.second);
            } else {
                node = std::move(view.value);
            }
        });
        return node;
    }

    auto extract(const_iterator pos) -> node_type { return extract(iterator{pos.current}); }

    auto extract(Key const& key) -> node_type {
        auto pos = find(key);
        if (pos == end()) {
            return node_type{};
        }
        return extract(pos);
    }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    auto extract(K&& key) -> node_type {
        auto pos = find(std::forward<K>(key));
        if (pos == end()) {
            return node_type{};
        }
        return extract(pos);
    }

    auto merge(inplace_table& other) -> void {
        if (this == &other) [[unlikely]] {
            return;
        }
        if (other.empty()) [[unlikely]] {
            return;
        }

        for (auto& bucket : other._buckets) {
            if (bucket.layout.dist_and_fingerprint > 0) {
                if constexpr (is_map_v<Value>) {
                    try_emplace(std::move(bucket.layout.key), std::move(bucket.layout.value));
                } else {
                    try_emplace(std::move(bucket.layout.key));
                }
            }
        }
    }

    auto merge(inplace_table&& other) -> void {
        if (this == &other) [[unlikely]] {
            return;
        }
        if (other.empty()) [[unlikely]] {
            return;
        }

        for (auto& bucket : other._buckets) {
            if (bucket.layout.dist_and_fingerprint > 0) {
                try_emplace(std::move(bucket.layout.key), std::move(bucket.layout.value));
            }
        }
    }

    // lookup functions
    [[nodiscard]] auto find(Key const& key) -> iterator { return do_find(key); }

    [[nodiscard]] auto find(Key const& key) const -> const_iterator { return do_find(key); }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    [[nodiscard]] auto at(Key const& key) -> Q& {
        return do_at(key);
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    [[nodiscard]] auto at(Key const& key) const -> Q const& {
        return do_at(key);
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    [[nodiscard]] auto operator[](Key const& key) -> Q& {
        return do_at(key);
    }

    template <typename Q = Value, std::enable_if_t<is_map_v<Q>, bool> = true>
    [[nodiscard]] auto operator[](Key&& key) -> Q& {
        return do_at(std::forward<Key>(key));
    }

    template <class K, typename Q = Value, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<is_map_v<Q> && detail::is_transparent_v<H, KE>, bool> = true>
    [[nodiscard]] auto operator[](K&& key) -> Q& {
        return do_at(std::forward<K>(key));
    }

    [[nodiscard]] auto count(Key const& key) const -> size_t { return find(key) == end() ? 0 : 1; }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    [[nodiscard]] auto count(K const& key) const -> size_t {
        return find(key) == end() ? 0 : 1;
    }

    auto contains(Key const& key) const -> bool { return find(key) != end(); }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    auto contains(K const& key) const -> bool {
        return find(key) != end();
    }

    auto equal_range(Key const& key) -> std::pair<iterator, iterator> {
        auto itr = find(key);
        return {itr, itr == end() ? end() : itr + 1};
    }

    auto equal_range(const Key& key) const -> std::pair<const_iterator, const_iterator> {
        auto itr = find(key);
        return {itr, itr == end() ? end() : itr + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key) const -> std::pair<const_iterator, const_iterator> {
        auto itr = find(key);
        return {itr, itr == end() ? end() : itr + 1};
    }

    template <class K, class H = Hash, class KE = KeyEqual,
              std::enable_if_t<detail::is_transparent_v<H, KE>, bool> = true>
    auto equal_range(K const& key) -> std::pair<iterator, iterator> {
        auto itr = find(key);
        return {itr, itr == end() ? end() : itr + 1};
    }

    // debug & print functions
    void print() const {
        fmt::print("inplace_table: size: {}, bucket_count: {}, max_bucket_capacity: {}\n", _size, _buckets.capacity(),
                   _max_bucket_capacity);
        for (auto& bucket : _buckets) {
            fmt::print("bucket: ");
            bucket.layout.print();
        }
    }
};  // class inplace_table
}  // namespace stdb::container