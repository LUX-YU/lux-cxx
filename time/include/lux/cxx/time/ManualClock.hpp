#pragma once

#include <lux/cxx/time/Timestamp.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>

namespace lux::cxx
{
    template <
        typename Domain,
        typename Duration = std::chrono::nanoseconds,
        std::integral Rep = std::int64_t
    >
    class ManualClock final
    {
      public:
        using timestamp_type = Timestamp<Domain, Duration, Rep>;
        using duration_type = typename timestamp_type::duration_type;

        constexpr ManualClock() noexcept = default;
        constexpr explicit ManualClock(timestamp_type initial) noexcept
            : now_(initial)
        {
        }

        [[nodiscard]] constexpr timestamp_type now() const noexcept
        {
            return now_;
        }

        constexpr void set(timestamp_type value) noexcept
        {
            now_ = value;
        }

        constexpr timestamp_type advance(duration_type duration) noexcept
        {
            now_ += duration;
            return now_;
        }

      private:
        timestamp_type now_{};
    };
} // namespace lux::cxx
