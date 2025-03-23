#pragma once
/**
 * @file OffsetSparseSet.hpp
 * @brief Provides two sparse-set data structures with a compile-time offset for keys:
 *        OffsetSparseSet and OffsetAutoSparseSet.
 *
 * @copyright
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 * A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <vector>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    /**
     * @class OffsetSparseSet
     * @brief A sparse set data structure allowing keys to start from a compile-time offset.
     *
     * This implementation provides:
     * - A sparse array (`sparse_`) indexed by `key - Offset`,
     * - Two dense arrays (`dense_keys_` and `dense_values_`) storing the actual (key, value) pairs.
     *
     * All operations (insert, erase, contains, etc.) work in O(1) on average. The offset
     * ensures that memory is not allocated for all key indices below `Offset`.
     *
     * @tparam Key    The integral type of the key.
     * @tparam Value  The type of the stored value.
     * @tparam Offset A compile-time constant indicating the minimum valid key.
     */
    template <typename Key, typename Value, Key Offset = 0>
    class OffsetSparseSet
    {
        static_assert(std::is_integral_v<Key>,
            "OffsetSparseSet: Key must be an integral type.");

    public:
        /**
         * @brief Type definition for size and index values.
         */
        using size_type = std::size_t;

        /**
         * @brief Constant representing an invalid index within the sparse array.
         */
        static constexpr size_type INVALID_INDEX =
            (std::numeric_limits<size_type>::max)();

        /**
         * @brief Default constructor; does not allocate any storage.
         */
        OffsetSparseSet() = default;

        /**
         * @brief Constructor with initial capacity for the dense arrays.
         *        The sparse array grows automatically based on the maximum key seen.
         * @param initial_capacity The initial capacity to reserve in the dense arrays.
         */
        explicit OffsetSparseSet(size_type initial_capacity)
        {
            reserve(initial_capacity);
        }

        /**
         * @brief Returns the number of stored elements.
         * @return Number of elements in the set.
         */
        size_type size() const noexcept
        {
            return dense_keys_.size();
        }

        /**
         * @brief Checks if the set is empty.
         * @return True if empty, false otherwise.
         */
        bool empty() const noexcept
        {
            return dense_keys_.empty();
        }

        /**
         * @brief Reserves storage for at least @p new_capacity elements in the dense arrays.
         * @param new_capacity The capacity to reserve for the dense arrays.
         */
        void reserve(size_type new_capacity)
        {
            dense_keys_.reserve(new_capacity);
            dense_values_.reserve(new_capacity);
        }

        /**
         * @brief Removes all elements from the set, clearing both the sparse and dense arrays.
         */
        void clear()
        {
            dense_keys_.clear();
            dense_values_.clear();
            sparse_.clear();
        }

        /**
         * @brief Checks if a given key is in the set.
         * @param key The key to check.
         * @return True if the key exists, false otherwise.
         */
        bool contains(Key key) const
        {
            if (key < Offset) {
                return false;
            }
            size_type idx = toIndex(key);
            return (idx < sparse_.size()) && (sparse_[idx] != INVALID_INDEX);
        }

        /**
         * @brief Inserts or updates a (key, value) pair by lvalue reference.
         * @param key   The key to insert or update.
         * @param value The value to associate with the key.
         *
         * If the key already exists, its value is overwritten.
         * Otherwise, a new pair is inserted.
         */
        void insert(Key key, const Value& value)
        {
            ensure_sparse_size(key);
            size_type idx = sparse_[toIndex(key)];
            if (idx == INVALID_INDEX) {
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(value);
                sparse_[toIndex(key)] = idx;
            }
            else {
                dense_values_[idx] = value;
            }
        }

        /**
         * @brief Inserts or updates a (key, value) pair by rvalue reference.
         * @param key   The key to insert or update.
         * @param value The value to associate with the key (rvalue reference).
         *
         * If the key already exists, its value is overwritten.
         * Otherwise, a new pair is inserted.
         */
        void insert(Key key, Value&& value)
        {
            ensure_sparse_size(key);
            size_type idx = sparse_[toIndex(key)];
            if (idx == INVALID_INDEX) {
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(std::move(value));
                sparse_[toIndex(key)] = idx;
            }
            else {
                dense_values_[idx] = std::move(value);
            }
        }

        /**
         * @brief Inserts a default-constructed value if key does not exist,
         *        otherwise returns a reference to the existing value.
         * @param key The key to insert or retrieve.
         * @return A reference to the associated value.
         */
        Value& operator[](Key key)
        {
            ensure_sparse_size(key);
            size_type idx = sparse_[toIndex(key)];
            if (idx == INVALID_INDEX) {
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(Value{});
                sparse_[toIndex(key)] = idx;
            }
            return dense_values_[idx];
        }

        /**
         * @brief Constructs and inserts (or updates) a value in-place for the given key.
         *
         * If the key already exists, the existing value is destructed and re-constructed.
         * Otherwise, a new (key, value) pair is inserted.
         *
         * @tparam Args Parameter pack to forward to Value's constructor.
         * @param key The key to insert or update.
         * @param args Arguments for constructing the value in-place.
         * @return A reference to the newly constructed value.
         */
        template <typename... Args>
        Value& emplace(Key key, Args&&... args)
        {
            ensure_sparse_size(key);
            size_type idx = sparse_[toIndex(key)];
            if (idx == INVALID_INDEX) {
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.emplace_back(std::forward<Args>(args)...);
                sparse_[toIndex(key)] = idx;
            }
            else {
                dense_values_[idx].~Value();
                new (&dense_values_[idx]) Value(std::forward<Args>(args)...);
            }
            return dense_values_[idx];
        }

        /**
         * @brief Removes the specified key from the set if it exists.
         * @param key The key to remove.
         * @return True if the key was removed, false if it was not found.
         *
         * This uses a "swap with last" approach in the dense arrays for O(1) removal.
         */
        bool erase(Key key)
        {
            if (!contains(key)) {
                return false;
            }
            size_type i = sparse_[toIndex(key)];
            size_type last = dense_keys_.size() - 1;

            if (i != last) {
                Key last_key = dense_keys_[last];
                dense_keys_[i] = last_key;
                dense_values_[i] = std::move(dense_values_[last]);
                sparse_[toIndex(last_key)] = i;
            }
            dense_keys_.pop_back();
            dense_values_.pop_back();
            sparse_[toIndex(key)] = INVALID_INDEX;
            return true;
        }

        /**
         * @brief Extracts (moves out) the value associated with a key, then erases the key.
         * @param key   The key to extract.
         * @param value An output reference where the value is moved.
         * @return False if the key is not found, true otherwise.
         */
        bool extract(Key key, Value& value)
        {
            if (!contains(key)) {
                return false;
            }
            size_type idx = sparse_[toIndex(key)];
            value = std::move(dense_values_[idx]);
            erase(key);
            return true;
        }

        /**
         * @brief Returns a reference to the value associated with a key.
         * @param key The key to look up.
         * @throws std::out_of_range if the key is not found.
         * @return A reference to the associated value.
         */
        Value& at(Key key)
        {
            checkKeyValid(key);
            return dense_values_.at(sparse_.at(toIndex(key)));
        }

        /**
         * @brief Returns a const reference to the value associated with a key.
         * @param key The key to look up.
         * @throws std::out_of_range if the key is not found.
         * @return A const reference to the associated value.
         */
        const Value& at(Key key) const
        {
            checkKeyValid(key);
            return dense_values_.at(sparse_.at(toIndex(key)));
        }

        /**
         * @brief Returns a const reference to the vector of keys in dense storage.
         * @return A const reference to dense_keys_.
         */
        const std::vector<Key>& keys() const noexcept
        {
            return dense_keys_;
        }

        /**
         * @brief Returns a const reference to the vector of values in dense storage.
         * @return A const reference to dense_values_.
         */
        const std::vector<Value>& values() const noexcept
        {
            return dense_values_;
        }

    private:
        /**
         * @brief Converts a key to an index in the sparse array by subtracting the Offset.
         * @param key The key to convert.
         * @return The index in the sparse array.
         */
        static constexpr size_type toIndex(Key key)
        {
            return static_cast<size_type>(key - Offset);
        }

        /**
         * @brief Ensures the sparse array is large enough to index a given key.
         * @param key The key to check.
         * @throws std::out_of_range if key < Offset.
         */
        void ensure_sparse_size(Key key)
        {
            checkKeyValid(key);
            const size_type idx = toIndex(key);
            if (idx >= sparse_.size()) {
                sparse_.resize(idx + 1, INVALID_INDEX);
            }
        }

        /**
         * @brief Checks if a key is valid (key >= Offset). Throws if not.
         * @param key The key to check.
         * @throws std::out_of_range if key < Offset.
         */
        static void checkKeyValid(Key key)
        {
            if (key < Offset) {
                throw std::out_of_range("OffsetSparseSet: key < Offset.");
            }
        }

    private:
        /**
         * @brief The sparse array where sparse_[key - Offset] stores the index of (key, value)
         *        in the dense arrays. INVALID_INDEX indicates an absent key.
         */
        std::vector<size_type> sparse_;

        /**
         * @brief Dense array of keys.
         */
        std::vector<Key>       dense_keys_;

        /**
         * @brief Dense array of values.
         */
        std::vector<Value>     dense_values_;
    };

    /**
     * @class OffsetAutoSparseSet
     * @brief A sparse set that automatically assigns new IDs (keys) with a compile-time offset.
     *
     * This class inherits OffsetSparseSet but manages key allocations:
     * - If there are free IDs from previously erased elements, reuse them.
     * - Otherwise, allocate keys from @p next_id_, starting at Offset.
     *
     * @tparam Value  The type of values to store.
     * @tparam Key    The integral type for the key.
     * @tparam Offset A compile-time offset from which to start allocating new keys.
     */
    template <typename Key, typename Value, Key Offset = 0>
    class OffsetAutoSparseSet : protected OffsetSparseSet<Key, Value, Offset>
    {
        static_assert(std::is_integral_v<Key>,
            "OffsetAutoSparseSet: Key must be an integral type.");

    public:
        using BaseType = OffsetSparseSet<Key, Value, Offset>;
        using size_type = typename BaseType::size_type;

        /**
         * @brief Default constructor. next_id_ is initialized to Offset.
         */
        OffsetAutoSparseSet()
            : BaseType()
            , next_id_(Offset)
        {
        }

        /**
         * @brief Constructor with initial capacity. next_id_ is initialized to Offset.
         * @param initial_capacity The capacity to reserve in the dense arrays.
         */
        explicit OffsetAutoSparseSet(size_type initial_capacity)
            : BaseType(initial_capacity)
            , next_id_(Offset)
        {
        }

        /**
         * @brief Returns the number of stored elements.
         * @return Number of elements in the container.
         */
        size_type size() const noexcept
        {
            return BaseType::size();
        }

        /**
         * @brief Checks if the set is empty.
         * @return True if empty, false otherwise.
         */
        bool empty() const noexcept
        {
            return BaseType::empty();
        }

        /**
         * @brief Clears the container, including free_ids_, and resets next_id_ to Offset.
         */
        void clear()
        {
            BaseType::clear();
            free_ids_.clear();
            next_id_ = Offset;
        }

        /**
         * @brief Reserves storage for at least @p new_capacity elements in the dense arrays.
         * @param new_capacity The desired capacity in the dense arrays.
         */
        void reserve(size_type new_capacity)
        {
            BaseType::reserve(new_capacity);
        }

        /**
         * @brief Inserts a new value by lvalue reference with an automatically assigned key.
         * @param value The value to insert.
         * @return The newly allocated key.
         */
        Key insert(const Value& value)
        {
            Key new_key = acquire_key();
            BaseType::insert(new_key, value);
            return new_key;
        }

        /**
         * @brief Inserts a new value by rvalue reference with an automatically assigned key.
         * @param value The value to insert (rvalue reference).
         * @return The newly allocated key.
         */
        Key insert(Value&& value)
        {
            Key new_key = acquire_key();
            BaseType::insert(new_key, std::move(value));
            return new_key;
        }

        /**
         * @brief Constructs a new value in-place, assigning a new key automatically.
         * @tparam Args Parameter pack for Value's constructor.
         * @param args Arguments used to construct the Value in-place.
         * @return The newly allocated key.
         */
        template <typename... Args>
        Key emplace(Args&&... args)
        {
            Key new_key = acquire_key();
            BaseType::emplace(new_key, std::forward<Args>(args)...);
            return new_key;
        }

        /**
         * @brief Erases the element associated with @p key if it exists.
         * @param key The key to remove.
         * @return True if the key was found and removed, false otherwise.
         *
         * The erased key is added to free_ids_ for future reuse.
         */
        bool erase(Key key)
        {
            if (BaseType::erase(key)) {
                free_ids_.push_back(key);
                return true;
            }
            return false;
        }

        /**
         * @brief Extracts the value associated with @p key if it exists, removing it from the set.
         * @param key   The key to extract.
         * @param value An output reference receiving the moved value.
         * @return True if the key existed, false otherwise.
         *
         * If extraction is successful, the key is reclaimed for future reuse.
         */
        bool extract(Key key, Value& value)
        {
            if (BaseType::extract(key, value)) {
                free_ids_.push_back(key);
                return true;
            }
            return false;
        }

        /**
         * @brief Checks if a given key is in the set.
         * @param key The key to check.
         * @return True if it exists, false otherwise.
         */
        bool contains(Key key) const
        {
            return BaseType::contains(key);
        }

        /**
         * @brief Returns the count of free IDs currently stored.
         * @return The number of available free IDs.
         */
        size_type free_ids_count() const noexcept
        {
            return free_ids_.size();
        }

        /**
         * @brief Returns a const reference to the dense array of keys in the base class.
         * @return A const reference to the keys.
         */
        const std::vector<Key>& keys() const noexcept
        {
            return BaseType::keys();
        }

        /**
         * @brief Returns a const reference to the dense array of values in the base class.
         * @return A const reference to the values.
         */
        const std::vector<Value>& values() const noexcept
        {
            return BaseType::values();
        }

        /**
         * @brief Returns the next ID that would be allocated if free_ids_ is empty.
         * @return The current value of next_id_.
         */
        Key next_id() const noexcept
        {
            return next_id_;
        }

        /**
         * @brief Allows manual insertion by key if desired, returning a reference to the value.
         * @param key The user-specified key.
         * @return A reference to the value, default-constructed if the key did not exist.
         */
        Value& operator[](Key key)
        {
            return BaseType::operator[](key);
        }

        /**
         * @brief Provides access to the value for the given key, throwing std::out_of_range if not found.
         * @param key The key to look up.
         * @throws std::out_of_range if the key does not exist.
         * @return A reference to the associated value.
         */
        Value& at(Key key)
        {
            return BaseType::at(key);
        }

        /**
         * @brief Provides const access to the value for the given key, throwing std::out_of_range if not found.
         * @param key The key to look up.
         * @throws std::out_of_range if the key does not exist.
         * @return A const reference to the associated value.
         */
        const Value& at(Key key) const
        {
            return BaseType::at(key);
        }

    protected:
        /**
         * @brief Acquires the next available key: reuses one from free_ids_ if possible,
         *        otherwise increments next_id_.
         * @return The newly acquired key.
         */
        Key acquire_key()
        {
            if (!free_ids_.empty()) {
                Key k = free_ids_.back();
                free_ids_.pop_back();
                return k;
            }
            return next_id_++;
        }

    private:
        /**
         * @brief A list of keys that were removed and can be reused.
         */
        std::vector<Key> free_ids_;

        /**
         * @brief A monotonically increasing key used for allocation if no free IDs are available.
         */
        Key next_id_;
    };

	template<typename Key, typename Value, Key offset = 0> using SparseSet     = OffsetSparseSet<Key, Value, offset>;
	template<typename Value, size_t offset = 0> using AutoSparseSet = OffsetAutoSparseSet<size_t, Value, offset>;

} // namespace lux::cxx
