#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include <lux/cxx/binary/Binary.hpp>
#include <lux/cxx/compile_time/expected.hpp>
#include <lux/cxx/reflection/runtime/MetaIr.hpp>

namespace lux::cxx::reflection::ir
{
    enum class EMetaIrBinaryErrorCode : std::uint8_t
    {
        BINARY_ERROR,
        INVALID_MAGIC,
        UNSUPPORTED_VERSION,
        LIMIT_EXCEEDED,
        INVALID_REFERENCE,
        INVALID_NODE_KIND
    };

    struct MetaIrBinaryError final
    {
        EMetaIrBinaryErrorCode code = EMetaIrBinaryErrorCode::BINARY_ERROR;
        BinaryError binary_error{};
        std::size_t record_index = 0;
    };

    struct MetaIrReadLimits final
    {
        std::size_t max_nodes = 1'000'000;
        std::size_t max_strings = 1'000'000;
        std::size_t max_attributes = 2'000'000;
        std::size_t max_string_bytes = 64U * 1024U * 1024U;
    };

    inline constexpr std::array<std::byte, 4> kMetaIrMagic{
        std::byte{static_cast<unsigned char>('L')},
        std::byte{static_cast<unsigned char>('X')},
        std::byte{static_cast<unsigned char>('M')},
        std::byte{static_cast<unsigned char>('I')}
    };
    inline constexpr std::uint8_t kMetaIrBinaryVersion = 2;

