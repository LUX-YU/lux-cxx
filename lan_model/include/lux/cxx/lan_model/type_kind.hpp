#pragma once

// see https://en.cppreference.com/w/cpp/language/type

namespace lux::cxx::lan_model
{
	enum class ETypeKind {
		INCOMPLETE = 0, // from struct CXXType

		// from struct Fundamental
		FUNDAMENTAL,
			VOID_TYPE, // from struct Void
			NULLPTR_T, // from struct Nullptr
			ARITHMETIC, // from struct Arithmetic
			// from struct Integral
				INTEGRAL,
					BOOL, // from struct Bool
					CHARACTER, // from struct Character
						NARROW_CHARACTER, // from struct NarrowCharacter
							ORDINARY_CHARACTER, // from struct OrdinaryCharacter
								CHAR,
								SIGNED_CHAR,
								UNSIGNED_CHAR,
#if __cplusplus > 201703L // only support c++20
							CHAR8T, // from struct Char8t
#endif
						WIDE_CHARACTER, // from struct WideCharacter
							CHAR16_T,
							CHAR32_T,
							WCHAR_T,
					SIGNED_INTEGRAL, // from struct SignedIntegral
						STANDARD_SIGNED, // from struct StandardSigned
							// SIGNED_CHAR
							SHORT_INT,
							INT,
							LONG_INT,
							LONG_LONG_INT,
						EXTENDED_SIGNED, // from struct ExtendedSigned

					// from struct UnsignedIntegral
					UNSIGNED_INTEGRAL,
						STANDARD_UNSIGNED, // from struct StandardUnsigned
							// UNSIGNED_CHAR
							UNSIGNED_SHORT_INT,
							UNSIGNED_INT,
							UNSIGNED_LONG_INT,
							UNSIGNED_LONG_LONG_INT,
						EXTENDED_UNSIGNED, // from struct ExtendedUnsigned

				// from struct FloatPoint
				FLOAT_POINT,
					STANDARD_FLOAT_POINT, // from struct StandardFloatPoint
						FLOAT,
						DOUBLE,
						LONG_DOUBLE,
					EXTENDED_FLOAT_POINT, // from struct ExtendedFloatPoint

		// from struct Compound
		COMPOUND,
			// from struct Reference
			REFERENCE,
				LVALUE_REFERENCE, // from struct LvalueReference
					LVALUE_REFERENCE_TO_OBJECT, // from struct LvalueReferenceToObject
					LVALUE_REFERENCE_TO_FUNCTION, // from struct LvalueReferenceToFunction
				RVALUE_REFERENCE, // from struct RvalueReference
					RVALUE_REFERENCE_TO_OBJECT, // from struct RvalueReferenceToObject
					RVALUE_REFERENCE_TO_FUNCTION, // from struct RvalueReferenceToFunction

			// from struct Pointer
			POINTER,
				POINTER_TO_OBJECT, // from struct PointerToObject
				POINTER_TO_FUNCTION, // from struct PointerToFunction

			// from struct PointerToMember
			POINTER_TO_MEMBER,
				POINTER_TO_DATA_MEMBER, // from struct PointerToDataMember
				POINTER_TO_MEMBER_FUNCTION, // from struct PointerToMemberFunction

			// from struct Array
			ARRAY,

			// from struct Function
			FUNCTION,

			// from struct Enumeration
			ENUMERATION,
				UNSCOPED_ENUMERATION, // from struct UnscopedEnumeration
				SCOPED_ENUMERATION, // from struct ScopedEnumeration

			// from struct Class
			CLASS,
				NON_UNION, // from struct NonUnion
				UNION, // from struct Union
		UNSUPPORTED
	};

