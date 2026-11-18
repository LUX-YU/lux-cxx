#pragma once
// One translation unit per executable. Count ordinary AND over-aligned allocations.
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#ifdef _MSC_VER
#include <malloc.h>
#endif
namespace allocation_test
{
    inline std::atomic<std::size_t> count{0};
    inline std::atomic<bool> enabled{false};
    inline void note() noexcept
    {
        if (enabled.load(std::memory_order_relaxed))
            count.fetch_add(1, std::memory_order_relaxed);
    }
    inline void start() noexcept
    {
        count.store(0);
        enabled.store(true);
    }
    inline std::size_t stop() noexcept
    {
        enabled.store(false);
        return count.load();
    }
    inline void* allocate(std::size_t n)
    {
        note();
        if (void* p = std::malloc(n ? n : 1))
            return p;
        std::abort();
    }
    inline void* aligned(std::size_t n, std::size_t align)
    {
        note();
#ifdef _MSC_VER
        void* p = _aligned_malloc(n ? n : 1, align);
#else
        n = n ? n : 1;
        const auto padded = ((n + align - 1) / align) * align;
        void* p = std::aligned_alloc(align, padded);
#endif
        if (p)
            return p;
        std::abort();
    }
    inline void free_aligned(void* p) noexcept
    {
#ifdef _MSC_VER
        _aligned_free(p);
#else
        std::free(p);
#endif
    }
} // namespace allocation_test
void* operator new(std::size_t n)
{
    return allocation_test::allocate(n);
}
void* operator new[](std::size_t n)
{
    return allocation_test::allocate(n);
}
void* operator new(std::size_t n, std::align_val_t a)
{
    return allocation_test::aligned(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a)
{
    return allocation_test::aligned(n, static_cast<std::size_t>(a));
}
void operator delete(void* p) noexcept
{
    std::free(p);
}
void operator delete[](void* p) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    allocation_test::free_aligned(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    allocation_test::free_aligned(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{
    allocation_test::free_aligned(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{
    allocation_test::free_aligned(p);
}
