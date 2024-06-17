#pragma once
#include <type_traits>
#include "entity.hpp"
#include "type_kind.hpp"

namespace lux::cxx::lan_model
{
	template<ETypeKind> struct type_kind_map;

	struct Type : public Entity
	{
		ETypeKind	type_kind;
		const char* name;
		bool		is_const;
		bool		is_volatile;
	};

	struct IncompleteType : public Type { static constexpr ETypeKind kind = ETypeKind::IMCOMPLETE; };
	struct Fundamental : public Type{ static constexpr ETypeKind kind = ETypeKind::FUNDAMENTAL; };
		struct Void : public Fundamental{ static constexpr ETypeKind kind = ETypeKind::VOID; };
		struct Nullptr_t : public Fundamental{ static constexpr ETypeKind kind = ETypeKind::NULLPTR_T; };

	struct Compound : public Type { static constexpr ETypeKind kind = ETypeKind::COMPOUND; };
	struct Unsupported : public Type { static constexpr ETypeKind kind = ETypeKind::UNSUPPORTED; };

	template<> struct type_kind_map<ETypeKind::IMCOMPLETE>	{ using type = IncompleteType; };
	template<> struct type_kind_map<ETypeKind::FUNDAMENTAL> { using type = Fundamental; };
		template<> struct type_kind_map<ETypeKind::VOID>		{ using type = Void; };
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
