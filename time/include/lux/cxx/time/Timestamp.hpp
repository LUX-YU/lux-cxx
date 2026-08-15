#pragma once

#include <chrono>
#include <compare>
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace lux::cxx
{
    struct SteadyTimeDomain final{};
    struct SystemTimeDomain final{};
    template <typename Tag> struct SensorTimeDomain final{};
    template <typename Tag> struct RemoteTimeDomain final{};

    template <
        typename Domain,
        typename Duration = std::chrono::nanoseconds,
        typename Rep = typename Duration::rep
    >
    class Timestamp final
    {
      public:
        using domain_type = Domain;
        using rep_type    = Rep;
        using period      = typename Duration::period;
        using duration_type = std::chrono::duration<Rep, period>;

        constexpr Timestamp() noexcept = default;

        constexpr explicit Timestamp(Rep ticks) noexcept
            : ticks_(ticks)
        {
        }

        constexpr explicit Timestamp(duration_type duration) noexcept
            : ticks_(duration.count())
        {
        }

        [[nodiscard]] constexpr Rep ticks() const noexcept
        {
            return ticks_;
        }

        [[nodiscard]] constexpr duration_type timeSinceOrigin() const noexcept
        {
            return duration_type(ticks_);
        }

        constexpr Timestamp& operator+=(duration_type duration) noexcept
        {
            ticks_ += duration.count();
            return *this;
        }

        constexpr Timestamp& operator-=(duration_type duration) noexcept
        {
            ticks_ -= duration.count();
            return *this;
        }

        [[nodiscard]] constexpr auto operator<=>(const Timestamp&) const noexcept = default;

        friend constexpr Timestamp operator+(Timestamp timestamp, duration_type duration) noexcept
        {
            return timestamp += duration;
        }

        friend constexpr Timestamp operator-(Timestamp timestamp, duration_type duration) noexcept
        {
            return timestamp -= duration;
        }

        friend constexpr duration_type operator-(Timestamp left, Timestamp right) noexcept
        {
            return duration_type(left.ticks_ - right.ticks_);
        }

      private:
        Rep ticks_{};
    };
} // namespace lux::cxx
