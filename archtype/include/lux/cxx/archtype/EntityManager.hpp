#pragma once
#include <vector>
#include "Common.hpp"

namespace lux::cxx::archtype {

    /**
     * @class EntityManager
     * @brief Allocates and recycles Entity IDs. It maintains a free list 
     *        so that destroyed IDs can be reused for future entities.
     */
    class EntityManager {
    public:
        EntityManager() : next_entity_id_(0) {}

        /**
         * @brief Creates a new entity ID. If there is an available ID in the free list,
         *        it will reuse that ID; otherwise, it generates a fresh one.
         * 
         * @return A unique Entity ID.
         */
        Entity createEntity() {
            if (!free_list_.empty()) {
                Entity id = free_list_.back();
                free_list_.pop_back();
                return id;
            } else {
                return next_entity_id_++;
            }
        }

        /**
         * @brief Destroys the given entity ID, making it available for reuse.
         * 
         * @param e The entity ID to destroy.
         */
        void destroyEntity(Entity e) {
            free_list_.push_back(e);
        }

    private:
        Entity next_entity_id_;      ///< The next new ID if the free list is empty
        std::vector<Entity> free_list_; ///< Recycled IDs to be reused later
    };

} // namespace lux::ecs
