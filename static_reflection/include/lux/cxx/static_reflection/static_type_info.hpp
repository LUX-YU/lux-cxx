#pragma once
#include "lux/cxx/compile_time/ct_string.hpp"
#include "lux/cxx/compile_time/type_info.hpp"
#include "lux/cxx/compile_time/tuple_traits.hpp"
#include "lux/cxx/lan_model/type.hpp"
#include <tuple>

namespace lux::cxx
{
    template<typename T, typename... Args> struct static_type_info;

    template<typename TypeInfo, typename Str>
    requires is_ct_string_v<Str>
    struct field_info
    {
        static_assert(is_type_template_of_v<static_type_info, TypeInfo>, "T must be static type info type");
        using type_info  = TypeInfo;
        using name       = Str;
    };

    template<typename T> concept field_info_type = is_type_template_of_v<field_info, T>;

    template<typename T, typename... Args>
    struct static_type_info
    {
        using fields = std::tuple<Args...>;
        static_assert(
            ((field_info_type<Args> || is_type_template_of_v<static_type_info, Args>) && ...), 
            "Args must be field_info type or static type info type"
        );

        static constexpr std::string_view name() noexcept
        {
            return type_name<T>();
        }

        static constexpr size_t hash() noexcept
        {
            return type_hash<T>();
        }

        static constexpr auto field_count() noexcept
        {
            return std::tuple_size_v<fields>;
        }

        template<size_t Index> using field_type = std::tuple_element_t<Index, fields>;

        /* FUNC: only accept template lambda and template function now
		 * example:
		 * static_type_info::for_each_field<test_tuple>(
		 *     []<field_info_type T, size_t I>
		 *     {
		 *			// ...
		 *     }
		 * );
		*/
        template<typename FUNC>
        static constexpr void for_each_field(FUNC&& func) noexcept
        {
            tuple_traits::for_each_type<fields>(std::forward<FUNC>(func));
        }
    };

    template<typename T> concept static_type_info_type = is_type_template_of_v<static_type_info, T>;
}

#define MAKE_FIELD_TYPE(type, field_name)\
    ::lux::cxx::field_info<::lux::cxx::static_type_info<type>, MAKE_CT_STRING_TYPE(#field_name)>

#define MAKE_FIELD_TYPE_EX(info, field_name)\
    ::lux::cxx::field_info<info, MAKE_CT_STRING_TYPE(#field_name)>
