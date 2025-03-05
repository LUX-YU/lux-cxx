/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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