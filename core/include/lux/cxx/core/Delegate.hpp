#pragma once

#include <cassert>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    template <typename Signature>
    class Delegate;

    template <typename Return, typename... Args>
    class Delegate<Return(Args...)> final
    {
      public:
        using function_type = Return(Args...);

        constexpr Delegate() noexcept = default;
        constexpr Delegate(std::nullptr_t) noexcept
        {
        }

        [[nodiscard]] static constexpr Delegate fromFunction(
            Return (*function)(Args...)
        ) noexcept
        {
            Delegate result;
            result.target_.function = function;
            result.invoke_ = &invokeFunction;
            return result;
        }

        template <auto Function>
        [[nodiscard]] static constexpr Delegate bind() noexcept
        {
            static_assert(std::is_invocable_r_v<Return, decltype(Function), Args...>);
            Delegate result;
            result.invoke_ = [](Target, Args&&... args) -> Return
            {
                if constexpr (std::is_void_v<Return>)
                {
                    std::invoke(Function, std::forward<Args>(args)...);
                }
                else
                {
                    return std::invoke(Function, std::forward<Args>(args)...);
                }
            };
            return result;
        }

        template <auto Method, typename Object>
        [[nodiscard]] static constexpr Delegate bind(Object& object) noexcept
        {
            static_assert(
                std::is_invocable_r_v<Return, decltype(Method), Object&, Args...>
            );
            Delegate result;
            result.target_.object = std::addressof(object);
            result.invoke_ = [](Target target, Args&&... args) -> Return
            {
                auto* value = static_cast<Object*>(
                    const_cast<void*>(target.object)
                );
                if constexpr (std::is_void_v<Return>)
                {
                    std::invoke(
                        Method,
                        *value,
                        std::forward<Args>(args)...
                    );
                }
                else
                {
                    return std::invoke(
                        Method,
                        *value,
                        std::forward<Args>(args)...
                    );
                }
            };
            return result;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return invoke_ != nullptr;
        }

        constexpr Return operator()(Args... args) const
        {
            assert(invoke_ != nullptr && "Delegate is empty");
            if constexpr (std::is_void_v<Return>)
            {
                invoke_(target_, std::forward<Args>(args)...);
            }
            else
            {
                return invoke_(target_, std::forward<Args>(args)...);
            }
        }

        constexpr void reset() noexcept
        {
            target_.object = nullptr;
            invoke_ = nullptr;
        }

      private:
        union Target
        {
            const void* object;
            Return (*function)(Args...);

            constexpr Target() noexcept
                : object(nullptr)
            {
            }
        } target_{};

        using Invoker = Return (*)(Target, Args&&...);

        static constexpr Return invokeFunction(Target target, Args&&... args)
        {
            if constexpr (std::is_void_v<Return>)
            {
                std::invoke(
                    target.function,
                    std::forward<Args>(args)...
                );
            }
            else
            {
                return std::invoke(
                    target.function,
                    std::forward<Args>(args)...
                );
            }
        }

        Invoker invoke_ = nullptr;
    };
} // namespace lux::cxx
