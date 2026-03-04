#pragma once
/**
 * @file SlotMap.hpp
 * @brief A slot map (dense storage with generational handles) for O(1) insert,
 *        remove, and lookup with stable, invalidation-safe handles.
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
#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    /**
     * @brief A generational handle returned by SlotMap.
     *
     * Holds an index into the slot array and a generation counter.
     * When a slot is reused after erasure, the generation is incremented
     * so that stale handles become invalid.
     *
     * @tparam IndexType     Unsigned integer type for the slot index.
     * @tparam GenerationType Unsigned integer type for the generation counter.
     */
    template <
        typename IndexType      = std::uint32_t,
        typename GenerationType = std::uint32_t
    >
    struct SlotKey
    {
        static_assert(std::is_unsigned_v<IndexType>,
            "SlotKey: IndexType must be unsigned");
        static_assert(std::is_unsigned_v<GenerationType>,
            "SlotKey: GenerationType must be unsigned");

        using index_type      = IndexType;
        using generation_type = GenerationType;

        index_type      index      = (std::numeric_limits<index_type>::max)();
        generation_type generation = 0;

        /** @brief Returns true if this key has never been assigned. */
        [[nodiscard]] constexpr bool is_null() const noexcept
        {
            return index == (std::numeric_limits<index_type>::max)();
        }

        [[nodiscard]] constexpr bool operator==(const SlotKey&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const SlotKey&) const noexcept = default;
    };

    /**
     * @class SlotMap
     * @brief Dense-storage associative container with O(1) insert, erase, and lookup.
     *
     * Internally maintains:
     * - A **slot array** (indirect layer) mapping slot index → dense index + generation.
     * - A **dense array** of values for cache-friendly iteration.
     * - A **reverse map** from dense index → slot index, used during swap-and-pop erasure.
     * - A **free list** of recycled slot indices.
     *
     * Insert returns a SlotKey. Lookup and erase validate the generation to detect
     * use-after-free of stale handles.
     *
     * @tparam Value          The element type.
     * @tparam IndexType      Unsigned integer for slot/dense indices.
     * @tparam GenerationType Unsigned integer for generation counters.
     */
    template <
        typename Value,
        typename IndexType      = std::uint32_t,
        typename GenerationType = std::uint32_t
    >
    class SlotMap
    {
        static_assert(std::is_unsigned_v<IndexType>,
            "SlotMap: IndexType must be unsigned");
        static_assert(std::is_unsigned_v<GenerationType>,
            "SlotMap: GenerationType must be unsigned");

    public:
        using key_type        = SlotKey<IndexType, GenerationType>;
        using value_type      = Value;
        using size_type       = std::size_t;
        using index_type      = IndexType;
        using generation_type = GenerationType;

        static constexpr index_type INVALID_INDEX =
            (std::numeric_limits<index_type>::max)();

        // ---- constructors ---------------------------------------------------

        SlotMap() = default;

        explicit SlotMap(size_type initial_capacity)
        {
            reserve(initial_capacity);
        }

        // ---- capacity -------------------------------------------------------

        [[nodiscard]] size_type size()  const noexcept { return dense_.size(); }
        [[nodiscard]] bool      empty() const noexcept { return dense_.empty(); }

        void reserve(size_type n)
        {
            slots_.reserve(n);
            dense_.reserve(n);
            dense_to_slot_.reserve(n);
        }

        void clear()
        {
            slots_.clear();
            dense_.clear();
            dense_to_slot_.clear();
            free_head_ = INVALID_INDEX;
        }

        // ---- insert ---------------------------------------------------------

        /**
         * @brief Inserts a value by copy. Returns the handle.
         */
        key_type insert(const Value& value)
        {
            return emplace_impl(value);
        }

        /**
         * @brief Inserts a value by move. Returns the handle.
         */
        key_type insert(Value&& value)
        {
            return emplace_impl(std::move(value));
        }

        /**
         * @brief Constructs a value in-place. Returns the handle.
         */
        template <typename... Args>
        key_type emplace(Args&&... args)
        {
            return emplace_impl(std::forward<Args>(args)...);
        }

        // ---- erase ----------------------------------------------------------

        /**
         * @brief Removes the element identified by @p key.
         * @param key The handle previously returned by insert/emplace.
         * @return True if the element was erased, false if the key was stale or invalid.
         */
        bool erase(key_type key)
        {
            if (!is_valid(key))
                return false;

            auto& slot = slots_[key.index];
            index_type dense_idx = slot.dense_index;
            index_type last_dense = static_cast<index_type>(dense_.size() - 1);

            // Swap-and-pop in the dense array.
            if (dense_idx != last_dense)
            {
                dense_[dense_idx] = std::move(dense_[last_dense]);
                dense_to_slot_[dense_idx] = dense_to_slot_[last_dense];
                // Update the swapped element's slot to point to the new dense position.
                slots_[dense_to_slot_[dense_idx]].dense_index = dense_idx;
            }
            dense_.pop_back();
            dense_to_slot_.pop_back();

            // Increment generation so stale handles are detected.
            slot.generation++;
            // Push this slot onto the free list.
            slot.dense_index = free_head_;
            free_head_ = key.index;

            return true;
        }

        // ---- lookup ---------------------------------------------------------

        /**
         * @brief Checks whether a key is currently valid.
         */
        [[nodiscard]] bool is_valid(key_type key) const noexcept
        {
            if (key.is_null())
                return false;
            if (key.index >= slots_.size())
                return false;
            return slots_[key.index].generation == key.generation;
        }

        /**
         * @brief Returns a pointer to the value, or nullptr if the key is stale.
         */
        [[nodiscard]] Value* find(key_type key) noexcept
        {
            if (!is_valid(key))
                return nullptr;
            return &dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value* find(key_type key) const noexcept
        {
            if (!is_valid(key))
                return nullptr;
            return &dense_[slots_[key.index].dense_index];
        }

        /**
         * @brief Returns a reference to the value. Throws if the key is invalid.
         */
        [[nodiscard]] Value& at(key_type key)
        {
            if (!is_valid(key))
                throw std::out_of_range("SlotMap::at: invalid or stale key");
            return dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value& at(key_type key) const
        {
            if (!is_valid(key))
                throw std::out_of_range("SlotMap::at: invalid or stale key");
            return dense_[slots_[key.index].dense_index];
        }

        /**
         * @brief Returns a reference to the value. Undefined behaviour if the key is invalid.
         */
        [[nodiscard]] Value& operator[](key_type key) noexcept
        {
            return dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value& operator[](key_type key) const noexcept
        {
            return dense_[slots_[key.index].dense_index];
        }

        // ---- dense iteration ------------------------------------------------

        /**
         * @brief Returns a const reference to the dense array of values.
         *
         * The order is unspecified but the array is contiguous — ideal for
         * cache-friendly iteration in ECS-style loops.
         */
        [[nodiscard]] const std::vector<Value>& values() const noexcept { return dense_; }
        [[nodiscard]]       std::vector<Value>& values()       noexcept { return dense_; }

        [[nodiscard]] typename std::vector<Value>::iterator       begin()        noexcept { return dense_.begin(); }
        [[nodiscard]] typename std::vector<Value>::iterator       end()          noexcept { return dense_.end(); }
        [[nodiscard]] typename std::vector<Value>::const_iterator begin()  const noexcept { return dense_.begin(); }
        [[nodiscard]] typename std::vector<Value>::const_iterator end()    const noexcept { return dense_.end(); }
        [[nodiscard]] typename std::vector<Value>::const_iterator cbegin() const noexcept { return dense_.cbegin(); }
        [[nodiscard]] typename std::vector<Value>::const_iterator cend()   const noexcept { return dense_.cend(); }

    private:
        /**
         * @brief Internal slot structure.
         *
         * When the slot is alive, @p dense_index points into the dense array.
         * When the slot is free, @p dense_index stores the next free-list index
         * (INVALID_INDEX means end of list).
         */
        struct Slot
        {
            index_type      dense_index = INVALID_INDEX;
            generation_type generation  = 0;
        };

        std::vector<Slot>       slots_;           ///< Indirect slot array.
        std::vector<Value>      dense_;            ///< Dense value storage.
        std::vector<index_type> dense_to_slot_;    ///< Reverse map: dense index → slot index.
        index_type              free_head_ = INVALID_INDEX; ///< Head of the free list.

        /**
         * @brief Allocates a slot (from the free list or by growing) and
         *        emplaces a value into the dense array.
         */
        template <typename... Args>
        key_type emplace_impl(Args&&... args)
        {
            index_type slot_idx;
            if (free_head_ != INVALID_INDEX)
            {
                // Reuse a recycled slot.
                slot_idx = free_head_;
                free_head_ = slots_[slot_idx].dense_index;
            }
            else
            {
                // Grow the slot array.
                slot_idx = static_cast<index_type>(slots_.size());
                slots_.push_back(Slot{});
            }

            index_type dense_idx = static_cast<index_type>(dense_.size());
            dense_.emplace_back(std::forward<Args>(args)...);
            dense_to_slot_.push_back(slot_idx);

            auto& slot = slots_[slot_idx];
            slot.dense_index = dense_idx;
            // generation was already incremented on erase (or is 0 for new slots).

            return key_type{ slot_idx, slot.generation };
        }
    };

} // namespace lux::cxx
