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

#include <cstdint>
#include <cstddef>
#include <limits>
#include "extended_type_traits.hpp"

namespace lux::cxx
{
	namespace detail
	{
        struct fnv1a_init_value {
            using type = std::uint64_t;
            static constexpr std::uint64_t offset = 14695981039346656037ull;
            static constexpr std::uint64_t prime  = 1099511628211ull;
        };

        template<typename Char>
        struct hash_string 
        {
            using value_type = Char;
            using size_type  = size_t;
            using hash_type  = uint64_t;

            const value_type*   repr;
            size_type           length;
            hash_type           hash;
        };

        template<typename Char, typename hash_str_t = hash_string<Char>>
        [[nodiscard]] constexpr auto fnv1a(const Char* str) noexcept
        {
            hash_str_t base{ str, 0u, fnv1a_init_value::offset };

            for (; str[base.length]; ++base.length) {
                base.hash = (base.hash ^ static_cast<fnv1a_init_value::type>(str[base.length])) * fnv1a_init_value::prime;
            }

            return base;
        }

        template<typename Char, typename hash_str_t = hash_string<Char>>
        [[nodiscard]] constexpr auto fnv1a(const Char* str, const std::size_t len) noexcept
        {
            hash_str_t base{ str, len, fnv1a_init_value::offset };

            for (typename hash_str_t::size_type pos{}; pos < len; ++pos) {
                base.hash = (base.hash ^ static_cast<fnv1a_init_value::type>(str[pos])) * fnv1a_init_value::prime;
            }

            return base;
        }

        inline consteval std::size_t combine_hashes_impl(std::size_t seed, std::size_t hash) noexcept {
            return seed * 16777619u + hash;
        }
    }

    constexpr inline std::string_view erase_elaborated_specifier(std::string_view sv) noexcept
    {
        constexpr std::string_view kw[] = { "struct ", "class ", "enum ", "union " };
        for (auto k : kw)
            if (sv.starts_with(k)) {
                sv.remove_prefix(k.size());
                break;
            }
        return sv;
    }

    template<typename Type>
    [[nodiscard]] constexpr std::string_view type_name() noexcept
    {
        constexpr auto value = strip_type_name<Type>();
        return erase_elaborated_specifier(value);
    }

    template<typename Type, typename hash_str_t = detail::hash_string<char>>
    [[nodiscard]] constexpr typename hash_str_t::hash_type type_hash() noexcept
    {
        constexpr auto stripped = type_name<Type>();
        return detail::fnv1a<char>(stripped.data(), stripped.size()).hash;
    }

	struct basic_type_info
	{
        using hash_str_t = detail::hash_string<char>;
		using id_t       = hash_str_t::hash_type;

        constexpr basic_type_info() {
			_id   = std::numeric_limits<id_t>::max();
			_name = "";
        }

		template<typename T>
        constexpr basic_type_info(std::in_place_type_t<T>) noexcept
			: _id(type_hash<T>()),
              _name(type_name<T>()){}

        constexpr basic_type_info(id_t id, std::string_view name) noexcept
            : _id(id), _name(name) {}

        [[nodiscard]] constexpr id_t hash() const noexcept {
            return _id;
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return _name;
        }

		constexpr bool isValid() const noexcept {
			return _id != std::numeric_limits<id_t>::max();
		}

	private:
        id_t             _id;
        std::string_view _name;
	};

    [[nodiscard]] inline constexpr bool operator==(const basic_type_info &lhs, const basic_type_info &rhs) noexcept {
        return lhs.hash() == rhs.hash();
    }

    [[nodiscard]] inline constexpr bool operator!=(const basic_type_info &lhs, const basic_type_info &rhs) noexcept {
        return !(lhs == rhs);
    }

    template<typename T> constexpr inline basic_type_info make_basic_type_info() noexcept {
        return basic_type_info(std::in_place_type<T>);
    }

    template<std::size_t... Hashes>
    consteval std::size_t combine_hashes(std::index_sequence<Hashes...>) noexcept {
        // offset of FNV-1a
        constexpr std::size_t offsetBasis = 14695981039346656037ull;
        std::size_t combined = offsetBasis;
        ((combined = detail::combine_hashes_impl(combined, Hashes)), ...);
        return combined;
    }
}