#pragma once
#include "function_type.hpp"
#include "class_type.hpp"

namespace lux::cxx::lan_model
{
	struct MemberPointerType : public Compound{ static constexpr ETypeKind kind = ETypeKind::POINTER_TO_MEMBER; ClassType*	class_type;};
		struct MemberFunctionPointerType : public MemberPointerType { static constexpr ETypeKind kind = ETypeKind::POINTER_TO_MEMBER_FUNCTION; FunctionType* pointee_type;};
		struct DataMemberPointerType : public MemberPointerType { static constexpr ETypeKind kind = ETypeKind::POINTER_TO_DATA_MEMBER; Type* pointee_type; };

	template<> struct type_kind_map<ETypeKind::POINTER_TO_MEMBER> { using type = MemberPointerType; };
		template<> struct type_kind_map<ETypeKind::POINTER_TO_MEMBER_FUNCTION> { using type = MemberFunctionPointerType; };
		template<> struct type_kind_map<ETypeKind::POINTER_TO_DATA_MEMBER> { using type = DataMemberPointerType; };
}
