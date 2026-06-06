#pragma once
/**
 * @file BasicSparseSet.hpp
 * @brief A traits-based sparse set that works with any addressable key type,
 *        including plain integers and generational SlotKey handles.
 *
 * This is the common implementation underlying both the legacy OffsetSparseSet
 * (integer keys) and the new SlotKeySparseSet (generational keys).
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

#include "SparseKeyTraits.hpp"

#include <vector>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lux::cxx
{
    /**
     * @class BasicSparseSet
     * @brief Sparse-set container parameterised on a key-traits policy.
     *
     * Provides O(1) average insert, erase, contains, and lookup.
     * Dense arrays allow cache-friendly iteration over all stored pairs.
     *
     * For SlotKey keys the traits use `key.index` to address the sparse
     * array but perform full-key equality (including generation) on every
     * hit, so that stale handles never match a reused slot.
     *
     * @tparam Key    The key type.
     * @tparam Value  The value type.
     * @tparam Traits A policy satisfying the sparse_key_traits concept.
     */
    template <class Key, class Value, class Traits = sparse_key_traits<Key>>
    class BasicSparseSet
    {
    public:
        using key_type   = Key;
        using value_type = Value;
        using size_type  = std::size_t;

        /**
         * @brief Sentinel value indicating an empty sparse slot.
         */
        static constexpr size_type INVALID_INDEX =
            (std::numeric_limits<size_type>::max)();

        // ---- constructors ---------------------------------------------------

        BasicSparseSet() = default;

        explicit BasicSparseSet(size_type initial_capacity)
        {
            reserve(initial_capacity);
        }

        // ---- capacity -------------------------------------------------------

        /**
         * @brief Returns the number of stored elements.
         */
        [[nodiscard]] size_type size() const noexcept
        {
            return dense_keys_.size();
        }

        /**
         * @brief Checks if the set is empty.
         */
        [[nodiscard]] bool empty() const noexcept
        {
            return dense_keys_.empty();
        }

        /**
         * @brief Reserves storage for at least @p n elements in the dense arrays.
         */
        void reserve(size_type n)
        {
            dense_keys_.reserve(n);
            dense_values_.reserve(n);
        }

        /**
         * @brief Removes all elements, clearing both sparse and dense arrays.
         */
        void clear()
        {
            sparse_.clear();
            dense_keys_.clear();
            dense_values_.clear();
        }

        // ---- lookup ---------------------------------------------------------

        /**
         * @brief Checks if a given key is present in the set.
         * @param key The key to check.
         * @return True if the key exists, false otherwise.
         *
         * For SlotKey keys this performs a full-key equality check
         * (index **and** generation) to respect stale-handle semantics.
         */
        [[nodiscard]] bool contains(Key key) const noexcept
        {
            if (!Traits::is_addressable(key))
                return false;
            const auto si = Traits::sparse_index(key);
            if (si >= sparse_.size())
                return false;
            const auto di = sparse_[si];
            if (di == INVALID_INDEX)
                return false;
            return Traits::equals(dense_keys_[di], key);
        }

        /**
         * @brief Returns a pointer to the value, or nullptr if absent.
         */
        [[nodiscard]] Value* tryGet(Key key) noexcept
        {
            if (!contains(key))
                return nullptr;
            return &dense_values_[sparse_[Traits::sparse_index(key)]];
        }

        /// @overload const version
        [[nodiscard]] const Value* tryGet(Key key) const noexcept
        {
            if (!contains(key))
                return nullptr;
            return &dense_values_[sparse_[Traits::sparse_index(key)]];
        }

        /**
         * @brief Returns a reference to the value. Throws if the key is absent.
         */
        [[nodiscard]] Value& at(Key key)
        {
            if (!contains(key))
                throw std::out_of_range("BasicSparseSet::at: key not found");
            return dense_values_[sparse_[Traits::sparse_index(key)]];
        }

        /// @overload const version
        [[nodiscard]] const Value& at(Key key) const
        {
            if (!contains(key))
                throw std::out_of_range("BasicSparseSet::at: key not found");
            return dense_values_[sparse_[Traits::sparse_index(key)]];
        }

        // ---- insert ---------------------------------------------------------

        /**
         * @brief Inserts or updates a (key, value) pair by lvalue reference.
         *
         * If the key already exists its value is overwritten.
         */
        void insert(Key key, const Value& value)
        {
            ensure_sparse(key);
            auto& slot = sparse_[Traits::sparse_index(key)];
            if (slot != INVALID_INDEX && Traits::equals(dense_keys_[slot], key))
            {
                dense_values_[slot] = value;
            }
            else
            {
                slot = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(value);
            }
        }

        /**
         * @brief Inserts or updates a (key, value) pair by rvalue reference.
         */
        void insert(Key key, Value&& value)
        {
            ensure_sparse(key);
            auto& slot = sparse_[Traits::sparse_index(key)];
            if (slot != INVALID_INDEX && Traits::equals(dense_keys_[slot], key))
            {
                dense_values_[slot] = std::move(value);
            }
            else
            {
                slot = dense_keys_.size();
                dense_keys_.push_back(key);
                dense_values_.push_back(std::move(value));
            }
        }

        /**
         * @brief Inserts a default-constructed value if the key does not exist,
         *        otherwise returns a reference to the existing value.
         */
        Value& operator[](Key key)
        {
            ensure_sparse(key);
            auto& slot = sparse_[Traits::sparse_index(key)];
            if (slot != INVALID_INDEX && Traits::equals(dense_keys_[slot], key))
                return dense_values_[slot];
            slot = dense_keys_.size();
            dense_keys_.push_back(key);
            dense_values_.push_back(Value{});
            return dense_values_.back();
        }

        /**
         * @brief Constructs a value in-place. If the key already exists the
         *        old value is destroyed and replaced.
         * @return A reference to the newly constructed value.
         */
        template <class... Args>
        Value& emplace(Key key, Args&&... args)
        {
            ensure_sparse(key);
            auto& slot = sparse_[Traits::sparse_index(key)];
            if (slot != INVALID_INDEX && Traits::equals(dense_keys_[slot], key))
            {
                dense_values_[slot].~Value();
                new (&dense_values_[slot]) Value(std::forward<Args>(args)...);
                return dense_values_[slot];
            }
            slot = dense_keys_.size();
            dense_keys_.push_back(key);
            dense_values_.emplace_back(std::forward<Args>(args)...);
            return dense_values_.back();
        }

        // ---- erase ----------------------------------------------------------

        /**
         * @brief Removes the element with the given key (swap-and-pop).
         * @return True if removed, false if the key was not found.
         */
        bool erase(Key key)
        {
            if (!contains(key))
                return false;

            const auto si   = Traits::sparse_index(key);
            const auto di   = sparse_[si];
            const auto last = dense_keys_.size() - 1;

            if (di != last)
            {
                dense_keys_[di]   = dense_keys_[last];
                dense_values_[di] = std::move(dense_values_[last]);
                sparse_[Traits::sparse_index(dense_keys_[di])] = di;
            }
            dense_keys_.pop_back();
            dense_values_.pop_back();
            sparse_[si] = INVALID_INDEX;
            return true;
        }

        /**
         * @brief Extracts (moves out) the value associated with a key, then erases it.
         * @param key   The key to extract.
         * @param value Output receiving the moved value.
         * @return False if the key is not found.
         */
        bool extract(Key key, Value& value)
        {
            if (!contains(key))
                return false;
            value = std::move(dense_values_[sparse_[Traits::sparse_index(key)]]);
            erase(key);
            return true;
        }

        // ---- dense access ---------------------------------------------------

        /**
         * @brief Returns a const reference to the dense key array.
         */
        [[nodiscard]] const std::vector<Key>& keys() const noexcept
        {
            return dense_keys_;
        }

        /**
         * @brief Returns a const reference to the dense value array.
         */
        [[nodiscard]] const std::vector<Value>& values() const noexcept
        {
            return dense_values_;
        }

    private:
        /**
         * @brief Ensures the sparse array is large enough for @p key.
         */
        void ensure_sparse(Key key)
        {
            const auto si = Traits::sparse_index(key);
            // sparse_ is indexed directly by si, so the array grows to si+1. Guard
            // against a pathological key whose sparse_index is SIZE_MAX: `si + 1`
            // would wrap to 0, defeating the bounds check and then indexing
            // sparse_[SIZE_MAX] out of bounds. (This container is meant for compact
            // keys; arbitrary huge/negative integral keys should use a hash map.)
            if (si == INVALID_INDEX)
                throw std::length_error("BasicSparseSet: key sparse-index too large");
            if (si >= sparse_.size())
                sparse_.resize(si + 1, INVALID_INDEX);
        }

        std::vector<size_type> sparse_;
        std::vector<Key>       dense_keys_;
        std::vector<Value>     dense_values_;
    };

    // =========================================================================
    //  GenericAutoSparseSet — traits-based auto-allocating sparse set
    // =========================================================================

    /**
     * @class GenericAutoSparseSet
     * @brief An auto-key-allocating sparse set parameterised on key traits.
     *
     * Like OffsetAutoSparseSet but works with any key type that has both
     * `sparse_key_traits` and `auto_key_traits` specialisations.
     *
     * @tparam Key         The key type.
     * @tparam Value       The value type.
     * @tparam STraits     Addressing traits (sparse_key_traits by default).
     * @tparam ATraits     Lifecycle traits  (auto_key_traits by default).
     */
    template <
        class Key,
        class Value,
        class STraits = sparse_key_traits<Key>,
        class ATraits = auto_key_traits<Key>
    >
    class GenericAutoSparseSet
    {
    public:
        using key_type   = Key;
        using value_type = Value;
        using size_type  = std::size_t;

        // ---- constructors ---------------------------------------------------

        GenericAutoSparseSet()
            : next_id_(ATraits::initial_next())
        {
        }

        explicit GenericAutoSparseSet(size_type initial_capacity)
            : next_id_(ATraits::initial_next())
        {
            base_.reserve(initial_capacity);
        }

        // ---- capacity -------------------------------------------------------

        [[nodiscard]] size_type size()  const noexcept { return base_.size(); }
        [[nodiscard]] bool      empty() const noexcept { return base_.empty(); }

        void reserve(size_type n) { base_.reserve(n); }

        void clear()
        {
            base_.clear();
            free_ids_.clear();
            next_id_ = ATraits::initial_next();
        }

        // ---- auto-insert ----------------------------------------------------

        /**
         * @brief Inserts a value with an automatically assigned key (copy).
         * @return The newly allocated key.
         */
        Key insert(const Value& value)
        {
            Key k = acquire_key();
            base_.insert(k, value);
            return k;
        }

        /**
         * @brief Inserts a value with an automatically assigned key (move).
         * @return The newly allocated key.
         */
        Key insert(Value&& value)
        {
            Key k = acquire_key();
            base_.insert(k, std::move(value));
            return k;
        }

        /**
         * @brief Constructs a value in-place with an automatically assigned key.
         * @return The newly allocated key.
         */
        template <class... Args>
        Key emplace(Args&&... args)
        {
            Key k = acquire_key();
            base_.emplace(k, std::forward<Args>(args)...);
            return k;
        }

        // ---- manual insert --------------------------------------------------

        /**
         * @brief Allows manual insertion by key, returning a reference.
         */
        Value& operator[](Key key)
        {
            return base_[key];
        }

        // ---- erase ----------------------------------------------------------

        /**
         * @brief Erases the element and reclaims the key for future reuse.
         */
        bool erase(Key key)
        {
            if (base_.erase(key))
            {
                free_ids_.push_back(ATraits::recycled(key));
                return true;
            }
            return false;
        }

        /**
         * @brief Extracts the value, erases the element, and reclaims the key.
         */
        bool extract(Key key, Value& value)
        {
            if (base_.extract(key, value))
            {
                free_ids_.push_back(ATraits::recycled(key));
                return true;
            }
            return false;
        }

        // ---- lookup ---------------------------------------------------------

        [[nodiscard]] bool          contains(Key key) const noexcept { return base_.contains(key); }
        [[nodiscard]] Value*        tryGet(Key key) noexcept         { return base_.tryGet(key); }
        [[nodiscard]] const Value*  tryGet(Key key) const noexcept   { return base_.tryGet(key); }
        [[nodiscard]] Value&        at(Key key)                      { return base_.at(key); }
        [[nodiscard]] const Value&  at(Key key) const                { return base_.at(key); }

        // ---- dense access ---------------------------------------------------

        [[nodiscard]] const std::vector<Key>&   keys()   const noexcept { return base_.keys(); }
        [[nodiscard]] const std::vector<Value>& values() const noexcept { return base_.values(); }

        // ---- allocator state ------------------------------------------------

        /**
         * @brief Returns the next key that would be allocated if no free IDs remain.
         */
        [[nodiscard]] Key next_id() const noexcept { return next_id_; }

        /**
         * @brief Returns the number of recycled IDs available for reuse.
         */
        [[nodiscard]] size_type free_ids_count() const noexcept { return free_ids_.size(); }

    private:
        Key acquire_key()
        {
            if (!free_ids_.empty())
            {
                Key k = free_ids_.back();
                free_ids_.pop_back();
                return k;
            }
            Key k = next_id_;
            next_id_ = ATraits::next_fresh(next_id_);
            return k;
        }

        BasicSparseSet<Key, Value, STraits> base_;
        std::vector<Key>                    free_ids_;
        Key                                 next_id_;
    };

    // =========================================================================
    //  Convenience aliases for SlotKey-based sparse sets
    // =========================================================================

    /**
     * @brief A sparse set keyed by SlotKey (externally managed IDs).
     *
     * Use this as a secondary index over objects whose IDs are allocated
     * by a SlotMap.  It does **not** own key lifecycle.
     */
    template <class SlotKeyT, class Value>
    using SlotKeySparseSet = BasicSparseSet<SlotKeyT, Value,
        sparse_key_traits<SlotKeyT>>;

    /**
     * @brief An auto-allocating sparse set keyed by SlotKey.
     *
     * Only suitable for subsystems that own their own key space.
     * **Do not** use this for IDs shared with a SlotMap — the SlotMap
     * must remain the single authority for those IDs.
     */
    template <class SlotKeyT, class Value>
    using SlotKeyAutoSparseSet = GenericAutoSparseSet<SlotKeyT, Value,
        sparse_key_traits<SlotKeyT>,
        auto_key_traits<SlotKeyT>>;

} // namespace lux::cxx
