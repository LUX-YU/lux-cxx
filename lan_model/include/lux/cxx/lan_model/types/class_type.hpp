#pragma once
#include <lux/cxx/lan_model/type.hpp>
#include <lux/cxx/lan_model/declaration.hpp>

namespace lux::cxx::lan_model
{
	// A class is a user-defined type.
	// https://en.cppreference.com/w/cpp/language/classes
	struct ClassType : public Compound
	{
		static constexpr auto kind = ETypeKind::CLASS;
	};

	template<> struct type_kind_map<ETypeKind::CLASS> { using type = ClassType; };
}
