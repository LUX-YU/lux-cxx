#pragma once
#include <cassert>
#include <cstddef>
#include <utility>
#include <atomic>
#include <type_traits>

/**
 * Minimal, header‑only intrusive smart‑pointer implementation.
 *
 * Namespace  : lux::cxx
 * Primary API: `intrusive_ptr<T>`
 * Helper mix‑in: `intrusive_ref_counter<Derived, ThreadSafe>`
 *
 * This file is meant as a lightweight drop‑in for Boost's `intrusive_ptr`, but
 * with zero external dependencies and a cleaner modern‑C++17 interface.
 */
namespace lux::cxx {

    // --------------------------------------------------------------------------
    // ADL hooks the *user* must supply when NOT using the mix‑in below.
    // --------------------------------------------------------------------------
    //   void intrusive_ptr_add_ref(T*  ) noexcept;
    //   void intrusive_ptr_release (T*  ) noexcept;
    //
    // Each call *must not throw*.  Typical implementation increments/decrements an
    // internal (atomic) counter and `delete`s when it reaches zero.
    // --------------------------------------------------------------------------

    template <typename T> void intrusive_ptr_add_ref(T* p);
    template <typename T> void intrusive_ptr_release(T* p);

    // ═════════════════════════════════ intrusive_ptr ════════════════════════════

    template <typename T>
    class intrusive_ptr {
    public:
        using element_type = T;

        /* ───── construction ─────────────────────────────────────────────── */
        constexpr intrusive_ptr() noexcept : px_(nullptr) {}
        constexpr intrusive_ptr(std::nullptr_t) noexcept : px_(nullptr) {}

        explicit intrusive_ptr(T* p, bool add_ref = true) : px_(p) {
            if (px_ && add_ref) intrusive_ptr_add_ref(px_);
        }

        intrusive_ptr(const intrusive_ptr& rhs) : px_(rhs.px_) {
            if (px_) intrusive_ptr_add_ref(px_);
        }
        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        intrusive_ptr(const intrusive_ptr<U>& rhs) : px_(rhs.get()) {
            if (px_) intrusive_ptr_add_ref(px_);
        }

        intrusive_ptr(intrusive_ptr&& rhs) noexcept : px_(rhs.px_) {
            rhs.px_ = nullptr;
        }
        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
        intrusive_ptr(intrusive_ptr<U>&& rhs) noexcept : px_(rhs.get()) {
            rhs.reset();
        }

        /* ───── destruction ──────────────────────────────────────────────── */
        ~intrusive_ptr() {
            if (px_) intrusive_ptr_release(px_);
        }

        /* ───── assignment ───────────────────────────────────────────────── */
        intrusive_ptr& operator=(const intrusive_ptr& rhs) {
            intrusive_ptr(rhs).swap(*this);
            return *this;
        }
        template <typename U>
        intrusive_ptr& operator=(const intrusive_ptr<U>& rhs) {
            intrusive_ptr(rhs).swap(*this);
            return *this;
        }
        intrusive_ptr& operator=(intrusive_ptr&& rhs) noexcept {
            intrusive_ptr(std::move(rhs)).swap(*this);
            return *this;
        }
        template <typename U>
        intrusive_ptr& operator=(intrusive_ptr<U>&& rhs) noexcept {
            intrusive_ptr(std::move(rhs)).swap(*this);
            return *this;
        }
        intrusive_ptr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        /* ───── modifiers ────────────────────────────────────────────────── */
        void reset() noexcept { intrusive_ptr().swap(*this); }

        void reset(T* p, bool add_ref = true) { intrusive_ptr(p, add_ref).swap(*this); }

        void swap(intrusive_ptr& other) noexcept { std::swap(px_, other.px_); }

        /* ───── observers ────────────────────────────────────────────────── */
        T* get()       noexcept { return px_; }
        T* get() const noexcept { return px_; }   // NOTE: returns *non‑const* ptr — same as std::shared_ptr

        T& operator*()  const noexcept { return *px_; }
        T* operator->() const noexcept { return  px_; }

        explicit operator bool() const noexcept { return px_ != nullptr; }

    private:
        T* px_;
    };

    // ═════ relational operators (pointer‑like semantics) ═════

