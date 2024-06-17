#pragma once
#include <lux/cxx/lan_model/type.hpp>

namespace lux::cxx::lan_model
{
	struct Array : public Compound
	{
		static constexpr ETypeKind kind = ETypeKind::ARRAY;
		Type* elem_type;
	};

	template<> struct type_kind_map<ETypeKind::ARRAY> { using type = Array; };
}
