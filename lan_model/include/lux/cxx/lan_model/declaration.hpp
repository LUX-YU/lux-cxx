#pragma once
#include "enumerator.hpp"
#include "type.hpp"
#include <string>
#include <vector>
#include <cstddef>

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
#define LUX_DECLARE_DECLARATION_KIND_MAP(kind, _type) template<> struct declaration_kind_map<kind> { using type = _type; };

	struct Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::BASIC;
		EDeclarationKind	declaration_kind;
		Type*				type;

		/*
		 * name is optional
		 * for class, name is class name
		 * for enumeration, name is enumeration type name
		 * for function and method, name is function/method name
		 * for constructor, name is class name
		 * for destructor, name is `~` + class name
		 * for parameter, name is parameter name (if exists)
		 * for variable, name is variable name
		*/
		std::string			name;
		std::string			full_qualified_name;
		std::string			spelling;
		std::string         usr;
		std::string			attribute;
	};

	enum class Visibility
	{
		INVALID,
		PUBLIC,
		PROTECTED,
		PRIVATE
	};

	struct ClassDeclaration;
	struct MemberDeclaration : Declaration
	{
		ClassDeclaration* class_declaration;
		Visibility		  visibility;
	};

	struct FieldDeclaration : MemberDeclaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::MEMBER_DATA;
		size_t offset;
	};

	struct ParameterDeclaration : Declaration
	{
		size_t index;
	};

	struct CallableDeclCommon
	{
		Type*								result_type;
		std::string							mangling;
		std::vector<ParameterDeclaration>	parameter_decls;
	};

	struct FunctionDeclaration : public Declaration, public CallableDeclCommon
	{
		static constexpr auto kind = EDeclarationKind::FUNCTION;
	};

	struct MemberFunctionDeclaration : MemberDeclaration, CallableDeclCommon
	{
		static constexpr auto kind = EDeclarationKind::MEMBER_FUNCTION;
		bool is_static;
		bool is_virtual;
	};

	struct ConstructorDeclaration : MemberDeclaration, CallableDeclCommon
	{
		static constexpr auto kind = EDeclarationKind::CONSTRUCTOR;
	};

	struct DestructorDeclaration : MemberDeclaration, CallableDeclCommon
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::DESTRUCTOR;
		bool is_virtual;
	};

	struct ClassDeclaration : Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::CLASS;

		std::vector<ClassDeclaration*>			base_class_decls;
		std::vector<ConstructorDeclaration>		constructor_decls;
		DestructorDeclaration					destructor_decl;
		std::vector<FieldDeclaration>			field_decls;
		std::vector<MemberFunctionDeclaration>	method_decls;
		std::vector<MemberFunctionDeclaration>	static_method_decls;
	};

	struct EnumerationDeclaration : Declaration
	{
		static constexpr EDeclarationKind kind = EDeclarationKind::ENUMERATION;
		std::vector<Enumerator>			enumerators;
		bool							is_scope;
		Type*							underlying_type;
	};

	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::BASIC,			Declaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::CLASS,			ClassDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::FUNCTION,		FunctionDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::PARAMETER,		ParameterDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::MEMBER_DATA,		FieldDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::MEMBER_FUNCTION, MemberFunctionDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::CONSTRUCTOR,		ConstructorDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::DESTRUCTOR,		DestructorDeclaration)
	LUX_DECLARE_DECLARATION_KIND_MAP(EDeclarationKind::ENUMERATION,		EnumerationDeclaration)
}
