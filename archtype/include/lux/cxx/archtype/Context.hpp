#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace lux::cxx::archtype {

    /**
     * @class ContextStorage
     * @brief Per-Registry singleton storage for resources such as Time, Input,
     *        AssetManager, RandomEngine — things that aren't entity components
     *        but you still want to hang on the world.
     *
     * Indexed by std::type_index, so the resource type space is independent
     * from the kMaxComponents=64 component bitset (you can have any number of
     * distinct resources). One instance per type. Resources own their storage
     * and are destroyed when the ContextStorage (and thus the Registry) goes
     * away.
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
            auto key = std::type_index(typeid(T));
            auto it  = storage_.find(key);
            if (it != storage_.end()) {
                if (it->second.data) it->second.destroy(it->second.data);
                it->second = {};
            } else {
                it = storage_.emplace(key, Slot{}).first;
            }

            void* mem = ::operator new(sizeof(T), std::align_val_t(alignof(T)));
            T* ptr = ::new (mem) T(std::forward<Args>(args)...);
            it->second.data    = mem;
            it->second.destroy = +[](void* p) noexcept {
                static_cast<T*>(p)->~T();
                ::operator delete(p, std::align_val_t(alignof(T)));
            };
            return *ptr;
        }

        /// Pointer to the resource of type T, or nullptr if not present.
        template<class T>
        T* find() noexcept {
            auto it = storage_.find(std::type_index(typeid(T)));
            return (it == storage_.end()) ? nullptr : static_cast<T*>(it->second.data);
        }

        template<class T>
        const T* find() const noexcept {
            auto it = storage_.find(std::type_index(typeid(T)));
            return (it == storage_.end()) ? nullptr : static_cast<const T*>(it->second.data);
        }

        /// Reference to the resource of type T. Asserts existence.
        template<class T>             T& get()       { T* p = find<T>();             assert(p && "ctx::get<T>: resource not present"); return *p; }
        template<class T> const T& get() const { const T* p = find<T>(); assert(p && "ctx::get<T>: resource not present"); return *p; }

        template<class T>
        bool contains() const noexcept {
            return storage_.find(std::type_index(typeid(T))) != storage_.end();
        }

        template<class T>
        void erase() {
            auto it = storage_.find(std::type_index(typeid(T)));
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
        };
        std::unordered_map<std::type_index, Slot> storage_;
    };

} // namespace lux::cxx::archtype
