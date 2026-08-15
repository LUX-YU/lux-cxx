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
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
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
     * @tparam Tag            Type tag for compile-time key discrimination (default: void).
     * @tparam IndexType      Unsigned integer type for the slot index.
     * @tparam GenerationType Unsigned integer type for the generation counter.
     */
    template <typename Tag = void, typename IndexType = std::uint32_t, typename GenerationType = std::uint32_t>
    struct SlotKey
    {
        static_assert(std::is_unsigned_v<IndexType>, "SlotKey: IndexType must be unsigned");
        static_assert(std::is_unsigned_v<GenerationType>, "SlotKey: GenerationType must be unsigned");

        using tag_t        = Tag;
        using index_type      = IndexType;
        using generation_type = GenerationType;

        index_type      index = (std::numeric_limits<index_type>::max)();
        generation_type gen   = 0;

        /** @brief Returns true if this key has never been assigned. */
        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return index == (std::numeric_limits<index_type>::max)();
        }

        /** @brief Returns true if this key refers to a potentially valid slot. */
        [[nodiscard]] constexpr bool isValid() const noexcept { return !isNull(); }

        /** @brief Returns an explicitly invalid (null) key. */
        [[nodiscard]] static constexpr SlotKey invalid() noexcept { return SlotKey{}; }

        [[nodiscard]] constexpr bool operator==(const SlotKey&) const noexcept = default;
        [[nodiscard]] constexpr bool operator!=(const SlotKey&) const noexcept = default;

        /** @brief Hash functor for use with std::unordered_map / std::unordered_set. */
        struct Hash
        {
            constexpr std::size_t operator()(const SlotKey& k) const noexcept
            {
                std::size_t h = static_cast<std::size_t>(k.index);
                h ^= static_cast<std::size_t>(k.gen) + std::size_t(0x9e3779b9) + (h << 6) + (h >> 2);
                return h;
            }
        };
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
     * @note **Pointer stability**: Because the dense array uses swap-and-pop erasure,
     *       pointers/references obtained via find() or operator[] are invalidated when
     *       **any** element is erased.  Do not cache pointers across erase() calls.
     *
     * @tparam Value          The element type.
     * @tparam Tag            Type tag forwarded to SlotKey for compile-time discrimination.
     * @tparam IndexType      Unsigned integer for slot/dense indices.
     * @tparam GenerationType Unsigned integer for generation counters.
     */
    template <
        typename Value,
        typename Tag = void,
        typename IndexType = std::uint32_t,
        typename GenerationType = std::uint32_t,
        typename Allocator = std::allocator<Value>
    >
    class SlotMap
    {
        static_assert(std::is_unsigned_v<IndexType>, "SlotMap: IndexType must be unsigned");
        static_assert(std::is_unsigned_v<GenerationType>, "SlotMap: GenerationType must be unsigned");

      private:
        struct Slot;
        using slot_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<Slot>;
        using index_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<IndexType>;

      public:
        using key_type        = SlotKey<Tag, IndexType, GenerationType>;
        using value_type      = Value;
        using size_type       = std::size_t;
        using index_type      = IndexType;
        using generation_type = GenerationType;
        using allocator_type = Allocator;
        using value_container = std::vector<Value, Allocator>;

        static constexpr index_type INVALID_INDEX =
            (std::numeric_limits<index_type>::max)();

        // ---- constructors ---------------------------------------------------

        SlotMap()
            : SlotMap(Allocator{})
        {
        }

        explicit SlotMap(const Allocator& allocator)
            : slots_(slot_allocator(allocator)),
              dense_(allocator),
              dense_to_slot_(index_allocator(allocator))
        {
        }

        explicit SlotMap(
            size_type initial_capacity,
            const Allocator& allocator = Allocator{}
        )
            : SlotMap(allocator)
        {
            reserve(initial_capacity);
        }

        [[nodiscard]] allocator_type get_allocator() const noexcept
        {
            return dense_.get_allocator();
        }

        // ---- capacity -------------------------------------------------------

        [[nodiscard]] size_type size()     const noexcept { return dense_.size(); }
        [[nodiscard]] bool   empty()    const noexcept { return dense_.empty(); }
        [[nodiscard]] size_type capacity() const noexcept { return slots_.capacity(); }

        void reserve(size_type n)
        {
            slots_.reserve(n);
            dense_.reserve(n);
            dense_to_slot_.reserve(n);
        }

        void shrink_to_fit()
        {
            slots_.shrink_to_fit();
            dense_.shrink_to_fit();
            dense_to_slot_.shrink_to_fit();
        }

        void clear()
        {
            dense_.clear();
            dense_to_slot_.clear();
            free_head_ = INVALID_INDEX;
            for (std::size_t index = slots_.size(); index > 0; --index)
            {
                auto& slot = slots_[index - 1];
                if (slot.generation == (std::numeric_limits<generation_type>::max)())
                {
                    slot.dense_index = INVALID_INDEX;
                    continue;
                }
                ++slot.generation;
                slot.dense_index = INVALID_INDEX;
                if (slot.generation != (std::numeric_limits<generation_type>::max)())
                {
                    slot.dense_index = free_head_;
                    free_head_ = static_cast<index_type>(index - 1);
                }
            }
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

        template <typename... Args>
        [[nodiscard]] std::optional<key_type> tryEmplace(Args&&... args)
        {
            try
            {
                return emplace_impl(std::forward<Args>(args)...);
            }
            catch (const std::bad_alloc&)
            {
                return std::nullopt;
            }
        }

        // ---- erase ----------------------------------------------------------

        /**
         * @brief Removes the element identified by @p key.
         * @param key The handle previously returned by insert/emplace.
         * @return True if the element was erased, false if the key was stale or invalid.
         */
        bool erase(key_type key)
        {
            if (!isValid(key))
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
            // Recycle the slot unless its generation just reached the maximum: at
            // that point RETIRE it (leave it off the free list) so a future erase
            // can never wrap the counter back to a value an old handle still holds
            // (ABA). The cost is one leaked slot per 2^bits reuses of that slot —
            // negligible for the default uint32 generation. A wider GenerationType
            // pushes this even further out.
            if (slot.generation != std::numeric_limits<generation_type>::max())
            {
                slot.dense_index = free_head_;
                free_head_ = key.index;
            }
            else
            {
                slot.dense_index = INVALID_INDEX;
            }

            return true;
        }

        // ---- lookup ---------------------------------------------------------

        /**
         * @brief Checks whether a key is currently valid.
         */
        [[nodiscard]] bool isValid(key_type key) const noexcept
        {
            if (key.isNull())
                return false;
            if (key.index >= slots_.size())
                return false;
            const auto& slot = slots_[key.index];
            return slot.generation == key.gen &&
                   slot.dense_index < dense_.size();
        }

        /**
         * @brief Returns a pointer to the value, or nullptr if the key is stale.
         */
        [[nodiscard]] Value* find(key_type key) noexcept
        {
            if (!isValid(key))
                return nullptr;
            return &dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value* find(key_type key) const noexcept
        {
            if (!isValid(key))
                return nullptr;
            return &dense_[slots_[key.index].dense_index];
        }

        /**
         * @brief Returns a reference to the value. Throws if the key is invalid.
         */
        [[nodiscard]] Value& at(key_type key)
        {
            if (!isValid(key))
                throw std::out_of_range("SlotMap::at: invalid or stale key");
            return dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value& at(key_type key) const
        {
            if (!isValid(key))
                throw std::out_of_range("SlotMap::at: invalid or stale key");
            return dense_[slots_[key.index].dense_index];
        }

        /**
         * @brief Returns a reference to the value. Undefined behaviour if the key is invalid.
         */
        [[nodiscard]] Value& operator[](key_type key) noexcept
        {
            assert(isValid(key) && "SlotMap::operator[] called with an invalid/stale key");
            return dense_[slots_[key.index].dense_index];
        }

        [[nodiscard]] const Value& operator[](key_type key) const noexcept
        {
            assert(isValid(key) && "SlotMap::operator[] called with an invalid/stale key");
            return dense_[slots_[key.index].dense_index];
        }

        // ---- dense iteration ------------------------------------------------

        /**
         * @brief Returns a const reference to the dense array of values.
         *
         * The order is unspecified but the array is contiguous — ideal for
         * cache-friendly iteration in ECS-style loops.
         */
        [[nodiscard]] const value_container& values() const noexcept { return dense_; }
        [[nodiscard]] value_container& values() noexcept { return dense_; }

        [[nodiscard]] auto begin() noexcept { return dense_.begin(); }
        [[nodiscard]] auto end() noexcept { return dense_.end(); }
        [[nodiscard]] auto begin() const noexcept { return dense_.begin(); }
        [[nodiscard]] auto end() const noexcept { return dense_.end(); }
        [[nodiscard]] auto cbegin() const noexcept { return dense_.cbegin(); }
        [[nodiscard]] auto cend() const noexcept { return dense_.cend(); }

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
            generation_type generation  = 1;
        };

        std::vector<Slot, slot_allocator> slots_; ///< Indirect slot array.
        value_container dense_;                  ///< Dense value storage.
        std::vector<index_type, index_allocator> dense_to_slot_; ///< Reverse map.
        index_type              free_head_ = INVALID_INDEX; ///< Head of the free list.

        /**
         * @brief Allocates a slot (from the free list or by growing) and
         *        emplaces a value into the dense array.
         */
        template <typename... Args>
        key_type emplace_impl(Args&&... args)
        {
            const bool reused = free_head_ != INVALID_INDEX;
            index_type slot_idx = free_head_;
            if (!reused)
            {
                if (slots_.size() >= static_cast<std::size_t>(INVALID_INDEX))
                {
                    throw std::length_error("SlotMap index space exhausted");
                }
                slot_idx = static_cast<index_type>(slots_.size());
                slots_.push_back(Slot{});
            }

            try
            {
                dense_.emplace_back(std::forward<Args>(args)...);
            }
            catch (...)
            {
                if (!reused) slots_.pop_back();
                throw;
            }

            try
            {
                dense_to_slot_.push_back(slot_idx);
            }
            catch (...)
            {
                dense_.pop_back();
                if (!reused) slots_.pop_back();
                throw;
            }

            auto& slot = slots_[slot_idx];
            if (reused) free_head_ = slot.dense_index;
            slot.dense_index = static_cast<index_type>(dense_.size() - 1);

            return key_type{ slot_idx, slot.generation };
        }
    };

} // namespace lux::cxx
