#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    namespace detail
    {
        template <typename Signature, typename Return, bool Noexcept, typename... Args> class TFunctionRef
        {
        public:
            using function_type = Signature;
            using FunctionPointer = Signature*;

            TFunctionRef(FunctionPointer function) noexcept : target_(function), invoke_(&invokeFunction) {}

            template <typename Callable>
                requires(!std::is_function_v<Callable> &&
                         !std::is_base_of_v<TFunctionRef, std::remove_cv_t<Callable>> &&
                         (std::is_invocable_r_v<Return, Callable&, Args...> &&
                          (!Noexcept || std::is_nothrow_invocable_r_v<Return, Callable&, Args...>)))
            TFunctionRef(Callable& callable) noexcept
                : target_(std::addressof(callable)), invoke_(&invokeObject<Callable>)
            {
            }

            TFunctionRef(const TFunctionRef&) noexcept = default;
            TFunctionRef& operator=(const TFunctionRef&) noexcept = default;

            Return operator()(Args... args) const noexcept(Noexcept)
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
                FunctionPointer function;

                constexpr Target(const void* value) noexcept : object(value) {}

                constexpr Target(FunctionPointer value) noexcept : function(value) {}
            };

            using Invoker = Return (*)(Target, Args&&...) noexcept(Noexcept);

            static Return invokeFunction(Target target, Args&&... args) noexcept(Noexcept)
            {
                if constexpr (std::is_void_v<Return>)
                {
                    std::invoke(target.function, std::forward<Args>(args)...);
                }
                else
                {
                    return std::invoke(target.function, std::forward<Args>(args)...);
                }
            }

            template <typename Callable> static Return invokeObject(Target target, Args&&... args) noexcept(Noexcept)
            {
                auto* callable = static_cast<Callable*>(const_cast<void*>(target.object));
                if constexpr (std::is_void_v<Return>)
                {
                    std::invoke(*callable, std::forward<Args>(args)...);
                }
                else
                {
                    return std::invoke(*callable, std::forward<Args>(args)...);
                }
            }

            Target target_;
            Invoker invoke_;
        };
        template <typename Signature> struct TFunctionRefType;
        template <typename Return, typename... Args> struct TFunctionRefType<Return(Args...)>
        {
            using Type = TFunctionRef<Return(Args...), Return, false, Args...>;
        };
        template <typename Return, typename... Args> struct TFunctionRefType<Return(Args...) noexcept>
        {
            using Type = TFunctionRef<Return(Args...) noexcept, Return, true, Args...>;
        };

    } // namespace detail
    template <typename Signature> class function_ref final : public detail::TFunctionRefType<Signature>::Type
    {
        using Base = typename detail::TFunctionRefType<Signature>::Type;

    public:
        using Base::Base;
        function_ref(const function_ref&) noexcept = default;
        function_ref& operator=(const function_ref&) noexcept = default;
    };
} // namespace lux::cxx
