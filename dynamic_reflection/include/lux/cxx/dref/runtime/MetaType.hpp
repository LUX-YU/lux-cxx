#pragma once
#include <variant>
#include <type_traits>
#include <string>
#include <string_view>
#include <vector>
#include "hash.hpp"
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref::runtime
{
	enum class MetaType
	{
		BASIC,
		DATA_TYPE,
		CLASS,
		ENUMERATION,
		FIELD,
		METHOD,
		FUNCTION,
	};

	struct TypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::BASIC;

		std::size_t			type_id;
		std::string			type_name;
		std::size_t			meta_unit_id;
		std::string			type_underlying_name; // currently no use and maybe not correct
		MetaType			meta_type;
	};

	struct DataTypeMeta : public TypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::DATA_TYPE;

		std::size_t	size;
		bool		is_const;
		bool		is_volatile;
		void*		(*instance_getter)(void**);
		void		(*instance_releaser)(void*);
	};

	struct FunctionTypeMeta : public TypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::FUNCTION ;

		bool					is_static;
		TypeMeta*				result_info;
		std::vector<TypeMeta*>	parameters_list;
		std::string				func_name;
		void*					func_wrap_ptr;
	};
}
