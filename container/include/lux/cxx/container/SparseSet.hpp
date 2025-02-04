#pragma once

#include <vector>
#include <limits>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace lux::cxx
{
    /**
     * @brief A sparse set data structure that associates a unique key with a value,
     *        optimized for O(1) average insertion, lookup, and removal.
     *
     * This implementation stores a range of possible keys (0 to max_key),
     * maintaining:
     * - a sparse array (`sparse_`) that maps from a key to an index in the dense arrays,
     * - two dense arrays (`dense_keys_` and `dense_values_`), which hold the actual (key, value) pairs.
     *
     * @tparam Key   The key type. Must be an integral or enum type that can be used as an index.
     * @tparam Value The type of values stored in the set.
     */
    template<typename Key, typename Value>
    class SparseSet
    {
    public:
        /**
         * @brief A type used for sizes and indices.
         */
        using size_type = std::size_t;

        /**
         * @brief Constant indicating an invalid index in the sparse array.
         */
        static constexpr size_type INVALID_INDEX = (std::numeric_limits<size_type>::max)();

        /**
         * @brief Constructs an empty SparseSet.
         */
        SparseSet() = default;

        /**
         * @brief Constructs an empty SparseSet with a reserved capacity.
         * @param initial_capacity The number of elements to reserve in the dense arrays.
         *
         * This does not resize the `sparse_` vector. Instead, it just ensures the
         * dense arrays can hold at least @p initial_capacity elements without reallocation.
         */
        explicit SparseSet(size_type initial_capacity)
        {
            reserve(initial_capacity);
        }

        /**
         * @brief Returns the number of elements currently stored in the set.
         * @return The size of the SparseSet.
         */
        size_type size() const noexcept
        {
            return dense_keys_.size();
        }

        /**
         * @brief Checks if the set is empty.
         * @return True if there are no elements in the set, false otherwise.
         */
        bool empty() const noexcept
        {
            return dense_keys_.empty();
        }

        /**
         * @brief Reserves storage for at least @p new_capacity elements in the dense arrays.
         * @param new_capacity The number of elements to reserve in the dense arrays.
         *
         * This can help reduce reallocation overhead if you know you will be inserting many elements.
         * It does not resize the sparse array. The sparse array will grow automatically based on the maximum key seen.
         */
        void reserve(size_type new_capacity)
        {
            dense_keys_.reserve(new_capacity);
            dense_values_.reserve(new_capacity);
        }

        /**
         * @brief Removes all elements from the set.
         *
         * After clearing, the set will be empty, and the sparse array will also be cleared.git 
         */
        void clear()
        {
            dense_keys_.clear();
            dense_values_.clear();
            sparse_.clear();
        }

        /**
         * @brief Checks whether a given key is present in the set.
         * @param key The key to look up.
         * @return True if @p key is in the set, false otherwise.
         */
        bool contains(Key key) const
        {
            return (key < sparse_.size()) && (sparse_[key] != INVALID_INDEX);
        }

        /**
         * @brief Inserts or updates an element with the specified key and value (lvalue reference).
         * @param key   The key to be inserted.
         * @param value The value to associate with the key.
         *
         * If the key already exists, its value is replaced. Otherwise, a new element is inserted.
         */
        void insert(Key key, const Value& value)
        {
            ensure_sparse_size(key);
            auto idx = sparse_[key];
            if (idx == INVALID_INDEX) {
                // Key does not exist yet; insert new.
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(value);
                sparse_[key] = idx;
            }
            else {
                // Key exists; update value.
                dense_values_[idx] = value;
            }
        }

		/**
		 * @brief Accesses the value associated with a given key.
		 * @param key The key to look up.
		 * @return A reference to the value associated with the key.
		 *
		 * If the key is not in the set, it is inserted with a default-constructed value.
		 */
		Value& operator[](Key key)
		{
			ensure_sparse_size(key);
			auto idx = sparse_[key];
			if (idx == INVALID_INDEX) {
				idx = dense_keys_.size();
				dense_keys_.push_back(key);
				dense_values_.push_back(Value{});
				sparse_[key] = idx;
			}
			return dense_values_[idx];
		}



        /**
         * @brief Inserts or updates an element with the specified key and value (rvalue reference).
         * @param key   The key to be inserted.
         * @param value The value to associate with the key (rvalue reference).
         *
         * If the key already exists, its value is replaced. Otherwise, a new element is inserted.
         */
        void insert(Key key, Value&& value)
        {
            ensure_sparse_size(key);
            auto idx = sparse_[key];
            if (idx == INVALID_INDEX) {
                // Key does not exist yet; insert new.
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(std::move(value));
                sparse_[key] = idx;
            }
            else {
                // Key exists; update value.
                dense_values_[idx] = std::move(value);
            }
        }

        /**
         * @brief Constructs and inserts (or updates) an element in place.
         * @tparam Args Parameter pack for constructing the @p Value object.
         * @param key  The key to be inserted.
         * @param args Arguments used to construct the value in place.
         * @return A reference to the newly emplaced or updated value.
         *
         * If the key already exists, the existing value is reinitialized via a constructor call with @p args.
         * Otherwise, a new element is inserted.
         */
        template<typename... Args>
        Value& emplace(Key key, Args&&... args)
        {
            ensure_sparse_size(key);
            auto idx = sparse_[key];
            if (idx == INVALID_INDEX) {
                // Insert new element.
                idx = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.emplace_back(std::forward<Args>(args)...);
                sparse_[key] = idx;
            }
            else {
                // Reconstruct existing value.
                dense_values_[idx] = Value(std::forward<Args>(args)...);
            }
            return dense_values_[idx];
        }

        /**
         * @brief Removes the specified key from the set.
         * @param key The key to remove.
         * @return True if the key was removed, or false if it was not found.
         *
         * The removal algorithm uses "swap with the last element" in the dense arrays to maintain
         * correct ordering in O(1) time (amortized).
         */
        bool erase(Key key)
        {
            if (!contains(key)) {
                return false;
            }
            auto idx_sparse = sparse_[key];
            auto last_idx = dense_keys_.size() - 1;

            // If the element to remove is not the last one in dense storage,
            // move the last element to its position.
            if (idx_sparse != last_idx) {
                auto last_key = dense_keys_[last_idx];
                dense_keys_[idx_sparse] = last_key;
                dense_values_[idx_sparse] = std::move(dense_values_[last_idx]);
                sparse_[last_key] = idx_sparse;
            }
            // Erase the last element from the dense arrays.
            dense_keys_.pop_back();
            dense_values_.pop_back();
            // Mark the key as invalid in the sparse array.
            sparse_[key] = INVALID_INDEX;
            return true;
        }

        /**
         * @brief Returns a reference to the value associated with a given key.
         * @param key The key to look up.
         * @throws std::out_of_range If the key is not in the set.
         * @return A reference to the associated value.
         */
        Value& at(Key key)
        {
            if (key >= sparse_.size() || sparse_[key] == INVALID_INDEX) {
                throw std::out_of_range("SparseSet::at: key not found");
            }
            return dense_values_[sparse_[key]];
        }

		/*
		* @brief Extracts the value associated with a given key.
        * @param key The key to look up.
		* @throws std::out_of_range If the key is not in the set.
		* @return The value associated with the key.
        */
		Value extract(Key key)
		{
			if (key >= sparse_.size() || sparse_[key] == INVALID_INDEX) {
				throw std::out_of_range("SparseSet::extract: key not found");
			}
			auto idx = sparse_[key];
			Value value = std::move(dense_values_[idx]);
			erase(key);
			return value;
		}

        /**
         * @brief Returns a const reference to the value associated with a given key.
         * @param key The key to look up.
         * @throws std::out_of_range If the key is not in the set.
         * @return A const reference to the associated value.
         */
        const Value& at(Key key) const
        {
            if (key >= sparse_.size() || sparse_[key] == INVALID_INDEX) {
                throw std::out_of_range("SparseSet::at: key not found");
            }
            return dense_values_[sparse_[key]];
        }

        /**
         * @brief Returns a read-only reference to the dense array of keys.
         *
         * These keys reflect the actual stored (active) elements. The order of
         * keys in this array corresponds to the order of values in `values()`.
         *
         * @return A constant reference to the internal vector of keys.
         */
        const std::vector<Key>& keys() const noexcept
        {
            return dense_keys_;
        }

        /**
         * @brief Returns a read-only reference to the dense array of values.
         *
         * The order of values in this array corresponds to the order of
         * keys in `keys()`.
         *
         * @return A constant reference to the internal vector of values.
         */
        const std::vector<Value>& values() const noexcept
        {
            return dense_values_;
        }

    private:
        /**
         * @brief Ensures that the sparse array has enough capacity for the given key.
         * @param key The key that needs to fit into `sparse_`.
         *
         * If `sparse_` is too small to index this @p key, it is resized to (`key + 1`)
         * and any new elements are initialized to `INVALID_INDEX`.
         */
        void ensure_sparse_size(Key key)
        {
            if (static_cast<size_type>(key) >= sparse_.size()) {
                sparse_.resize(key + 1, INVALID_INDEX);
            }
        }

    private:
        /**
         * @brief Sparse array mapping from Key -> index in the dense arrays.
         *
         * If `sparse_[k] == INVALID_INDEX`, it means key k is not in the set.
         */
        std::vector<size_type> sparse_;

        /**
         * @brief Dense array of keys corresponding to stored elements.
         *
         * The position of a key in this array is the same as the position
         * of its associated value in `dense_values_`.
         */
        std::vector<Key>       dense_keys_;

        /**
         * @brief Dense array of values corresponding to stored elements.
         *
         * The position of a value in this array is the same as the position
         * of its associated key in `dense_keys_`.
         */
        std::vector<Value>     dense_values_;
    };

} // namespace lux::cxx
