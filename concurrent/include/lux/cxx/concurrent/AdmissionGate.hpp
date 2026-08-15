#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace lux::cxx
{
    template <std::unsigned_integral Counter>
    class AdmissionGate;

    template <std::unsigned_integral Counter = std::size_t>
    class AdmissionTicket final
    {
        friend class AdmissionGate<Counter>;

      public:
        constexpr AdmissionTicket() noexcept = default;
        AdmissionTicket(const AdmissionTicket&) = delete;
        AdmissionTicket& operator=(const AdmissionTicket&) = delete;

        AdmissionTicket(AdmissionTicket&& other) noexcept
            : gate_(std::exchange(other.gate_, nullptr))
        {
        }

        AdmissionTicket& operator=(AdmissionTicket&& other) noexcept
        {
            if (this != &other)
            {
                release();
                gate_ = std::exchange(other.gate_, nullptr);
            }
            return *this;
        }

        ~AdmissionTicket()
        {
            release();
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return gate_ != nullptr;
        }

        void release() noexcept
        {
            if (gate_ == nullptr) return;
            gate_->release();
            gate_ = nullptr;
        }

      private:
        explicit AdmissionTicket(AdmissionGate<Counter>* gate) noexcept
            : gate_(gate)
        {
        }

        AdmissionGate<Counter>* gate_ = nullptr;
    };

    template <std::unsigned_integral Counter = std::size_t>
    class AdmissionGate final
    {
        friend class AdmissionTicket<Counter>;

      public:
        using counter_type = Counter;
        using ticket_type = AdmissionTicket<Counter>;

        AdmissionGate() noexcept = default;
        AdmissionGate(const AdmissionGate&) = delete;
        AdmissionGate& operator=(const AdmissionGate&) = delete;
        AdmissionGate(AdmissionGate&&) = delete;
        AdmissionGate& operator=(AdmissionGate&&) = delete;

        ~AdmissionGate()
        {
            assert(in_flight_.load(std::memory_order_relaxed) == 0 &&
                   "AdmissionTicket outlived AdmissionGate");
        }

        [[nodiscard]] std::optional<ticket_type> tryAcquire() noexcept
        {
            if (closed_.load(std::memory_order_acquire)) return std::nullopt;
            auto current = in_flight_.load(std::memory_order_relaxed);
            for (;;)
            {
                if (current == (std::numeric_limits<Counter>::max)())
                {
                    return std::nullopt;
                }
                if (in_flight_.compare_exchange_weak(
                    current,
                    static_cast<Counter>(current + 1),
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed
                ))
                {
                    break;
                }
                if (closed_.load(std::memory_order_acquire))
                {
                    return std::nullopt;
                }
            }
            if (closed_.load(std::memory_order_acquire))
            {
                in_flight_.fetch_sub(1, std::memory_order_release);
                return std::nullopt;
            }
            return ticket_type(this);
        }

        void close() noexcept
        {
            closed_.store(true, std::memory_order_release);
        }

        [[nodiscard]] bool closed() const noexcept
        {
            return closed_.load(std::memory_order_acquire);
        }

        [[nodiscard]] Counter inFlight() const noexcept
        {
            return in_flight_.load(std::memory_order_acquire);
        }

      private:
        void release() noexcept
        {
            const auto previous = in_flight_.fetch_sub(
                1,
                std::memory_order_acq_rel
            );
            assert(previous > 0);
        }

        std::atomic<Counter> in_flight_{0};
        std::atomic<bool> closed_{false};
    };
} // namespace lux::cxx
