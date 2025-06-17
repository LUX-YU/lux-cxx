#pragma once
#include <bitset>
#include <cstdint>
#include <limits>

namespace lux::cxx::archtype {

    /**
     * @brief Maximum number of distinct component types supported by this ECS.
     *        This is used for bitset-based signatures. Adjust as needed.
     */
    constexpr std::size_t kMaxComponents = 64;

    /**
     * @brief Number of entities stored in each Chunk. Increasing this 
     *        can reduce the number of Chunks (and allocations) required, 
     *        but increases per-Chunk memory usage.
     */
    constexpr std::size_t kChunkSize = 64;

    /**
     * @brief Basic entity type, currently just a 32-bit ID.
     *        This can be extended to (index, generation) if desired.
     */
    using Entity = uint32_t;

    /**
     * @brief A sentinel entity ID that signifies an invalid or non-existent entity.
     */
    constexpr Entity kInvalidEntity = std::numeric_limits<Entity>::max();

    /**
     * @brief A bitset signature for indicating which components an entity (or Archetype) has.
     */
    using Signature = std::bitset<kMaxComponents>;

} // namespace lux::ecs
