#pragma once
#include <cstdint>
#include "extended_type_traits.hpp"

namespace lux::cxx
{
	namespace detail
	{
        struct fnv1a_init_value {
            using type = std::uint64_t;
            static constexpr std::uint64_t offset = 14695981039346656037ull;
            static constexpr std::uint64_t prime = 1099511628211ull;
        };

        template<typename Char>
        struct hash_string {
            using value_type = Char;
            using size_type = size_t;
            using hash_type = uint64_t;

            const value_type* repr;
            size_type           length;
            hash_type           hash;
        };

        template<typename Char, typename hash_str_t = hash_string<Char>>
        [[nodiscard]] constexpr auto hash_helper(const Char* str) noexcept {
            hashed_str_t base{ str, 0u, fnv1a_init_value::offset };

            for (; str[base.length]; ++base.length) {
                base.hash = (base.hash ^ static_cast<fnv1a_init_value::type>(str[base.length])) * fnv1a_init_value::prime;
            }

            return base;
        }

        template<typename Char, typename hash_str_t = hash_string<Char>>
        [[nodiscard]] constexpr auto hash_helper(const Char* str, const std::size_t len) noexcept {
            hashed_str_t base{ str, len, fnv1a_init_value::offset };

            for (hashed_str_t::size_type pos{}; pos < len; ++pos) {
                base.hash = (base.hash ^ static_cast<fnv1a_init_value::type>(str[pos])) * fnv1a_init_value::prime;
            }

            return base;
        }
	}

    template<typename Type>
    [[nodiscard]] constexpr std::string_view type_name() noexcept {
        constexpr auto value = strip_type_name<Type>();
        return value;
    }

    template<typename Type, typename hash_str_t = detail::hash_string<char>>
    [[nodiscard]] constexpr hash_str_t::hash_type type_hash() noexcept {
        constexpr auto stripped = type_name<Type>();
        return detail::hash_helper<char>(stripped.data(), stripped.size()).hash;
    }

	struct type_info
	{
        using hash_str_t = detail::hash_string<char>;
		using id_t       = hash_str_t::hash_type;

		template<typename T>
        constexpr type_info(std::in_place_type_t<T>) noexcept
			: id(type_hash<T>()),
              name(type_name<T>()){}

        constexpr type_info(id_t id, std::string_view name) noexcept
            : id(id), name(name) {}

        [[nodiscard]] constexpr id_t hash() const noexcept {
            return id;
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return name;
        }

	private:
        id_t             id;
        std::string_view name;
	};
}