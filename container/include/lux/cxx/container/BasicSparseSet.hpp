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

#include <algorithm>
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

        /** Prepares both dense and sparse storage without inserting keys. */
        void prepareCapacity(size_type dense_count, size_type sparse_count)
        {
            const auto grow = [](auto& values, size_type required)
            {
                if (required > values.capacity())
                {
                    const auto capacity = values.capacity();
                    const auto extra = capacity / 2;
                    const auto grown = extra <= values.max_size() - capacity ? capacity + extra : required;
                    values.reserve((std::max)(required, grown));
                }
            };
            grow(dense_keys_, dense_count);
            grow(dense_values_, dense_count);
            if (sparse_count > sparse_.size())
            {
                sparse_.resize(sparse_count, INVALID_INDEX);
            }
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
            return dense_index_of(key) != INVALID_INDEX;
        }

        /**
         * @brief Returns a pointer to the value, or nullptr if absent.
         */
        [[nodiscard]] Value* tryGet(Key key) noexcept
        {
            const auto di = dense_index_of(key);
            return di == INVALID_INDEX ? nullptr : &dense_values_[di];
        }

        /// @overload const version
        [[nodiscard]] const Value* tryGet(Key key) const noexcept
        {
            const auto di = dense_index_of(key);
            return di == INVALID_INDEX ? nullptr : &dense_values_[di];
        }

        /**
         * @brief Returns a reference to the value. Throws if the key is absent.
         */
        [[nodiscard]] Value& at(Key key)
        {
            const auto di = dense_index_of(key);
            if (di == INVALID_INDEX)
                throw std::out_of_range("BasicSparseSet::at: key not found");
            return dense_values_[di];
        }

        /// @overload const version
        [[nodiscard]] const Value& at(Key key) const
        {
            const auto di = dense_index_of(key);
            if (di == INVALID_INDEX)
                throw std::out_of_range("BasicSparseSet::at: key not found");
            return dense_values_[di];
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
            const auto di = dense_index_of(key);
            if (di == INVALID_INDEX)
                return false;

            const auto si   = Traits::sparse_index(key);   // cheap: no sparse_ load
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
            const auto di = dense_index_of(key);
            if (di == INVALID_INDEX)
                return false;
            value = std::move(dense_values_[di]);
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
         * @brief Single-lookup key resolution: returns the dense index for @p key,
         *        or INVALID_INDEX if absent.
         *
         * This is the one place that validates a key (addressable -> in-range ->
         * live slot -> full-key equality). contains()/tryGet()/at()/erase() all go
         * through it so the (potentially cache-missing) sparse_ load and the
         * stale-handle equals() check happen exactly ONCE per lookup, instead of
         * contains() computing the dense index and the caller re-deriving it.
         */
        // Whether lookup must do a full-key equality check. Generational keys
        // (SlotKey) need it to reject stale handles; injective integral keys don't,
        // so they save the extra dense_keys_ load. Traits that don't declare the
        // flag default to checking (safe).
        static constexpr bool needs_full_key_check_v = []
        {
            if constexpr (requires { Traits::needs_full_key_check; })
                return Traits::needs_full_key_check;
            else
                return true;
        }();

        [[nodiscard]] size_type dense_index_of(Key key) const noexcept
        {
            if (!Traits::is_addressable(key))      return INVALID_INDEX;
            const auto si = Traits::sparse_index(key);
            if (si >= sparse_.size())              return INVALID_INDEX;
            const auto di = sparse_[si];
            if (di == INVALID_INDEX)               return INVALID_INDEX;
            if constexpr (needs_full_key_check_v)
                if (!Traits::equals(dense_keys_[di], key)) return INVALID_INDEX;
            return di;
        }

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
            reserve(initial_capacity);
        }

        // ---- capacity -------------------------------------------------------

        [[nodiscard]] size_type size()  const noexcept { return base_.size(); }
        [[nodiscard]] bool      empty() const noexcept { return base_.empty(); }

        void reserve(size_type n)
        {
            if (n > size() && !prepareInsert(n - size()))
            {
                throw std::length_error("GenericAutoSparseSet: key space exhausted");
            }
        }

        /**
         * Prepares an insertion batch without issuing identities or moving values.
         * Returns false on identity/size exhaustion. Allocation failures retain the
         * normal allocator contract. Prepared nothrow values can be inserted and
         * extracted without further allocation, provided no intervening insertion
         * consumes the prepared capacity. Erased generations are never reissued.
         */
        [[nodiscard]] bool prepareInsert(size_type additional)
        {
            if (additional == 0)
            {
                return true;
            }
            constexpr auto limit = (std::numeric_limits<size_type>::max)();
            const bool is_dense_overflow = additional > limit - size();
            if (is_dense_overflow)
            {
                return false;
            }
            const auto fresh = additional > free_ids_.size() ? additional - free_ids_.size() : 0;
            if constexpr (requires { ATraits::remaining_fresh(next_id_); })
            {
                if (fresh > ATraits::remaining_fresh(next_id_))
                {
                    return false;
                }
            }
            const auto next_index = STraits::sparse_index(next_id_);
            const bool is_sparse_overflow = fresh > limit - next_index;
            const bool is_recycle_overflow = free_ids_.size() > limit - size() ||
                fresh > limit - size() - free_ids_.size();
            if (is_sparse_overflow || is_recycle_overflow)
            {
                return false;
            }
            base_.prepareCapacity(size() + additional, fresh == 0 ? 0 : next_index + fresh);
            const auto recyclable = size() + fresh + free_ids_.size();
            if (recyclable > free_ids_.capacity())
            {
                const auto capacity = free_ids_.capacity();
                const auto extra = capacity / 2;
                const auto grown = extra <= free_ids_.max_size() - capacity ? capacity + extra : recyclable;
                free_ids_.reserve((std::max)(recyclable, grown));
            }
            return true;
        }

        void clear()
        {
            if constexpr (preserve_issued_keys)
            {
                // Recycle live generations; keep already-free and exhausted slots.
                while (!empty())
                {
                    erase(base_.keys().back());
                }
            }
            else
            {
                base_.clear();
                free_ids_.clear();
                next_id_ = ATraits::initial_next();
            }
        }

        // ---- auto-insert ----------------------------------------------------

        /**
         * @brief Inserts a value with an automatically assigned key (copy).
         * @return The newly allocated key.
         */
        Key insert(const Value& value)
        {
            if (!prepareInsert(1))
            {
                return Key{};
            }
            const auto k = peekKey();
            base_.insert(k, value);
            consumeKey();
            return k;
        }

        /**
         * @brief Inserts a value with an automatically assigned key (move).
         * @return The newly allocated key.
         */
        Key insert(Value&& value)
        {
            if (!prepareInsert(1))
            {
                return Key{};
            }
            const auto k = peekKey();
            base_.insert(k, std::move(value));
            consumeKey();
            return k;
        }

        /**
         * @brief Constructs a value in-place with an automatically assigned key.
         * @return The newly allocated key.
         */
        template <class... Args>
        Key emplace(Args&&... args)
        {
            if (!prepareInsert(1))
            {
                return Key{};
            }
            const auto k = peekKey();
            base_.emplace(k, std::forward<Args>(args)...);
            consumeKey();
            return k;
        }

        // ---- manual insert --------------------------------------------------

        /**
         * @brief Accesses a value. Only non-generational keys allow manual insertion;
         * generational identities must be issued by the allocator above.
         */
        Value& operator[](Key key)
        {
            if constexpr (preserve_issued_keys)
            {
                return base_.at(key);
            }
            else
            {
                return base_[key];
            }
        }

        // ---- erase ----------------------------------------------------------

        /**
         * @brief Erases the element and reclaims the key for future reuse.
         */
        bool erase(Key key)
        {
            if (base_.erase(key))
            {
                recycleKey(key);
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
                recycleKey(key);
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
        static constexpr bool preserve_issued_keys = []
        {
            if constexpr (requires { ATraits::preserve_issued_keys; })
            {
                return ATraits::preserve_issued_keys;
            }
            return false;
        }();

        [[nodiscard]] Key peekKey() const noexcept
        {
            return free_ids_.empty() ? next_id_ : free_ids_.back();
        }

        void consumeKey() noexcept
        {
            if (!free_ids_.empty())
            {
                free_ids_.pop_back();
            }
            else
            {
                next_id_ = ATraits::next_fresh(next_id_);
            }
        }

        void recycleKey(Key key)
        {
            const auto recycled = ATraits::recycled(key);
            if (!ATraits::is_null(recycled))
            {
                free_ids_.push_back(recycled);
            }
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
