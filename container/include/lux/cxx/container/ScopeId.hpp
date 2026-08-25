#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace lux::cxx
{
    template <class Tag>
    class ScopeIdSource;

    /**
     * Process-local identity for one logical container instance.
     *
     * ScopeId is deliberately independent from SlotKey: SlotKey identifies a
     * generation within one container, while ScopeId distinguishes peer
     * containers. It is runtime-only and must never be persisted.
     */
    template <class Tag>
    class ScopeId final
    {
    public:
        constexpr ScopeId() noexcept = default;

        [[nodiscard]] constexpr bool isNull() const noexcept
        {
            return value_ == 0U;
        }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return !isNull();
        }

        [[nodiscard]] constexpr bool operator==(
            const ScopeId&
        ) const noexcept = default;

        struct Hash final
        {
            [[nodiscard]] constexpr std::size_t operator()(
                ScopeId value
            ) const noexcept
            {
                return std::hash<std::uint64_t>{}(value.value_);
            }
        };

    private:
        explicit constexpr ScopeId(std::uint64_t value) noexcept
            : value_(value)
        {
        }

        std::uint64_t value_{};

        friend class ScopeIdSource<Tag>;
    };

    /**
     * Monotonic source for a ScopeId domain.
     *
     * The owning compiled module must keep exactly one source for a Tag. This
     * avoids one inline counter per consumer DLL while retaining a tiny generic
     * value type. Exhaustion is an unrecoverable process invariant violation.
     */
    template <class Tag>
    class ScopeIdSource final
    {
    public:
        ScopeIdSource() noexcept = default;

        ScopeIdSource(const ScopeIdSource&) = delete;
        ScopeIdSource& operator=(const ScopeIdSource&) = delete;
        ScopeIdSource(ScopeIdSource&&) = delete;
        ScopeIdSource& operator=(ScopeIdSource&&) = delete;

        [[nodiscard]] ScopeId<Tag> acquire() noexcept
        {
            const std::uint64_t value = next_.fetch_add(
                1U,
                std::memory_order_relaxed
            );
            if (value == 0U)
                std::abort();
            return ScopeId<Tag>(value);
        }

    private:
        std::atomic<std::uint64_t> next_{1U};
    };
}
