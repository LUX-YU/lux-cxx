#pragma once

#include <cstddef>
#include <cstring>
#include <concepts>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace lux::cxx::event
{

    // ── SlotCallback<E> ──────────────────────────────────────────
    // SBO (Small Buffer Optimization) type-erased callback.
    // Stores callables ≤ kSBOSize inline; larger ones go to heap.
    //
    // Fast path: when constructed from (void*, void(*)(void*, const E&)),
    // invoke is a direct function-pointer call with zero overhead.

    template <typename E>
    class SlotCallback
    {
    public:
        static constexpr std::size_t kSBOSize  = 48;
        static constexpr std::size_t kSBOAlign = alignof(std::max_align_t);

        // ── Vtable ───────────────────────────────────────────
        struct Ops
        {
            void (*invoke)(const void *storage, const E &event);
            void (*move_construct)(void *dst, void *src);
            void (*destroy)(void *storage);
        };

        // ── Default (empty) ──────────────────────────────────

        SlotCallback() noexcept : ops_(nullptr) {}

        // ── From function pointer + context (zero overhead) ──

        using RawFn = void (*)(void *, const E &);

        SlotCallback(void *ctx, RawFn fn) noexcept
            : ops_(&kRawFnOps)
        {
            auto &s = raw_slot();
            s.ctx = ctx;
            s.fn  = fn;
        }

        // ── From any callable ────────────────────────────────

        template <typename F>
            requires std::invocable<F, const E &>
                  && (!std::same_as<std::decay_t<F>, SlotCallback>)
        SlotCallback(F &&f)
        {
            using Decay = std::decay_t<F>;

            if constexpr (sizeof(Decay) <= kSBOSize
                       && alignof(Decay) <= kSBOAlign
                       && std::is_nothrow_move_constructible_v<Decay>)
            {
                // SBO: construct in-place
                ::new (static_cast<void *>(storage_)) Decay(std::forward<F>(f));
                ops_ = &sbo_ops<Decay>();
            }
            else
            {
                // Heap allocation
                auto *p = new Decay(std::forward<F>(f));
                *reinterpret_cast<Decay **>(storage_) = p;
                ops_ = &heap_ops<Decay>();
            }
        }

        // ── Move ─────────────────────────────────────────────

        SlotCallback(SlotCallback &&o) noexcept
            : ops_(o.ops_)
        {
            if (ops_ && ops_->move_construct)
                ops_->move_construct(storage_, o.storage_);
            else
                std::memcpy(storage_, o.storage_, kSBOSize); // raw slot or trivial
            o.ops_ = nullptr;
        }

        SlotCallback &operator=(SlotCallback &&o) noexcept
        {
            if (this != &o)
            {
                destroy();
                ops_ = o.ops_;
                if (ops_ && ops_->move_construct)
                    ops_->move_construct(storage_, o.storage_);
                else if (ops_)
                    std::memcpy(storage_, o.storage_, kSBOSize);
                o.ops_ = nullptr;
            }
            return *this;
        }

        // ── Non-copyable ────────────────────────────────────

        SlotCallback(const SlotCallback &) = delete;
        SlotCallback &operator=(const SlotCallback &) = delete;

        // ── Destructor ───────────────────────────────────────

        ~SlotCallback() { destroy(); }

        // ── Invoke ───────────────────────────────────────────

        void operator()(const E &event) const
        {
            ops_->invoke(storage_, event);
        }

        explicit operator bool() const noexcept { return ops_ != nullptr; }

    private:
        alignas(kSBOAlign) unsigned char storage_[kSBOSize]{};
        const Ops *ops_ = nullptr;

        void destroy()
        {
            if (ops_ && ops_->destroy)
                ops_->destroy(storage_);
            ops_ = nullptr;
        }

        // ── Raw function-pointer layout ──────────────────────
        struct RawSlot
        {
            void *ctx;
            RawFn fn;
        };
        static_assert(sizeof(RawSlot) <= kSBOSize);

        RawSlot       &raw_slot()       { return *reinterpret_cast<RawSlot *>(storage_); }
        const RawSlot &raw_slot() const { return *reinterpret_cast<const RawSlot *>(storage_); }

        static inline const Ops kRawFnOps{
            // invoke
            [](const void *storage, const E &event)
            {
                auto &s = *reinterpret_cast<const RawSlot *>(storage);
                s.fn(s.ctx, event);
            },
            nullptr, // move: trivial memcpy
            nullptr  // destroy: nothing to do
        };

        // ── SBO ops for type F ───────────────────────────────

        template <typename F>
        static const Ops &sbo_ops()
        {
            static const Ops ops{
                // invoke
                [](const void *storage, const E &event)
                {
                    const auto &fn = *reinterpret_cast<const F *>(storage);
                    fn(event);
                },
                // move_construct
                [](void *dst, void *src)
                {
                    ::new (dst) F(std::move(*reinterpret_cast<F *>(src)));
                    reinterpret_cast<F *>(src)->~F();
                },
                // destroy
                [](void *storage)
                {
                    reinterpret_cast<F *>(storage)->~F();
                }
            };
            return ops;
        }

        // ── Heap ops for type F ──────────────────────────────

        template <typename F>
        static const Ops &heap_ops()
        {
            static const Ops ops{
                // invoke
                [](const void *storage, const E &event)
                {
                    const auto *p = *reinterpret_cast<F *const *>(storage);
                    (*p)(event);
                },
                // move_construct (move the pointer)
                [](void *dst, void *src)
                {
                    auto *&sp = *reinterpret_cast<F **>(src);
                    *reinterpret_cast<F **>(dst) = sp;
                    sp = nullptr;
                },
                // destroy
                [](void *storage)
                {
                    auto *p = *reinterpret_cast<F **>(storage);
                    delete p;
                }
            };
            return ops;
        }
    };

} // namespace lux::cxx::event
