#pragma once

#include <lux/cxx/compile_time/type_info.hpp>

#include <cstdint>
#include <string_view>

namespace lux::cxx
{
    class TypeToken final
    {
      public:
        using hash_type = std::uint64_t;

        constexpr TypeToken() noexcept = default;

        constexpr TypeToken(hash_type hash, std::string_view name) noexcept
            : hash_(hash), name_(name)
        {
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return !name_.empty();
        }

        [[nodiscard]] constexpr hash_type hash() const noexcept
        {
            return hash_;
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept
        {
            return name_;
        }

        [[nodiscard]] constexpr bool operator==(
            const TypeToken& other
        ) const noexcept
        {
            return hash_ == other.hash_ && name_ == other.name_;
        }

      private:
        hash_type hash_ = 0;
        std::string_view name_;
    };

    template <typename Type>
    [[nodiscard]] constexpr TypeToken typeToken() noexcept
    {
        return TypeToken(type_hash<Type>(), type_name<Type>());
    }
} // namespace lux::cxx
