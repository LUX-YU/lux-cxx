#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file type_map.hpp
 * @brief A compile-time type-indexed heterogeneous map.
 *
 * Provides TypeMap<Pairs...> where each Pair is a type_map_entry<Key, Value>.
 * Lookup, mutation and iteration are all O(1) (resolved at compile time).
 * Integrates with lux::cxx::tuple_traits for iteration.
 */

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    // ---- entry type ---------------------------------------------------------

    /**
     * @brief A single (Key → Value) entry in a TypeMap.
     * @tparam Key   The type used as the map key (only its identity matters).
     * @tparam Value The value type stored for this key.
     */
    template <typename Key, typename Value>
    struct type_map_entry
    {
        using key_type   = Key;
        using value_type = Value;

        Value value{};

        constexpr type_map_entry() = default;
        constexpr explicit type_map_entry(const Value& v) : value(v) {}
        constexpr explicit type_map_entry(Value&& v) : value(std::move(v)) {}
    };

    // ---- detail helpers -----------------------------------------------------

    namespace detail
    {
        /**
         * @brief Finds the index of the first entry whose key_type matches Key.
         *        Returns sizeof...(Entries) if not found.
         */
        template <typename Key, typename... Entries>
        struct type_map_index;

        template <typename Key>
        struct type_map_index<Key>
        {
            static constexpr std::size_t value = 0;
        };

        template <typename Key, typename Entry, typename... Rest>
        struct type_map_index<Key, Entry, Rest...>
        {
            static constexpr std::size_t value =
                std::is_same_v<Key, typename Entry::key_type>
                    ? 0
                    : 1 + type_map_index<Key, Rest...>::value;
        };

        template <typename Key, typename... Entries>
        inline constexpr std::size_t type_map_index_v = type_map_index<Key, Entries...>::value;

        /**
         * @brief Checks if Key exists among the entries.
         */
        template <typename Key, typename... Entries>
        inline constexpr bool type_map_contains_v =
            (std::is_same_v<Key, typename Entries::key_type> || ...);

        /**
         * @brief Static assertion helper: all keys must be unique.
         */
        template <typename... Entries>
        struct assert_unique_keys;

        template <>
        struct assert_unique_keys<> : std::true_type {};

        template <typename First, typename... Rest>
        struct assert_unique_keys<First, Rest...>
        {
            static_assert(
                !(std::is_same_v<typename First::key_type, typename Rest::key_type> || ...),
                "TypeMap: duplicate key types are not allowed"
            );
            static constexpr bool value = assert_unique_keys<Rest...>::value;
        };
    } // namespace detail

    // ---- TypeMap class -------------------------------------------------------

    /**
     * @class TypeMap
     * @brief A compile-time heterogeneous map indexed by type.
     *
     * @tparam Entries  A pack of type_map_entry<Key, Value> types.
     *
     * Example usage:
     * @code
     *   TypeMap<
     *       type_map_entry<int,    std::string>,
     *       type_map_entry<float,  double>,
     *       type_map_entry<MyTag,  std::vector<int>>
     *   > map;
     *
     *   map.get<int>() = "hello";
     *   map.get<float>() = 3.14;
     *   assert(map.get<int>() == "hello");
     * @endcode
     */
    template <typename... Entries>
    class TypeMap
    {
        static_assert(detail::assert_unique_keys<Entries...>::value,
            "TypeMap: all key types must be distinct");

    public:
        using storage_type = std::tuple<Entries...>;

        /** @brief Number of entries in this map. */
        static constexpr std::size_t entry_count = sizeof...(Entries);

        constexpr TypeMap() = default;

        /**
         * @brief Constructs the map with initial values for each entry.
         * @param entries  Entries to copy/move into the map.
         */
        constexpr explicit TypeMap(Entries... entries)
            : storage_(std::move(entries)...)
        {
        }

        // ---- lookup by key type ---------------------------------------------

        /**
         * @brief Checks if a key type exists in the map (compile-time).
         * @tparam Key  The key type to look up.
         */
        template <typename Key>
        [[nodiscard]] static constexpr bool contains() noexcept
        {
            return detail::type_map_contains_v<Key, Entries...>;
        }

        /**
         * @brief Returns a mutable reference to the value associated with Key.
         * @tparam Key  The key type to look up.
         */
        template <typename Key>
        [[nodiscard]] constexpr auto& get() noexcept
        {
            static_assert(contains<Key>(), "TypeMap::get: key type not found");
            constexpr auto idx = detail::type_map_index_v<Key, Entries...>;
            return std::get<idx>(storage_).value;
        }

        /**
         * @brief Returns a const reference to the value associated with Key.
         * @tparam Key  The key type to look up.
         */
        template <typename Key>
        [[nodiscard]] constexpr const auto& get() const noexcept
        {
            static_assert(contains<Key>(), "TypeMap::get: key type not found");
            constexpr auto idx = detail::type_map_index_v<Key, Entries...>;
            return std::get<idx>(storage_).value;
        }

        // ---- lookup by index ------------------------------------------------

        /**
         * @brief Returns the entry at the given index.
         * @tparam I  Index into the entries (0-based).
         */
        template <std::size_t I>
        [[nodiscard]] constexpr auto& get_entry() noexcept
        {
            return std::get<I>(storage_);
        }

        template <std::size_t I>
        [[nodiscard]] constexpr const auto& get_entry() const noexcept
        {
            return std::get<I>(storage_);
        }

        // ---- iteration ------------------------------------------------------

        /**
         * @brief Applies a callable to each entry. The callable must accept
         *        a template parameter pack: func.template operator()<Key, Value, I>(value).
         *
         * Example:
         * @code
         *   map.for_each([]<typename Key, typename Value, std::size_t I>(Value& v) {
         *       // ...
         *   });
         * @endcode
         */
        template <typename Func>
        constexpr void for_each(Func&& func)
        {
            for_each_impl(std::forward<Func>(func), std::index_sequence_for<Entries...>{});
        }

        template <typename Func>
        constexpr void for_each(Func&& func) const
        {
            for_each_impl(std::forward<Func>(func), std::index_sequence_for<Entries...>{});
        }

        /** @brief Returns a reference to the underlying tuple storage. */
        [[nodiscard]] constexpr       storage_type& storage()       noexcept { return storage_; }
        [[nodiscard]] constexpr const storage_type& storage() const noexcept { return storage_; }

    private:
        storage_type storage_;

        template <typename Func, std::size_t... Is>
        constexpr void for_each_impl(Func&& func, std::index_sequence<Is...>)
        {
            (func.template operator()<
                typename std::tuple_element_t<Is, storage_type>::key_type,
                typename std::tuple_element_t<Is, storage_type>::value_type,
                Is
            >(std::get<Is>(storage_).value), ...);
        }

        template <typename Func, std::size_t... Is>
        constexpr void for_each_impl(Func&& func, std::index_sequence<Is...>) const
        {
            (func.template operator()<
                typename std::tuple_element_t<Is, storage_type>::key_type,
                typename std::tuple_element_t<Is, storage_type>::value_type,
                Is
            >(std::get<Is>(storage_).value), ...);
        }
    };

    // ---- convenience alias --------------------------------------------------

    /**
     * @brief Shorthand for creating a type_map_entry.
     *
     * Usage: TypeMap< entry<int, std::string>, entry<float, double> >
     */
    template <typename Key, typename Value>
    using entry = type_map_entry<Key, Value>;

} // namespace lux::cxx
