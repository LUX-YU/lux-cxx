#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

#include "Common.hpp"

namespace lux::cxx::archtype {

    /**
     * @brief Type-erased lifecycle ops for a component.
     *
     * For trivially-relocatable / trivially-destructible types, the
     * corresponding function pointer is **nullptr** so that hot paths can
     * detect that case in one branch and fall back to an inline memcpy,
     * avoiding the indirect-call overhead and letting the compiler
     * specialize on the (cached, per-archetype) component size.
     */
    struct ComponentOps {
        /// Move-construct dest from src AND destroy src in place.
        /// nullptr  =>  the type is trivially relocatable; caller should memcpy `size` bytes.
        void (*move_and_destroy)(void* dest, void* src) = nullptr;

        /// Destroy a component in place.
        /// nullptr  =>  trivial destructor; nothing to do.
        void (*destroy)(void* ptr) = nullptr;
    };

    /// Bit flags carried in ComponentTypeInfo::flags.
    enum ComponentFlag : std::uint8_t {
        kFlagTriviallyRelocatable  = 1u << 0, ///< ops.move_and_destroy is nullptr
        kFlagTriviallyDestructible = 1u << 1, ///< ops.destroy           is nullptr
        kFlagTag                   = 1u << 2, ///< std::is_empty_v<T>; size is stored as 0 — no chunk storage
    };

    /**
     * @struct ComponentTypeInfo
     * @brief Metadata for a single component type. Looked up by ComponentTypeID.
     */
    struct ComponentTypeInfo {
        const char*       name      = nullptr;
        std::uint32_t     size      = 0;
        std::uint32_t     alignment = 0;
        std::uint8_t      flags     = 0; ///< bitwise OR of ComponentFlag values
        ComponentOps      ops{};
    };

    /**
     * @class TypeRegistry
     * @brief Hands out a stable ComponentTypeID per component type T and stores its metadata.
     *
     * Backed by a static array indexed by ID, so getTypeInfo(id) is one array load
     * with no hash / function-pointer overhead in the hot path.
     */
    class TypeRegistry {
    public:
        template<typename T>
        static ComponentTypeID getTypeID() {
            static const ComponentTypeID id = registerType<T>();
            return id;
        }

        static const ComponentTypeInfo& getTypeInfo(ComponentTypeID id) noexcept {
            return type_infos_[id];
        }

        static const ComponentOps& getOps(ComponentTypeID id) noexcept {
            return type_infos_[id].ops;
        }

        static ComponentTypeID getTypeCount() noexcept { return type_count_.load(std::memory_order_relaxed); }

    private:
        template<typename T>
        static ComponentTypeID registerType() {
            // fetch_add reserves a slot atomically, so distinct T's registering
            // concurrently (function-local-static init across templates is only
            // serialized per-T) can't race on the counter.
            const ComponentTypeID id = type_count_.fetch_add(1, std::memory_order_relaxed);
            // The 65th distinct component type would write past type_infos_ and
            // corrupt adjacent statics; fail loudly instead. (Signature is 64-bit,
            // so kMaxComponents is a hard architectural limit.)
            assert(id < kMaxComponents && "TypeRegistry: too many distinct component types (raise kMaxComponents)");
            if (id >= kMaxComponents)
                throw std::length_error("TypeRegistry: exceeded kMaxComponents distinct component types");
            auto& info = type_infos_[id];

            info.name      = typeid(T).name();
            // Empty / tag components carry no data: report size=0 so chunk
            // layout reserves zero bytes for them. (sizeof(T) is at least 1 in
            // C++ even for empty classes — that minimum byte is purely a
            // language artifact, not data we need to store.)
            constexpr bool is_tag = std::is_empty_v<T>;
            info.size      = is_tag ? 0u : static_cast<std::uint32_t>(sizeof(T));
            info.alignment = static_cast<std::uint32_t>(alignof(T));

            std::uint8_t flags = 0;
            if constexpr (is_tag) {
                flags |= kFlagTag;
            }

            // ---- move_and_destroy ----
            // "Trivially relocatable" approximation: trivially copyable AND trivially destructible.
            // The standard guarantees memcpy preserves object identity for trivially-copyable types,
            // and a trivial destructor needs no call.
            if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
                flags |= kFlagTriviallyRelocatable;
                info.ops.move_and_destroy = nullptr; // caller uses memcpy(size)
            } else if constexpr (std::is_move_constructible_v<T>) {
                info.ops.move_and_destroy = [](void* dst, void* src) {
                    T* s = static_cast<T*>(src);
                    ::new (dst) T(std::move(*s));
                    s->~T();
                };
            } else if constexpr (std::is_copy_constructible_v<T>) {
                info.ops.move_and_destroy = [](void* dst, void* src) {
                    T* s = static_cast<T*>(src);
                    ::new (dst) T(*s);
                    s->~T();
                };
            }

            // ---- destroy ----
            if constexpr (std::is_trivially_destructible_v<T>) {
                flags |= kFlagTriviallyDestructible;
                info.ops.destroy = nullptr;        // hot path: `if (ops.destroy)` short-circuits
            } else {
                info.ops.destroy = [](void* p) { static_cast<T*>(p)->~T(); };
            }

            info.flags = flags;
            return id;
        }

        static inline ComponentTypeInfo            type_infos_[kMaxComponents]{};
        static inline std::atomic<ComponentTypeID> type_count_{0};
    };

} // namespace lux::cxx::archtype
