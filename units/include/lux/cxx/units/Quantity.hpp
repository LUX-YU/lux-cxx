#pragma once

#include <lux/cxx/compile_time/expected.hpp>
#include <lux/cxx/core/CheckedArithmetic.hpp>

#include <compare>
#include <concepts>
#include <limits>
#include <ratio>
#include <type_traits>

namespace lux::cxx
{
    template <typename Tag, typename Rep, typename Ratio = std::ratio<1>>
    requires std::is_arithmetic_v<Rep>
    class Quantity final
    {
      public:
        using tag_type = Tag;
        using rep_type = Rep;
        using ratio = Ratio;

        constexpr Quantity() noexcept = default;
        constexpr explicit Quantity(Rep value) noexcept
            : value_(value)
        {
        }

        [[nodiscard]] constexpr Rep value() const noexcept
        {
            return value_;
        }

        constexpr Quantity& operator+=(Quantity other) noexcept
        {
            value_ += other.value_;
            return *this;
        }

        constexpr Quantity& operator-=(Quantity other) noexcept
        {
            value_ -= other.value_;
            return *this;
        }

        [[nodiscard]] constexpr auto operator<=>(
            const Quantity&
        ) const noexcept = default;

        friend constexpr Quantity operator+(
            Quantity left,
            Quantity right
        ) noexcept
        {
            return left += right;
        }

        friend constexpr Quantity operator-(
            Quantity left,
            Quantity right
        ) noexcept
        {
            return left -= right;
        }

      private:
        Rep value_{};
    };

    template <typename Target, typename Tag, typename Rep, typename Ratio>
    [[nodiscard]] constexpr expected<Target, EArithmeticError> quantityCast(
        Quantity<Tag, Rep, Ratio> source
    ) noexcept
    {
        static_assert(std::same_as<typename Target::tag_type, Tag>);
        using Conversion = std::ratio_divide<Ratio, typename Target::ratio>;
        using TargetRep = typename Target::rep_type;
        const long double converted =
            static_cast<long double>(source.value()) *
            static_cast<long double>(Conversion::num) /
            static_cast<long double>(Conversion::den);

        constexpr auto kLowest = static_cast<long double>(
            (std::numeric_limits<TargetRep>::lowest)()
        );
        constexpr auto kMax = static_cast<long double>(
            (std::numeric_limits<TargetRep>::max)()
        );
        if (converted < kLowest || converted > kMax)
        {
            return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
        }

        const auto narrowed = static_cast<TargetRep>(converted);
        if constexpr (std::integral<TargetRep>)
        {
            if (static_cast<long double>(narrowed) != converted)
            {
                return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
            }
        }
        return Target(narrowed);
    }
} // namespace lux::cxx
