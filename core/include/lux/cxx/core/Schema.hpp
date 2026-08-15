#pragma once

#include <lux/cxx/core/StableNameId.hpp>

#include <compare>
#include <cstdint>

namespace lux::cxx
{
    struct SchemaVersion final
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;

        [[nodiscard]] constexpr bool isCompatibleWith(SchemaVersion required) const noexcept
        {
            return major == required.major && minor >= required.minor;
        }

        [[nodiscard]] constexpr auto operator<=>(const SchemaVersion& other) const noexcept = default;
    };

    template <typename Tag> using SchemaId     = StableNameId<Tag>;
    template <typename Tag> using SchemaIdView = StableNameIdView<Tag>;
} // namespace lux::cxx
