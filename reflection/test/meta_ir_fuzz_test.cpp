#include <lux/cxx/reflection/runtime/MetaIrBinary.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace ir = lux::cxx::reflection::ir;

namespace
{
    template<typename Condition>
    void require(Condition&& condition)
    {
        if (!condition)
        {
            std::abort();
        }
    }

    constexpr std::uint64_t next(std::uint64_t& state) noexcept
    {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        return state;
    }

    std::vector<std::byte> validUnit()
    {
        ir::MetaUnitBuilder builder;
        const auto root = builder.addNode(ir::EMetaNodeKind::NAMESPACE, "lux");
        require(root);
        const auto record = builder.addNode(
            ir::EMetaNodeKind::RECORD,
            "RobotState",
            *root
        );
        require(record);
        require(builder.addAttribute(*record, "stable", "true"));
        const auto unit = std::move(builder).freeze();
        lux::cxx::BinaryVectorWriter writer;
        require(ir::writeMetaUnitBinary(writer, unit));
        return std::move(writer).take();
    }
}

int main()
{
    std::uint64_t random = 0x98fe'1234'5678'abcdULL;
    const auto canonical = validUnit();
    const ir::MetaIrReadLimits limits{256, 256, 256, 4096};

    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        auto mutated = canonical;
        const auto mutation_count = 1U + static_cast<unsigned>(next(random) % 4U);
        for (unsigned mutation = 0; mutation < mutation_count; ++mutation)
        {
            const auto index = static_cast<std::size_t>(next(random) % mutated.size());
            mutated[index] = std::byte(static_cast<unsigned char>(next(random)));
        }
        if ((next(random) & 3U) == 0)
        {
            mutated.resize(static_cast<std::size_t>(next(random) % (mutated.size() + 1)));
        }

        const auto result = ir::readMetaUnitBinary(mutated, limits);
        if (result)
        {
            lux::cxx::BinaryVectorWriter writer;
            require(ir::writeMetaUnitBinary(writer, *result));
            const auto reparsed = ir::readMetaUnitBinary(writer.data(), limits);
            require(reparsed);
        }
    }

    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        std::array<std::byte, 128> bytes{};
        const auto size = static_cast<std::size_t>(next(random) % (bytes.size() + 1));
        for (std::size_t index = 0; index < size; ++index)
        {
            bytes[index] = std::byte(static_cast<unsigned char>(next(random)));
        }
        static_cast<void>(ir::readMetaUnitBinary(
            std::span<const std::byte>(bytes).first(size),
            limits
        ));
    }
}
