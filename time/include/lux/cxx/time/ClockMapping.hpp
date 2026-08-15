#pragma once

#include <lux/cxx/compile_time/expected.hpp>
#include <lux/cxx/time/Timestamp.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <limits>

namespace lux::cxx
{
    enum class EClockMappingError : unsigned char
    {
        INVALID_SCALE,
        RESULT_OUT_OF_RANGE,
    };

    template <
        typename FromDomain,
        typename ToDomain,
        std::floating_point ScaleRep = double,
        typename Duration = std::chrono::nanoseconds,
        std::integral Rep = std::int64_t
    >
    class ClockMapping final
    {
      public:
        using from_timestamp = Timestamp<FromDomain, Duration, Rep>;
        using to_timestamp   = Timestamp<ToDomain, Duration, Rep>;
        using duration_type  = typename from_timestamp::duration_type;

        constexpr ClockMapping() noexcept = default;

        constexpr ClockMapping(
            from_timestamp from_origin,
            to_timestamp to_origin,
            ScaleRep scale,
            duration_type uncertainty = duration_type{},
            std::uint64_t revision = 0
        ) noexcept
            : from_origin_(from_origin),
              to_origin_(to_origin),
              scale_(scale),
              uncertainty_(uncertainty),
              revision_(revision)
        {
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            constexpr auto kMax = (std::numeric_limits<ScaleRep>::max)();
            return scale_ > ScaleRep{} && scale_ == scale_ && scale_ <= kMax;
        }

        [[nodiscard]] constexpr expected<
            to_timestamp,
            EClockMappingError
        > map(from_timestamp timestamp) const noexcept
        {
            if (!isValid())
            {
                return unexpected(EClockMappingError::INVALID_SCALE);
            }
            const auto delta = static_cast<long double>(timestamp.ticks()) -
                               static_cast<long double>(from_origin_.ticks());
            const auto scaled = delta * static_cast<long double>(scale_);
            const auto mapped = scaled >= 0.0L ? scaled + 0.5L : scaled - 0.5L;
            constexpr auto kMin = static_cast<long double>(
                (std::numeric_limits<Rep>::min)()
            );
            constexpr auto kMax = static_cast<long double>(
                (std::numeric_limits<Rep>::max)()
            );
            const auto origin = static_cast<long double>(to_origin_.ticks());
            if (mapped < kMin - origin || mapped > kMax - origin)
            {
                return unexpected(EClockMappingError::RESULT_OUT_OF_RANGE);
            }
            return to_timestamp(
                static_cast<Rep>(origin + mapped)
            );
        }

        [[nodiscard]] constexpr from_timestamp fromOrigin() const noexcept
        {
            return from_origin_;
        }

        [[nodiscard]] constexpr to_timestamp toOrigin() const noexcept
        {
            return to_origin_;
        }

        [[nodiscard]] constexpr ScaleRep scale() const noexcept
        {
            return scale_;
        }

        [[nodiscard]] constexpr duration_type uncertainty() const noexcept
        {
            return uncertainty_;
        }

        [[nodiscard]] constexpr std::uint64_t revision() const noexcept
        {
            return revision_;
        }

      private:
        from_timestamp from_origin_{};
        to_timestamp to_origin_{};
        ScaleRep scale_ = ScaleRep{1};
        duration_type uncertainty_{};
        std::uint64_t revision_ = 0;
    };
} // namespace lux::cxx
