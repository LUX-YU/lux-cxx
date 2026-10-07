#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    namespace detail
    {
        template <typename Return, bool Noexcept, typename... Args> class TMoveOnlyFunction
        {
        public:
            static constexpr std::size_t inplace_size = 32;
            static constexpr std::size_t inplace_alignment = alignof(std::max_align_t);

            template <typename Callable>
            static constexpr bool stores_inplace = sizeof(std::decay_t<Callable>) <= inplace_size &&
                                                   alignof(std::decay_t<Callable>) <= inplace_alignment &&
                                                   std::is_nothrow_move_constructible_v<std::decay_t<Callable>>;

            constexpr TMoveOnlyFunction() noexcept = default;
            constexpr TMoveOnlyFunction(std::nullptr_t) noexcept {}

            template <typename Callable>
                requires(
                    !std::is_base_of_v<TMoveOnlyFunction, std::remove_cvref_t<Callable>> &&
                    (std::is_invocable_r_v<Return, std::decay_t<Callable>&, Args...> &&
                     (!Noexcept || std::is_nothrow_invocable_r_v<Return, std::decay_t<Callable>&, Args...>))
                )
            TMoveOnlyFunction(Callable&& callable)
            {
                emplace<std::decay_t<Callable>>(std::forward<Callable>(callable));
            }

            TMoveOnlyFunction(const TMoveOnlyFunction&) = delete;
            TMoveOnlyFunction& operator=(const TMoveOnlyFunction&) = delete;

            TMoveOnlyFunction(TMoveOnlyFunction&& other) noexcept
            {
                moveFrom(other);
            }

            TMoveOnlyFunction& operator=(TMoveOnlyFunction&& other) noexcept
            {
                if (this != &other)
                {
                    reset();
                    moveFrom(other);
                }
                return *this;
            }

            template <typename Callable>
                requires(
                    !std::is_base_of_v<TMoveOnlyFunction, std::remove_cvref_t<Callable>> &&
                    (std::is_invocable_r_v<Return, std::decay_t<Callable>&, Args...> &&
                     (!Noexcept || std::is_nothrow_invocable_r_v<Return, std::decay_t<Callable>&, Args...>))
                )
            TMoveOnlyFunction& operator=(Callable&& callable)
            {
                TMoveOnlyFunction temporary(std::forward<Callable>(callable));
                *this = std::move(temporary);
                return *this;
            }

            TMoveOnlyFunction& operator=(std::nullptr_t) noexcept
            {
                reset();
                return *this;
            }

            ~TMoveOnlyFunction()
            {
                reset();
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return manager_ != nullptr;
            }

            Return operator()(Args... args) noexcept(Noexcept)
            {
                assert(manager_ != nullptr && "TMoveOnlyFunction is empty");
                if constexpr (std::is_void_v<Return>)
                {
                    manager_->invoke(storageAddress(), std::forward<Args>(args)...);
                }
                else
                {
                    return manager_->invoke(storageAddress(), std::forward<Args>(args)...);
                }
            }

            void reset() noexcept
            {
                if (manager_ == nullptr)
                {
                    return;
                }
                manager_->destroy(storageAddress());
                manager_ = nullptr;
            }

            void swap(TMoveOnlyFunction& other) noexcept
            {
                if (this == &other)
                {
                    return;
                }
                TMoveOnlyFunction temporary(std::move(other));
                other = std::move(*this);
                *this = std::move(temporary);
            }

        private:
            struct Manager final
            {
                void (*destroy)(void*) noexcept;
                void (*move)(void*, void*) noexcept;
                Return (*invoke)(void*, Args&&...) noexcept(Noexcept);
                bool heap;
            };

            union Storage
            {
                void* pointer;
                alignas(inplace_alignment) std::byte bytes[inplace_size];

                constexpr Storage() noexcept : pointer(nullptr) {}
            } storage_{};

            template <typename Callable> [[nodiscard]] static const Manager& inplaceManager() noexcept
            {
                static const Manager manager{
                    [](void* storage) noexcept { std::destroy_at(static_cast<Callable*>(storage)); },
                    [](void* destination, void* source) noexcept
                    {
                        auto* callable = static_cast<Callable*>(source);
                        std::construct_at(static_cast<Callable*>(destination), std::move(*callable));
                        std::destroy_at(callable);
                    },
                    [](void* storage, Args&&... args) noexcept(Noexcept) -> Return
                    {
                        if constexpr (std::is_void_v<Return>)
                        {
                            std::invoke(*static_cast<Callable*>(storage), std::forward<Args>(args)...);
                        }
                        else
                        {
                            return std::invoke(*static_cast<Callable*>(storage), std::forward<Args>(args)...);
                        }
                    },
                    false
                };
                return manager;
            }

            template <typename Callable> [[nodiscard]] static const Manager& heapManager() noexcept
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
                    [](void* storage, Args&&... args) noexcept(Noexcept) -> Return
                    {
                        auto** pointer = static_cast<void**>(storage);
                        if constexpr (std::is_void_v<Return>)
                        {
                            std::invoke(*static_cast<Callable*>(*pointer), std::forward<Args>(args)...);
                        }
                        else
                        {
                            return std::invoke(*static_cast<Callable*>(*pointer), std::forward<Args>(args)...);
                        }
                    },
                    true
                };
                return manager;
            }

            template <typename Callable, typename Source> void emplace(Source&& source)
            {
                if constexpr (stores_inplace<Callable>)
                {
                    std::construct_at(reinterpret_cast<Callable*>(storage_.bytes), std::forward<Source>(source));
                    manager_ = &inplaceManager<Callable>();
                }
                else
                {
                    storage_.pointer = new Callable(std::forward<Source>(source));
                    manager_ = &heapManager<Callable>();
                }
            }

            [[nodiscard]] void* storageAddress() noexcept
            {
                return manager_ != nullptr && manager_->heap ? static_cast<void*>(&storage_.pointer)
                                                             : static_cast<void*>(storage_.bytes);
            }

            void moveFrom(TMoveOnlyFunction& other) noexcept
            {
                if (other.manager_ == nullptr)
                {
                    return;
                }
                manager_ = other.manager_;
                manager_->move(storageAddress(), other.storageAddress());
                other.manager_ = nullptr;
            }

            const Manager* manager_ = nullptr;
        };
        template <typename Signature> struct TMoveOnlyFunctionType;
        template <typename Return, typename... Args> struct TMoveOnlyFunctionType<Return(Args...)>
        {
            using Type = TMoveOnlyFunction<Return, false, Args...>;
        };
        template <typename Return, typename... Args> struct TMoveOnlyFunctionType<Return(Args...) noexcept>
        {
            using Type = TMoveOnlyFunction<Return, true, Args...>;
        };

    } // namespace detail
    template <typename Signature> class move_only_function final : public detail::TMoveOnlyFunctionType<Signature>::Type
    {
        using Base = typename detail::TMoveOnlyFunctionType<Signature>::Type;

    public:
        using Base::Base;
        constexpr move_only_function() noexcept = default;
        move_only_function(const move_only_function&) = delete;
        move_only_function& operator=(const move_only_function&) = delete;
        move_only_function(move_only_function&&) noexcept = default;
        move_only_function& operator=(move_only_function&&) noexcept = default;
        template <typename Callable>
            requires(!std::same_as<std::remove_cvref_t<Callable>, move_only_function> && std::is_constructible_v<Base, Callable>)
        move_only_function& operator=(Callable&& callable)
        {
            Base::operator=(std::forward<Callable>(callable));
            return *this;
        }
        move_only_function& operator=(std::nullptr_t) noexcept
        {
            Base::operator=(nullptr);
            return *this;
        }
    };
    template <typename Signature>
    void swap(move_only_function<Signature>& left, move_only_function<Signature>& right) noexcept
    {
        left.swap(right);
    }
} // namespace lux::cxx
