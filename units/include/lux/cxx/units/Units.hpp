#pragma once

#include <lux/cxx/units/Quantity.hpp>

#include <cstdint>
#include <ratio>

namespace lux::cxx
{
    struct ByteCountTag final{};
    struct FrequencyTag final{};
    struct AngleTag final{};

    template <typename Rep = std::uint64_t, typename Ratio = std::ratio<1>>
    using ByteCount = Quantity<ByteCountTag, Rep, Ratio>;

    template <typename Rep = double, typename Ratio = std::ratio<1>>
    using Frequency = Quantity<FrequencyTag, Rep, Ratio>;

    template <typename Rep = double>
    using Angle = Quantity<AngleTag, Rep, std::ratio<1>>;

    template <typename Rep = std::uint64_t>
    using KibibyteCount = ByteCount<Rep, std::ratio<1024>>;

    template <typename Rep = double>
    using Kilohertz = Frequency<Rep, std::kilo>;

    template <typename Rep>
    [[nodiscard]] constexpr Angle<Rep> degreesToRadians(Rep degrees) noexcept
    {
        constexpr long double kPi = 3.141592653589793238462643383279502884L;
        return Angle<Rep>(static_cast<Rep>(
            static_cast<long double>(degrees) * kPi / 180.0L
        ));
    }

    template <typename Rep>
    [[nodiscard]] constexpr Rep radiansToDegrees(Angle<Rep> radians) noexcept
    {
        constexpr long double kPi = 3.141592653589793238462643383279502884L;
        return static_cast<Rep>(
            static_cast<long double>(radians.value()) * 180.0L / kPi
        );
    }
} // namespace lux::cxx
