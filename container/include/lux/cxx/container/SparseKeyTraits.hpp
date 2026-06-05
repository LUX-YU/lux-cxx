#pragma once
/**
 * @file SparseKeyTraits.hpp
 * @brief Key-traits abstractions for sparse-set containers.
 *
 * Provides two trait layers:
 * - `sparse_key_traits<Key>`:  addressing semantics (sparse index, equality).
 * - `auto_key_traits<Key>`:    lifecycle semantics (allocation, recycling).
 *
 * Built-in specialisations cover:
 * - Plain integral keys (with optional offset via `offset_sparse_key_traits`).
 * - Generational `SlotKey` handles from SlotMap.hpp.
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

#include "SlotMap.hpp"

#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace lux::cxx
{
    // =========================================================================
    //  Layer 1 – addressing traits  (used by BasicSparseSet)
    // =========================================================================

    /**
     * @brief Primary template – must be specialised per key kind.
     *
     * Required static members:
     *  - `bool is_addressable(key_type)`  — can this key be looked up?
     *  - `size_t sparse_index(key_type)`  — position in the sparse array.
     *  - `bool equals(key_type, key_type)` — full-key equality (including generation for SlotKey).
     */
    template <class Key, class = void>
    struct sparse_key_traits;

    /**
     * @brief Specialisation for plain integral keys (no offset).
     */
    template <std::integral Key>
    struct sparse_key_traits<Key>
    {
        using key_type = Key;

        static constexpr bool is_addressable(key_type /*k*/) noexcept
        {
            return true;
        }

        static constexpr std::size_t sparse_index(key_type k) noexcept
        {
            return static_cast<std::size_t>(k);
        }

        static constexpr bool equals(key_type a, key_type b) noexcept
        {
            return a == b;
        }
    };

    /**
     * @brief Traits for integral keys with a compile-time offset.
     *
     * Keys below @p Offset are considered non-addressable.
     *
     * @tparam Key    Integral key type.
     * @tparam Offset Compile-time minimum valid key.
     */
    template <std::integral Key, Key Offset>
    struct offset_sparse_key_traits
    {
        using key_type = Key;

        static constexpr bool is_addressable(key_type k) noexcept
        {
            return k >= Offset;
        }

        static constexpr std::size_t sparse_index(key_type k) noexcept
        {
            return static_cast<std::size_t>(k - Offset);
        }

        static constexpr bool equals(key_type a, key_type b) noexcept
        {
            return a == b;
        }
    };

    /**
     * @brief Specialisation for generational SlotKey handles.
     *
     * Addressing uses `key.index`; equality uses full `operator==`
     * (index **and** generation), so that stale handles never match
     * a reused slot.
     */
    template <class Tag, class IndexType, class GenerationType>
    struct sparse_key_traits<SlotKey<Tag, IndexType, GenerationType>>
    {
        using key_type = SlotKey<Tag, IndexType, GenerationType>;

        static constexpr bool is_addressable(key_type k) noexcept
        {
            return !k.is_null();
        }

        static constexpr std::size_t sparse_index(key_type k) noexcept
        {
            return static_cast<std::size_t>(k.index);
        }

        static constexpr bool equals(key_type a, key_type b) noexcept
        {
            return a == b;
        }
    };

    // =========================================================================
    //  Layer 2 – lifecycle traits  (used by AutoSparseSet)
    // =========================================================================

    /**
     * @brief Primary template – must be specialised per key kind.
     *
     * Required static members:
     *  - `key_type initial_next()` — first key to allocate.
     *  - `key_type next_fresh(key_type)` — advance to next fresh key.
     *  - `key_type recycled(key_type)` — produce a safe-to-reuse key from an erased one.
     *  - `bool is_null(key_type)` — check for null / invalid sentinel.
     */
    template <class Key, class = void>
    struct auto_key_traits;

    /**
     * @brief Lifecycle traits for plain integral keys.
     *
     * Recycling returns the same key (integers have no generation).
     */
    template <std::integral Key>
    struct auto_key_traits<Key>
    {
        using key_type = Key;

        static constexpr key_type initial_next() noexcept
        {
            return Key{0};
        }

        static constexpr key_type next_fresh(key_type k) noexcept
        {
            return static_cast<key_type>(k + 1);
        }

        static constexpr key_type recycled(key_type k) noexcept
        {
            return k;
        }

        static constexpr bool is_null(key_type /*k*/) noexcept
        {
            return false;
        }
    };

    /**
     * @brief Lifecycle traits for integral keys with a compile-time offset.
     */
    template <std::integral Key, Key Offset>
    struct offset_auto_key_traits
    {
        using key_type = Key;

        static constexpr key_type initial_next() noexcept
        {
            return Offset;
        }

        static constexpr key_type next_fresh(key_type k) noexcept
        {
            return static_cast<key_type>(k + 1);
        }

        static constexpr key_type recycled(key_type k) noexcept
        {
            return k;
        }

        static constexpr bool is_null(key_type /*k*/) noexcept
        {
            return false;
        }
    };

    /**
     * @brief Lifecycle traits for generational SlotKey handles.
     *
     * On recycling the generation is bumped so that stale handles
     * pointing to the same slot index remain invalid.
     */
    template <class Tag, class IndexType, class GenerationType>
    struct auto_key_traits<SlotKey<Tag, IndexType, GenerationType>>
    {
        using key_type = SlotKey<Tag, IndexType, GenerationType>;

        static constexpr key_type initial_next() noexcept
        {
            return key_type{ IndexType{0}, GenerationType{1} };
        }

        static constexpr key_type next_fresh(key_type k) noexcept
        {
            return key_type{
                static_cast<IndexType>(k.index + 1),
                GenerationType{1}
            };
        }

        /** @brief Bumps generation so stale handles with the same index stay invalid. */
        static constexpr key_type recycled(key_type old_key) noexcept
        {
            return key_type{
                old_key.index,
                static_cast<GenerationType>(old_key.gen + 1)
            };
        }

        static constexpr bool is_null(key_type k) noexcept
        {
            return k.is_null();
        }
    };

} // namespace lux::cxx
