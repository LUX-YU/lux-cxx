#pragma once
#include <cstring>
#include <type_traits>

namespace lux::cxx::archtype {

    /**
     * @struct ComponentOps
     * @brief Stores type-erased function pointers for moving and destroying
     *        a particular component type. This enables dynamic operations 
     *        (like copy or move-construct) without direct knowledge of T.
     */
    struct ComponentOps {
        /**
         * @brief Function pointer to move-construct or copy-construct the component.
         *        If null, indicates no special construction is needed (e.g. trivial type).
         */
        void (*move_construct)(void* dest, void* src) = nullptr;

        /**
         * @brief Function pointer to destroy the component at a given memory location.
         *        If null, indicates trivial destruction.
         */
        void (*destroy)(void* ptr) = nullptr;
    };

} // namespace lux::ecs
