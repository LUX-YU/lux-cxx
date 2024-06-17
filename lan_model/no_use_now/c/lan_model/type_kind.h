#ifndef __LUX_C_TYPE_KIND_H__
#define __LUX_C_TYPE_KIND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	E_LUX_C_TYPE_KIND_IMCOMPLETE = 0, // from struct CXXType

	// from struct Fundamental
	E_LUX_C_TYPE_KIND_FUNDAMENTAL,
	E_LUX_C_TYPE_KIND_VOID, // from struct Void
	E_LUX_C_TYPE_KIND_NULLPTR_T, // from struct Nullptr
		E_LUX_C_TYPE_KIND_ARITHMETIC, // from struct Arithmetic
		// from struct Integral
			E_LUX_C_TYPE_KIND_INTEGRAL,
				E_LUX_C_TYPE_KIND_BOOL, // from struct Bool
				E_LUX_C_TYPE_KIND_CHARACTER, // from struct Character
					E_LUX_C_TYPE_KIND_NARROW_CHARACTER, // from struct NarrowCharacter
						E_LUX_C_TYPE_KIND_ORDINARY_CHARACTER, // from struct OrdinaryCharacter
							E_LUX_C_TYPE_KIND_CHAR,
							E_LUX_C_TYPE_KIND_SIGNED_CHAR,
							E_LUX_C_TYPE_KIND_UNSIGNED_CHAR,
						E_LUX_C_TYPE_KIND_CHAR8T, // from struct Char8t
					E_LUX_C_TYPE_KIND_WIDE_CHARACTER, // from struct WideCharacter
						E_LUX_C_TYPE_KIND_CHAR16_T,
						E_LUX_C_TYPE_KIND_CHAR32_T,
						E_LUX_C_TYPE_KIND_WCHAR_T,
				E_LUX_C_TYPE_KIND_SIGNED_INTEGRAL, // from struct SignedIntegral
					E_LUX_C_TYPE_KIND_STANDARD_SIGNED, // from struct StandardSigned
						// SIGNED_CHAR
						E_LUX_C_TYPE_KIND_SHORT_INT,
						E_LUX_C_TYPE_KIND_INT,
						E_LUX_C_TYPE_KIND_LONG_INT,
						E_LUX_C_TYPE_KIND_LONG_LONG_INT,
					E_LUX_C_TYPE_KIND_EXTENDED_SIGNED, // from struct ExtendedSigned

				// from struct UnsignedIntegral
				E_LUX_C_TYPE_KIND_UNSIGNED_INTEGRAL,
					E_LUX_C_TYPE_KIND_STANDARD_UNSIGNED, // from struct StandardUnsigned
						// UNSIGNED_CHAR
						E_LUX_C_TYPE_KIND_UNSIGNED_SHORT_INT,
						E_LUX_C_TYPE_KIND_UNSIGNED_INT,
						E_LUX_C_TYPE_KIND_UNSIGNED_LONG_INT,
						E_LUX_C_TYPE_KIND_UNSIGNED_LONG_LONG_INT,
					E_LUX_C_TYPE_KIND_EXTENDED_UNSIGNED, // from struct ExtendedUnsigned

			// from struct FloatPoint
			E_LUX_C_TYPE_KIND_FLOAT_POINT,
				E_LUX_C_TYPE_KIND_STANDARD_FLOAT_POINT, // from struct StandardFloatPoint
					E_LUX_C_TYPE_KIND_FLOAT,
					E_LUX_C_TYPE_KIND_DOUBLE,
					E_LUX_C_TYPE_KIND_LONG_DOUBLE,
				E_LUX_C_TYPE_KIND_EXTENDED_FLOAT_POINT, // from struct ExtendedFloatPoint

	// from struct Compound
	E_LUX_C_TYPE_KIND_COMPOUND,
		// from struct Reference
		E_LUX_C_TYPE_KIND_REFERENCE,
			E_LUX_C_TYPE_KIND_LVALUE_REFERENCE, // from struct LvalueReference
				E_LUX_C_TYPE_KIND_LVALUE_REFERENCE_TO_OBJECT, // from struct LvalueReferenceToObject
				E_LUX_C_TYPE_KIND_LVALUE_REFERENCE_TO_FUNCTION, // from struct LvalueReferenceToFunction
			E_LUX_C_TYPE_KIND_RVALUE_REFERENCE, // from struct RvalueReference
				E_LUX_C_TYPE_KIND_RVALUE_REFERENCE_TO_OBJECT, // from struct RvalueReferenceToObject
				E_LUX_C_TYPE_KIND_RVALUE_REFERENCE_TO_FUNCTION, // from struct RvalueReferenceToFunction

		// from struct Pointer
		E_LUX_C_TYPE_KIND_POINTER,
			E_LUX_C_TYPE_KIND_POINTER_TO_OBJECT, // from struct PointerToObject
			E_LUX_C_TYPE_KIND_POINTER_TO_FUNCTION, // from struct PointerToFunction

		// from struct PointerToMember
		E_LUX_C_TYPE_KIND_POINTER_TO_MEMBER,
			E_LUX_C_TYPE_KIND_POINTER_TO_DATA_MEMBER, // from struct PointerToDataMember
			E_LUX_C_TYPE_KIND_POINTER_TO_MEMBER_FUNCTION, // from struct PointerToMemberFunction

		// from struct Array
		E_LUX_C_TYPE_KIND_ARRAY,

		// from struct Function
		E_LUX_C_TYPE_KIND_FUNCTION,

		// from struct Enumeration
		E_LUX_C_TYPE_KIND_ENUMERATION,
			E_LUX_C_TYPE_KIND_UNSCOPED_ENUMERATION, // from struct UnscopedEnumeration
			E_LUX_C_TYPE_KIND_SCOPED_ENUMERATION, // from struct ScopedEnumeration

		// from struct Class
		E_LUX_C_TYPE_KIND_CLASS,
			E_LUX_C_TYPE_KIND_NON_UNION, // from struct NonUnion
			E_LUX_C_TYPE_KIND_UNION, // from struct Union
	E_LUX_C_TYPE_KIND_UNSUPPORTED
}E_LUX_C_TYPE_KIND;


#ifdef __cplusplus
}
#endif

#endif
