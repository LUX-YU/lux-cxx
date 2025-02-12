#pragma once
#include "function_type.hpp"

namespace lux::cxx::lan_model
{
	struct PointerType : public Compound { static constexpr ETypeKind kind = ETypeKind::POINTER; };
		struct ObjectPointerType : public PointerType { static constexpr ETypeKind kind = ETypeKind::POINTER_TO_OBJECT;};
		struct FunctionPointerType : public PointerType { static constexpr ETypeKind kind = ETypeKind::POINTER_TO_FUNCTION;};

	template<> struct type_kind_map<ETypeKind::POINTER> { using type = PointerType; };
		template<> struct type_kind_map<ETypeKind::POINTER_TO_OBJECT> { using type = ObjectPointerType; };
		template<> struct type_kind_map<ETypeKind::POINTER_TO_FUNCTION> { using type = FunctionPointerType; };
}
