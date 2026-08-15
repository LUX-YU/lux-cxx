#pragma once

#include <lux/cxx/binary/BinaryError.hpp>

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

namespace lux::cxx
{
    template<typename Value>
    concept binary_floating_point = std::floating_point<Value>
        && (sizeof(Value) == sizeof(std::uint32_t)
            || sizeof(Value) == sizeof(std::uint64_t));

    namespace binary_detail
    {
        template<std::unsigned_integral Bits>
        [[nodiscard]] constexpr Bits canonicalFloatBits(Bits bits) noexcept
        {
            constexpr Bits kSign = Bits{1} << (sizeof(Bits) * 8U - 1U);
            constexpr Bits kExponent = sizeof(Bits) == 4
                ? static_cast<Bits>(0x7f80'0000U)
                : static_cast<Bits>(0x7ff0'0000'0000'0000ULL);
            constexpr Bits kFraction = sizeof(Bits) == 4
                ? static_cast<Bits>(0x007f'ffffU)
                : static_cast<Bits>(0x000f'ffff'ffff'ffffULL);
            constexpr Bits kQuietNan = sizeof(Bits) == 4
                ? static_cast<Bits>(0x7fc0'0000U)
                : static_cast<Bits>(0x7ff8'0000'0000'0000ULL);

            if ((bits & ~kSign) == 0) return 0;
            if ((bits & kExponent) == kExponent && (bits & kFraction) != 0)
            {
                return kQuietNan;
            }
            return bits;
        }
    } // namespace binary_detail

    struct LittleEndian final
    {
        static constexpr bool least_significant_byte_first = true;
    };

    struct BigEndian final
    {
        static constexpr bool least_significant_byte_first = false;
    };

    template <typename EndianPolicy = LittleEndian>
    class BasicBinaryReader final
    {
      public:
        constexpr explicit BasicBinaryReader(
            std::span<const std::byte> input,
            std::size_t maximum_length = 64U * 1024U * 1024U
        ) noexcept
            : input_(input), maximum_length_(maximum_length)
        {
        }

        template <std::unsigned_integral Value>
        constexpr bool readUnsigned(Value& output) noexcept
        {
            if (!prepare(sizeof(Value), "unsigned integer")) return false;
            Value value = 0;
            if constexpr (EndianPolicy::least_significant_byte_first)
            {
                for (std::size_t index = 0; index < sizeof(Value); ++index)
                {
                    value |= static_cast<Value>(
                        std::to_integer<unsigned char>(input_[offset_ + index])
                    ) << (index * 8U);
                }
            }
            else
            {
                for (std::size_t index = 0; index < sizeof(Value); ++index)
                {
                    value = static_cast<Value>(
                        (value << 8U) |
                        std::to_integer<unsigned char>(input_[offset_ + index])
                    );
                }
            }
            offset_ += sizeof(Value);
            output = value;
            return true;
        }

        template <std::signed_integral Value>
        constexpr bool readSigned(Value& output) noexcept
        {
            using Unsigned = std::make_unsigned_t<Value>;
            Unsigned bits = 0;
            if (!readUnsigned(bits)) return false;
            output = std::bit_cast<Value>(bits);
            return true;
        }

        template <binary_floating_point Value>
        constexpr bool readFloat(
            Value& output,
            EFloatingPointPolicy policy = EFloatingPointPolicy::CANONICAL
        ) noexcept
        {
            using Bits = std::conditional_t<
                sizeof(Value) == sizeof(std::uint32_t),
                std::uint32_t,
                std::uint64_t
            >;
            Bits bits = 0;
            if (!readUnsigned(bits)) return false;
            if (policy == EFloatingPointPolicy::CANONICAL
                && bits != binary_detail::canonicalFloatBits(bits))
            {
                fail(EBinaryErrorCode::NON_CANONICAL_VALUE, "floating point");
                return false;
            }
            output = std::bit_cast<Value>(bits);
            return true;
        }

        constexpr bool readBool(bool& output) noexcept
        {
            std::uint8_t value = 0;
            if (!readUnsigned(value)) return false;
            if (value > 1)
            {
                fail(EBinaryErrorCode::INVALID_VALUE, "boolean");
                return false;
            }
            output = value != 0;
            return true;
        }

        constexpr bool readBytes(std::size_t count, std::span<const std::byte>& output ) noexcept
        {
            if (count > maximum_length_)
            {
                fail(EBinaryErrorCode::LENGTH_LIMIT_EXCEEDED, "byte sequence");
                return false;
            }
            if (!prepare(count, "byte sequence")) return false;
            output = input_.subspan(offset_, count);
            offset_ += count;
            return true;
        }

        constexpr bool readString(std::size_t count, std::string_view& output) noexcept
        {
            std::span<const std::byte> bytes;
            if (!readBytes(count, bytes)) return false;
            output = std::string_view(
                reinterpret_cast<const char*>(bytes.data()),
                bytes.size()
            );
            return true;
        }

        constexpr bool readVarUint(std::uint64_t& output) noexcept
        {
            if (error_) return false;
            std::uint64_t value = 0;
            std::size_t count = 0;
            for (; count < 10; ++count)
            {
                if (offset_ >= input_.size())
                {
                    fail(EBinaryErrorCode::INPUT_EXHAUSTED, "varuint");
                    return false;
                }
                const auto byte = std::to_integer<std::uint8_t>(input_[offset_++]);
                if (count == 9 && byte > 1)
                {
                    fail(EBinaryErrorCode::INVALID_VALUE, "varuint overflow");
                    return false;
                }
                value |= static_cast<std::uint64_t>(byte & 0x7fU) << (count * 7U);
                if ((byte & 0x80U) == 0)
                {
                    ++count;
                    if (count != varUintSize(value))
                    {
                        fail(
                            EBinaryErrorCode::NON_CANONICAL_VALUE,
                            "varuint"
                        );
                        return false;
                    }
                    output = value;
                    return true;
                }
            }
            fail(EBinaryErrorCode::INVALID_VALUE, "varuint overflow");
            return false;
        }

        constexpr bool readVarInt(std::int64_t& output) noexcept
        {
            std::uint64_t encoded = 0;
            if (!readVarUint(encoded)) return false;
            output = static_cast<std::int64_t>(encoded >> 1U) ^
                     -static_cast<std::int64_t>(encoded & 1U);
            return true;
        }

        constexpr bool requireFullyConsumed() noexcept
        {
            if (error_) return false;
            if (offset_ != input_.size())
            {
                fail(EBinaryErrorCode::TRAILING_DATA, "input");
                return false;
            }
            return true;
        }

        [[nodiscard]] constexpr bool good() const noexcept
        {
            return !error_;
        }

        [[nodiscard]] constexpr const BinaryError& error() const noexcept
        {
            return error_;
        }

        [[nodiscard]] constexpr std::size_t offset() const noexcept
        {
            return offset_;
        }

        [[nodiscard]] constexpr std::size_t remaining() const noexcept
        {
            return input_.size() - offset_;
        }

        [[nodiscard]] static constexpr std::size_t varUintSize(
            std::uint64_t value
        ) noexcept
        {
            std::size_t size = 1;
            while (value >= 0x80U)
            {
                value >>= 7U;
                ++size;
            }
            return size;
        }

      private:
        constexpr bool prepare(std::size_t count, std::string_view context) noexcept
        {
            if (error_) return false;
            if (count > input_.size() - offset_)
            {
                fail(EBinaryErrorCode::INPUT_EXHAUSTED, context);
                return false;
            }
            return true;
        }

        constexpr void fail(EBinaryErrorCode code, std::string_view context) noexcept
        {
            if (!error_) error_ = BinaryError{code, offset_, context};
        }

        std::span<const std::byte>  input_;
        std::size_t                 maximum_length_;
        std::size_t                 offset_ = 0;
        BinaryError                 error_{};
    };

    using BinaryReader          = BasicBinaryReader<LittleEndian>;
    using BigEndianBinaryReader = BasicBinaryReader<BigEndian>;
} // namespace lux::cxx
