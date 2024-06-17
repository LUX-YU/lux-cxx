#pragma once
#include <lux/cxx/lan_model/declaration.hpp>

namespace lux::cxx::lan_model
{
	struct FunctionType : public Compound
	{
		static constexpr ETypeKind kind = ETypeKind::FUNCTION;
	};

	template<> struct type_kind_map<ETypeKind::FUNCTION> { using type = FunctionType; };
}
