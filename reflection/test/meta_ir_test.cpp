#include <lux/cxx/reflection/runtime/MetaIr.hpp>
#include <lux/cxx/reflection/runtime/MetaIrBinary.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace ir = lux::cxx::reflection::ir;

namespace
{
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
        assert(root);
        const auto record = builder.addNode(
            ir::EMetaNodeKind::RECORD,
            "Vector",
            *root
        );
        assert(record);
        const auto field = builder.addNode(
            ir::EMetaNodeKind::FIELD,
            "x",
            *record,
            7
        );
        assert(field);
        assert(builder.addAttribute(*record, "serializable", "true"));
        return std::move(builder).freeze();
    }
}

int main()
{
    const auto first = buildUnit();
    const auto second = buildUnit();
    assert(first.nodes().size() == 3);
    assert(first.attributes().size() == 1);
    assert(first.storage().nodes == second.storage().nodes);
    assert(first.storage().attributes == second.storage().attributes);
    assert(first.contains(ir::MetaNodeId{2}));
    assert(!first.contains(ir::MetaNodeId{3}));
    assert(first.string(first.nodes()[1].name) == "Vector");
    assert(first.nodes()[2].parent == ir::MetaNodeId{1});

    lux::cxx::BinaryVectorWriter writer;
    assert(ir::writeMetaUnitBinary(writer, first));
    const auto second_writer = [&]
    {
        lux::cxx::BinaryVectorWriter output;
        assert(ir::writeMetaUnitBinary(output, second));
        return std::move(output).take();
    }();
    assert(writer.data() == second_writer);

    const auto decoded = ir::readMetaUnitBinary(writer.data());
    assert(decoded);
    assert(decoded->storage().nodes == first.storage().nodes);
    assert(decoded->storage().attributes == first.storage().attributes);
    assert(decoded->string(decoded->nodes()[1].name) == "Vector");

    auto corrupted = writer.data();
    corrupted[0] = std::byte{0};
    const auto bad_magic = ir::readMetaUnitBinary(corrupted);
    assert(!bad_magic);
    assert(bad_magic.error().code == ir::EMetaIrBinaryErrorCode::INVALID_MAGIC);

    for (std::size_t size = 0; size < writer.data().size(); ++size)
    {
        const auto truncated = ir::readMetaUnitBinary(
            std::span<const std::byte>(writer.data()).first(size)
        );
        assert(!truncated);
    }

    ir::MetaUnitBuilder invalid_builder;
    const auto invalid = invalid_builder.addNode(
        ir::EMetaNodeKind::FIELD,
        "orphan",
        ir::MetaNodeId{8}
    );
    assert(!invalid);
    assert(invalid.error() == ir::EMetaIrError::INVALID_INDEX);
}
