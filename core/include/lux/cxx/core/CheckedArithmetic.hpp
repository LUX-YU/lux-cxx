#pragma once

#include <lux/cxx/compile_time/expected.hpp>

#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    enum class EArithmeticError : unsigned char
    {
        ARITHMETIC_OVERFLOW,
        VALUE_OUT_OF_RANGE,
        INVALID_ALIGNMENT,
    };

    template <std::integral Target, std::integral Source>
    [[nodiscard]] constexpr expected<Target, EArithmeticError> checkedNarrow(
        Source value
    ) noexcept
    {
        if (!std::in_range<Target>(value))
        {
            return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
        }
        return static_cast<Target>(value);
    }

    template <std::integral Result, std::integral Left, std::integral Right>
    [[nodiscard]] constexpr expected<Result, EArithmeticError> checkedAdd(
        Left left,
        Right right
    ) noexcept
    {
        const auto converted_left = checkedNarrow<Result>(left);
        const auto converted_right = checkedNarrow<Result>(right);
        if (!converted_left || !converted_right)
        {
            return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
        }

        const Result a = *converted_left;
        const Result b = *converted_right;
        constexpr Result kMax = (std::numeric_limits<Result>::max)();
        constexpr Result kMin = (std::numeric_limits<Result>::min)();

        if constexpr (std::is_unsigned_v<Result>)
        {
            if (b > kMax - a) return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
        }
        else
        {
            if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b))
            {
                return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
            }
        }
        return static_cast<Result>(a + b);
    }

    template <std::integral Value>
    [[nodiscard]] constexpr expected<Value, EArithmeticError> checkedAdd(
        Value left,
        Value right
    ) noexcept
    {
        return checkedAdd<Value, Value, Value>(left, right);
    }

    template <std::integral Result, std::integral Left, std::integral Right>
    [[nodiscard]] constexpr expected<Result, EArithmeticError> checkedSub(
        Left left,
        Right right
    ) noexcept
    {
        const auto converted_left = checkedNarrow<Result>(left);
        const auto converted_right = checkedNarrow<Result>(right);
        if (!converted_left || !converted_right)
        {
            return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
        }

        const Result a = *converted_left;
        const Result b = *converted_right;
        constexpr Result kMax = (std::numeric_limits<Result>::max)();
        constexpr Result kMin = (std::numeric_limits<Result>::min)();

        if constexpr (std::is_unsigned_v<Result>)
        {
            if (a < b) return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
        }
        else
        {
            if ((b > 0 && a < kMin + b) || (b < 0 && a > kMax + b))
            {
                return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
            }
        }
        return static_cast<Result>(a - b);
    }

    template <std::integral Value>
    [[nodiscard]] constexpr expected<Value, EArithmeticError> checkedSub(
        Value left,
        Value right
    ) noexcept
    {
        return checkedSub<Value, Value, Value>(left, right);
    }

    template <std::integral Result, std::integral Left, std::integral Right>
    [[nodiscard]] constexpr expected<Result, EArithmeticError> checkedMul(
        Left left,
        Right right
    ) noexcept
    {
        const auto converted_left = checkedNarrow<Result>(left);
        const auto converted_right = checkedNarrow<Result>(right);
        if (!converted_left || !converted_right)
        {
            return unexpected(EArithmeticError::VALUE_OUT_OF_RANGE);
        }

        const Result a = *converted_left;
        const Result b = *converted_right;
        constexpr Result kMax = (std::numeric_limits<Result>::max)();
        constexpr Result kMin = (std::numeric_limits<Result>::min)();

        if (a == 0 || b == 0) return Result{};
        if constexpr (std::is_unsigned_v<Result>)
        {
            if (a > kMax / b) return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
        }
        else
        {
            if ((a == -1 && b == kMin) || (b == -1 && a == kMin))
            {
                return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
            }
            if (a > 0)
            {
                if ((b > 0 && a > kMax / b) || (b < 0 && b < kMin / a))
                {
                    return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
                }
            }
            else
            {
                if ((b > 0 && a < kMin / b) || (b < 0 && a < kMax / b))
                {
                    return unexpected(EArithmeticError::ARITHMETIC_OVERFLOW);
                }
            }
        }
        return static_cast<Result>(a * b);
    }

    template <std::integral Value>
    [[nodiscard]] constexpr expected<Value, EArithmeticError> checkedMul(
        Value left,
        Value right
    ) noexcept
    {
        return checkedMul<Value, Value, Value>(left, right);
    }

    template <std::integral Value>
    [[nodiscard]] constexpr Value saturatingAdd(Value left, Value right) noexcept
    {
        const auto result = checkedAdd<Value, Value, Value>(left, right);
        if (result) return *result;
        if constexpr (std::is_unsigned_v<Value>)
        {
            return (std::numeric_limits<Value>::max)();
        }
        return right < 0
            ? (std::numeric_limits<Value>::min)()
            : (std::numeric_limits<Value>::max)();
    }

    template <std::unsigned_integral Value>
    [[nodiscard]] constexpr expected<Value, EArithmeticError> alignUpChecked(
        Value value,
        Value alignment
    ) noexcept
    {
        if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        {
            return unexpected(EArithmeticError::INVALID_ALIGNMENT);
        }
        const auto adjusted = checkedAdd<Value, Value, Value>(
            value,
            alignment - 1
        );
        if (!adjusted) return unexpected(adjusted.error());
        return static_cast<Value>(*adjusted & ~(alignment - 1));
    }
} // namespace lux::cxx
