#pragma once
#include <type_traits>
#include <string>
#include "entity.hpp"
#include "type_kind.hpp"

namespace lux::cxx::lan_model
{
	template<ETypeKind> struct type_kind_map;

	struct Declaration;
	struct Type : Entity
	{
		ETypeKind		type_kind;
		std::string 	name;
		bool			is_const;
		bool			is_volatile;
		int				size;
		int				align;
		Type*			pointee_type{ nullptr };  // only if type is (R/L)reference or point
		Declaration*	declaration{ nullptr };   // only if type is class, struct or enum
		Type*			element_type{ nullptr };  // only if type is array
	};

	struct IncompleteType : Type { static constexpr ETypeKind kind = ETypeKind::INCOMPLETE; };
	struct Fundamental : Type{ static constexpr ETypeKind kind = ETypeKind::FUNDAMENTAL; };
		struct Void : Fundamental{ static constexpr ETypeKind kind = ETypeKind::VOID_TYPE; };
		struct Nullptr_t :  Fundamental{ static constexpr ETypeKind kind = ETypeKind::NULLPTR_T; };

	struct Compound :  Type { static constexpr ETypeKind kind = ETypeKind::COMPOUND; };
	struct Unsupported :  Type { static constexpr ETypeKind kind = ETypeKind::UNSUPPORTED; };

	template<> struct type_kind_map<ETypeKind::INCOMPLETE>	{ using type = IncompleteType; };
	template<> struct type_kind_map<ETypeKind::FUNDAMENTAL> { using type = Fundamental; };
		template<> struct type_kind_map<ETypeKind::VOID_TYPE>	{ using type = Void; };
		template<> struct type_kind_map<ETypeKind::NULLPTR_T>	{ using type = Nullptr_t; };

	template<> struct type_kind_map<ETypeKind::COMPOUND>	{ using type = Compound; };
	template<> struct type_kind_map<ETypeKind::UNSUPPORTED> { using type = Unsupported; };

	template<typename T>
	static consteval bool is_object()
	requires std::is_base_of_v<Type, T>
	{
		return is_object(T::kind);
	}

	template<typename T>
	static constexpr bool is_incomplete()
	requires std::is_base_of_v<Type, T>
	{
		return is_incomplete(T::kind);
	}

	// is scalar
	// is trivial
}
