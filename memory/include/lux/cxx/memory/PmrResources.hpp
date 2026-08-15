#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <limits>
#include <memory_resource>
#include <new>

namespace lux::cxx
{
    class CountingMemoryResource final : public std::pmr::memory_resource
    {
      public:
        explicit CountingMemoryResource(
            std::pmr::memory_resource* upstream = std::pmr::get_default_resource()
        ) noexcept
            : upstream_(upstream)
        {
        }

        [[nodiscard]] std::size_t currentBytes() const noexcept
        {
            return current_bytes_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::size_t peakBytes() const noexcept
        {
            return peak_bytes_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::size_t allocationCount() const noexcept
        {
            return allocation_count_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::size_t deallocationCount() const noexcept
        {
            return deallocation_count_.load(std::memory_order_relaxed);
        }

      private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            void* memory = upstream_->allocate(bytes, alignment);
            allocation_count_.fetch_add(1, std::memory_order_relaxed);
            const auto current = current_bytes_.fetch_add(
                bytes,
                std::memory_order_relaxed
            ) + bytes;
            auto peak = peak_bytes_.load(std::memory_order_relaxed);
            while (peak < current && !peak_bytes_.compare_exchange_weak(
                peak,
                current,
                std::memory_order_relaxed
            ))
            {
            }
            return memory;
        }

        void do_deallocate(
            void* memory,
            std::size_t bytes,
            std::size_t alignment
        ) override
        {
            upstream_->deallocate(memory, bytes, alignment);
            current_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
            deallocation_count_.fetch_add(1, std::memory_order_relaxed);
        }

        [[nodiscard]] bool do_is_equal(
            const std::pmr::memory_resource& other
        ) const noexcept override
        {
            return this == &other;
        }

        std::pmr::memory_resource* upstream_;
        std::atomic<std::size_t> current_bytes_{0};
        std::atomic<std::size_t> peak_bytes_{0};
        std::atomic<std::size_t> allocation_count_{0};
        std::atomic<std::size_t> deallocation_count_{0};
    };

    class BudgetMemoryResource final : public std::pmr::memory_resource
    {
      public:
        explicit BudgetMemoryResource(
            std::size_t budget,
            std::pmr::memory_resource* upstream = std::pmr::get_default_resource()
        ) noexcept
            : budget_(budget), upstream_(upstream)
        {
        }

        [[nodiscard]] std::size_t budget() const noexcept
        {
            return budget_;
        }

        [[nodiscard]] std::size_t currentBytes() const noexcept
        {
            return current_bytes_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::size_t peakBytes() const noexcept
        {
            return peak_bytes_.load(std::memory_order_relaxed);
        }

      private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            auto current = current_bytes_.load(std::memory_order_relaxed);
            for (;;)
            {
                if (bytes > budget_ - (std::min)(budget_, current))
                {
                    throw std::bad_alloc{};
                }
                if (current_bytes_.compare_exchange_weak(
                    current,
                    current + bytes,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ))
                {
                    break;
                }
            }

            try
            {
                void* memory = upstream_->allocate(bytes, alignment);
                auto peak = peak_bytes_.load(std::memory_order_relaxed);
                const auto reserved = current + bytes;
                while (peak < reserved && !peak_bytes_.compare_exchange_weak(
                    peak,
                    reserved,
                    std::memory_order_relaxed
                ))
                {
                }
                return memory;
            }
            catch (...)
            {
                current_bytes_.fetch_sub(bytes, std::memory_order_release);
                throw;
            }
        }

        void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override
        {
            upstream_->deallocate(memory, bytes, alignment);
            current_bytes_.fetch_sub(bytes, std::memory_order_release);
        }

        [[nodiscard]] bool do_is_equal(
            const std::pmr::memory_resource& other
        ) const noexcept override
        {
            return this == &other;
        }

        std::size_t budget_;
        std::pmr::memory_resource* upstream_;
        std::atomic<std::size_t> current_bytes_{0};
        std::atomic<std::size_t> peak_bytes_{0};
    };

    class FailingMemoryResource final : public std::pmr::memory_resource
    {
      public:
        explicit FailingMemoryResource(
            std::size_t successful_allocations,
            std::pmr::memory_resource* upstream = std::pmr::get_default_resource()
        ) noexcept
            : remaining_(successful_allocations), upstream_(upstream)
        {
        }

        void reset(std::size_t successful_allocations) noexcept
        {
            remaining_.store(successful_allocations, std::memory_order_relaxed);
        }

      private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            auto remaining = remaining_.load(std::memory_order_relaxed);
            for (;;)
            {
                if (remaining == 0) throw std::bad_alloc{};
                if (remaining_.compare_exchange_weak(
                    remaining,
                    remaining - 1,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ))
                {
                    break;
                }
            }
            try
            {
                return upstream_->allocate(bytes, alignment);
            }
            catch (...)
            {
                remaining_.fetch_add(1, std::memory_order_release);
                throw;
            }
        }

        void do_deallocate(void* memory, std::size_t bytes, std::size_t alignment) override
        {
            upstream_->deallocate(memory, bytes, alignment);
        }

        [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        {
            return this == &other;
        }

        std::atomic<std::size_t>    remaining_;
        std::pmr::memory_resource*  upstream_;
    };
} // namespace lux::cxx
