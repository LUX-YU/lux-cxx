#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/algotithm/hash.hpp>

namespace lux::cxx::dref::runtime
{
    /**
     * @brief Finds record metadata by record name.
     *
     * Note: The lookup currently converts the name to a string to search the map,
     * which uses a hash as the key. It is assumed that the record's unique hash corresponds to its name.
     *
     * @param name The name of the record.
     * @return Pointer to the corresponding RecordRuntimeMeta if found; nullptr otherwise.
     */
    RecordRuntimeMeta* RuntimeRegistry::findRecord(std::string_view name)
    {
		size_t hash = algorithm::fnv1a(name);
        auto it = record_map_.find(hash);
        if (it != record_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    RecordRuntimeMeta* RuntimeRegistry::findRecord(size_t hash)
    {
        auto it = record_map_.find(hash);
        if (it != record_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Registers enum or record metadata.
     * @param meta Pointer to the EnumRuntimeMeta to be registered.
     */
    void RuntimeRegistry::registerMeta(RecordRuntimeMeta* meta)
    {
        record_map_[meta->hash] = meta;
    }

    void RuntimeRegistry::registerMeta(EnumRuntimeMeta* meta)
    {
        enum_map_[meta->hash] = meta;
    }

    bool RuntimeRegistry::hasRecordMeta(std::string_view name)
    {
		return record_map_.find(algorithm::fnv1a(name)) != record_map_.end();
    }
    bool RuntimeRegistry::hasRecordMeta(size_t hash)
    {
		return record_map_.find(hash) != record_map_.end();
    }

    bool RuntimeRegistry::hasEnumMeta(std::string_view name)
    {
		return enum_map_.find(algorithm::fnv1a(name)) != enum_map_.end();
    }
    bool RuntimeRegistry::hasEnumMeta(size_t hash)
    {
		return enum_map_.find(hash) != enum_map_.end();
    }

    /**
     * @brief Finds enum metadata by enum name.
     *
     * Similar to findRecord, the lookup assumes that the enum's unique hash corresponds to its name.
     *
     * @param name The name of the enum.
     * @return Pointer to the corresponding EnumRuntimeMeta if found; nullptr otherwise.
     */
    EnumRuntimeMeta* RuntimeRegistry::findEnum(std::string_view name)
    {
        size_t hash = algorithm::fnv1a(name);
        auto it = enum_map_.find(hash);
        return (it != enum_map_.end()) ? it->second : nullptr;
    }

    EnumRuntimeMeta* RuntimeRegistry::findEnum(size_t hash)
    {
        auto it = enum_map_.find(hash);
        return (it != enum_map_.end()) ? it->second : nullptr;
    }
}