#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace lux::cxx::algorithm
{
    using Sha256Digest = std::array<std::byte, 32>;

    class Sha256 final
    {
      public:
        constexpr Sha256() noexcept { reset(); }

        constexpr void reset() noexcept
        {
            state_ = {
                0x6a09e667U,
                0xbb67ae85U,
                0x3c6ef372U,
                0xa54ff53aU,
                0x510e527fU,
                0x9b05688cU,
                0x1f83d9abU,
                0x5be0cd19U,
            };
            buffered_ = 0;
            total_bytes_ = 0;
            buffer_.fill(std::byte{0});
        }

        constexpr void update(std::span<const std::byte> bytes) noexcept
        {
            total_bytes_ += bytes.size();

            if (buffered_ != 0)
            {
                const auto count = min(bytes.size(), buffer_.size() - buffered_);
                copy(bytes.first(count), buffer_.begin() + buffered_);
                buffered_ += count;
                bytes = bytes.subspan(count);
                if (buffered_ == buffer_.size())
                {
                    transform(buffer_);
                    buffered_ = 0;
                }
                else
                {
                    return;
                }
            }

            while (bytes.size() >= buffer_.size())
            {
                std::array<std::byte, 64> block{};
                copy(bytes.first(block.size()), block.begin());
                transform(block);
                bytes = bytes.subspan(block.size());
            }

            copy(bytes, buffer_.begin());
            buffered_ = bytes.size();
        }

        constexpr void update(std::string_view text) noexcept
        {
            for (const char character : text)
            {
                const std::byte value{
                    static_cast<unsigned char>(character)
                };
                update(std::span<const std::byte>{&value, 1});
            }
        }

        [[nodiscard]] constexpr Sha256Digest digest() const noexcept
        {
            Sha256 copy = *this;
            return copy.finish();
        }

        [[nodiscard]] static constexpr Sha256Digest hash(
            std::span<const std::byte> bytes
        ) noexcept
        {
            Sha256 hasher;
            hasher.update(bytes);
            return hasher.digest();
        }

        [[nodiscard]] static constexpr Sha256Digest hash(
            std::string_view text
        ) noexcept
        {
            Sha256 hasher;
            hasher.update(text);
            return hasher.digest();
        }

      private:
        static constexpr std::array<std::uint32_t, 64> kRoundConstants{
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        };

        [[nodiscard]] static constexpr std::size_t min(
            const std::size_t left,
            const std::size_t right
        ) noexcept
        {
            return left < right ? left : right;
        }

        static constexpr void copy(
            std::span<const std::byte> source,
            std::array<std::byte, 64>::iterator destination
        ) noexcept
        {
            for (const std::byte value : source)
            {
                *destination = value;
                ++destination;
            }
        }

        [[nodiscard]] static constexpr std::uint32_t loadBigEndian(
            const std::byte* bytes
        ) noexcept
        {
            return (std::to_integer<std::uint32_t>(bytes[0]) << 24U) |
                   (std::to_integer<std::uint32_t>(bytes[1]) << 16U) |
                   (std::to_integer<std::uint32_t>(bytes[2]) << 8U) |
                   std::to_integer<std::uint32_t>(bytes[3]);
        }

        constexpr void transform(
            const std::array<std::byte, 64>& block
        ) noexcept
        {
            std::array<std::uint32_t, 64> words{};
            for (std::size_t index = 0; index < 16; ++index)
            {
                words[index] = loadBigEndian(block.data() + index * 4);
            }
            for (std::size_t index = 16; index < words.size(); ++index)
            {
                const auto s0 = std::rotr(words[index - 15], 7) ^
                                std::rotr(words[index - 15], 18) ^
                                (words[index - 15] >> 3U);
                const auto s1 = std::rotr(words[index - 2], 17) ^
                                std::rotr(words[index - 2], 19) ^
                                (words[index - 2] >> 10U);
                words[index] = words[index - 16] + s0 +
                               words[index - 7] + s1;
            }

            auto a = state_[0];
            auto b = state_[1];
            auto c = state_[2];
            auto d = state_[3];
            auto e = state_[4];
            auto f = state_[5];
            auto g = state_[6];
            auto h = state_[7];

            for (std::size_t index = 0; index < words.size(); ++index)
            {
                const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^
                                  std::rotr(e, 25);
                const auto choice = (e & f) ^ (~e & g);
                const auto temporary1 = h + sum1 + choice +
                                        kRoundConstants[index] + words[index];
                const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^
                                  std::rotr(a, 22);
                const auto majority = (a & b) ^ (a & c) ^ (b & c);
                const auto temporary2 = sum0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temporary1;
                d = c;
                c = b;
                b = a;
                a = temporary1 + temporary2;
            }

            state_[0] += a;
            state_[1] += b;
            state_[2] += c;
            state_[3] += d;
            state_[4] += e;
            state_[5] += f;
            state_[6] += g;
            state_[7] += h;
        }

        [[nodiscard]] constexpr Sha256Digest finish() noexcept
        {
            const auto message_bits = total_bytes_ * 8U;
            buffer_[buffered_++] = std::byte{0x80};
            if (buffered_ > 56)
            {
                while (buffered_ < buffer_.size())
                    buffer_[buffered_++] = std::byte{0};
                transform(buffer_);
                buffered_ = 0;
            }
            while (buffered_ < 56)
                buffer_[buffered_++] = std::byte{0};
            for (std::size_t index = 0; index < 8; ++index)
            {
                const auto shift = static_cast<unsigned>((7 - index) * 8);
                buffer_[56 + index] = std::byte{
                    static_cast<unsigned char>(message_bits >> shift)
                };
            }
            transform(buffer_);

            Sha256Digest output{};
            for (std::size_t index = 0; index < state_.size(); ++index)
            {
                output[index * 4] = std::byte{
                    static_cast<unsigned char>(state_[index] >> 24U)
                };
                output[index * 4 + 1] = std::byte{
                    static_cast<unsigned char>(state_[index] >> 16U)
                };
                output[index * 4 + 2] = std::byte{
                    static_cast<unsigned char>(state_[index] >> 8U)
                };
                output[index * 4 + 3] = std::byte{
                    static_cast<unsigned char>(state_[index])
                };
            }
            return output;
        }

        std::array<std::uint32_t, 8> state_{};
        std::array<std::byte, 64> buffer_{};
        std::size_t buffered_ = 0;
        std::uint64_t total_bytes_ = 0;
    };

    [[nodiscard]] inline std::string toHex(
        const Sha256Digest& digest
    )
    {
        constexpr std::string_view kDigits = "0123456789abcdef";
        std::string output;
        output.resize(digest.size() * 2);
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            const auto value = std::to_integer<unsigned char>(digest[index]);
            output[index * 2] = kDigits[value >> 4U];
            output[index * 2 + 1] = kDigits[value & 0x0fU];
        }
        return output;
    }
} // namespace lux::cxx::algorithm
