#pragma once

#include <concepts>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <lux/cxx/compile_time/expected.hpp>
#include <lux/cxx/core/StableNameId.hpp>
#include <lux/cxx/core/StrongId.hpp>

namespace lux::cxx::reflection::ir
{
    enum class EMetaNodeKind : std::uint8_t
    {
        INVALID = 0,
        NAMESPACE = 1,
        RECORD = 2,
        ENUMERATION = 3,
        FUNCTION = 4,
        FIELD = 5,
        ENUMERATOR = 6,
        PARAMETER = 7,
        TYPE = 8
    };

    enum class EMetaIrError : std::uint8_t
    {
        INVALID_INDEX,
        SIZE_LIMIT_EXCEEDED
    };

    struct MetaNodeIdTag final
    {
    };

    struct MetaStringIdTag final
    {
    };

    using MetaNodeId = StrongId<MetaNodeIdTag, std::uint32_t>;
    using MetaStringId = StrongId<MetaStringIdTag, std::uint32_t>;

    struct MetaStringRecord final
    {
        std::uint32_t offset = 0;
        std::uint32_t size = 0;

        [[nodiscard]] constexpr auto operator<=>(
            const MetaStringRecord&
        ) const noexcept = default;
    };

    struct MetaNodeRecord final
    {
        EMetaNodeKind kind = EMetaNodeKind::INVALID;
        MetaStringId name{};
        MetaNodeId parent{};
        std::uint32_t payload_index = 0;
        std::uint32_t flags = 0;

        [[nodiscard]] constexpr auto operator<=>(
            const MetaNodeRecord&
        ) const noexcept = default;
    };

    struct MetaAttributeRecord final
    {
        MetaNodeId owner{};
        MetaStringId name{};
        MetaStringId value{};

        [[nodiscard]] constexpr auto operator<=>(
            const MetaAttributeRecord&
        ) const noexcept = default;
    };

    [[nodiscard]] constexpr bool isDeclarationKind(
        EMetaNodeKind kind
    ) noexcept
    {
        return kind >= EMetaNodeKind::NAMESPACE
            && kind <= EMetaNodeKind::PARAMETER;
    }

    template<typename Id>
    [[nodiscard]] constexpr bool isValidIndex(
        Id id,
        std::size_t size
    ) noexcept
    {
        return id.isValid()
            && static_cast<std::size_t>(id.value()) < size;
    }

