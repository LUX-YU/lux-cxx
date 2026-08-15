#pragma once

#include <concepts>
#include <type_traits>

namespace lux::cxx
{
    template <typename Enum>
    struct enable_enum_flags : std::false_type
    {
    };

    template <typename Enum>
    concept enum_flags_enabled =
        std::is_enum_v<Enum> && enable_enum_flags<Enum>::value;

    template <enum_flags_enabled Enum>
    class EnumFlags final
    {
      public:
        using enum_type = Enum;
        using underlying_type = std::underlying_type_t<Enum>;

        constexpr EnumFlags() noexcept = default;
        constexpr EnumFlags(Enum value) noexcept
            : bits_(static_cast<underlying_type>(value))
        {
        }

        [[nodiscard]] static constexpr EnumFlags fromBits(
            underlying_type bits
        ) noexcept
        {
            EnumFlags result;
            result.bits_ = bits;
            return result;
        }

        [[nodiscard]] constexpr underlying_type bits() const noexcept
        {
            return bits_;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return bits_ == underlying_type{};
        }

        [[nodiscard]] constexpr bool containsAll(EnumFlags other) const noexcept
        {
            return (bits_ & other.bits_) == other.bits_;
        }

        [[nodiscard]] constexpr bool containsAny(EnumFlags other) const noexcept
        {
            return (bits_ & other.bits_) != underlying_type{};
        }

        [[nodiscard]] constexpr EnumFlags withoutBitsIn(
            EnumFlags other
        ) const noexcept
        {
            return fromBits(bits_ & static_cast<underlying_type>(~other.bits_));
        }

        constexpr EnumFlags& operator|=(EnumFlags other) noexcept
        {
            bits_ |= other.bits_;
            return *this;
        }

        constexpr EnumFlags& operator&=(EnumFlags other) noexcept
        {
            bits_ &= other.bits_;
            return *this;
        }

        constexpr EnumFlags& operator^=(EnumFlags other) noexcept
        {
            bits_ ^= other.bits_;
            return *this;
        }

        [[nodiscard]] constexpr bool operator==(const EnumFlags&) const noexcept = default;

        friend constexpr EnumFlags operator|(EnumFlags left, EnumFlags right) noexcept
        {
            return left |= right;
        }

        friend constexpr EnumFlags operator&(EnumFlags left, EnumFlags right) noexcept
        {
            return left &= right;
        }

        friend constexpr EnumFlags operator^(EnumFlags left, EnumFlags right) noexcept
        {
            return left ^= right;
        }

        friend constexpr EnumFlags operator~(EnumFlags value) noexcept
        {
            return fromBits(static_cast<underlying_type>(~value.bits_));
        }

      private:
        underlying_type bits_{};
    };

    template <enum_flags_enabled Enum>
    [[nodiscard]] constexpr EnumFlags<Enum> operator|(Enum left, Enum right) noexcept
    {
        return EnumFlags<Enum>(left) | EnumFlags<Enum>(right);
    }

    template <enum_flags_enabled Enum>
    [[nodiscard]] constexpr EnumFlags<Enum> operator&(Enum left, Enum right) noexcept
    {
        return EnumFlags<Enum>(left) & EnumFlags<Enum>(right);
    }
} // namespace lux::cxx
