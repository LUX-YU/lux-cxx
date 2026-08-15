#pragma once

#include <lux/cxx/binary/BinaryError.hpp>
#include <lux/cxx/binary/BinaryReader.hpp>

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lux::cxx
{
    template <typename EndianPolicy = LittleEndian>
    class BasicBinarySpanWriter final
    {
      public:
        constexpr explicit BasicBinarySpanWriter(
            std::span<std::byte> output
        ) noexcept
            : output_(output)
        {
        }

        template <std::unsigned_integral Value>
        constexpr bool writeUnsigned(Value value) noexcept
        {
            if (!prepare(sizeof(Value), "unsigned integer")) return false;
            for (std::size_t index = 0; index < sizeof(Value); ++index)
            {
                const auto source_index =
                    EndianPolicy::least_significant_byte_first
                        ? index
                        : sizeof(Value) - 1 - index;
                output_[offset_ + index] = std::byte(
                    static_cast<unsigned char>(
                        value >> (source_index * 8U)
                    )
                );
            }
            offset_ += sizeof(Value);
            return true;
        }

        template <std::signed_integral Value>
        constexpr bool writeSigned(Value value) noexcept
        {
            return writeUnsigned(std::bit_cast<std::make_unsigned_t<Value>>(value));
        }

        template <binary_floating_point Value>
        constexpr bool writeFloat(
            Value value,
            EFloatingPointPolicy policy = EFloatingPointPolicy::CANONICAL
        ) noexcept
        {
            using Bits = std::conditional_t<
                sizeof(Value) == sizeof(std::uint32_t),
                std::uint32_t,
                std::uint64_t
            >;
            auto bits = std::bit_cast<Bits>(value);
            if (policy == EFloatingPointPolicy::CANONICAL)
            {
                bits = binary_detail::canonicalFloatBits(bits);
            }
            return writeUnsigned(bits);
        }

        constexpr bool writeBool(bool value) noexcept
        {
            return writeUnsigned<std::uint8_t>(value ? 1 : 0);
        }

        constexpr bool writeBytes(std::span<const std::byte> bytes) noexcept
        {
            if (!prepare(bytes.size(), "byte sequence")) return false;
            for (const auto byte : bytes) output_[offset_++] = byte;
            return true;
        }

        constexpr bool writeString(std::string_view text) noexcept
        {
            if (!prepare(text.size(), "string")) return false;
            for (const char character : text)
            {
                output_[offset_++] = std::byte(
                    static_cast<unsigned char>(character)
                );
            }
            return true;
        }

        constexpr bool writeVarUint(std::uint64_t value) noexcept
        {
            const auto size = BasicBinaryReader<EndianPolicy>::varUintSize(value);
            if (!prepare(size, "varuint")) return false;
            do
            {
                auto byte = static_cast<std::uint8_t>(value & 0x7fU);
                value >>= 7U;
                if (value != 0) byte |= 0x80U;
                output_[offset_++] = std::byte(byte);
            } while (value != 0);
            return true;
        }

        constexpr bool writeVarInt(std::int64_t value) noexcept
        {
            const auto encoded =
                (static_cast<std::uint64_t>(value) << 1U) ^
                static_cast<std::uint64_t>(-(value < 0));
            return writeVarUint(encoded);
        }

        [[nodiscard]] constexpr bool good() const noexcept
        {
            return !error_;
        }

        [[nodiscard]] constexpr const BinaryError& error() const noexcept
        {
            return error_;
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return offset_;
        }

        [[nodiscard]] constexpr std::span<const std::byte> written() const noexcept
        {
            return output_.first(offset_);
        }

      private:
        constexpr bool prepare(std::size_t count, std::string_view context) noexcept
        {
            if (error_) return false;
            if (count > output_.size() - offset_)
            {
                error_ = BinaryError{
                    EBinaryErrorCode::OUTPUT_EXHAUSTED,
                    offset_,
                    context
                };
                return false;
            }
            return true;
        }

        std::span<std::byte> output_;
        std::size_t offset_ = 0;
        BinaryError error_{};
    };

    template <
        typename Container = std::vector<std::byte>,
        typename EndianPolicy = LittleEndian
    >
    class BasicBinaryVectorWriter final
    {
      public:
        using container_type = Container;

        BasicBinaryVectorWriter() = default;

        explicit BasicBinaryVectorWriter(Container container)
            : output_(std::move(container))
        {
        }

        template <std::unsigned_integral Value>
        bool writeUnsigned(Value value)
        {
            std::array<std::byte, sizeof(Value)> bytes{};
            BasicBinarySpanWriter<EndianPolicy> writer(bytes);
            static_cast<void>(writer.writeUnsigned(value));
            return append(bytes);
        }

        template <std::signed_integral Value>
        bool writeSigned(Value value)
        {
            return writeUnsigned(std::bit_cast<std::make_unsigned_t<Value>>(value));
        }

        template <binary_floating_point Value>
        bool writeFloat(
            Value value,
            EFloatingPointPolicy policy = EFloatingPointPolicy::CANONICAL
        )
        {
            using Bits = std::conditional_t<
                sizeof(Value) == sizeof(std::uint32_t),
                std::uint32_t,
                std::uint64_t
            >;
            auto bits = std::bit_cast<Bits>(value);
            if (policy == EFloatingPointPolicy::CANONICAL)
            {
                bits = binary_detail::canonicalFloatBits(bits);
            }
            return writeUnsigned(bits);
        }

        bool writeBool(bool value)
        {
            return writeUnsigned<std::uint8_t>(value ? 1 : 0);
        }

        bool writeBytes(std::span<const std::byte> bytes)
        {
            return append(bytes);
        }

        bool writeString(std::string_view text)
        {
            output_.reserve(output_.size() + text.size());
            for (const char character : text)
            {
                output_.push_back(std::byte(
                    static_cast<unsigned char>(character)
                ));
            }
            return true;
        }

        bool writeVarUint(std::uint64_t value)
        {
            do
            {
                auto byte = static_cast<std::uint8_t>(value & 0x7fU);
                value >>= 7U;
                if (value != 0) byte |= 0x80U;
                output_.push_back(std::byte(byte));
            } while (value != 0);
            return true;
        }

        bool writeVarInt(std::int64_t value)
        {
            const auto encoded =
                (static_cast<std::uint64_t>(value) << 1U) ^
                static_cast<std::uint64_t>(-(value < 0));
            return writeVarUint(encoded);
        }

        [[nodiscard]] const Container& data() const noexcept
        {
            return output_;
        }

        [[nodiscard]] Container&& take() && noexcept
        {
            return std::move(output_);
        }

      private:
        bool append(std::span<const std::byte> bytes)
        {
            output_.insert(output_.end(), bytes.begin(), bytes.end());
            return true;
        }

        Container output_;
    };

    using BinarySpanWriter = BasicBinarySpanWriter<LittleEndian>;
    using BigEndianBinarySpanWriter = BasicBinarySpanWriter<BigEndian>;
    using BinaryVectorWriter = BasicBinaryVectorWriter<>;
} // namespace lux::cxx
