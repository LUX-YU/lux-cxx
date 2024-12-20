#pragma once

// see https://en.cppreference.com/w/cpp/language/type

namespace lux::cxx::lan_model
{
	enum class ETypeKind {
		IMCOMPLETE = 0, // from struct CXXType

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

	static constexpr inline bool is_object(ETypeKind kind)
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

	static constexpr inline bool is_imcomplete(ETypeKind kind)
	{
		return kind == ETypeKind::IMCOMPLETE 
			|| kind == ETypeKind::VOID_TYPE;
	}

	/*
	template<CxxTypeEnumKind> struct cxx_type_kind_enum_map;

	class CXXTypeKind
	{
	public:
		static constexpr CxxTypeEnumKind type_enum = CxxTypeEnumKind::CXXTYPE_KIND;

		bool is_type_kind(CxxTypeEnumKind type_kind)
		{
			return _type_kind == type_kind;
		}

		static inline bool is_kind_of(CxxTypeEnumKind type)
		{
			return type == CxxTypeEnumKind::CXXTYPE_KIND;
		}

		static const char* type_kind_name()
		{
			return "CXXTypeKind";
		}

		CxxTypeEnumKind type_kind()
		{
			return _type_kind;
		}

	protected:

		static inline bool __is_kind_of(CxxTypeEnumKind type)
		{
			return false;
		}

		CxxTypeEnumKind _type_kind;
	};
	template<> struct cxx_type_kind_enum_map<CxxTypeEnumKind::CXXTYPE_KIND> { using type = CXXTypeKind; };

#define __DEFINE_CXX_TYPE_START__(class_name, father_name, enum_name)\
	class class_name : public father_name\
	{\
	public:\
		using parent_type = father_name;\
		static constexpr CxxTypeEnumKind type_enum = enum_name;\
		class_name(){ _type_kind = type_enum;};\
		template<CxxTypeEnumKind Type> static constexpr bool is_kind_of()\
		{\
			return std::is_base_of_v<typename cxx_type_kind_enum_map<Type>::type, class_name>;\
		};\
		template<typename T> static constexpr bool is_kind_of()\
		{\
			return std::is_base_of_v<T, class_name>;\
		};\
		static inline bool is_kind_of(CxxTypeEnumKind _type)\
		{\
			if (_type == CxxTypeEnumKind::CXXTYPE_KIND) return true;\
			return parent_type::__is_kind_of(_type);\
		}\
		static const char* type_kind_name()\
		{\
			return #class_name ;\
		}\
	protected:\
		static inline bool __is_kind_of(CxxTypeEnumKind _type)\
		{\
			if (class_name::type_enum == _type) return true; \
				return parent_type::__is_kind_of(_type); \
		}\

#define __DEFINE_CXX_TYPE_END__(class_name, enum_name)\
	};\
	template<> struct cxx_type_kind_enum_map<enum_name> { using type = class_name; };
	

	// type ('*' means the leaf node)
	__DEFINE_CXX_TYPE_START__(Fundamental, CXXTypeKind, CxxTypeEnumKind::FUNDAMENTAL)
	__DEFINE_CXX_TYPE_END__(Fundamental, CxxTypeEnumKind::FUNDAMENTAL)

	// type::fundamental
	// 1* void
	// 2* nullptr
	__DEFINE_CXX_TYPE_START__(Void, Fundamental, CxxTypeEnumKind::VOID_TYPE)
	__DEFINE_CXX_TYPE_END__(Void, CxxTypeEnumKind::VOID_TYPE)
	__DEFINE_CXX_TYPE_START__(Nullptr, Fundamental, CxxTypeEnumKind::NULLPTR)
	__DEFINE_CXX_TYPE_END__(Nullptr, CxxTypeEnumKind::NULLPTR)

	__DEFINE_CXX_TYPE_START__(Arithmetic, Fundamental, CxxTypeEnumKind::ARITHMETIC)
	__DEFINE_CXX_TYPE_END__(Arithmetic, CxxTypeEnumKind::ARITHMETIC)
	// type::fundamental::arithmetic
	__DEFINE_CXX_TYPE_START__(Integral, Arithmetic, CxxTypeEnumKind::INTEGRAL)
	__DEFINE_CXX_TYPE_END__(Integral, CxxTypeEnumKind::INTEGRAL)
	// type::fundamental::arithmetic::integral
	// 3* bool
	__DEFINE_CXX_TYPE_START__(Bool, Integral, CxxTypeEnumKind::BOOL)
	__DEFINE_CXX_TYPE_END__(Bool, CxxTypeEnumKind::BOOL)

	__DEFINE_CXX_TYPE_START__(Character, Integral, CxxTypeEnumKind::CHARACTER)
	__DEFINE_CXX_TYPE_END__(Character, CxxTypeEnumKind::CHARACTER)
	// type::fundamental::arithmetic::character
	__DEFINE_CXX_TYPE_START__(NarrowCharacter, Character, CxxTypeEnumKind::NARROW_CHARACTER)
	__DEFINE_CXX_TYPE_END__(NarrowCharacter, CxxTypeEnumKind::NARROW_CHARACTER)
	// type::fundamental::arithmetic::character::narrow
	// 4* ordinary_character
	// 5* char8_t
	__DEFINE_CXX_TYPE_START__(OrdinaryCharacter, NarrowCharacter, CxxTypeEnumKind::ORDINARY_CHARACTER)
	__DEFINE_CXX_TYPE_END__(OrdinaryCharacter, CxxTypeEnumKind::ORDINARY_CHARACTER)
	__DEFINE_CXX_TYPE_START__(Char8t, NarrowCharacter, CxxTypeEnumKind::CHAR8T)
	__DEFINE_CXX_TYPE_END__(Char8t, CxxTypeEnumKind::CHAR8T)

	// type::fundamental::arithmetic::character
	// 6* wide_char
	__DEFINE_CXX_TYPE_START__(WideCharacter, Character, CxxTypeEnumKind::WIDE_CHARACTER)
	__DEFINE_CXX_TYPE_END__(WideCharacter, CxxTypeEnumKind::WIDE_CHARACTER)

	// type::fundamental::arithmetic::integral
	__DEFINE_CXX_TYPE_START__(SignedIntegral, Integral, CxxTypeEnumKind::SIGNED_INTEGRAL)
	__DEFINE_CXX_TYPE_END__(SignedIntegral, CxxTypeEnumKind::SIGNED_INTEGRAL)
	// type::fundamental::arithmetic::integral::signed
	// 7* standard_signed
	// 8* extended_signed 
	__DEFINE_CXX_TYPE_START__(StandardSigned, SignedIntegral, CxxTypeEnumKind::STANDARD_SIGNED)
	__DEFINE_CXX_TYPE_END__(StandardSigned, CxxTypeEnumKind::STANDARD_SIGNED)
	__DEFINE_CXX_TYPE_START__(ExtendedSigned, SignedIntegral, CxxTypeEnumKind::EXTENDED_SIGNED)
	__DEFINE_CXX_TYPE_END__(ExtendedSigned, CxxTypeEnumKind::EXTENDED_SIGNED)

	// type::fundamental::arithmetic::integral
	__DEFINE_CXX_TYPE_START__(UnsignedIntegral, Integral, CxxTypeEnumKind::UNSIGNED_INTEGRAL)
	__DEFINE_CXX_TYPE_END__(UnsignedIntegral, CxxTypeEnumKind::UNSIGNED_INTEGRAL)
	// type::fundamental::arithmetic::integral:unsigned
	// 9* standard_unsigned
	// 10* standard_unsigned
	__DEFINE_CXX_TYPE_START__(StandardUnsigned, UnsignedIntegral, CxxTypeEnumKind::STANDARD_UNSIGNED)
	__DEFINE_CXX_TYPE_END__(StandardUnsigned, CxxTypeEnumKind::STANDARD_UNSIGNED)
	__DEFINE_CXX_TYPE_START__(ExtendedUnsigned, UnsignedIntegral, CxxTypeEnumKind::EXTENDED_UNSIGNED)
	__DEFINE_CXX_TYPE_END__(ExtendedUnsigned, CxxTypeEnumKind::EXTENDED_UNSIGNED)

	// type::fundamental
	__DEFINE_CXX_TYPE_START__(FloatPoint, Fundamental, CxxTypeEnumKind::FLOAT_POINT)
	__DEFINE_CXX_TYPE_END__(FloatPoint, CxxTypeEnumKind::FLOAT_POINT)
	// type::fundamental::float
	// 11* standard_float
	// 12* standard_float
	__DEFINE_CXX_TYPE_START__(StandardFloatPoint, FloatPoint, CxxTypeEnumKind::STANDARD_FLOAT_POINT)
	__DEFINE_CXX_TYPE_END__(StandardFloatPoint, CxxTypeEnumKind::STANDARD_FLOAT_POINT)
	__DEFINE_CXX_TYPE_START__(ExtendedFloatPoint, FloatPoint, CxxTypeEnumKind::EXTENDED_FLOAT_POINT)
	__DEFINE_CXX_TYPE_END__(ExtendedFloatPoint, CxxTypeEnumKind::EXTENDED_FLOAT_POINT)

	// type
	__DEFINE_CXX_TYPE_START__(Compound, CXXTypeKind, CxxTypeEnumKind::COMPOUND)
	__DEFINE_CXX_TYPE_END__(Compound, CxxTypeEnumKind::COMPOUND)
	// type::compund
	__DEFINE_CXX_TYPE_START__(Reference, Compound, CxxTypeEnumKind::REFERENCE)
	__DEFINE_CXX_TYPE_END__(Reference, CxxTypeEnumKind::REFERENCE)
	// type::compund::reference
	__DEFINE_CXX_TYPE_START__(LvalueReference, Reference, CxxTypeEnumKind::LVALUE_REFERENCE)
	__DEFINE_CXX_TYPE_END__(LvalueReference, CxxTypeEnumKind::LVALUE_REFERENCE)
	// type::compund::reference::lvalue
	// 13* lvalue_ref_to_obj
	// 14* lvalue_ref_to_func
	__DEFINE_CXX_TYPE_START__(LvalueReferenceToObject, LvalueReference, CxxTypeEnumKind::LVALUE_REFERENCE_TO_OBJECT)
	__DEFINE_CXX_TYPE_END__(LvalueReferenceToObject, CxxTypeEnumKind::LVALUE_REFERENCE_TO_OBJECT)
	__DEFINE_CXX_TYPE_START__(LvalueReferenceToFunction, LvalueReference, CxxTypeEnumKind::LVALUE_REFERENCE_TO_FUNCTION)
	__DEFINE_CXX_TYPE_END__(LvalueReferenceToFunction, CxxTypeEnumKind::LVALUE_REFERENCE_TO_FUNCTION)
	
	// type::compund::reference
	__DEFINE_CXX_TYPE_START__(RvalueReference, Reference, CxxTypeEnumKind::RVALUE_REFERENCE)
	__DEFINE_CXX_TYPE_END__(RvalueReference, CxxTypeEnumKind::RVALUE_REFERENCE)
	// type::compund::reference::rvalue
	// 15* rvalue_ref_to_obj
	// 16* rvalue_ref_to_func
	__DEFINE_CXX_TYPE_START__(RvalueReferenceToObject, RvalueReference, CxxTypeEnumKind::RVALUE_REFERENCE_TO_OBJECT)
	__DEFINE_CXX_TYPE_END__(RvalueReferenceToObject, CxxTypeEnumKind::RVALUE_REFERENCE_TO_OBJECT)
	__DEFINE_CXX_TYPE_START__(RvalueReferenceToFunction, RvalueReference, CxxTypeEnumKind::RVALUE_REFERENCE_TO_FUNCTION)
	__DEFINE_CXX_TYPE_END__(RvalueReferenceToFunction, CxxTypeEnumKind::RVALUE_REFERENCE_TO_FUNCTION)
	
	// type::compund
	__DEFINE_CXX_TYPE_START__(Pointer, Compound, CxxTypeEnumKind::POINTER)
	__DEFINE_CXX_TYPE_END__(Pointer, CxxTypeEnumKind::POINTER)
	// type::compund::pointer
	// 17* point_to_obj
	// 18* point_to_func
	__DEFINE_CXX_TYPE_START__(PointerToObject, Pointer, CxxTypeEnumKind::POINTER_TO_OBJECT)
	__DEFINE_CXX_TYPE_END__(PointerToObject, CxxTypeEnumKind::POINTER_TO_OBJECT)
	__DEFINE_CXX_TYPE_START__(PointerToFunction, Pointer, CxxTypeEnumKind::POINTER_TO_FUNCTION)
	__DEFINE_CXX_TYPE_END__(PointerToFunction, CxxTypeEnumKind::POINTER_TO_FUNCTION)
	
	// type::compund
	__DEFINE_CXX_TYPE_START__(PointerToMember, Compound, CxxTypeEnumKind::POINTER_TO_MEMBER)
	__DEFINE_CXX_TYPE_END__(PointerToMember, CxxTypeEnumKind::POINTER_TO_MEMBER)
	// type::compund::pointer_to_member
	// 19* point_to_data_member
	// 20* point_to_func_member
	__DEFINE_CXX_TYPE_START__(PointerToDataMember, PointerToMember, CxxTypeEnumKind::POINTER_TO_DATA_MEMBER)
	__DEFINE_CXX_TYPE_END__(PointerToDataMember, CxxTypeEnumKind::POINTER_TO_DATA_MEMBER)
	__DEFINE_CXX_TYPE_START__(PointerToMemberFunction, PointerToMember, CxxTypeEnumKind::POINTER_TO_MEMBER_FUNCTION)
	__DEFINE_CXX_TYPE_END__(PointerToMemberFunction, CxxTypeEnumKind::POINTER_TO_MEMBER_FUNCTION)
	
	// type::compund
	// 21* array
	// 22* function
	__DEFINE_CXX_TYPE_START__(Array, Compound, CxxTypeEnumKind::ARRAY)
	__DEFINE_CXX_TYPE_END__(Array, CxxTypeEnumKind::ARRAY)

	__DEFINE_CXX_TYPE_START__(Function, Compound, CxxTypeEnumKind::FUNCTION)
	__DEFINE_CXX_TYPE_END__(Function, CxxTypeEnumKind::FUNCTION)
	
	__DEFINE_CXX_TYPE_START__(Enumeration, Compound, CxxTypeEnumKind::ENUMERATION)
	__DEFINE_CXX_TYPE_END__(Enumeration, CxxTypeEnumKind::ENUMERATION)

	template<typename T> concept concept_cxx_type_kind = std::is_base_of_v<CXXTypeKind, T>;
	
	#if __cplusplus > 202002L // only support c++23
	// type::compund::enumeration
	// 23* unscoped_enumeration
	// 24* scoped_enumeration
	__DEFINE_CXX_TYPE_START__(UnscopedEnumeration, Enumeration, CxxTypeEnumKind::UNSCOPED_ENUMERATION)
	__DEFINE_CXX_TYPE_END__(UnscopedEnumeration, CxxTypeEnumKind::UNSCOPED_ENUMERATION)
	__DEFINE_CXX_TYPE_START__(ScopedEnumeration, Enumeration, CxxTypeEnumKind::SCOPED_ENUMERATION)
	__DEFINE_CXX_TYPE_END__(ScopedEnumeration, CxxTypeEnumKind::SCOPED_ENUMERATION)
	#endif
	
	// type::compund
	__DEFINE_CXX_TYPE_START__(Class, Compound, CxxTypeEnumKind::CLASS)
	__DEFINE_CXX_TYPE_END__(Class, CxxTypeEnumKind::CLASS)
	// type::compund::class
	// 25* non_union
	// 26* union
	__DEFINE_CXX_TYPE_START__(NonUnion, Class, CxxTypeEnumKind::NON_UNION)
	__DEFINE_CXX_TYPE_END__(NonUnion, CxxTypeEnumKind::NON_UNION)

	__DEFINE_CXX_TYPE_START__(Union, Class, CxxTypeEnumKind::UNION)
	__DEFINE_CXX_TYPE_END__(Union, CxxTypeEnumKind::UNION)

	template<typename T>
	struct type_check
	{
	public:
		static constexpr auto __get_type_of() {
			if constexpr (std::is_void_v<T>)
				return Void{}; // 1*
			else if constexpr (std::is_null_pointer_v<T>)
				return Nullptr{}; // 2*
			// integral and float point types
			else if constexpr (std::is_arithmetic_v<T>)
			{
				if constexpr (std::is_integral_v<T>)
				{
					if constexpr (std::is_same_v<T, bool>)
						return Bool{}; // 3*
					// narrow character types
					else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>)
						return OrdinaryCharacter{}; // 4*
					else if constexpr (std::is_same_v<T, char8_t>)
						return Char8t{}; // 5*
					// wide character types
					else if constexpr (std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> || std::is_same_v<T, wchar_t>)
						return WideCharacter{}; // 6*
					
					else if constexpr (std::is_signed_v<T>) // short int, int, long int, long long int
						return StandardSigned{}; // 7* 8* This also includes ExtendedSigned
					else if constexpr (std::is_unsigned_v<T>)
						return StandardUnsigned{}; // 9* 10* This also includes ExtendedUnsigned
				}
				else if constexpr (std::is_floating_point_v<T>)
					return StandardFloatPoint{}; // 11* 12* This also includes ExtendedFloatPoint
			}
			else if constexpr (std::is_reference_v<T>)
			{
				if constexpr (std::is_lvalue_reference_v<T>) {
					if constexpr (std::is_function_v<std::remove_reference_t<T>>)
						return LvalueReferenceToFunction{}; // 14*
					else
						return LvalueReferenceToObject{}; // 13*
				}
				else if constexpr (std::is_rvalue_reference_v<T>) {
					if constexpr (std::is_function_v<std::remove_reference_t<T>>)
						return RvalueReferenceToFunction{}; // 16*
					else
						return RvalueReferenceToObject{}; // 15*
				}
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				if constexpr (std::is_function_v<std::remove_pointer_t<T>>)
					return PointerToFunction{}; // 18*
				else 
					return PointerToObject{}; // 17*
			}
			else if constexpr (std::is_member_pointer_v<T>)
			{
				if constexpr (std::is_member_function_pointer_v<T>)
					return PointerToMemberFunction{}; // 20*
				else if constexpr (std::is_member_object_pointer_v<T>)
					return PointerToDataMember{}; // 19*
			}
			else if constexpr (std::is_array_v<T>)
				return Array{}; // 21*
			else if constexpr (std::is_function_v<T>)
				return Function{}; // 22*

			else if constexpr (std::is_enum_v<T>)
			{
#if __cplusplus > 202002L
				if constexpr (std::is_scoped_enum_v<T>)
					return ScopedEnumeration{}; // 24*
				else
					return UnscopedEnumeration{}; // 23*
#else
				return Enumeration{};
#endif
			}
			else if constexpr (std::is_union_v<T>)
				return Union{}; // 26*
			else if constexpr (std::is_class_v<T>)
				return NonUnion{}; // 25*
			else
				return CXXTypeKind{}; // Default
		}

	public:
		using type = decltype(__get_type_of());
	};

	template<typename T> using type_check_t = typename type_check<T>::type;
	*/
}