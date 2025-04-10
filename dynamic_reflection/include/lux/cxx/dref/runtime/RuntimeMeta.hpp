#pragma once
#include <variant>
#include <memory>
#include <memory_resource>
#include "MetaUnit.hpp"

namespace lux::cxx::dref::runtime
{
	enum class EAllocPolicy
	{
		Heap,
		Stack,
		Shared,
		Unique,
		Pmr,
	};

    struct HeapPolicy {
        static constexpr const char* PolicyName() { return "Heap"; }

        template<typename T, typename... Args>
        static T* Create(std::pmr::memory_resource* /*mr*/, Args&&... args) {
            return new T(std::forward<Args>(args)...);
        }
    };

    struct PMRPolicy {
        static constexpr const char* PolicyName() { return "PMR"; }
        template<typename T, typename... Args>
        static T* Create(std::pmr::memory_resource* mr, Args&&... args) {
            std::pmr::polymorphic_allocator<T> alloc(mr);
            T* p = alloc.allocate(1);
            new (p) T(std::forward<Args>(args)...);
            return p;
        }
    };

    struct StackPolicy {
        template<typename T, typename... Args>
        static T* Create(void* buffer, Args&&... args) {
            return new (buffer) T(std::forward<Args>(args)...);
        }
    };

    struct SharedPolicy {
        template<typename T, typename... Args>
        static std::shared_ptr<T> Create(Args&&... args) {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
    };

    struct UniquePolicy {
        template<typename T, typename... Args>
        static std::unique_ptr<T> Create(Args&&... args) {
            return std::make_unique<T>(std::forward<Args>(args)...);
        }
    };

    class InstanceCreator
    {
    public:
        InstanceCreator();
    };
} // namespace lux::cxx::dref::runtime
