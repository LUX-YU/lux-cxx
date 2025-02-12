#pragma once
#include <lux/cxx/lan_model/type.hpp>

namespace lux::cxx::lan_model
{
	struct Arithmetic : public Fundamental { static constexpr auto kind = ETypeKind::ARITHMETIC;};
		struct IntegralType : public Arithmetic { static constexpr auto kind = ETypeKind::INTEGRAL; bool is_const; bool is_volatile; };
			struct Bool : public IntegralType{ static constexpr auto kind = ETypeKind::BOOL;};

			struct CharacterTypes : public IntegralType{ static constexpr auto kind = ETypeKind::CHARACTER; };
				struct NarrowCharacterType : public CharacterTypes { static constexpr auto kind = ETypeKind::NARROW_CHARACTER; };
					struct OrdinaryCharacterType : public NarrowCharacterType { static constexpr auto kind = ETypeKind::ORDINARY_CHARACTER; };
						struct Char : public OrdinaryCharacterType { static constexpr auto kind = ETypeKind::CHAR; using type = char; };
						// signed char and unsigned char are narrow character types, but they are not character types.In other words, 
						// the set of narrow character types is not a subset of the set of character types.
						struct SignedChar : public OrdinaryCharacterType { static constexpr auto kind = ETypeKind::SIGNED_CHAR; using type = signed char; };
						struct UnsignedChar : public OrdinaryCharacterType { static constexpr auto kind = ETypeKind::UNSIGNED_CHAR; using type = unsigned char; };
#if __cplusplus > 201703L // only support c++20
					struct CHAR8T : public NarrowCharacterType{ static constexpr auto kind = ETypeKind::CHAR8T; using type = char8_t; };
#endif
				struct WideCharacterType : public CharacterTypes { static constexpr auto kind = ETypeKind::WIDE_CHARACTER; };
					struct CHAR16T : public WideCharacterType { static constexpr auto kind = ETypeKind::CHAR16_T; using type = char16_t; };
					struct CHAR32T : public WideCharacterType { static constexpr auto kind = ETypeKind::CHAR32_T; using type = char32_t; };
					struct WCHART  : public WideCharacterType { static constexpr auto kind = ETypeKind::WCHAR_T; using type = wchar_t; };

			struct SignedIntegral : public IntegralType{ static constexpr auto kind = ETypeKind::SIGNED_INTEGRAL;};
				struct StandardSigned : public SignedIntegral { static constexpr auto kind = ETypeKind::STANDARD_SIGNED; };
					struct ShortInt : public StandardSigned { static constexpr auto kind = ETypeKind::SHORT_INT; using type = short int; };
					struct Int : public StandardSigned { static constexpr auto kind = ETypeKind::INT; using type = int; };
					struct LongInt : public StandardSigned { static constexpr auto kind = ETypeKind::LONG_INT; using type = long int; };
					struct LongLongInt : public StandardSigned { static constexpr auto kind = ETypeKind::LONG_LONG_INT; using type = long long int; };
				struct ExtendedSigned : public SignedIntegral { static constexpr auto kind = ETypeKind::EXTENDED_SIGNED; };

			struct UnsignedIntegral : public IntegralType { static constexpr auto kind = ETypeKind::UNSIGNED_INTEGRAL; };
				struct StandardUnsigned : public UnsignedIntegral { static constexpr auto kind = ETypeKind::STANDARD_UNSIGNED; };
					struct UnsignedShortInt : public StandardUnsigned { static constexpr auto kind = ETypeKind::UNSIGNED_SHORT_INT; using type = unsigned short int; };
					struct UnsignedInt : public StandardUnsigned { static constexpr auto kind = ETypeKind::UNSIGNED_INT; using type = unsigned int; };
					struct UnsignedLongInt : public StandardUnsigned { static constexpr auto kind = ETypeKind::UNSIGNED_LONG_INT; using type = unsigned long int; };
					struct UnsignedLongLongInt : public StandardUnsigned { static constexpr auto kind = ETypeKind::UNSIGNED_LONG_LONG_INT; using type = unsigned long long int; };
				struct ExtendedUnsigned : public UnsignedIntegral { static constexpr auto kind = ETypeKind::EXTENDED_UNSIGNED; };

