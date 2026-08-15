#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <optional>
#include <utility>

namespace lux::cxx
{
    template <std::unsigned_integral ItemCounter, std::unsigned_integral ByteCounter>
    class BudgetGate;

    template <
        std::unsigned_integral ItemCounter = std::size_t,
        std::unsigned_integral ByteCounter = std::size_t
    >
    class BudgetReservation final
    {
        friend class BudgetGate<ItemCounter, ByteCounter>;

      public:
        constexpr BudgetReservation() noexcept = default;
        BudgetReservation(const BudgetReservation&) = delete;
        BudgetReservation& operator=(const BudgetReservation&) = delete;

        BudgetReservation(BudgetReservation&& other) noexcept
            : gate_(std::exchange(other.gate_, nullptr)),
              items_(other.items_),
              bytes_(other.bytes_)
        {
        }

        BudgetReservation& operator=(BudgetReservation&& other) noexcept
        {
            if (this != &other)
            {
                release();
                gate_ = std::exchange(other.gate_, nullptr);
                items_ = other.items_;
                bytes_ = other.bytes_;
            }
            return *this;
        }

        ~BudgetReservation()
        {
            release();
        }

        void release() noexcept
        {
            if (gate_ == nullptr) return;
            gate_->release(items_, bytes_);
            gate_ = nullptr;
        }

      private:
        BudgetReservation(
            BudgetGate<ItemCounter, ByteCounter>* gate,
            ItemCounter items,
            ByteCounter bytes
        ) noexcept
            : gate_(gate), items_(items), bytes_(bytes)
        {
        }

        BudgetGate<ItemCounter, ByteCounter>* gate_ = nullptr;
        ItemCounter items_{};
        ByteCounter bytes_{};
    };

    template <
        std::unsigned_integral ItemCounter = std::size_t,
        std::unsigned_integral ByteCounter = std::size_t
    >
    class BudgetGate final
    {
        friend class BudgetReservation<ItemCounter, ByteCounter>;

      public:
        using reservation_type = BudgetReservation<ItemCounter, ByteCounter>;

        constexpr BudgetGate(
            ItemCounter item_limit,
            ByteCounter byte_limit
        ) noexcept
            : item_limit_(item_limit), byte_limit_(byte_limit)
        {
        }

        [[nodiscard]] static constexpr bool fitsBudget(
            ItemCounter current_items,
            ByteCounter current_bytes,
            ItemCounter requested_items,
            ByteCounter requested_bytes,
            ItemCounter item_limit,
            ByteCounter byte_limit
        ) noexcept
        {
            return current_items <= item_limit &&
                   current_bytes <= byte_limit &&
                   requested_items <= item_limit - current_items &&
                   requested_bytes <= byte_limit - current_bytes;
        }

        [[nodiscard]] std::optional<reservation_type> tryAcquire(
            ItemCounter items,
            ByteCounter bytes
        ) noexcept
        {
            if (!reserve(items_, items, item_limit_)) return std::nullopt;
            if (!reserve(bytes_, bytes, byte_limit_))
            {
                items_.fetch_sub(items, std::memory_order_release);
                return std::nullopt;
            }
            return reservation_type(this, items, bytes);
        }

        [[nodiscard]] ItemCounter currentItems() const noexcept
        {
            return items_.load(std::memory_order_acquire);
        }

        [[nodiscard]] ByteCounter currentBytes() const noexcept
        {
            return bytes_.load(std::memory_order_acquire);
        }

      private:
        template <typename Counter>
        [[nodiscard]] static bool reserve(
            std::atomic<Counter>& current,
            Counter requested,
            Counter limit
        ) noexcept
        {
            auto value = current.load(std::memory_order_relaxed);
            for (;;)
            {
                if (value > limit || requested > limit - value) return false;
                if (current.compare_exchange_weak(
                    value,
                    value + requested,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ))
                {
                    return true;
                }
            }
        }

        void release(ItemCounter items, ByteCounter bytes) noexcept
        {
            bytes_.fetch_sub(bytes, std::memory_order_release);
            items_.fetch_sub(items, std::memory_order_release);
        }

        ItemCounter item_limit_;
        ByteCounter byte_limit_;
        std::atomic<ItemCounter> items_{0};
        std::atomic<ByteCounter> bytes_{0};
    };
} // namespace lux::cxx