    template <typename T, typename U>
    bool operator==(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept { return a.get() == b.get(); }

    template <typename T, typename U>
    bool operator!=(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept { return a.get() != b.get(); }

    template <typename T>
    bool operator==(const intrusive_ptr<T>& a, std::nullptr_t) noexcept { return a.get() == nullptr; }

    template <typename T>
    bool operator==(std::nullptr_t, const intrusive_ptr<T>& a) noexcept { return nullptr == a.get(); }

    template <typename T>
    bool operator!=(const intrusive_ptr<T>& a, std::nullptr_t) noexcept { return a.get() != nullptr; }

    template <typename T>
    bool operator!=(std::nullptr_t, const intrusive_ptr<T>& a) noexcept { return nullptr != a.get(); }

    // enable ADL‑found swap
    template <typename T>
    void swap(intrusive_ptr<T>& lhs, intrusive_ptr<T>& rhs) noexcept { lhs.swap(rhs); }

    // ═════ pointer casts (mirroring <memory>) ═════

    template <class T, class U>
    intrusive_ptr<T> static_pointer_cast(const intrusive_ptr<U>& p) {
        return intrusive_ptr<T>(static_cast<T*>(p.get()));
    }

    template <class T, class U>
    intrusive_ptr<T> const_pointer_cast(const intrusive_ptr<U>& p) {
        return intrusive_ptr<T>(const_cast<T*>(p.get()));
    }

    template <class T, class U>
    intrusive_ptr<T> dynamic_pointer_cast(const intrusive_ptr<U>& p) {
        auto raw = p.get();
        if (auto q = dynamic_cast<T*>(raw)) {
            return intrusive_ptr<T>(q);
        }
        return intrusive_ptr<T>();
    }

    // ═════ helper mix‑in: intrusive_ref_counter ═════

    /**
     * Mix‑in that equips a class with an embedded reference counter and the
     * requisite `intrusive_ptr_add_ref/release` hooks (via ADL).
     *
     * ```cpp
     * class Texture : public lux::cxx::intrusive_ref_counter<Texture> { … };
     * ```
     *
     * Passing `ThreadSafe = false` trades thread‑safety for one instruction per
     * copy/decrement — useful for single‑threaded hot paths.
     */

    template <typename Derived, bool ThreadSafe = true>
    class intrusive_ref_counter {
    public:
        intrusive_ref_counter() noexcept : refcnt_(0) {}
        intrusive_ref_counter(const intrusive_ref_counter&) noexcept : refcnt_(0) {}
        intrusive_ref_counter& operator=(const intrusive_ref_counter&) noexcept { return *this; }
    protected:
        ~intrusive_ref_counter() = default;   // deletion always goes through Release
    private:
        using counter_type = std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t>;
        mutable counter_type refcnt_;

        friend void intrusive_ptr_add_ref(Derived* p) noexcept {
            if constexpr (ThreadSafe)
                p->refcnt_.fetch_add(1, std::memory_order_relaxed);
            else
                ++p->refcnt_;
        }
        friend void intrusive_ptr_release(Derived* p) noexcept {
            bool last;
            if constexpr (ThreadSafe) {
                // assert the prior count was non-zero: a 0->wraparound here means an
                // unbalanced release (or adopting a fresh object with add_ref=false),
                // which would otherwise silently leak (count wraps to SIZE_MAX) or
                // double-free. Loud in debug, no-op in release.
                const auto prev = p->refcnt_.fetch_sub(1, std::memory_order_acq_rel);
                assert(prev != 0 && "intrusive_ptr_release on a zero refcount");
                last = (prev == 1);
            } else {
                assert(p->refcnt_ != 0 && "intrusive_ptr_release on a zero refcount");
                last = (--p->refcnt_ == 0);
            }
            if (last) delete p;
        }
    };

    // ═════ canonical safe factory ═════
    // Constructs T and adopts it with add_ref=true (the ctor default), so the
    // 0-initialised intrusive_ref_counter reaches exactly 1. Prefer this over
    // `intrusive_ptr<T>(new T(...))`, and never adopt a freshly-new'd object with
    // add_ref=false (that leaves the count at 0 — see intrusive_ptr_release).
    template <class T, class... Args>
    [[nodiscard]] intrusive_ptr<T> make_intrusive(Args&&... args) {
        return intrusive_ptr<T>(new T(std::forward<Args>(args)...));
    }

} // namespace lux::cxx