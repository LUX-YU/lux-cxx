#pragma once

#include <compare>
#include <concepts>
#include <limits>

namespace lux::cxx
{
    template <
        typename Tag,
        std::integral Rep,
        Rep InvalidValue = (std::numeric_limits<Rep>::max)()
    >
    class StrongId final
    {
      public:
        using tag_type = Tag;
        using rep_type = Rep;

        constexpr StrongId() noexcept = default;
        constexpr explicit StrongId(Rep value) noexcept
            : value_(value)
        {
        }

        [[nodiscard]] static constexpr StrongId invalid() noexcept
        {
            return StrongId{};
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return value_ != InvalidValue;
        }

        [[nodiscard]] constexpr Rep value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr auto operator<=>(const StrongId&) const noexcept = default;

      private:
        Rep value_ = InvalidValue;
    };
} // namespace lux::cxx
