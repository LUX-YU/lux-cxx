#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    template <typename Signature>
    class function_ref;

    template <typename Return, typename... Args>
    class function_ref<Return(Args...)> final
    {
      public:
        using function_type = Return(Args...);

        function_ref(Return (*function)(Args...)) noexcept
            : target_(function), invoke_(&invokeFunction)
        {
        }

        template <typename Callable>
        requires (
            !std::is_function_v<Callable> &&
            !std::same_as<std::remove_cv_t<Callable>, function_ref> &&
            std::is_invocable_r_v<Return, Callable&, Args...>
        )
        function_ref(Callable& callable) noexcept
            : target_(std::addressof(callable)),
              invoke_(&invokeObject<Callable>)
        {
        }

        function_ref(const function_ref&) noexcept = default;
        function_ref& operator=(const function_ref&) noexcept = default;

        Return operator()(Args... args) const
        {
            if constexpr (std::is_void_v<Return>)
            {
                invoke_(target_, std::forward<Args>(args)...);
            }
            else
            {
                return invoke_(target_, std::forward<Args>(args)...);
            }
        }

      private:
        union Target
        {
            const void* object;
            Return (*function)(Args...);

            constexpr Target(const void* value) noexcept
                : object(value)
            {
            }

            constexpr Target(Return (*value)(Args...)) noexcept
                : function(value)
            {
            }
        };

        using Invoker = Return (*)(Target, Args&&...);

        static Return invokeFunction(Target target, Args&&... args)
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

        template <typename Callable>
        static Return invokeObject(Target target, Args&&... args)
        {
            auto* callable = static_cast<Callable*>(
                const_cast<void*>(target.object)
            );
            if constexpr (std::is_void_v<Return>)
            {
                std::invoke(
                    *callable,
                    std::forward<Args>(args)...
                );
            }
            else
            {
                return std::invoke(
                    *callable,
                    std::forward<Args>(args)...
                );
            }
        }

        Target target_;
        Invoker invoke_;
    };
} // namespace lux::cxx
