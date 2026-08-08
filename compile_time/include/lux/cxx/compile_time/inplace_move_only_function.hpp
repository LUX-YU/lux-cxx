#pragma once

#include <cassert>
#include <cstddef>
#include <concepts>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    /// Move-only callable with fixed inline storage and no heap fallback.
    template <class Signature, std::size_t InlineBytes,
              std::size_t InlineAlign = alignof(std::max_align_t)>
    class inplace_move_only_function;

    template <class R, class... Args, std::size_t InlineBytes,
              std::size_t InlineAlign>
    class inplace_move_only_function<
        R(Args...),
        InlineBytes,
        InlineAlign> final
    {
    public:
        inplace_move_only_function() noexcept = default;

        template <class F>
        requires (
            !std::same_as<std::remove_cvref_t<F>, inplace_move_only_function> &&
            std::is_nothrow_invocable_r_v<R, std::decay_t<F>&, Args...> &&
            std::is_nothrow_move_constructible_v<std::decay_t<F>> &&
            sizeof(std::decay_t<F>) <= InlineBytes &&
            alignof(std::decay_t<F>) <= InlineAlign)
        inplace_move_only_function(F&& function) noexcept(
            std::is_nothrow_constructible_v<std::decay_t<F>, F&&>)
        {
            emplace<std::decay_t<F>>(std::forward<F>(function));
        }

        inplace_move_only_function(const inplace_move_only_function&) = delete;
        inplace_move_only_function& operator=(
            const inplace_move_only_function&) = delete;

        inplace_move_only_function(
            inplace_move_only_function&& other) noexcept
        {
            moveFrom(other);
        }

        inplace_move_only_function& operator=(
            inplace_move_only_function&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                moveFrom(other);
            }
            return *this;
        }

        ~inplace_move_only_function()
        {
            reset();
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return ops_ != nullptr;
        }

        R operator()(Args... args) noexcept
        {
            assert(ops_ != nullptr && "inplace_move_only_function is empty");
            if constexpr (std::is_void_v<R>)
            {
                ops_->invoke(storage_, std::forward<Args>(args)...);
            }
            else
            {
                return ops_->invoke(storage_, std::forward<Args>(args)...);
            }
        }

        void reset() noexcept
        {
            if (ops_ == nullptr) return;
            ops_->destroy(storage_);
            ops_ = nullptr;
        }

    private:
        struct Ops
        {
            R (*invoke)(void*, Args&&...) noexcept;
            void (*move)(void*, void*) noexcept;
            void (*destroy)(void*) noexcept;
        };

        template <class F>
        static const Ops& operations() noexcept
        {
            static constexpr Ops value{
                [](void* object, Args&&... args) noexcept -> R
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        std::invoke(
                            *static_cast<F*>(object),
                            std::forward<Args>(args)...
                        );
                    }
                    else
                    {
                        return std::invoke(
                            *static_cast<F*>(object),
                            std::forward<Args>(args)...
                        );
                    }
                },
                [](void* destination, void* source) noexcept
                {
                    F* value = static_cast<F*>(source);
                    std::construct_at(
                        static_cast<F*>(destination),
                        std::move(*value)
                    );
                    std::destroy_at(value);
                },
                [](void* object) noexcept
                {
                    std::destroy_at(static_cast<F*>(object));
                }
            };
            return value;
        }

        template <class F, class Source>
        void emplace(Source&& source) noexcept(
            std::is_nothrow_constructible_v<F, Source&&>)
        {
            std::construct_at(
                reinterpret_cast<F*>(storage_),
                std::forward<Source>(source)
            );
            ops_ = &operations<F>();
        }

        void moveFrom(inplace_move_only_function& other) noexcept
        {
            if (other.ops_ == nullptr) return;
            ops_ = other.ops_;
            ops_->move(storage_, other.storage_);
            other.ops_ = nullptr;
        }

        alignas(InlineAlign) std::byte storage_[InlineBytes]{};
        const Ops* ops_ = nullptr;
    };
} // namespace lux::cxx
