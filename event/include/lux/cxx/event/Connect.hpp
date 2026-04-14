#pragma once

#include "Signal.hpp"

namespace lux::cxx::event
{

    // ── connect() free functions ─────────────────────────────────
    // Qt-style API for connecting signals to slots.

    // ── Member function ──────────────────────────────────────
    //   connect<&MyApp::onResize>(window.on_resize, this)

    template <auto MemFn, Event E, typename T>
        requires std::is_member_function_pointer_v<decltype(MemFn)>
              && std::is_invocable_v<decltype(MemFn), T *, const E &>
    [[nodiscard]] ScopedConnection connect(Signal<E> &signal, T *receiver, int16_t priority = 0)
    {
        return signal.template connect<MemFn>(receiver, priority);
    }

    // ── Lambda / callable ────────────────────────────────────
    //   connect(window.on_key, [](const KeyEvent& e) { ... })

    template <Event E, typename F>
        requires std::invocable<F, const E &>
    [[nodiscard]] ScopedConnection connect(Signal<E> &signal, F &&slot, int16_t priority = 0)
    {
        return signal.connect(SlotCallback<E>{std::forward<F>(slot)}, priority);
    }

    // ── Lambda + receiver (for Trackable support) ────────────
    //   connect(window.on_key, this, [this](const KeyEvent& e) { ... })

    template <Event E, typename T, typename F>
        requires std::invocable<F, const E &>
              && (!std::is_same_v<std::decay_t<T>, int16_t>) // avoid ambiguity with priority
    [[nodiscard]] ScopedConnection connect(Signal<E> &signal, T *receiver, F &&slot, int16_t priority = 0)
    {
        // receiver is available for Trackable auto-disconnect.
        (void)receiver;
        return signal.connect(SlotCallback<E>{std::forward<F>(slot)}, priority);
    }

} // namespace lux::cxx::event
