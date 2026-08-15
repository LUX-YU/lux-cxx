#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <lux/cxx/algorithm/sha256.hpp>

namespace lux::cxx
{
    struct SemanticVersion final
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t patch = 0;

        [[nodiscard]] constexpr auto operator<=>(
            const SemanticVersion&
        ) const noexcept = default;
    };

    struct AbiStringView final
    {
        const char* data = nullptr;
        std::size_t size = 0;

        constexpr AbiStringView() noexcept = default;

        constexpr explicit AbiStringView(std::string_view text) noexcept
            : data(text.data()), size(text.size())
        {
        }

        [[nodiscard]] constexpr std::string_view view() const noexcept
        {
            return {data, size};
        }
    };

    struct AbiByteView final
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;

        [[nodiscard]] constexpr std::span<const std::uint8_t> view() const noexcept
        {
            return {data, size};
        }
    };

    class AbiFingerprint final
    {
      public:
        constexpr AbiFingerprint() noexcept = default;

        constexpr explicit AbiFingerprint(
            algorithm::Sha256Digest digest
        ) noexcept
            : digest_(digest)
        {
        }

        [[nodiscard]] static constexpr AbiFingerprint fromCanonical(
            std::string_view canonical_fields
        ) noexcept
        {
            return AbiFingerprint{
                algorithm::Sha256::hash(canonical_fields)
            };
        }

        [[nodiscard]] constexpr const algorithm::Sha256Digest& digest() const noexcept
        {
            return digest_;
        }

        [[nodiscard]] constexpr auto operator<=>(
            const AbiFingerprint&
        ) const noexcept = default;

      private:
        algorithm::Sha256Digest digest_{};
    };
} // namespace lux::cxx
