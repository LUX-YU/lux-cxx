#pragma once
#include "extended_type_traits.hpp"
#include <string_view>

// need c++ 20
// clang >= 13 (4 Oct 2021)
// gcc >= 9.2, if gcc = 9.x add -std=c++2a -fconcepts
// msvc >= 19.29(16.11)
namespace lux::cxx
{
    template<typename T, std::size_t N>
    struct __ct_string
    {
        using char_type = T;
        constexpr static std::size_t size = N;
        char_type data[size + 1] = {};
        
        constexpr __ct_string() requires (N == 0) : data{ 0 } {}

        constexpr __ct_string(char_type c) requires (N == 1) : data{ c, 0 } {}

        constexpr __ct_string(std::basic_string_view<char_type> view)
        {
            for (std::size_t i = 0; i < size; i++)
                data[i] = view[i];
            data[N] = 0;
        }

        constexpr __ct_string(const char_type(&ptr)[N + 1])
        {
            for (std::size_t i = 0; i < size; i++)
                data[i] = ptr[i];
            data[N] = 0;
        }

        template<typename... C>
        constexpr __ct_string(std::in_place_t, C... c)
            : data{ static_cast<char_type>(c)..., 0 } {}
    };

    template<__ct_string _str>
    struct ct_string
    {
        using char_type = typename decltype(_str)::char_type;
        // using __ct_string_type = __ct_string;
        static constexpr    const char_type* data() { return _str.data; }
        static constexpr    std::size_t size() { return _str.size; }
        static constexpr    std::basic_string_view<char_type> view() { return _str.data; }
        constexpr operator  std::basic_string_view<char_type>() { return view(); }
    };

    template<char... C> using ct_string_s =
        ct_string < __ct_string<char, sizeof...(C)>{std::in_place, C...} > ;
    template<auto c> using ct_string_c =
        ct_string <__ct_string<decltype(c), 1>(c)>;

    // lux::cxx::is_type_template_of can not be used here
    // because __ct_string is a LiteralType
    template<typename T>
    struct is_ct_string : public is_none_type_template_of<ct_string, T> {};

    template<typename T> constexpr bool is_ct_string_v = is_ct_string<T>::value;
    
    template<typename T> concept ct_string_type = is_ct_string_v<T>;

#define MAKE_CT_STRING(str)\
([]{\
        constexpr std::basic_string_view s{str};\
        return ::lux::cxx::ct_string<\
            ::lux::cxx::__ct_string<typename decltype(s)::value_type, s.size()>{str} \
        > {};\
   }()\
)

#define MAKE_CT_STRING_TYPE(str) decltype(MAKE_CT_STRING(str))

    template<typename S1, typename S2>
    struct ct_string_cat_helper
    {
    private:
        template<std::size_t... NS1, std::size_t... NS2>
        static constexpr auto _get(std::index_sequence<NS1...>, std::index_sequence<NS2...>)
        {
            return ct_string_s<(S1::view()[NS1], ...), (S2::view()[NS2], ...)>{};
        }

        static constexpr auto _make()
        {
            return _get(
                std::make_index_sequence < S1::size() > {},
                std::make_index_sequence < S2::size() > {}
            );
        }

    public:
        using type = decltype(_make());
    };

    template<typename S1, typename S2> using ct_string_cat_helper_t =
        typename ct_string_cat_helper<S1, S2>::type;

    template<typename S1, typename S2>
    constexpr auto ct_string_cat(S1, S2)
    {
        return typename ct_string_cat_helper<S1, S2>::type{};
    }
}