	constexpr static std::string_view kind2str(const ETypeKind kind) {
    switch(kind) {
        case ETypeKind::INCOMPLETE:             return "INCOMPLETE";
        case ETypeKind::FUNDAMENTAL:            return "FUNDAMENTAL";
        case ETypeKind::VOID_TYPE:              return "VOID_TYPE";
        case ETypeKind::NULLPTR_T:              return "NULLPTR_T";
        case ETypeKind::ARITHMETIC:             return "ARITHMETIC";
        case ETypeKind::INTEGRAL:               return "INTEGRAL";
        case ETypeKind::BOOL:                   return "BOOL";
        case ETypeKind::CHARACTER:              return "CHARACTER";
        case ETypeKind::NARROW_CHARACTER:       return "NARROW_CHARACTER";
        case ETypeKind::ORDINARY_CHARACTER:     return "ORDINARY_CHARACTER";
        case ETypeKind::CHAR:                   return "CHAR";
        case ETypeKind::SIGNED_CHAR:            return "SIGNED_CHAR";
        case ETypeKind::UNSIGNED_CHAR:          return "UNSIGNED_CHAR";
#if __cplusplus > 201703L
        case ETypeKind::CHAR8T:                 return "CHAR8T";
#endif
        case ETypeKind::WIDE_CHARACTER:         return "WIDE_CHARACTER";
        case ETypeKind::CHAR16_T:               return "CHAR16_T";
        case ETypeKind::CHAR32_T:               return "CHAR32_T";
        case ETypeKind::WCHAR_T:                return "WCHAR_T";
        case ETypeKind::SIGNED_INTEGRAL:        return "SIGNED_INTEGRAL";
        case ETypeKind::STANDARD_SIGNED:        return "STANDARD_SIGNED";
        case ETypeKind::SHORT_INT:              return "SHORT_INT";
        case ETypeKind::INT:                    return "INT";
        case ETypeKind::LONG_INT:               return "LONG_INT";
        case ETypeKind::LONG_LONG_INT:          return "LONG_LONG_INT";
        case ETypeKind::EXTENDED_SIGNED:        return "EXTENDED_SIGNED";
        case ETypeKind::UNSIGNED_INTEGRAL:      return "UNSIGNED_INTEGRAL";
        case ETypeKind::STANDARD_UNSIGNED:      return "STANDARD_UNSIGNED";
        case ETypeKind::UNSIGNED_SHORT_INT:     return "UNSIGNED_SHORT_INT";
        case ETypeKind::UNSIGNED_INT:           return "UNSIGNED_INT";
        case ETypeKind::UNSIGNED_LONG_INT:      return "UNSIGNED_LONG_INT";
        case ETypeKind::UNSIGNED_LONG_LONG_INT: return "UNSIGNED_LONG_LONG_INT";
        case ETypeKind::EXTENDED_UNSIGNED:      return "EXTENDED_UNSIGNED";
        case ETypeKind::FLOAT_POINT:            return "FLOAT_POINT";
        case ETypeKind::STANDARD_FLOAT_POINT:   return "STANDARD_FLOAT_POINT";
        case ETypeKind::FLOAT:                  return "FLOAT";
        case ETypeKind::DOUBLE:                 return "DOUBLE";
        case ETypeKind::LONG_DOUBLE:            return "LONG_DOUBLE";
        case ETypeKind::EXTENDED_FLOAT_POINT:   return "EXTENDED_FLOAT_POINT";
        case ETypeKind::COMPOUND:               return "COMPOUND";
        case ETypeKind::REFERENCE:              return "REFERENCE";
        case ETypeKind::LVALUE_REFERENCE:       return "LVALUE_REFERENCE";
        case ETypeKind::LVALUE_REFERENCE_TO_OBJECT:   return "LVALUE_REFERENCE_TO_OBJECT";
        case ETypeKind::LVALUE_REFERENCE_TO_FUNCTION:  return "LVALUE_REFERENCE_TO_FUNCTION";
        case ETypeKind::RVALUE_REFERENCE:       return "RVALUE_REFERENCE";
        case ETypeKind::RVALUE_REFERENCE_TO_OBJECT:   return "RVALUE_REFERENCE_TO_OBJECT";
        case ETypeKind::RVALUE_REFERENCE_TO_FUNCTION:  return "RVALUE_REFERENCE_TO_FUNCTION";
        case ETypeKind::POINTER:                return "POINTER";
        case ETypeKind::POINTER_TO_OBJECT:      return "POINTER_TO_OBJECT";
        case ETypeKind::POINTER_TO_FUNCTION:    return "POINTER_TO_FUNCTION";
        case ETypeKind::POINTER_TO_MEMBER:      return "POINTER_TO_MEMBER";
        case ETypeKind::POINTER_TO_DATA_MEMBER: return "POINTER_TO_DATA_MEMBER";
        case ETypeKind::POINTER_TO_MEMBER_FUNCTION: return "POINTER_TO_MEMBER_FUNCTION";
        case ETypeKind::ARRAY:                  return "ARRAY";
        case ETypeKind::FUNCTION:               return "FUNCTION";
        case ETypeKind::ENUMERATION:            return "ENUMERATION";
        case ETypeKind::UNSCOPED_ENUMERATION:   return "UNSCOPED_ENUMERATION";
        case ETypeKind::SCOPED_ENUMERATION:     return "SCOPED_ENUMERATION";
        case ETypeKind::CLASS:                  return "CLASS";
        case ETypeKind::NON_UNION:              return "NON_UNION";
        case ETypeKind::UNION:                  return "UNION";
        case ETypeKind::UNSUPPORTED:            return "UNSUPPORTED";
    }
    // 默认返回（理论上永远不会到达这里）
    return "UNKNOWN";
}

	constexpr static inline bool is_object(ETypeKind kind)
	{
		return kind != ETypeKind::FUNCTION
			&& kind != ETypeKind::REFERENCE
			&& kind != ETypeKind::LVALUE_REFERENCE
			&& kind != ETypeKind::LVALUE_REFERENCE_TO_FUNCTION
			&& kind != ETypeKind::LVALUE_REFERENCE_TO_OBJECT
			&& kind != ETypeKind::RVALUE_REFERENCE
			&& kind != ETypeKind::RVALUE_REFERENCE_TO_FUNCTION
			&& kind != ETypeKind::RVALUE_REFERENCE_TO_OBJECT;
	}

	constexpr static inline bool is_incomplete(const ETypeKind kind)
	{
		return kind == ETypeKind::INCOMPLETE
			|| kind == ETypeKind::VOID_TYPE;
	}
}