#pragma once
#include <cstdint>
#include <cstddef>
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

        static inline consteval std::size_t combine_hashes_impl(std::size_t seed, std::size_t hash) noexcept {
            return seed * 16777619u + hash;
        }
    }

    template<typename Type>
    [[nodiscard]] constexpr std::string_view type_name() noexcept
    {
        constexpr auto value = strip_type_name<Type>();
        return value;
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

    template<typename T> constexpr static inline basic_type_info make_basic_type_info() noexcept {
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