#pragma once

#include <cstddef>
#include <string_view>

namespace lux::cxx
{
    enum class EFloatingPointPolicy : unsigned char
    {
        CANONICAL,
        PRESERVE_BITS,
    };

    enum class EBinaryErrorCode : unsigned char
    {
        NONE,
        INPUT_EXHAUSTED,
        OUTPUT_EXHAUSTED,
        INVALID_VALUE,
        NON_CANONICAL_VALUE,
        LENGTH_LIMIT_EXCEEDED,
        TRAILING_DATA,
    };

    struct BinaryError final
    {
        EBinaryErrorCode code = EBinaryErrorCode::NONE;
        std::size_t offset = 0;
        std::string_view context;

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code != EBinaryErrorCode::NONE;
        }

        [[nodiscard]] constexpr bool operator==(
            const BinaryError&
        ) const noexcept = default;
    };
} // namespace lux::cxx
