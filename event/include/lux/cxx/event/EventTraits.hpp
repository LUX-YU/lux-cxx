#pragma once

#include <cstdint>
#include <concepts>
#include <type_traits>
#include "EventTypeId.hpp"

namespace lux::cxx::event
{
    // ── Event Concepts ───────────────────────────────────────────

    /// Basic event: movable and destructible.
    template <typename E>
    concept Event = std::movable<E> && std::destructible<E>;

    /// Inline event: small, trivially copyable — fits in inline storage.
    template <typename E>
    concept InlineEvent = Event<E> && std::is_trivially_copyable_v<E> && (sizeof(E) <= 64) && (alignof(E) <= alignof(std::max_align_t));

    /// Cross-thread event: must be trivially copyable for safe memcpy across threads.
    template <typename E>
    concept CrossThreadEvent = Event<E> && std::is_trivially_copyable_v<E>;

    // ── Event Traits ─────────────────────────────────────────────
    // Users may specialize for specific event types to add domain-specific attributes.

    template <Event E>
    struct EventTraits
    {
        static constexpr bool allow_cross_thread = std::is_trivially_copyable_v<E>;
    };

} // namespace lux::cxx::event
