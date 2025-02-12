#pragma once
#include "arithmetic.hpp"

namespace lux::cxx::lan_model
{
	struct EnumerationType : public Compound
	{
		static constexpr ETypeKind kind = ETypeKind::ENUMERATION;
	};

	struct ScopedEnumeration : public EnumerationType {};
	struct UnscopedEnumeration : public EnumerationType {};

	template<> struct type_kind_map<ETypeKind::ENUMERATION> { using type = EnumerationType; };
	template<> struct type_kind_map<ETypeKind::SCOPED_ENUMERATION> { using type = ScopedEnumeration; };
	template<> struct type_kind_map<ETypeKind::UNSCOPED_ENUMERATION> { using type = UnscopedEnumeration; };
}