    [[nodiscard]] constexpr std::uint64_t metaSortKey(
        EMetaNodeKind kind,
        std::string_view name
    ) noexcept
    {
        return (static_cast<std::uint64_t>(kind) << 56U)
            | (Fnv1a64::hash(name) & 0x00ff'ffff'ffff'ffffULL);
    }

    [[nodiscard]] constexpr std::size_t metaIndexCapacity(
        std::size_t expected_values
    ) noexcept
    {
        std::size_t capacity = 16;
        const auto required = expected_values > (std::numeric_limits<std::size_t>::max)() / 2
            ? (std::numeric_limits<std::size_t>::max)()
            : expected_values * 2;
        while (capacity < required
            && capacity <= (std::numeric_limits<std::size_t>::max)() / 2)
        {
            capacity *= 2;
        }
        return capacity;
    }

    template<typename Allocator = std::allocator<std::byte>>
    struct MetaUnitStorage final
    {
        template<typename Value>
        using rebound_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<Value>;

        using char_vector = std::vector<char, rebound_allocator<char>>;
        using string_vector = std::vector<
            MetaStringRecord,
            rebound_allocator<MetaStringRecord>
        >;
        using node_vector = std::vector<
            MetaNodeRecord,
            rebound_allocator<MetaNodeRecord>
        >;
        using attribute_vector = std::vector<
            MetaAttributeRecord,
            rebound_allocator<MetaAttributeRecord>
        >;

        explicit MetaUnitStorage(const Allocator& allocator = Allocator{})
            : characters(rebound_allocator<char>{allocator}),
              strings(rebound_allocator<MetaStringRecord>{allocator}),
              nodes(rebound_allocator<MetaNodeRecord>{allocator}),
              attributes(rebound_allocator<MetaAttributeRecord>{allocator})
        {
        }

        char_vector characters;
        string_vector strings;
        node_vector nodes;
        attribute_vector attributes;
    };

    template<typename Storage>
    concept meta_unit_storage = requires(const Storage& storage)
    {
        { storage.characters.data() } -> std::convertible_to<const char*>;
        { storage.strings.data() } -> std::convertible_to<const MetaStringRecord*>;
        { storage.nodes.data() } -> std::convertible_to<const MetaNodeRecord*>;
        { storage.attributes.data() } -> std::convertible_to<const MetaAttributeRecord*>;
        { storage.characters.size() } -> std::convertible_to<std::size_t>;
        { storage.strings.size() } -> std::convertible_to<std::size_t>;
        { storage.nodes.size() } -> std::convertible_to<std::size_t>;
        { storage.attributes.size() } -> std::convertible_to<std::size_t>;
    };

    template<meta_unit_storage Storage>
    class BasicMetaUnit final
    {
      public:
        using storage_type = Storage;

        explicit BasicMetaUnit(Storage storage)
        noexcept(std::is_nothrow_move_constructible_v<Storage>)
            : storage_(std::move(storage))
        {
        }

        [[nodiscard]] std::span<const MetaNodeRecord> nodes() const noexcept
        {
            return storage_.nodes;
        }

        [[nodiscard]] std::span<const MetaAttributeRecord> attributes() const noexcept
        {
            return storage_.attributes;
        }

        [[nodiscard]] bool contains(MetaNodeId id) const noexcept
        {
            return isValidIndex(id, storage_.nodes.size());
        }

        [[nodiscard]] std::string_view string(MetaStringId id) const noexcept
        {
            if (!isValidIndex(id, storage_.strings.size())) return {};
            const auto record = storage_.strings[id.value()];
            if (record.offset > storage_.characters.size()
                || record.size > storage_.characters.size() - record.offset)
            {
                return {};
            }
            if (record.size == 0) return {};
            return {
                storage_.characters.data() + record.offset,
                record.size
            };
        }

        [[nodiscard]] const Storage& storage() const noexcept
        {
            return storage_;
        }

      private:
        Storage storage_;
    };

    template<typename Allocator = std::allocator<std::byte>>
    class MetaUnitBuilder final
    {
      public:
        using allocator_type = Allocator;
        using storage_type = MetaUnitStorage<Allocator>;
        using unit_type = BasicMetaUnit<storage_type>;
        using index_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<std::uint32_t>;

        explicit MetaUnitBuilder(const Allocator& allocator = Allocator{})
            : storage_(allocator), intern_buckets_(index_allocator{allocator})
        {
        }

        void reserve(
            std::size_t node_count,
            std::size_t string_bytes,
            std::size_t attribute_count = 0
        )
        {
            storage_.nodes.reserve(node_count);
            storage_.strings.reserve(node_count);
            storage_.characters.reserve(string_bytes);
            storage_.attributes.reserve(attribute_count);
            rehash(metaIndexCapacity(node_count));
        }

        [[nodiscard]] expected<MetaStringId, EMetaIrError> intern(
            std::string_view text
        )
        {
            ensureInternCapacity();
            const auto hash = Fnv1a64::hash(text);
            auto bucket = static_cast<std::size_t>(hash)
                & (intern_buckets_.size() - 1);
            for (;;)
            {
                const auto encoded_id = intern_buckets_[bucket];
                if (encoded_id == 0) break;
                const MetaStringId id{encoded_id - 1};
                if (view(id) == text) return id;
                bucket = (bucket + 1) & (intern_buckets_.size() - 1);
            }

            constexpr auto kLimit = (std::numeric_limits<std::uint32_t>::max)();
            if (storage_.strings.size() >= kLimit
                || text.size() > kLimit
                || storage_.characters.size() > kLimit - text.size())
            {
                return unexpected(EMetaIrError::SIZE_LIMIT_EXCEEDED);
            }

            const MetaStringRecord record{
                static_cast<std::uint32_t>(storage_.characters.size()),
                static_cast<std::uint32_t>(text.size())
            };
            storage_.characters.insert(
                storage_.characters.end(),
                text.begin(),
                text.end()
            );
            storage_.strings.push_back(record);
            const MetaStringId result{
                static_cast<std::uint32_t>(storage_.strings.size() - 1)
            };
            intern_buckets_[bucket] = result.value() + 1;
            return result;
        }

        [[nodiscard]] expected<MetaNodeId, EMetaIrError> addNode(
            EMetaNodeKind kind,
            std::string_view name,
            MetaNodeId parent = MetaNodeId::invalid(),
            std::uint32_t payload_index = 0,
            std::uint32_t flags = 0
        )
        {
            if (parent.isValid() && !isValidIndex(parent, storage_.nodes.size()))
            {
                return unexpected(EMetaIrError::INVALID_INDEX);
            }
            if (storage_.nodes.size() >= (std::numeric_limits<std::uint32_t>::max)())
            {
                return unexpected(EMetaIrError::SIZE_LIMIT_EXCEEDED);
            }
            auto name_id = intern(name);
            if (!name_id) return unexpected(name_id.error());

            storage_.nodes.push_back(MetaNodeRecord{
                kind,
                *name_id,
                parent,
                payload_index,
                flags
            });
            return MetaNodeId{
                static_cast<std::uint32_t>(storage_.nodes.size() - 1)
            };
        }

        [[nodiscard]] expected<void, EMetaIrError> addAttribute(
            MetaNodeId owner,
            std::string_view name,
            std::string_view value
        )
        {
            if (!isValidIndex(owner, storage_.nodes.size()))
            {
                return unexpected(EMetaIrError::INVALID_INDEX);
            }
            auto name_id = intern(name);
            if (!name_id) return unexpected(name_id.error());
            auto value_id = intern(value);
            if (!value_id) return unexpected(value_id.error());
            storage_.attributes.push_back({owner, *name_id, *value_id});
            return {};
        }

        [[nodiscard]] unit_type freeze() &&
        {
            return unit_type{std::move(storage_)};
        }

      private:
        void ensureInternCapacity()
        {
            if (intern_buckets_.empty())
            {
                rehash(16);
                return;
            }
            if (storage_.strings.size() + 1
                > intern_buckets_.size() * 7 / 10)
            {
                rehash(intern_buckets_.size() * 2);
            }
        }

        void rehash(std::size_t requested_capacity)
        {
            const auto capacity = metaIndexCapacity(
                (requested_capacity + 1) / 2
            );
            if (capacity <= intern_buckets_.size()) return;

            decltype(intern_buckets_) replacement(
                capacity,
                0,
                intern_buckets_.get_allocator()
            );
            for (std::size_t index = 0; index < storage_.strings.size(); ++index)
            {
                const MetaStringId id{static_cast<std::uint32_t>(index)};
                auto bucket = static_cast<std::size_t>(Fnv1a64::hash(view(id)))
                    & (replacement.size() - 1);
                while (replacement[bucket] != 0)
                {
                    bucket = (bucket + 1) & (replacement.size() - 1);
                }
                replacement[bucket] = id.value() + 1;
            }
            intern_buckets_.swap(replacement);
        }

        [[nodiscard]] std::string_view view(MetaStringId id) const noexcept
        {
            const auto record = storage_.strings[id.value()];
            if (record.size == 0) return {};
            return {
                storage_.characters.data() + record.offset,
                record.size
            };
        }

        storage_type storage_;
        std::vector<std::uint32_t, index_allocator> intern_buckets_;
    };

    using MetaUnit = BasicMetaUnit<MetaUnitStorage<>>;
} // namespace lux::cxx::reflection::ir
