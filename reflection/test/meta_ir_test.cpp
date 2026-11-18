#include <lux/cxx/reflection/runtime/MetaIr.hpp>
#include <lux/cxx/reflection/runtime/MetaIrBinary.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

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

    static_assert(sizeof(ir::MetaNodeId) == sizeof(std::uint32_t));
    static_assert(sizeof(ir::MetaStringId) == sizeof(std::uint32_t));
    static_assert(ir::isDeclarationKind(ir::EMetaNodeKind::RECORD));
    static_assert(!ir::isDeclarationKind(ir::EMetaNodeKind::TYPE));
    static_assert(ir::metaSortKey(ir::EMetaNodeKind::FIELD, "value")
        == ir::metaSortKey(ir::EMetaNodeKind::FIELD, "value"));
    static_assert(ir::metaIndexCapacity(0) == 16);
    static_assert(ir::metaIndexCapacity(9) == 32);

    ir::MetaUnit buildUnit()
    {
        ir::MetaUnitBuilder builder;
        builder.reserve(3, 64, 1);
        const auto root = builder.addNode(
            ir::EMetaNodeKind::NAMESPACE,
            "lux"
        );
        require(root);
        const auto record = builder.addNode(
            ir::EMetaNodeKind::RECORD,
            "Vector",
            *root
        );
        require(record);
        const auto field = builder.addNode(
            ir::EMetaNodeKind::FIELD,
            "x",
            *record,
            7
        );
        require(field);
        require(builder.addAttribute(*record, "serializable", "true"));
        return std::move(builder).freeze();
    }
}

int main()
{
    const auto first = buildUnit();
    const auto second = buildUnit();
    require(first.nodes().size() == 3);
    require(first.attributes().size() == 1);
    require(first.storage().nodes == second.storage().nodes);
    require(first.storage().attributes == second.storage().attributes);
    require(first.contains(ir::MetaNodeId{2}));
    require(!first.contains(ir::MetaNodeId{3}));
    require(first.string(first.nodes()[1].name) == "Vector");
    require(first.nodes()[2].parent == ir::MetaNodeId{1});

    lux::cxx::BinaryVectorWriter writer;
    require(ir::writeMetaUnitBinary(writer, first));
    const auto second_writer = [&]
    {
        lux::cxx::BinaryVectorWriter output;
        require(ir::writeMetaUnitBinary(output, second));
        return std::move(output).take();
    }();
    require(writer.data() == second_writer);

    const auto decoded = ir::readMetaUnitBinary(writer.data());
    require(decoded);
    require(decoded->storage().nodes == first.storage().nodes);
    require(decoded->storage().attributes == first.storage().attributes);
    require(decoded->string(decoded->nodes()[1].name) == "Vector");

    auto corrupted = writer.data();
    corrupted[0] = std::byte{0};
    const auto bad_magic = ir::readMetaUnitBinary(corrupted);
    require(!bad_magic);
    require(bad_magic.error().code == ir::EMetaIrBinaryErrorCode::INVALID_MAGIC);

    auto version_one = writer.data();
    version_one[ir::kMetaIrMagic.size()] = std::byte{1};
    const auto unsupported_version = ir::readMetaUnitBinary(version_one);
    require(!unsupported_version);
    require(unsupported_version.error().code
        == ir::EMetaIrBinaryErrorCode::UNSUPPORTED_VERSION);

    for (std::size_t size = 0; size < writer.data().size(); ++size)
    {
        const auto truncated = ir::readMetaUnitBinary(
            std::span<const std::byte>(writer.data()).first(size)
        );
        require(!truncated);
    }

    ir::MetaUnitBuilder invalid_builder;
    const auto invalid = invalid_builder.addNode(
        ir::EMetaNodeKind::FIELD,
        "orphan",
        ir::MetaNodeId{8}
    );
    require(!invalid);
    require(invalid.error() == ir::EMetaIrError::INVALID_INDEX);
}
