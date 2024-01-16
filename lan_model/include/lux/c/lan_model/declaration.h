#ifndef __DECLARATION_H__
#define __DECLARATION_H__
#include <stdint.h>
#include <stddef.h>
#include "enumerator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum 
{
	E_LUX_DECL_KIND_BASIC,
	E_LUX_DECL_KIND_CLASS,
	E_LUX_DECL_KIND_ENUMERATION,
	E_LUX_DECL_KIND_FUNCTION,
	E_LUX_DECL_KIND_PARAMETER,

	E_LUX_DECL_KIND_MEMBER_DATA,
	E_LUX_DECL_KIND_MEMBER_FUNCTION,
	E_LUX_DECL_KIND_CONSTRUCTOR,
	E_LUX_DECL_KIND_DESTRUCTOR,

	E_LUX_DECL_KIND_UNKNOWN
}E_LUX_C_DECL_KIND;

struct S_LUX_C_Type;

typedef struct{
	E_LUX_C_DECL_KIND					kind;
	S_LUX_C_Type*						type;

	// name is optional
	// for class, name is class name
	// for enumeration, name is enumeration type name
	// for function and method, name is function/method name
	// for constructor, name is class name
	// for destructor,	name is `~` + class name
	// for parameter,	name is parameter name (is exist)
	// for variable,	name is variable name
	const char*							name;
	const char*							attribute;
}S_LUX_C_Declaration;

/**************** Function Related Declaration ****************/
typedef struct{
	S_LUX_C_Declaration					basic_declaration;
	size_t								index;
}S_LUX_C_ParameterDeclaration;

typedef struct{
	S_LUX_C_Type*						result_type;
	S_LUX_C_ParameterDeclaration*		parameter_list;
	size_t								parameter_num;
}S_LUX_C_CallableDeclarationCommon;

typedef struct {
	S_LUX_C_Declaration					basic_declaration;
	S_LUX_C_CallableDeclarationCommon	callable_common;
}S_LUX_C_FunctionDeclaration;
/**************** Class Related Declaration ****************/
typedef enum
{
	INVALID,
	PUBLIC,
	PROTECTED,
	PRIVATE
}E_LUX_C_VISIBILITY;

struct S_LUX_C_ClassDeclaration;
typedef struct {
	S_LUX_C_Declaration					basic_declaration;
	S_LUX_C_ClassDeclaration*			class_declaration;
	E_LUX_C_VISIBILITY					visibility;
} S_LUX_C_MemberDeclaration;

typedef struct {
	S_LUX_C_MemberDeclaration			member_declaration;
	size_t								offset;
}S_LUX_C_FieldDeclaration;

typedef struct {
	S_LUX_C_MemberDeclaration			member_declaration;
	S_LUX_C_CallableDeclarationCommon	callable_common;
	bool								is_explicit;
}S_LUX_C_Constructor;

typedef struct {
	S_LUX_C_MemberDeclaration			member_declaration;
	S_LUX_C_CallableDeclarationCommon	callable_common;
	bool								is_virtual;
}S_LUX_C_Destructor;

typedef struct {
	S_LUX_C_MemberDeclaration			member_declaration;
	S_LUX_C_CallableDeclarationCommon	callable_common;
	bool								is_virtual;
	bool								is_static;
}S_LUX_C_MemberFunction;

typedef struct {
	S_LUX_C_Declaration					basic_declaration;

	S_LUX_C_ClassDeclaration*			base_class_declarations;
	size_t								base_class_num;

	S_LUX_C_Constructor*				constructor_declarations;
	size_t								constructor_num;

	S_LUX_C_Destructor*					destructor_declarations;
	size_t								destructor_num;

	S_LUX_C_MemberFunction*				member_function_declarations;
	size_t								member_function_num;

	S_LUX_C_MemberFunction*				static_member_function_declarations;
	size_t								static_member_function_num;
}S_LUX_C_ClassDeclaration;

/**************** Enumeration Related Declaration ****************/
typedef struct{
	S_LUX_C_Declaration					basic_declaration;

	S_LUX_C_Enumerator*					enumerators;
	size_t								enumerators_num;
	bool								is_scope;
	S_LUX_C_Type*						underlying_type;
}EnumerationDeclaration;

#ifdef __cplusplus
}
#endif

#endif // !__DECLARATION_H__