#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>
#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/types/arithmetic.hpp>
#include <lux/cxx/lan_model/types/pointer_type.hpp>
#include <lux/cxx/lan_model/types/member_pointer_type.hpp>
#include <lux/cxx/lan_model/types/reference_type.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <cstring>
#include <vector>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	Type CxxParserImpl::rootAnonicalType(const Type& type)
	{
		if (type.typeKind() == CXType_Typedef)
		{
			return rootAnonicalType(type.canonicalType());
		}
		return type;
	}

	static void basicTypeMetaInit(TypeMeta* meta_type, const Type& type)
	{
		meta_type->is_const		= type.isConstQualifiedType();
		meta_type->is_volatile	= type.isVolatileQualifiedType();
		meta_type->name			= CxxParserImpl::nameFromClangString(type.typeSpelling());
	}

	template<TypeMetaKind E>
	TypeMeta* createSimpleTypeMeta(const Type& type)
	{
		auto* meta_type = new typename type_kind_map<E>::type;
		meta_type->type_kind = E;
		basicTypeMetaInit(meta_type, type);
		return meta_type;
	}

	template<>
	ObjectPointerType* CxxParserImpl::TParseTypeMeta<TypeMetaKind::POINTER_TO_OBJECT>(const Type& type, ObjectPointerType* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
		return meta_type;
	}
	template<>
	FunctionPointerType* CxxParserImpl::TParseTypeMeta<TypeMetaKind::POINTER_TO_FUNCTION>(const Type& type, FunctionPointerType* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<TypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
		return meta_type;
	}

	template<>
	DataMemberPointerType* CxxParserImpl::TParseTypeMeta<TypeMetaKind::POINTER_TO_DATA_MEMBER>(const Type& type, DataMemberPointerType* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
		return meta_type;
	}
	template<>
	MemberFunctionPointerType* CxxParserImpl::TParseTypeMeta<TypeMetaKind::POINTER_TO_MEMBER_FUNCTION>(const Type& type, MemberFunctionPointerType* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<TypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
		return meta_type;
	}

	template<>
	RValueReferenceToObject* CxxParserImpl::TParseTypeMeta<TypeMetaKind::RVALUE_REFERENCE_TO_OBJECT>(const Type& type, RValueReferenceToObject* meta_type)
	{
		meta_type->reference_type = parseUncertainTypeMeta(rootAnonicalType(type));
		return meta_type;
	}
	template<>
	RValueReferenceToFuntion* CxxParserImpl::TParseTypeMeta<TypeMetaKind::RVALUE_REFERENCE_TO_FUNCTION>(const Type& type, RValueReferenceToFuntion* meta_type)
	{
		meta_type->reference_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<TypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
		return meta_type;
	}

	template<>
	LValueReferenceToObject* CxxParserImpl::TParseTypeMeta<TypeMetaKind::LVALUE_REFERENCE_TO_OBJECT>(const Type& type, LValueReferenceToObject* meta_type)
	{
		meta_type->reference_type = parseUncertainTypeMeta(rootAnonicalType(type));
		return meta_type;
	}
	template<>
	LValueReferenceToFuntion* CxxParserImpl::TParseTypeMeta<TypeMetaKind::LVALUE_REFERENCE_TO_FUNCTION>(const Type& type, LValueReferenceToFuntion* meta_type)
	{
		meta_type->reference_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<TypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
		return meta_type;
	}

	TypeMeta* CxxParserImpl::parseUncertainTypeMeta(const Type& type)
	{
		auto id = type_meta_id(type.typeSpelling().c_str());
		if (hasTypeMetaInContextById(id))
		{
			return getTypeMetaFromContextById(id);
		}

		auto type_kind = type.typeKind();
		TypeMeta* type_meta{ nullptr };

		switch (type_kind)
		{
		case CXType_Invalid:
			break;
		case CXType_Unexposed:
			type_meta = createSimpleTypeMeta<TypeMetaKind::IMCOMPLETE>(type);
			break;
		case CXType_Void:
			type_meta = createSimpleTypeMeta<TypeMetaKind::VOID>(type);
			break;
		case CXType_Bool:
			type_meta = createSimpleTypeMeta<TypeMetaKind::BOOL>(type);
			break;
		case CXType_Char_U:
			type_meta = createSimpleTypeMeta<TypeMetaKind::CHAR>(type);
			break;
		case CXType_UChar:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSIGNED_CHAR>(type);
			break;
		case CXType_Char16:
			type_meta = createSimpleTypeMeta<TypeMetaKind::CHAR16_T>(type);
			break;
		case CXType_Char32:
			type_meta = createSimpleTypeMeta<TypeMetaKind::CHAR32_T>(type);
			break;
		case CXType_UShort:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSIGNED_SHORT_INT>(type);
			break;
		case CXType_UInt:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSIGNED_INT>(type);
			break;
		case CXType_ULong:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSIGNED_LONG_INT>(type);
			break;
		case CXType_ULongLong:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSIGNED_LONG_LONG_INT>(type);
			break;
		case CXType_UInt128:
			type_meta = createSimpleTypeMeta<TypeMetaKind::EXTENDED_UNSIGNED>(type);
			break;
		case CXType_Char_S:
			type_meta = createSimpleTypeMeta<TypeMetaKind::SIGNED_CHAR>(type);
			break;
		case CXType_SChar:
			type_meta = createSimpleTypeMeta<TypeMetaKind::SIGNED_CHAR>(type);
			break;
		case CXType_WChar:
			type_meta = createSimpleTypeMeta<TypeMetaKind::WCHAR_T>(type);
			break;
		case CXType_Short:
			type_meta = createSimpleTypeMeta<TypeMetaKind::SHORT_INT>(type);
			break;
		case CXType_Int:
			type_meta = createSimpleTypeMeta<TypeMetaKind::INT>(type);
			break;
		case CXType_Long:
			type_meta = createSimpleTypeMeta<TypeMetaKind::LONG_INT>(type);
			break;
		case CXType_LongLong:
			type_meta = createSimpleTypeMeta<TypeMetaKind::LONG_LONG_INT>(type);
			break;
		case CXType_Int128:
			type_meta = createSimpleTypeMeta<TypeMetaKind::EXTENDED_SIGNED>(type);
			break;
		case CXType_Float:
			type_meta = createSimpleTypeMeta<TypeMetaKind::FLOAT>(type);
			break;
		case CXType_Double:
			type_meta = createSimpleTypeMeta<TypeMetaKind::DOUBLE>(type);
			break;
		case CXType_LongDouble:
			type_meta = createSimpleTypeMeta<TypeMetaKind::LONG_DOUBLE>(type);
			break;
		case CXType_NullPtr:
			type_meta = createSimpleTypeMeta<TypeMetaKind::NULLPTR_T>(type);
			break;
		case CXType_Overload:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Dependent:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Float128:
			type_meta = createSimpleTypeMeta<TypeMetaKind::EXTENDED_FLOAT_POINT>(type);
			break;
		case CXType_Half:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Float16:
			type_meta = createSimpleTypeMeta<TypeMetaKind::EXTENDED_FLOAT_POINT>(type);
			break;
		case CXType_ShortAccum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Accum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_LongAccum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_UShortAccum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_UAccum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_ULongAccum:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_BFloat16:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Ibm128:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Complex:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Pointer:
		{
			return parseReferenceSemtantic<TypeMetaKind::POINTER_TO_OBJECT, 
				TypeMetaKind::POINTER_TO_FUNCTION>(type, id);
		}
			/* for obj - c
			case CXType_BlockPointer:
				break;
			*/
		case CXType_LValueReference:
		{
			return parseReferenceSemtantic<TypeMetaKind::LVALUE_REFERENCE_TO_OBJECT, 
				TypeMetaKind::LVALUE_REFERENCE_TO_FUNCTION>(type, id);
		}
		case CXType_RValueReference:
		{
			return parseReferenceSemtantic<TypeMetaKind::RVALUE_REFERENCE_TO_OBJECT,
				TypeMetaKind::RVALUE_REFERENCE_TO_FUNCTION>(type, id);
		}
		case CXType_Record:
		{
			return TParseTypeMetaDecoratorNoCheck<TypeMetaKind::CLASS>(type, id);
		}
		case CXType_Enum:
			return TParseTypeMetaDecoratorNoCheck<TypeMetaKind::ENUMERATION>(type, id);
		case CXType_Typedef:
		{
			return parseUncertainTypeMeta(rootAnonicalType(type));
		}
		/* for obj - c
		case CXType_ObjCInterface:
			break;
		case CXType_ObjCObjectPointer:
			break;
		*/
		case CXType_FunctionNoProto:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_FunctionProto:
			type_meta = createSimpleTypeMeta<TypeMetaKind::FUNCTION>(type);
			break;
		case CXType_ConstantArray:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Vector:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_IncompleteArray:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_VariableArray:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_DependentSizedArray:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_MemberPointer:
		{
			return parseReferenceSemtantic<TypeMetaKind::POINTER_TO_DATA_MEMBER,
				TypeMetaKind::POINTER_TO_MEMBER_FUNCTION>(type, id);
		}
		case CXType_Auto:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		case CXType_Elaborated:
			return parseUncertainTypeMeta(type.getNamedType());
		case CXType_Attributed:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		default:
			type_meta = createSimpleTypeMeta<TypeMetaKind::UNSUPPORTED>(type);
			break;
		}

		if (!hasTypeMetaInContextById(id))
		{
			if (type_meta)
			{
				registTypeMeta(id, type_meta);
			}
			else
			{
				delete type_meta;
				type_meta = nullptr;
			}
		}

		return type_meta;
	}
}