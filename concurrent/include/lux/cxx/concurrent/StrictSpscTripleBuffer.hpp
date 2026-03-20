#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace lux::cxx
{
    /**
     * @brief Strict single-producer / single-consumer triple buffer.
     *
     * This class is designed for frame-style handoff between two threads,
     * such as:
     *   - game thread  -> producer
     *   - render thread -> consumer
     *
     * Unlike a typical "latest-only" triple buffer, this implementation does
     * not allow the producer to run arbitrarily ahead. After publishing one
     * completed frame, the producer must wait until the consumer acquires that
     * frame and returns the previous read slot. As a result:
     *
     *   1. No published frame is overwritten before the consumer acquires it.
     *   2. The producer can be at most one published frame ahead of the consumer.
     *   3. No mutex is required; synchronization is performed only with atomics.
     *
     * Ownership model:
     *   - One slot is currently owned by the consumer for reading.
     *   - One slot is currently owned by the producer for writing.
     *   - One slot is spare.
     *
     * Slot handoff sequence:
     *   - Producer writes into its write slot.
     *   - Producer publishes that slot.
     *   - Consumer acquires the published slot and starts reading it.
     *   - Consumer returns its previous read slot.
     *   - Producer reclaims the returned slot and uses it as the next spare.
     *
     * Threading contract:
     *   - All producer methods must be called from exactly one producer thread.
     *   - All consumer methods must be called from exactly one consumer thread.
     *   - Calling producer methods from multiple threads, or consumer methods
     *     from multiple threads, is undefined behavior.
     *
     * Type requirements:
     *   - This implementation stores three persistent T objects internally.
     *   - Therefore T must be default constructible.
     *
     * @tparam T Frame payload type stored in the three slots.
     */
    template <typename T>
    class StrictSpscTripleBuffer
    {
        static_assert(std::is_default_constructible_v<T>,
            "StrictSpscTripleBuffer<T> requires T to be default constructible.");

    private:
        static constexpr std::uint8_t invalidIndex = 0xFF;
        static constexpr std::size_t slotCount = 3;

#if defined(__cpp_lib_hardware_interference_size)
        static constexpr std::size_t cacheLineSize =
            std::hardware_destructive_interference_size;
#else
        static constexpr std::size_t cacheLineSize = 64;
#endif

        /**
         * @brief Shared state exchanged between producer and consumer.
         *
         * readyIndex:
         *   The slot published by the producer and not yet acquired by the consumer.
         *
         * returnedIndex:
         *   The previous consumer read slot returned back to the producer.
         */
        struct alignas(cacheLineSize) SharedState
        {
            std::atomic<std::uint8_t> readyIndex{ invalidIndex };
            std::atomic<std::uint8_t> returnedIndex{ invalidIndex };
        };

        /**
         * @brief Producer-local state.
         *
         * writeIndex:
         *   The slot currently owned by the producer for writing.
         *   When invalid, the producer is not allowed to start a new frame yet.
         *
         * spareIndex:
         *   The spare slot that will become relevant after the next reclaim.
         */
        struct alignas(cacheLineSize) ProducerState
        {
            std::uint8_t writeIndex{ 1 };
            std::uint8_t spareIndex{ 2 };
        };

        /**
         * @brief Consumer-local state.
         *
         * readIndex:
         *   The slot currently owned by the consumer for reading.
         */
        struct alignas(cacheLineSize) ConsumerState
        {
            std::uint8_t readIndex{ 0 };
        };

    public:
        /**
         * @brief Construct the triple buffer.
         *
         * Initial slot assignment:
         *   - consumer reads slot 0
         *   - producer writes slot 1
         *   - slot 2 is spare
         *
         * All three slots are default constructed.
         */
        StrictSpscTripleBuffer() = default;

        StrictSpscTripleBuffer(const StrictSpscTripleBuffer&) = delete;
        StrictSpscTripleBuffer& operator=(const StrictSpscTripleBuffer&) = delete;
        StrictSpscTripleBuffer(StrictSpscTripleBuffer&&) = delete;
        StrictSpscTripleBuffer& operator=(StrictSpscTripleBuffer&&) = delete;

        ~StrictSpscTripleBuffer() = default;

        // ---------------------------------------------------------------------
        // Producer API
        // ---------------------------------------------------------------------

        /**
         * @brief Try to obtain the current writable slot.
         *
         * If the producer already owns a write slot, this function returns it.
         * Otherwise, it attempts to reclaim the slot returned by the consumer.
         *
         * This function returns nullptr when the producer must not begin a new
         * frame yet. That can happen when:
         *   - the previously published frame has not been acquired yet, or
         *   - the consumer has not returned its previous read slot yet.
         *
         * This strict behavior is what enforces "producer may lead consumer by
         * at most one frame".
         *
         * Memory ordering:
         *   - An acquire exchange on returnedIndex synchronizes with the
         *     consumer's release store when returning the old read slot.
         *   - This guarantees the producer does not reuse that slot until the
         *     consumer has finished reading from it.
         *
         * @return Pointer to the writable slot, or nullptr if writing cannot
         *         begin yet.
         *
         * @warning Must be called only from the producer thread.
         */
        [[nodiscard]] T* tryBeginWrite() noexcept
        {
            if (producerState_.writeIndex != invalidIndex) {
                return &slots_[producerState_.writeIndex];
            }

            // The previously published frame must already have been acquired.
            if (sharedState_.readyIndex.load(std::memory_order_acquire) != invalidIndex) {
                return nullptr;
            }

            // Reclaim the slot returned by the consumer.
            const std::uint8_t returned =
                sharedState_.returnedIndex.exchange(invalidIndex, std::memory_order_acquire);

            if (returned == invalidIndex) {
                return nullptr;
            }

            producerState_.writeIndex = producerState_.spareIndex;
            producerState_.spareIndex = returned;
            return &slots_[producerState_.writeIndex];
        }

        /**
         * @brief Publish the currently written slot to the consumer.
         *
         * The producer must have fully finished writing the slot before calling
         * this function.
         *
         * After a successful publish:
         *   - the consumer may acquire the new frame,
         *   - the producer loses its current write slot,
         *   - the producer cannot begin another frame until the consumer has
         *     acquired the published frame and returned its previous read slot.
         *
         * Memory ordering:
         *   - A release store to readyIndex publishes all prior writes made to
         *     the slot.
         *   - The consumer's acquire exchange on readyIndex guarantees that it
         *     sees the fully written frame contents.
         *
         * @return true if the frame was published successfully, false otherwise.
         *
         * @warning Must be called only from the producer thread.
         */
        [[nodiscard]] bool publishWrite() noexcept
        {
            if (producerState_.writeIndex == invalidIndex) {
                return false;
            }

            // Under the strict protocol there should never already be a pending frame.
            if (sharedState_.readyIndex.load(std::memory_order_acquire) != invalidIndex) {
                return false;
            }

            sharedState_.readyIndex.store(producerState_.writeIndex, std::memory_order_release);
            producerState_.writeIndex = invalidIndex;
            return true;
        }

        /**
         * @brief Check whether the producer currently owns a writable slot.
         *
         * @return true if the producer may write immediately without first
         *         reclaiming a slot; false otherwise.
         *
         * @warning Must be called only from the producer thread if you rely on
         *          it for control flow.
         */
        [[nodiscard]] bool hasWriteSlot() const noexcept
        {
            return producerState_.writeIndex != invalidIndex;
        }

        // ---------------------------------------------------------------------
        // Consumer API
        // ---------------------------------------------------------------------

        /**
         * @brief Try to acquire the next published frame.
         *
         * If a published frame is available, this function makes it the new
         * current read slot and returns the previously read slot back to the
         * producer.
         *
         * If no new frame is pending, this function returns false and the
         * consumer continues reading from the current slot.
         *
         * Memory ordering:
         *   - An acquire exchange on readyIndex synchronizes with the producer's
         *     release store in publishWrite().
         *   - This guarantees the consumer sees all writes performed by the
         *     producer before publication.
         *
         *   - A release store to returnedIndex synchronizes with the producer's
         *     acquire exchange in tryBeginWrite().
         *   - This guarantees the producer does not reuse the old read slot
         *     until the consumer has logically finished with it.
         *
         * @return true if a new frame was acquired, false if no published frame
         *         was available.
         *
         * @warning Must be called only from the consumer thread.
         */
        [[nodiscard]] bool tryAcquireRead() noexcept
        {
            const std::uint8_t next =
                sharedState_.readyIndex.exchange(invalidIndex, std::memory_order_acquire);

            if (next == invalidIndex) {
                return false;
            }

            const std::uint8_t old = consumerState_.readIndex;
            consumerState_.readIndex = next;

            sharedState_.returnedIndex.store(old, std::memory_order_release);
            return true;
        }

        /**
         * @brief Get the slot currently owned by the consumer for reading.
         *
         * This always returns a valid reference. If no new frame was acquired,
         * it refers to the previously active read slot.
         *
         * @return Reference to the current consumer-readable slot.
         *
         * @warning Must be called only from the consumer thread.
         */
        [[nodiscard]] const T& currentRead() const noexcept
        {
            assert(consumerState_.readIndex != invalidIndex);
            return slots_[consumerState_.readIndex];
        }

        /**
         * @brief Get the slot currently owned by the consumer for reading.
         *
         * This overload is provided only when mutable access is required on the
         * consumer side for internal housekeeping. Most consumers should prefer
         * the const overload.
         *
         * @return Reference to the current consumer-readable slot.
         *
         * @warning Must be called only from the consumer thread.
         */
        [[nodiscard]] T& currentRead() noexcept
        {
            assert(consumerState_.readIndex != invalidIndex);
            return slots_[consumerState_.readIndex];
        }

        // ---------------------------------------------------------------------
        // Shared status helpers
        // ---------------------------------------------------------------------

        /**
         * @brief Check whether a published frame is waiting to be acquired.
         *
         * This is primarily a diagnostic/helper method. Because producer and
         * consumer execute concurrently, the result is only a momentary snapshot.
         *
         * @return true if a published frame is pending, false otherwise.
         */
        [[nodiscard]] bool hasPendingFrame() const noexcept
        {
            return sharedState_.readyIndex.load(std::memory_order_acquire) != invalidIndex;
        }

    private:
        /**
         * @brief The three persistent frame slots.
         *
         * Slot ownership is transferred by index; the objects themselves are
         * never moved.
         */
        alignas(cacheLineSize) std::array<T, slotCount> slots_{};

        /**
         * @brief Shared synchronization state.
         */
        SharedState sharedState_{};

        /**
         * @brief Producer-local state.
         */
        ProducerState producerState_{};

        /**
         * @brief Consumer-local state.
         */
        ConsumerState consumerState_{};
    };

} // namespace lux::cxx