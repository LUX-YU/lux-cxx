#pragma once
#include <lux/cxx/compile_time/name_traits.hpp>
#include <lux/cxx/lan_model/type_kind.hpp>
#include <string>
#include <memory>
#include <vector>

namespace lux::cxx
{
	struct TypeInfo
	{
		std::string_view type_kind_name;
		std::string_view type_name;
	};

	template<typename T>
	struct TTypeInfo
	{
		using type_kind = lan_model::type_check_t<T>;
		static constexpr std::string_view type_name					= nameof::nameof_type<T>();
		static constexpr lan_model::CxxTypeEnumKind cxx_type_enum	= type_kind::type_enum;
		static constexpr std::size_t type_size						= sizeof(T);

		static void toInfo(TypeInfo& info)
		{
			info.type_kind_name = type_kind::name;
			info.type_name = type_name;

			return info;
		}
	};
}
