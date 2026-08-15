#pragma once

#include <cstdint>

namespace lux::cxx
{
    enum class EScalarKind : std::uint8_t
    {
        BOOL = 1,
        I8 = 2,
        U8 = 3,
        I16 = 4,
        U16 = 5,
        I32 = 6,
        U32 = 7,
        I64 = 8,
        U64 = 9,
        F32 = 10,
        F64 = 11,
        UTF8 = 12,
        BYTES = 13,
    };

    struct ScalarSchema final
    {
        EScalarKind kind = EScalarKind::BYTES;
        std::uint8_t major = 1;
        std::uint8_t minor = 0;

        [[nodiscard]] constexpr bool isKnown() const noexcept
        {
            const auto value = static_cast<std::uint8_t>(kind);
            return value >= static_cast<std::uint8_t>(EScalarKind::BOOL) &&
                   value <= static_cast<std::uint8_t>(EScalarKind::BYTES);
        }

        [[nodiscard]] constexpr bool operator==(
            const ScalarSchema&
        ) const noexcept = default;
    };
} // namespace lux::cxx