		struct FloatPoint : public Arithmetic{ static constexpr auto kind = ETypeKind::FLOAT_POINT; };
			struct StandardFloatPoint : public FloatPoint { static constexpr auto kind = ETypeKind::STANDARD_FLOAT_POINT; };
				struct Float : public StandardFloatPoint { static constexpr auto kind = ETypeKind::FLOAT; using type = float; };
				struct Double : public StandardFloatPoint { static constexpr auto kind = ETypeKind::DOUBLE; using type = double; };
				struct LongDouble : public StandardFloatPoint { static constexpr auto kind = ETypeKind::LONG_DOUBLE; using type = long double; };
			struct ExtendFloatPoint : public FloatPoint { static constexpr auto kind = ETypeKind::EXTENDED_FLOAT_POINT; };

	// specialized version
	template<> struct type_kind_map<ETypeKind::ARITHMETIC> { using type = Arithmetic; };
		template<> struct type_kind_map<ETypeKind::INTEGRAL> { using type = IntegralType; };
			template<> struct type_kind_map<ETypeKind::BOOL> { using type = Bool; };
			template<> struct type_kind_map<ETypeKind::CHARACTER> { using type = CharacterTypes; };
				template<> struct type_kind_map<ETypeKind::NARROW_CHARACTER> { using type = NarrowCharacterType; };
					template<> struct type_kind_map<ETypeKind::ORDINARY_CHARACTER> { using type = OrdinaryCharacterType; };
						template<> struct type_kind_map<ETypeKind::CHAR> { using type = Char; };
						template<> struct type_kind_map<ETypeKind::SIGNED_CHAR> { using type = SignedChar; };
						template<> struct type_kind_map<ETypeKind::UNSIGNED_CHAR> { using type = UnsignedChar; };
#if __cplusplus > 201703L // only support c++20
				template<> struct type_kind_map<ETypeKind::CHAR8T> { using type = CHAR8T; };
#endif
				template<> struct type_kind_map<ETypeKind::WIDE_CHARACTER> { using type = WideCharacterType; };
					template<> struct type_kind_map<ETypeKind::CHAR16_T> { using type = CHAR16T; };
					template<> struct type_kind_map<ETypeKind::CHAR32_T> { using type = CHAR32T; };
					template<> struct type_kind_map<ETypeKind::WCHAR_T> { using type = WCHART; };
			template<> struct type_kind_map<ETypeKind::SIGNED_INTEGRAL> { using type = SignedIntegral; };
				template<> struct type_kind_map<ETypeKind::STANDARD_SIGNED> { using type = StandardSigned; };
					template<> struct type_kind_map<ETypeKind::SHORT_INT> { using type = ShortInt; };
					template<> struct type_kind_map<ETypeKind::INT> { using type = Int; };
					template<> struct type_kind_map<ETypeKind::LONG_INT> { using type = LongInt; };
					template<> struct type_kind_map<ETypeKind::LONG_LONG_INT> { using type = LongLongInt; };
				template<> struct type_kind_map<ETypeKind::EXTENDED_SIGNED> { using type = ExtendedSigned; };
			template<> struct type_kind_map<ETypeKind::UNSIGNED_INTEGRAL> { using type = UnsignedIntegral; };
				template<> struct type_kind_map<ETypeKind::STANDARD_UNSIGNED> { using type = StandardUnsigned; };
					template<> struct type_kind_map<ETypeKind::UNSIGNED_SHORT_INT> { using type = UnsignedShortInt; };
					template<> struct type_kind_map<ETypeKind::UNSIGNED_INT> { using type = UnsignedInt; };
					template<> struct type_kind_map<ETypeKind::UNSIGNED_LONG_INT> { using type = UnsignedLongInt; };
					template<> struct type_kind_map<ETypeKind::UNSIGNED_LONG_LONG_INT> { using type = UnsignedLongLongInt; };
				template<> struct type_kind_map<ETypeKind::EXTENDED_UNSIGNED> { using type = ExtendedUnsigned; };
		template<> struct type_kind_map<ETypeKind::FLOAT_POINT> { using type = FloatPoint; };
			template<> struct type_kind_map<ETypeKind::STANDARD_FLOAT_POINT> { using type = StandardFloatPoint; };
				template<> struct type_kind_map<ETypeKind::FLOAT> { using type = Float; };
				template<> struct type_kind_map<ETypeKind::DOUBLE> { using type = Double; };
				template<> struct type_kind_map<ETypeKind::LONG_DOUBLE> { using type = LongDouble; };
			template<> struct type_kind_map<ETypeKind::EXTENDED_FLOAT_POINT> { using type = ExtendFloatPoint; };
}
