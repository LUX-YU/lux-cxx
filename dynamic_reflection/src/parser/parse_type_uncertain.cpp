#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <lux/cxx/lan_model/types/class_type.hpp>
#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/types/arithmetic.hpp>
#include <lux/cxx/lan_model/types/pointer_type.hpp>
#include <lux/cxx/lan_model/types/member_pointer_type.hpp>
#include <lux/cxx/lan_model/types/reference_type.hpp>

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

	static void basicTypeMetaInit(const Type& type, TypeMeta& meta_type)
	{
		meta_type.is_const		= type.isConstQualifiedType();
		meta_type.is_volatile	= type.isVolatileQualifiedType();
		meta_type.name			= type.typeSpelling().to_std();
	}

	template<ETypeMetaKind E>
	void createSimpleTypeMeta(const Type& type, TypeMeta& meta_type)
	{
		meta_type.type_kind = E;
		meta_type.size	    = type.typeSizeof();
		meta_type.align		= type.typeAlignof();
		basicTypeMetaInit(type, meta_type);
	}

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::POINTER_TO_OBJECT>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
	}

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::POINTER_TO_FUNCTION>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<ETypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
	}

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::POINTER_TO_DATA_MEMBER>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
	}
	template<>
	void  CxxParserImpl::TParseTypeMeta<ETypeMetaKind::POINTER_TO_MEMBER_FUNCTION>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<ETypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
	}

	template<>
	void  CxxParserImpl::TParseTypeMeta<ETypeMetaKind::RVALUE_REFERENCE_TO_OBJECT>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
	}

	template<>
	void  CxxParserImpl::TParseTypeMeta<ETypeMetaKind::RVALUE_REFERENCE_TO_FUNCTION>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<ETypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
	}

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::LVALUE_REFERENCE_TO_OBJECT>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = parseUncertainTypeMeta(rootAnonicalType(type));
	}
	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::LVALUE_REFERENCE_TO_FUNCTION>(const Type& type, TypeMeta* meta_type)
	{
		meta_type->pointee_type = static_cast<FunctionType*>(
			TParseTypeMetaDecorator<ETypeMetaKind::FUNCTION>(rootAnonicalType(type))
		);
	}

	TypeMeta* CxxParserImpl::parseUncertainTypeMeta(const Type& type)
	{
		const auto id = type_meta_id(type.typeSpelling().c_str());
		if (hasTypeMetaInContextById(id))
		{
			return getTypeMetaFromContextById(id);
		}

		auto type_kind = type.typeKind();
		TypeMeta type_meta{};

		switch (type_kind)
		{
		case CXType_Invalid:
			break;
		case CXType_Unexposed:
			createSimpleTypeMeta<ETypeMetaKind::INCOMPLETE>(type, type_meta);
			break;
		case CXType_Void:
			createSimpleTypeMeta<ETypeMetaKind::VOID_TYPE>(type, type_meta);
			break;
		case CXType_Bool:
			createSimpleTypeMeta<ETypeMetaKind::BOOL>(type, type_meta);
			break;
		case CXType_Char_U:
			createSimpleTypeMeta<ETypeMetaKind::CHAR>(type, type_meta);
			break;
		case CXType_UChar:
			createSimpleTypeMeta<ETypeMetaKind::UNSIGNED_CHAR>(type, type_meta);
			break;
		case CXType_Char16:
			createSimpleTypeMeta<ETypeMetaKind::CHAR16_T>(type, type_meta);
			break;
		case CXType_Char32:
			createSimpleTypeMeta<ETypeMetaKind::CHAR32_T>(type, type_meta);
			break;
		case CXType_UShort:
			createSimpleTypeMeta<ETypeMetaKind::UNSIGNED_SHORT_INT>(type, type_meta);
			break;
		case CXType_UInt:
			createSimpleTypeMeta<ETypeMetaKind::UNSIGNED_INT>(type, type_meta);
			break;
		case CXType_ULong:
			createSimpleTypeMeta<ETypeMetaKind::UNSIGNED_LONG_INT>(type, type_meta);
			break;
		case CXType_ULongLong:
			createSimpleTypeMeta<ETypeMetaKind::UNSIGNED_LONG_LONG_INT>(type, type_meta);
			break;
		case CXType_UInt128:
			createSimpleTypeMeta<ETypeMetaKind::EXTENDED_UNSIGNED>(type, type_meta);
			break;
		case CXType_Char_S:
			createSimpleTypeMeta<ETypeMetaKind::SIGNED_CHAR>(type, type_meta);
			break;
		case CXType_SChar:
			createSimpleTypeMeta<ETypeMetaKind::SIGNED_CHAR>(type, type_meta);
			break;
		case CXType_WChar:
			createSimpleTypeMeta<ETypeMetaKind::WCHAR_T>(type, type_meta);
			break;
		case CXType_Short:
			createSimpleTypeMeta<ETypeMetaKind::SHORT_INT>(type, type_meta);
			break;
		case CXType_Int:
			createSimpleTypeMeta<ETypeMetaKind::INT>(type, type_meta);
			break;
		case CXType_Long:
			createSimpleTypeMeta<ETypeMetaKind::LONG_INT>(type, type_meta);
			break;
		case CXType_LongLong:
			createSimpleTypeMeta<ETypeMetaKind::LONG_LONG_INT>(type, type_meta);
			break;
		case CXType_Int128:
			createSimpleTypeMeta<ETypeMetaKind::EXTENDED_SIGNED>(type, type_meta);
			break;
		case CXType_Float:
			createSimpleTypeMeta<ETypeMetaKind::FLOAT>(type, type_meta);
			break;
		case CXType_Double:
			createSimpleTypeMeta<ETypeMetaKind::DOUBLE>(type, type_meta);
			break;
		case CXType_LongDouble:
			createSimpleTypeMeta<ETypeMetaKind::LONG_DOUBLE>(type, type_meta);
			break;
		case CXType_NullPtr:
			createSimpleTypeMeta<ETypeMetaKind::NULLPTR_T>(type, type_meta);
			break;
		case CXType_Overload:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Dependent:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Float128:
			createSimpleTypeMeta<ETypeMetaKind::EXTENDED_FLOAT_POINT>(type, type_meta);
			break;
		case CXType_Half:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Float16:
			createSimpleTypeMeta<ETypeMetaKind::EXTENDED_FLOAT_POINT>(type, type_meta);
			break;
		case CXType_ShortAccum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Accum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_LongAccum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_UShortAccum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_UAccum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_ULongAccum:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_BFloat16:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Ibm128:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Complex:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Pointer:
		{
			return parseReferenceSemtantic<ETypeMetaKind::POINTER_TO_OBJECT, 
				ETypeMetaKind::POINTER_TO_FUNCTION>(type, id);
		}
		/* for obj - c
		case CXType_BlockPointer:
			break;
		*/
		case CXType_LValueReference:
		{
			return parseReferenceSemtantic<ETypeMetaKind::LVALUE_REFERENCE_TO_OBJECT, 
				ETypeMetaKind::LVALUE_REFERENCE_TO_FUNCTION>(type, id);
		}
		case CXType_RValueReference:
		{
			return parseReferenceSemtantic<ETypeMetaKind::RVALUE_REFERENCE_TO_OBJECT,
				ETypeMetaKind::RVALUE_REFERENCE_TO_FUNCTION>(type, id);
		}
		case CXType_Record:
		{
			return TParseTypeMetaDecoratorNoCheck<ETypeMetaKind::CLASS>(type, id);
		}
		case CXType_Enum:
			return TParseTypeMetaDecoratorNoCheck<ETypeMetaKind::ENUMERATION>(type, id);
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
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_FunctionProto:
			createSimpleTypeMeta<ETypeMetaKind::FUNCTION>(type, type_meta);
			break;
		case CXType_ConstantArray:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Vector:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_IncompleteArray:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_VariableArray:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_DependentSizedArray:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_MemberPointer:
		{
			return parseReferenceSemtantic<ETypeMetaKind::POINTER_TO_DATA_MEMBER,
				ETypeMetaKind::POINTER_TO_MEMBER_FUNCTION>(type, id);
		}
		case CXType_Auto:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		case CXType_Elaborated:
			return parseUncertainTypeMeta(type.getNamedType());
		case CXType_Attributed:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		default:
			createSimpleTypeMeta<ETypeMetaKind::UNSUPPORTED>(type, type_meta);
			break;
		}
		if (!hasTypeMetaInContextById(id))
		{
			return registerTypeMeta(id, type_meta);
		}

		return nullptr;
	}
}