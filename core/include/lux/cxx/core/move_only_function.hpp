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
    template <typename Signature>
    class move_only_function;

    template <typename Return, typename... Args>
    class move_only_function<Return(Args...)> final
    {
      public:
        static constexpr std::size_t inplace_size = 32;
        static constexpr std::size_t inplace_alignment = alignof(std::max_align_t);

        template <typename Callable>
        static constexpr bool stores_inplace =
            sizeof(std::decay_t<Callable>) <= inplace_size &&
            alignof(std::decay_t<Callable>) <= inplace_alignment &&
            std::is_nothrow_move_constructible_v<std::decay_t<Callable>>;

        constexpr move_only_function() noexcept = default;
        constexpr move_only_function(std::nullptr_t) noexcept
        {
        }

        template <typename Callable>
        requires (
            !std::same_as<std::remove_cvref_t<Callable>, move_only_function> &&
            std::is_invocable_r_v<Return, std::decay_t<Callable>&, Args...>
        )
        move_only_function(Callable&& callable)
        {
            emplace<std::decay_t<Callable>>(
                std::forward<Callable>(callable)
            );
        }

        move_only_function(const move_only_function&) = delete;
        move_only_function& operator=(const move_only_function&) = delete;

        move_only_function(move_only_function&& other) noexcept
        {
            moveFrom(other);
        }

        move_only_function& operator=(move_only_function&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                moveFrom(other);
            }
            return *this;
        }

        template <typename Callable>
        requires (
            !std::same_as<std::remove_cvref_t<Callable>, move_only_function> &&
            std::is_invocable_r_v<Return, std::decay_t<Callable>&, Args...>
        )
        move_only_function& operator=(Callable&& callable)
        {
            move_only_function temporary(
                std::forward<Callable>(callable)
            );
            *this = std::move(temporary);
            return *this;
        }

        move_only_function& operator=(std::nullptr_t) noexcept
        {
            reset();
            return *this;
        }

        ~move_only_function()
        {
            reset();
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return manager_ != nullptr;
        }

        Return operator()(Args... args)
        {
            assert(manager_ != nullptr && "move_only_function is empty");
            if constexpr (std::is_void_v<Return>)
            {
                manager_->invoke(
                    storageAddress(),
                    std::forward<Args>(args)...
                );
            }
            else
            {
                return manager_->invoke(
                    storageAddress(),
                    std::forward<Args>(args)...
                );
            }
        }

        void reset() noexcept
        {
            if (manager_ == nullptr) return;
            manager_->destroy(storageAddress());
            manager_ = nullptr;
        }

        void swap(move_only_function& other) noexcept
        {
            if (this == &other) return;
            move_only_function temporary(std::move(other));
            other = std::move(*this);
            *this = std::move(temporary);
        }

      private:
        struct Manager final
        {
            void (*destroy)(void*) noexcept;
            void (*move)(void*, void*) noexcept;
            Return (*invoke)(void*, Args&&...);
            bool heap;
        };

        union Storage
        {
            void* pointer;
            alignas(inplace_alignment) std::byte bytes[inplace_size];

            constexpr Storage() noexcept
                : pointer(nullptr)
            {
            }
        } storage_{};

        template <typename Callable>
        [[nodiscard]] static const Manager& inplaceManager() noexcept
        {
            static const Manager manager{
                [](void* storage) noexcept
                {
                    std::destroy_at(static_cast<Callable*>(storage));
                },
                [](void* destination, void* source) noexcept
                {
                    auto* callable = static_cast<Callable*>(source);
                    std::construct_at(
                        static_cast<Callable*>(destination),
                        std::move(*callable)
                    );
                    std::destroy_at(callable);
                },
                [](void* storage, Args&&... args) -> Return
                {
                    if constexpr (std::is_void_v<Return>)
                    {
                        std::invoke(
                            *static_cast<Callable*>(storage),
                            std::forward<Args>(args)...
                        );
                    }
                    else
                    {
                        return std::invoke(
                            *static_cast<Callable*>(storage),
                            std::forward<Args>(args)...
                        );
                    }
                },
                false
            };
            return manager;
        }

        template <typename Callable>
        [[nodiscard]] static const Manager& heapManager() noexcept
        {
            static const Manager manager{
                [](void* storage) noexcept
                {
                    auto** pointer = static_cast<void**>(storage);
                    delete static_cast<Callable*>(*pointer);
                },
                [](void* destination, void* source) noexcept
                {
                    auto** destination_pointer = static_cast<void**>(destination);
                    auto** source_pointer = static_cast<void**>(source);
                    *destination_pointer = *source_pointer;
                    *source_pointer = nullptr;
                },
                [](void* storage, Args&&... args) -> Return
                {
                    auto** pointer = static_cast<void**>(storage);
                    if constexpr (std::is_void_v<Return>)
                    {
                        std::invoke(
                            *static_cast<Callable*>(*pointer),
                            std::forward<Args>(args)...
                        );
                    }
                    else
                    {
                        return std::invoke(
                            *static_cast<Callable*>(*pointer),
                            std::forward<Args>(args)...
                        );
                    }
                },
                true
            };
            return manager;
        }

        template <typename Callable, typename Source>
        void emplace(Source&& source)
        {
            if constexpr (stores_inplace<Callable>)
            {
                std::construct_at(
                    reinterpret_cast<Callable*>(storage_.bytes),
                    std::forward<Source>(source)
                );
                manager_ = &inplaceManager<Callable>();
            }
            else
            {
                storage_.pointer = new Callable(
                    std::forward<Source>(source)
                );
                manager_ = &heapManager<Callable>();
            }
        }

        [[nodiscard]] void* storageAddress() noexcept
        {
            return manager_ != nullptr && manager_->heap
                ? static_cast<void*>(&storage_.pointer)
                : static_cast<void*>(storage_.bytes);
        }

        void moveFrom(move_only_function& other) noexcept
        {
            if (other.manager_ == nullptr) return;
            manager_ = other.manager_;
            manager_->move(storageAddress(), other.storageAddress());
            other.manager_ = nullptr;
        }

        const Manager* manager_ = nullptr;
    };

    template <typename Return, typename... Args>
    void swap(
        move_only_function<Return(Args...)>& left,
        move_only_function<Return(Args...)>& right
    ) noexcept
    {
        left.swap(right);
    }
} // namespace lux::cxx
