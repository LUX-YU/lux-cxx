#pragma once
#include "function_type.hpp"

namespace lux::cxx::lan_model
{
	struct ReferenceType : public Compound{ static constexpr ETypeKind kind = ETypeKind::REFERENCE; };
		struct LValueReferenceType : public ReferenceType { static constexpr ETypeKind kind = ETypeKind::LVALUE_REFERENCE; };
			struct LValueReferenceToObject : public LValueReferenceType { static constexpr ETypeKind kind = ETypeKind::LVALUE_REFERENCE_TO_OBJECT;};
			struct LValueReferenceToFunction : public LValueReferenceType { static constexpr ETypeKind kind = ETypeKind::LVALUE_REFERENCE_TO_FUNCTION;};
		struct RValueReferenceType : public ReferenceType { static constexpr ETypeKind kind = ETypeKind::RVALUE_REFERENCE; };
			struct RValueReferenceToObject : public RValueReferenceType { static constexpr ETypeKind kind = ETypeKind::RVALUE_REFERENCE_TO_OBJECT;};
			struct RValueReferenceToFunction : public RValueReferenceType { static constexpr ETypeKind kind = ETypeKind::RVALUE_REFERENCE_TO_FUNCTION;};

	template<> struct type_kind_map<ETypeKind::REFERENCE> { using type = ReferenceType; };
		template<> struct type_kind_map<ETypeKind::LVALUE_REFERENCE> { using type = LValueReferenceType; };
			template<> struct type_kind_map<ETypeKind::LVALUE_REFERENCE_TO_OBJECT> { using type = LValueReferenceToObject; };
			template<> struct type_kind_map<ETypeKind::LVALUE_REFERENCE_TO_FUNCTION> { using type = LValueReferenceToFunction; };
		template<> struct type_kind_map<ETypeKind::RVALUE_REFERENCE> { using type = RValueReferenceType; };
			template<> struct type_kind_map<ETypeKind::RVALUE_REFERENCE_TO_OBJECT> { using type = RValueReferenceToObject; };
			template<> struct type_kind_map<ETypeKind::RVALUE_REFERENCE_TO_FUNCTION> { using type = RValueReferenceToFunction; };
}
