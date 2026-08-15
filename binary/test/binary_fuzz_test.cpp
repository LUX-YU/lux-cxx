#include <lux/cxx/binary/Binary.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace
{
    constexpr std::uint64_t next(std::uint64_t& state) noexcept
    {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        return state;
    }
}

int main()
{
    std::uint64_t random = 0x5eed'1234'9876'abcdULL;
    for (std::size_t iteration = 0; iteration < 20000; ++iteration)
    {
        std::array<std::byte, 32> input{};
        const auto size = static_cast<std::size_t>(next(random) % (input.size() + 1));
        for (std::size_t index = 0; index < size; ++index)
        {
            input[index] = std::byte(static_cast<unsigned char>(next(random)));
        }

        lux::cxx::BinaryReader reader(
            std::span<const std::byte>(input).first(size),
            16
        );
        std::uint64_t value = 0;
        const bool decoded = reader.readVarUint(value);
        assert(reader.offset() <= size);
        if (decoded)
        {
            std::array<std::byte, 10> encoded{};
            lux::cxx::BinarySpanWriter writer(encoded);
            assert(writer.writeVarUint(value));
            assert(writer.size() == reader.offset());
            for (std::size_t index = 0; index < writer.size(); ++index)
            {
                assert(writer.written()[index] == input[index]);
            }
        }
        else
        {
            const auto sticky_error = reader.error();
            bool boolean = false;
            assert(!reader.readBool(boolean));
            assert(reader.error() == sticky_error);
        }

        lux::cxx::BinaryReader length_reader(
            std::span<const std::byte>(input).first(size),
            16
        );
        std::span<const std::byte> bytes;
        const auto requested = static_cast<std::size_t>(next(random) % 40);
        const bool read = length_reader.readBytes(requested, bytes);
        assert(length_reader.offset() <= size);
        if (read)
        {
            assert(bytes.size() == requested);
        }
    }
}
