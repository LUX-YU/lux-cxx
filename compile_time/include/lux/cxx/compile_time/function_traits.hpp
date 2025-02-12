#pragma once
#include <type_traits>
#include <tuple>

namespace lux::cxx
{
    template<typename T>
    struct function_traits_impl;

    template<typename Ret, typename... Args>
    struct function_traits_impl<Ret(Args...)> {
        using return_type = Ret;
        using argument_types = std::tuple<Args...>;
    };

    template<typename Ret, typename... Args>
    struct function_traits_impl<Ret(*)(Args...)> : function_traits_impl<Ret(Args...)> {};

    template<typename Ret, typename Class, typename... Args>
    struct function_traits_impl<Ret(Class::*)(Args...)> : function_traits_impl<Ret(Args...)> {};

    template<typename Ret, typename Class, typename... Args>
    struct function_traits_impl<Ret(Class::*)(Args...) const> : function_traits_impl<Ret(Args...)> {};

    template<typename Func>
    struct function_traits_impl {
    private:
        using call_operator_type = decltype(&std::decay_t<Func>::operator());
    public:
        using return_type    = typename function_traits_impl<call_operator_type>::return_type;
        using argument_types = typename function_traits_impl<call_operator_type>::argument_types;
    };

    template<typename Func> struct function_traits : function_traits_impl<Func> {};
}