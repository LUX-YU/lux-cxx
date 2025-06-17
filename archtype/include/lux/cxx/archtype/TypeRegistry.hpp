#pragma once
#include <typeinfo>
#include <string>
#include <unordered_map>
#include "Common.hpp"
#include "Component.hpp"

namespace lux::cxx::archtype {

    using ComponentTypeID = std::size_t;

    /**
     * @struct ComponentTypeInfo
     * @brief Holds metadata about a single component type, 
     *        including size, alignment, and a hash value 
     *        (often derived from std::type_info).
     */
    struct ComponentTypeInfo {
        const char* name = nullptr;
        std::size_t size = 0;
        std::size_t alignment = 0;
        std::size_t hash = 0; 
    };

    /**
     * @class TypeRegistry
     * @brief Responsible for assigning unique component type IDs, 
     *        storing type metadata (size, alignment), and 
     *        storing type-erased operations (move_construct, destroy).
     * 
     *        Typically used through getTypeID<T>() to retrieve a 
     *        stable ID for each distinct component type.
     */
    class TypeRegistry {
    public:
        /**
         * @brief Retrieves a unique ComponentTypeID for type T, 
         *        assigning one if it hasn't been registered yet.
         * 
         * @tparam T The component type to register or retrieve.
         * @return The stable, unique ID for T.
         */
        template<typename T>
        static ComponentTypeID getTypeID() {
            static const ComponentTypeID type_id = registerType<T>();
            return type_id;
        }

        /**
         * @brief Retrieves the ComponentTypeInfo for a given ID, 
         *        containing size, alignment, etc.
         */
        static const ComponentTypeInfo& getTypeInfo(ComponentTypeID id) {
            return type_infos_[id];
        }

        /**
         * @brief Retrieves the type-erased ComponentOps for a given ID, 
         *        which includes pointers for move_construct and destroy.
         */
        static const ComponentOps& getOps(ComponentTypeID id) {
            return type_ops_[id];
        }

        /**
         * @brief Returns the number of registered component types so far.
         */
        static ComponentTypeID getTypeCount() {
            return type_count_;
        }

    private:
        /**
         * @brief Registers the component type T by creating 
         *        a new ID and storing metadata/ops.
         */
        template<typename T>
        static ComponentTypeID registerType() {
            ComponentTypeID id = type_count_++;

            // Fill type information
            type_infos_[id].name      = typeid(T).name();
            type_infos_[id].size      = sizeof(T);
            type_infos_[id].alignment = alignof(T);
            type_infos_[id].hash      = typeid(T).hash_code();

            // Build type-erased ops
            ComponentOps ops{};
            if constexpr (std::is_trivially_copyable_v<T>) {
                ops.move_construct = [](void* dest, void* src) {
                    std::memcpy(dest, src, sizeof(T));
                };
            } else if constexpr (std::is_move_constructible_v<T>) {
                ops.move_construct = [](void* dest, void* src) {
                    new(dest) T(std::move(*reinterpret_cast<T*>(src)));
                };
            } else if constexpr (std::is_copy_constructible_v<T>) {
                ops.move_construct = [](void* dest, void* src) {
                    new(dest) T(*reinterpret_cast<T*>(src));
                };
            }
            if constexpr (std::is_trivially_destructible_v<T>) {
                ops.destroy = [](void*) {
                    // trivial destructor - do nothing
                };
            } else {
                ops.destroy = [](void* ptr) {
                    reinterpret_cast<T*>(ptr)->~T();
                };
            }
            type_ops_[id] = ops;
            return id;
        }

        static inline ComponentTypeInfo  type_infos_[kMaxComponents]{};
        static inline ComponentOps       type_ops_[kMaxComponents]{};
        static inline ComponentTypeID    type_count_ = 0;
    };

} // namespace lux::ecs
