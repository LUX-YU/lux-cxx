#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <lux/cxx/compile_time/type_info.hpp>

namespace lux::cxx::archtype {

    /**
     * @class ContextStorage
     * @brief Per-Registry singleton storage for resources such as Time, Input,
     *        AssetManager, RandomEngine — things that aren't entity components
     *        but you still want to hang on the world.
     *
     * Indexed by lux::cxx::type_hash (RTTI-free), so the resource type space is
     * independent from the kMaxComponents=64 component bitset (you can have any
     * number of distinct resources). One instance per type. Resources own their
     * storage and are destroyed when the ContextStorage (and thus the Registry)
     * goes away.
     *
     * type_hash is a 64-bit FNV-1a over the compiler-formatted type name; the
     * slot keeps type_name as a tie-breaker and asserts on hash collision.
     */
    class ContextStorage {
    public:
        ContextStorage() = default;
        ~ContextStorage() { clear(); }

        ContextStorage(const ContextStorage&)            = delete;
        ContextStorage& operator=(const ContextStorage&) = delete;
        ContextStorage(ContextStorage&&) noexcept            = default;
        ContextStorage& operator=(ContextStorage&&) noexcept = default;

        /// Construct (or overwrite) the resource of type T in place.
        template<class T, class... Args>
        T& emplace(Args&&... args) {
            auto it = findSlot<T>();
            if (it != storage_.end()) {
                if (it->second.data) it->second.destroy(it->second.data);
                it->second = {};
            } else {
                it = storage_.emplace(type_hash<T>(), Slot{}).first;
            }

            void* mem = ::operator new(sizeof(T), std::align_val_t(alignof(T)));
            T* ptr = ::new (mem) T(std::forward<Args>(args)...);
            it->second.data    = mem;
            it->second.name    = type_name<T>();
            it->second.destroy = +[](void* p) noexcept {
                static_cast<T*>(p)->~T();
                ::operator delete(p, std::align_val_t(alignof(T)));
            };
            return *ptr;
        }

        /// Pointer to the resource of type T, or nullptr if not present.
        template<class T>
        T* find() noexcept {
            auto it = findSlot<T>();
            return (it == storage_.end()) ? nullptr : static_cast<T*>(it->second.data);
        }

        template<class T>
        const T* find() const noexcept {
            auto it = findSlot<T>();
            return (it == storage_.end()) ? nullptr : static_cast<const T*>(it->second.data);
        }

        /// Reference to the resource of type T. Asserts existence.
        template<class T>             T& get()       { T* p = find<T>();             assert(p && "ctx::get<T>: resource not present"); return *p; }
        template<class T> const T& get() const { const T* p = find<T>(); assert(p && "ctx::get<T>: resource not present"); return *p; }

        template<class T>
        bool contains() const noexcept {
            return findSlot<T>() != storage_.end();
        }

        template<class T>
        void erase() {
            auto it = findSlot<T>();
            if (it != storage_.end()) {
                if (it->second.data) it->second.destroy(it->second.data);
                storage_.erase(it);
            }
        }

        void clear() noexcept {
            for (auto& [k, slot] : storage_) {
                if (slot.data) slot.destroy(slot.data);
            }
            storage_.clear();
        }

        std::size_t size()  const noexcept { return storage_.size(); }
        bool        empty() const noexcept { return storage_.empty(); }

    private:
        struct Slot {
            void* data = nullptr;
            void (*destroy)(void*) noexcept = nullptr;
            std::string_view name{};   ///< type_name<T>, collision tie-breaker
        };
        using Storage = std::unordered_map<basic_type_info::id_t, Slot>;

        template<class T>
        Storage::iterator findSlot() noexcept {
            auto it = storage_.find(type_hash<T>());
            assert((it == storage_.end() || it->second.name == type_name<T>())
                   && "ContextStorage: type_hash collision between distinct resource types");
            return it;
        }

        template<class T>
        Storage::const_iterator findSlot() const noexcept {
            auto it = storage_.find(type_hash<T>());
            assert((it == storage_.end() || it->second.name == type_name<T>())
                   && "ContextStorage: type_hash collision between distinct resource types");
            return it;
        }

        Storage storage_;
    };

} // namespace lux::cxx::archtype