    template<typename Writer, meta_unit_storage Storage>
    [[nodiscard]] bool writeMetaUnitBinary(
        Writer& writer,
        const BasicMetaUnit<Storage>& unit
    )
    {
        if (!writer.writeBytes(kMetaIrMagic)) return false;
        if (!writer.template writeUnsigned<std::uint8_t>(kMetaIrBinaryVersion))
        {
            return false;
        }

        const auto& storage = unit.storage();
        if (!writer.writeVarUint(storage.strings.size())
            || !writer.writeVarUint(storage.nodes.size())
            || !writer.writeVarUint(storage.attributes.size()))
        {
            return false;
        }

        for (std::size_t index = 0; index < storage.strings.size(); ++index)
        {
            const auto text = unit.string(
                MetaStringId{static_cast<std::uint32_t>(index)}
            );
            if (!writer.writeVarUint(text.size()) || !writer.writeString(text))
            {
                return false;
            }
        }

        for (const auto& node : unit.nodes())
        {
            const auto encoded_parent = node.parent.isValid()
                ? static_cast<std::uint64_t>(node.parent.value()) + 1U
                : 0U;
            if (!writer.template writeUnsigned<std::uint8_t>(
                    static_cast<std::uint8_t>(node.kind)
                )
                || !writer.writeVarUint(node.name.value())
                || !writer.writeVarUint(encoded_parent)
                || !writer.writeVarUint(node.payload_index)
                || !writer.writeVarUint(node.flags))
            {
                return false;
            }
        }

        for (const auto& attribute : unit.attributes())
        {
            if (!writer.writeVarUint(attribute.owner.value())
                || !writer.writeVarUint(attribute.name.value())
                || !writer.writeVarUint(attribute.value.value()))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] inline expected<MetaUnit, MetaIrBinaryError> readMetaUnitBinary(
        std::span<const std::byte> input,
        MetaIrReadLimits limits = {}
    )
    {
        BinaryReader reader(input, limits.max_string_bytes);
        auto binaryFailure = [&]() -> unexpected<MetaIrBinaryError>
        {
            return unexpected(MetaIrBinaryError{
                EMetaIrBinaryErrorCode::BINARY_ERROR,
                reader.error(),
                0
            });
        };

        std::span<const std::byte> magic;
        if (!reader.readBytes(kMetaIrMagic.size(), magic)) return binaryFailure();
        for (std::size_t index = 0; index < kMetaIrMagic.size(); ++index)
        {
            if (magic[index] != kMetaIrMagic[index])
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::INVALID_MAGIC,
                    {},
                    index
                });
            }
        }

        std::uint8_t version = 0;
        if (!reader.readUnsigned(version)) return binaryFailure();
        if (version != kMetaIrBinaryVersion)
        {
            return unexpected(MetaIrBinaryError{
                EMetaIrBinaryErrorCode::UNSUPPORTED_VERSION,
                {},
                version
            });
        }

        std::uint64_t string_count = 0;
        std::uint64_t node_count = 0;
        std::uint64_t attribute_count = 0;
        if (!reader.readVarUint(string_count)
            || !reader.readVarUint(node_count)
            || !reader.readVarUint(attribute_count))
        {
            return binaryFailure();
        }
        if (string_count > limits.max_strings
            || node_count > limits.max_nodes
            || attribute_count > limits.max_attributes
            || string_count > (std::numeric_limits<std::uint32_t>::max)()
            || node_count > (std::numeric_limits<std::uint32_t>::max)())
        {
            return unexpected(MetaIrBinaryError{
                EMetaIrBinaryErrorCode::LIMIT_EXCEEDED
            });
        }

        MetaUnitBuilder builder;
        builder.reserve(
            static_cast<std::size_t>(node_count),
            (std::min)(
                (std::min)(input.size(), limits.max_string_bytes),
                std::size_t{1024U * 1024U}
            ),
            static_cast<std::size_t>(attribute_count)
        );
        std::vector<MetaStringId> strings;
        std::vector<std::string_view> string_views;
        std::vector<MetaNodeId> nodes;
        strings.reserve(static_cast<std::size_t>(string_count));
        string_views.reserve(static_cast<std::size_t>(string_count));
        nodes.reserve(static_cast<std::size_t>(node_count));

        std::size_t total_string_bytes = 0;
        for (std::size_t index = 0; index < string_count; ++index)
        {
            std::uint64_t size = 0;
            if (!reader.readVarUint(size)) return binaryFailure();
            if (size > limits.max_string_bytes - total_string_bytes)
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::LIMIT_EXCEEDED,
                    {},
                    index
                });
            }
            std::string_view text;
            if (!reader.readString(static_cast<std::size_t>(size), text))
            {
                return binaryFailure();
            }
            total_string_bytes += static_cast<std::size_t>(size);
            auto id = builder.intern(text);
            if (!id)
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::LIMIT_EXCEEDED,
                    {},
                    index
                });
            }
            strings.push_back(*id);
            string_views.push_back(text);
        }

        for (std::size_t index = 0; index < node_count; ++index)
        {
            std::uint8_t raw_kind = 0;
            std::uint64_t raw_name = 0;
            std::uint64_t raw_parent = 0;
            std::uint64_t payload = 0;
            std::uint64_t flags = 0;
            if (!reader.readUnsigned(raw_kind)
                || !reader.readVarUint(raw_name)
                || !reader.readVarUint(raw_parent)
                || !reader.readVarUint(payload)
                || !reader.readVarUint(flags))
            {
                return binaryFailure();
            }
            if (raw_kind == 0
                || raw_kind > static_cast<std::uint8_t>(EMetaNodeKind::TYPE))
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::INVALID_NODE_KIND,
                    {},
                    index
                });
            }
            if (raw_name >= strings.size()
                || raw_parent > nodes.size()
                || payload > (std::numeric_limits<std::uint32_t>::max)()
                || flags > (std::numeric_limits<std::uint32_t>::max)())
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::INVALID_REFERENCE,
                    {},
                    index
                });
            }
            const auto parent = raw_parent == 0
                ? MetaNodeId::invalid()
                : nodes[static_cast<std::size_t>(raw_parent - 1)];
            auto id = builder.addNode(
                static_cast<EMetaNodeKind>(raw_kind),
                string_views[static_cast<std::size_t>(raw_name)],
                parent,
                static_cast<std::uint32_t>(payload),
                static_cast<std::uint32_t>(flags)
            );
            if (!id)
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::LIMIT_EXCEEDED,
                    {},
                    index
                });
            }
            nodes.push_back(*id);
        }

        // Attribute decoding is intentionally after all nodes so owner indexes
        // can be checked with one dense bound.
        for (std::size_t index = 0; index < attribute_count; ++index)
        {
            std::uint64_t owner = 0;
            std::uint64_t name = 0;
            std::uint64_t value = 0;
            if (!reader.readVarUint(owner)
                || !reader.readVarUint(name)
                || !reader.readVarUint(value))
            {
                return binaryFailure();
            }
            if (owner >= nodes.size() || name >= strings.size() || value >= strings.size())
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::INVALID_REFERENCE,
                    {},
                    index
                });
            }
            const auto added = builder.addAttribute(
                nodes[static_cast<std::size_t>(owner)],
                string_views[static_cast<std::size_t>(name)],
                string_views[static_cast<std::size_t>(value)]
            );
            if (!added)
            {
                return unexpected(MetaIrBinaryError{
                    EMetaIrBinaryErrorCode::INVALID_REFERENCE,
                    {},
                    index
                });
            }
        }

        if (!reader.requireFullyConsumed()) return binaryFailure();
        return std::move(builder).freeze();
    }
} // namespace lux::cxx::reflection::ir
