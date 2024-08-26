#pragma once
#include "lux/cxx/compile_time/ct_string.hpp"
#include "lux/cxx/compile_time/type_info.hpp"
#include "lux/cxx/compile_time/tuple_traits.hpp"
#include "lux/cxx/lan_model/type.hpp"
#include <tuple>

namespace lux::cxx
{
    template<typename T, typename... Args> struct static_type_info;

    template<auto Field, typename TypeInfo, typename Str>
    requires is_ct_string_v<Str>
    struct field_info
    {
        template<typename U, typename T>
        static constexpr auto extract_owner(T U::*member) noexcept -> U;

        using owner      = decltype(extract_owner(Field));
        using type_info  = TypeInfo;
        using name       = Str;

        static inline constexpr type_info::type& get(owner& _owner) noexcept
        requires std::is_member_object_pointer_v<decltype(Field)>
        {
            return _owner.*Field;
        }

        static inline constexpr const type_info::type& get(const owner& _owner) noexcept
        requires std::is_member_object_pointer_v<decltype(Field)>
        {
            return _owner.*Field;
        }

        template<typename... Args>
        static inline constexpr auto invoke(owner& _owner, Args&&... args)
        requires std::is_member_function_pointer_v<decltype(Field)>
        {
            return (_owner.*Field)(std::forward<Args>(args)...);
        }
    };

    template<template<auto,typename,typename> class T, class U>
    class is_field_info_of
    {
    private:
        template<auto Ptr, typename... _TPack>
        static constexpr auto _d(T<Ptr, _TPack...>*) -> std::true_type;
        static constexpr auto _d(...) -> std::false_type;
    public:
        static constexpr bool value = decltype(_d((U*)nullptr))::value;
    };

    template<typename T> static inline constexpr bool is_field_info_of_v = is_field_info_of<field_info, T>::value;
    template<typename T> concept field_info_type = is_field_info_of_v<T>;

    template<typename T, typename... Args>
    struct static_type_info
    {
        using type   = T;
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

        /* FUNC: only accept template lambda and template function now
		 * example:
		 * static_type_info::for_each_field<test_tuple>(
		 *     []<field_info_type T, size_t I>(T&& obj)
		 *     {
		 *			// ...
		 *     }
		 * );
		*/
        template<typename FUNC, typename Obj>
        static constexpr void for_each_field(FUNC&& _func, Obj&& _obj) noexcept
        {
            tuple_traits::for_each_type<fields>(
                [func = std::forward<FUNC>(_func), obj = std::forward<Obj>(_obj)]<typename U, size_t I>() mutable
                {
                    func.template operator()<U, I>(std::forward<Obj>(obj));
                }
            );
        }
    };

    template<typename T> concept static_type_info_type = is_type_template_of_v<static_type_info, T>;
}

#define MAKE_FIELD_TYPE(owner, type, field_name)\
    ::lux::cxx::field_info<&owner::field_name, ::lux::cxx::static_type_info<type>, MAKE_CT_STRING_TYPE(#field_name)>

#define MAKE_FIELD_TYPE_EX(owner, info, field_name)\
    ::lux::cxx::field_info<&owner::field_name, info, MAKE_CT_STRING_TYPE(#field_name)>

#define FIELD_TYPE(name)\
    MAKE_FIELD_TYPE(_type, ::lux::cxx::static_type_info<decltype(_type::name)>, name)

#define FIELD_TYPE_EX(info, name)\
    MAKE_FIELD_TYPE_EX(_type, info, name)

#define START_STATIC_TYPE_INFO_DECLARATION(type)\
    decltype(\
        ([](){using _type = type; return ::lux::cxx::static_type_info<type, \

#define END_STATIC_TYPE_INFO_DECLARATION() >{};}()))