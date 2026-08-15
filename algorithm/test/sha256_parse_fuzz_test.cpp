#include <lux/cxx/algorithm/sha256.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace
{
    constexpr std::uint32_t next(std::uint32_t& state) noexcept
    {
        state = state * 1664525U + 1013904223U;
        return state;
    }
}

int main()
{
    std::uint32_t random = 0x1234abcdU;
    std::array<char, 80> text{};
    constexpr std::string_view alphabet = "0123456789abcdefABCDEFxyz-";
    for (std::size_t iteration = 0; iteration < 20000; ++iteration)
    {
        const auto size = static_cast<std::size_t>(next(random) % (text.size() + 1));
        bool valid = size == lux::cxx::algorithm::Sha256Digest::hex_size;
        for (std::size_t index = 0; index < size; ++index)
        {
            text[index] = alphabet[next(random) % alphabet.size()];
            const char character = text[index];
            valid = valid && (
                (character >= '0' && character <= '9')
                || (character >= 'a' && character <= 'f')
                || (character >= 'A' && character <= 'F')
            );
        }
        const auto parsed = lux::cxx::algorithm::Sha256Digest::fromHex(
            std::string_view(text.data(), size)
        );
        assert(parsed.has_value() == valid);
        if (parsed)
        {
            std::array<char, lux::cxx::algorithm::Sha256Digest::hex_size> encoded{};
            parsed->formatHex(encoded);
            const auto reparsed = lux::cxx::algorithm::Sha256Digest::fromHex(
                std::string_view(encoded.data(), encoded.size())
            );
            assert(reparsed && *reparsed == *parsed);
        }
    }
}
