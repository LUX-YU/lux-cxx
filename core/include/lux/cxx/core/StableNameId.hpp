#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace lux::cxx
{
    struct Fnv1a64 final
    {
        [[nodiscard]] static constexpr std::uint64_t hash(
            std::string_view text
        ) noexcept
        {
            std::uint64_t value = 14695981039346656037ULL;
            for (const unsigned char character : text)
            {
                value ^= character;
                value *= 1099511628211ULL;
            }
            return value;
        }
    };

    template <typename Tag, typename Hash = Fnv1a64>
    class StableNameIdView final
    {
      public:
        using tag_type = Tag;
        using hash_type = std::uint64_t;

        constexpr StableNameIdView() noexcept = default;

        constexpr explicit StableNameIdView(std::string_view name) noexcept
            : name_(name), hash_(Hash::hash(name))
        {
        }

        [[nodiscard]] static constexpr StableNameIdView fromVerified(
            std::string_view name,
            hash_type hash
        ) noexcept
        {
            if (name.empty() || Hash::hash(name) != hash) return {};
            StableNameIdView result;
            result.name_ = name;
            result.hash_ = hash;
            return result;
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return !name_.empty();
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept
        {
            return name_;
        }

        [[nodiscard]] constexpr hash_type hash() const noexcept
        {
            return hash_;
        }

        [[nodiscard]] constexpr bool operator==(
            const StableNameIdView& other
        ) const noexcept
        {
            return hash_ == other.hash_ && name_ == other.name_;
        }

      private:
        std::string_view name_;
        hash_type hash_ = 0;
    };

    template <
        typename Tag,
        typename Hash = Fnv1a64,
        typename Allocator = std::allocator<char>
    >
    class StableNameId final
    {
      public:
        using tag_type = Tag;
        using hash_type = std::uint64_t;
        using allocator_type = Allocator;
        using string_type = std::basic_string<
            char,
            std::char_traits<char>,
            Allocator
        >;

        StableNameId() = default;

        explicit StableNameId(
            std::string_view name,
            const Allocator& allocator = Allocator{}
        )
            : name_(name, allocator), hash_(Hash::hash(name))
        {
        }

        [[nodiscard]] bool isValid() const noexcept
        {
            return !name_.empty();
        }

        [[nodiscard]] std::string_view name() const noexcept
        {
            return name_;
        }

        [[nodiscard]] hash_type hash() const noexcept
        {
            return hash_;
        }

        [[nodiscard]] StableNameIdView<Tag, Hash> view() const noexcept
        {
            return StableNameIdView<Tag, Hash>::fromVerified(name_, hash_);
        }

        [[nodiscard]] bool operator==(const StableNameId& other) const noexcept
        {
            return hash_ == other.hash_ && name_ == other.name_;
        }

      private:
        string_type name_;
        hash_type hash_ = 0;
    };
} // namespace lux::cxx
