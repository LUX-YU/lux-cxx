#pragma once
#include "enumerator.hpp"
#include "type.hpp"
#include <cstdint>

namespace lux::cxx::lan_model
{
	enum class EDeclarationKind
	{
		BASIC,
		CLASS,
		ENUMERATION,
		FUNCTION,
		PARAMETER,

		MEMBER_DATA,
		MEMBER_FUNCTION,
		CONSTRUCTOR,
		DESTRUCTOR,
		
		UNKNOWN
	};

	template<EDeclarationKind> struct declaration_kind_map;

	struct Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::BASIC;
		EDeclarationKind	declaration_kind;
		Type*				type;

		// name is optional
		// for class, name is class name
		// for enumeration, name is enumeration type name
		// for function and method, name is function/method name
		// for constructor, name is class name
		// for destructor, name is `~` + class name
		// for parameter, name is parameter name (is exist)
		// for variable, name is variable name
		const char*			name;
		const char*			attribute;
	};

	template<> struct declaration_kind_map<EDeclarationKind::BASIC> { using type = Declaration; };

	enum class Visibility
	{
		INVALID,
		PUBLIC,
		PROTECTED,
		PRIVATE
	};

	struct ClassDeclaration;
	struct MemberDeclaration : public Declaration
	{
		ClassDeclaration* class_declaration;
		Visibility		  visibility;
	};

	struct FieldDeclaration : public MemberDeclaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::MEMBER_DATA;
		size_t offset;
	};
	template<> struct declaration_kind_map<EDeclarationKind::MEMBER_DATA> { using type = FieldDeclaration; };

	struct ConstructorDeclaration;
	struct DestructorDeclaration;
	struct MemberFunctionDeclaration;
	struct ClassDeclaration : public Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::CLASS;

		ClassDeclaration**			base_class_decls;
		size_t						base_class_num;

		ConstructorDeclaration**	constructor_decls;
		size_t						constructor_num;

		DestructorDeclaration*		destructor_decl;

		FieldDeclaration**			field_decls;
		size_t						field_num;

		MemberFunctionDeclaration** member_function_decls;
		size_t						member_function_num;

		MemberFunctionDeclaration** static_member_function_decls;
		size_t						static_member_function_num;
	};
	template<> struct declaration_kind_map<EDeclarationKind::CLASS> { using type = ClassDeclaration; };

	struct ParameterDeclaration : public Declaration
	{
		size_t index;
	};
	template<> struct declaration_kind_map<EDeclarationKind::PARAMETER> { using type = ParameterDeclaration; };

	struct CallableDeclCommon
	{
		Type*					result_type;
		ParameterDeclaration**	parameter_decls;
		size_t					parameter_number;
	};

	struct FunctionDeclaration : public Declaration, public CallableDeclCommon
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::FUNCTION;
	};
	template<> struct declaration_kind_map<EDeclarationKind::FUNCTION> { using type = FunctionDeclaration; };

	struct MemberFunctionDeclaration : public MemberDeclaration, public CallableDeclCommon
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::MEMBER_FUNCTION;
		bool is_static;
		bool is_virtual;
	};
	template<> struct declaration_kind_map<EDeclarationKind::MEMBER_FUNCTION> { using type = MemberFunctionDeclaration; };

	struct ConstructorDeclaration : 
		public MemberDeclaration, public CallableDeclCommon
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::CONSTRUCTOR;
	};
	struct DestructorDeclaration : 
		public MemberDeclaration, public CallableDeclCommon 
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::DESTRUCTOR;
		bool is_virtual;
	};
	template<> struct declaration_kind_map<EDeclarationKind::CONSTRUCTOR> { using type = ConstructorDeclaration; };
	template<> struct declaration_kind_map<EDeclarationKind::DESTRUCTOR> { using type = DestructorDeclaration; };

	struct EnumerationDeclaration : public Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::ENUMERATION;
		Enumerator**  enumerators;
		size_t		  enumerators_num;
		bool		  is_scope;
		Type*		  underlying_type;
	};
	template<> struct declaration_kind_map<EDeclarationKind::ENUMERATION> { using type = EnumerationDeclaration; };
}
