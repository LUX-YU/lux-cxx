#pragma once
#include <string_view>
#include <array>
#include <utility>
#include "function_name.hpp"

#define __HAS_MEMBER_FUNCTION_CONCEPT(func_name)\
template<class T> concept has_member_ ## func_name = requires(T x){\
    x.func_name();\
};

namespace lux::cxx
{
    template<typename T> extern const T fake_obj;

    template<typename Type>
    [[nodiscard]] consteval auto strip_type_name() noexcept {
        std::string_view function_name{ LUX_FUNCTION_NAME };
        auto first = function_name.find_first_not_of(' ', function_name.find_first_of(LUX_FUNCTION_NAME_PREFIX) + 1);
        auto value = function_name.substr(first, function_name.find_last_of(LUX_FUNCTION_NAME_SUFFIX) - first);
        return value;
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
    template <class T>
    struct clang_wrapper_t {
        T v;
    };
    template <class T>
    clang_wrapper_t(T) -> clang_wrapper_t<T>;

    template <class T>
    constexpr auto make_clang_wrapper(const T& arg) noexcept {
        return clang_wrapper_t{arg};
    }

#else

    template <class T>
    constexpr const T& make_clang_wrapper(const T& arg) noexcept {
        return arg;
    }

#endif

    template<typename T> extern const T fakeObj;
    // msvc output :
    // Function name: void __cdecl strip_field_name<&fake_obj<struct test_type>->a>(void) noexcept
    // gcc output :
    // Function name: lux::cxx::strip_field_name() [with auto V = (& fakeObj<test_type>.test_type::b)]
    // clang output:
    // Function name: auto lux::cxx::strip_field_name() [V = clang_wrapper_t<const double *>{&fakeObj.b}]
    template<auto V>
    consteval auto strip_field_name_impl() noexcept {
        std::string_view function_name{ LUX_FUNCTION_NAME };
        
        auto first = function_name.find_first_not_of(' ', function_name.find_first_of(LUX_FUNCTION_NAME_PREFIX) + 1);
        std::string_view type_name = function_name.substr(first, function_name.find_last_of(LUX_FUNCTION_NAME_SUFFIX) - first);
         
        first = type_name.find_first_not_of(' ', type_name.find_first_of(LUX_FUNCTION_MEM_NAME_PREFIX) + 1);
        first += (sizeof(LUX_FUNCTION_MEM_NAME_PREFIX) - 2);
#if defined __clang__ || defined __GNUC__
        auto value = type_name.substr(first, type_name.find_last_of(LUX_FUNCTION_MEM_NAME_SUFFIX) - first);
#elif defined _MSC_VER
        auto value = type_name.substr(first, type_name.size());
#endif
        return value;
    }

    /*
    constexpr auto field_name = 
        lux::cxx::strip_field_name<
            lux::cxx::make_clang_wrapper(std::addressof(lux::cxx::fakeObj<test_type>.b))
        >();
        std::cout << "Field Name:" << field_name << std::endl;
    */

#ifdef __clang__
#pragma clang diagnostic pop
#endif

    template<template<typename...> class T, class U>
    class is_type_template_of
    {
    private:
        template<typename... _TPack>
        static constexpr auto _d(T<_TPack...>*) -> std::true_type;
        static constexpr auto _d(...) -> std::false_type;
    public:
        static constexpr bool value = decltype(_d((U*)nullptr))::value;
    };

    template<template<typename...> class T, class U>
    constexpr inline bool is_type_template_of_v = is_type_template_of<T, U>::value;

    template<template<auto...> class T, class U>
    class is_none_type_template_of
    {
    private:
        template<auto... _TPack>
        static constexpr auto _d(T<_TPack...>*) -> std::true_type;
        static constexpr auto _d(...) -> std::false_type;
    public:
        static constexpr bool value = decltype(_d((U*)nullptr))::value;
    };

    template<template<auto...> class T, class U> 
    constexpr inline bool is_none_type_template_of_v = is_none_type_template_of<T, U>::value;

    /**
     * @brief convert a integer to integer sequence(std::integer_sequence<char, ...>).
     * for example :
     *    integer_to_sequence<18645>::type is std::integer_sequence<char, 1, 8, 6, 4, 5>;
     * 
     * @tparam _Ty the integer will be convert to sequence by digits
     */
    template <size_t _Ty>
    class integer_to_sequence
    {
    private:
        template <size_t num, size_t next>
        struct convertor;

        template <size_t num>
        struct convertor<num, 0>
        {
            using type = std::integer_sequence<char, num>;
        };

        template <size_t num, size_t next = num / 10>
        struct convertor : private convertor<next>
        {
            using parent_type = convertor<next>;
            using parent_seq_type = typename convertor<next>::type;

            template <char... pseq>
            static constexpr auto concat(std::integer_sequence<char, pseq...>)
            {
                return std::integer_sequence<char, pseq..., num % 10>{};
            };

            using type = decltype(concat(parent_seq_type{}));
        };

    public:
        using type = typename convertor<_Ty>::type;
    };

    template<size_t _Ty> using integer_to_sequence_t = typename integer_to_sequence<_Ty>::type;
} // namespace lux::engine::platform
