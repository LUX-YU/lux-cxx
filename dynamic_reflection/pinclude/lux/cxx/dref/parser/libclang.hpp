#pragma once
#include <clang-c/Index.h>
#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <functional>
#include <vector>
#include <ostream>

namespace lux::cxx::dref 
{
	class Cursor;

	class TranslationUnit
	{
		friend class Cursor;
		friend class CursorKind;
	public:
		explicit TranslationUnit(CXTranslationUnit unit)
			: _unit(unit) {}

		~TranslationUnit()
		{
			if (_unit)
			{
				clang_disposeTranslationUnit(_unit);
			}
		}

		bool isVaild()
		{
			return _unit != nullptr;
		}

		std::vector<std::string> diagnostics() {
			int nbDiag = clang_getNumDiagnostics(_unit);

			bool foundError = false;
			std::vector<std::string> error_list;
			for (unsigned int currentDiag = 0; currentDiag < nbDiag; ++currentDiag) {
				CXDiagnostic diagnotic = clang_getDiagnostic(_unit, currentDiag);
				CXString errorString = clang_formatDiagnostic(diagnotic, clang_defaultDiagnosticDisplayOptions());
				std::string tmp{clang_getCString(errorString)};
				error_list.push_back(std::move(tmp));
			}
			return error_list;
		}

	private:
		CXTranslationUnit _unit;
	};

	class String
	{
		friend class Type;
		friend class Cursor;
		friend class CursorKind;
	public:
		String(String&& other) noexcept
		{
			_string = other._string;
			other._string.data = nullptr;
		}

		String& operator=(String&& other) noexcept
		{
			_string = other._string;
			other._string.data = nullptr;
			return *this;
		}

		String& operator=(const String&) = delete;
		String(const String&) = delete;

		~String()
		{
			if (_string.data) clang_disposeString(_string);
		}

		[[nodiscard]] std::string to_std() const
		{
			const char* c_str = clang_getCString(_string);
			std::string ret(c_str);
			return ret;
		}

		[[nodiscard]] std::size_t size() const
		{
			const char* c_str = clang_getCString(_string);
			return std::strlen(c_str);
		}

		[[nodiscard]] const char* c_str() const
		{
			return clang_getCString(_string);
		}

	private:
		explicit String(CXString string)
			:_string(string) {}

		CXString _string{};
	};

	static std::ostream& operator<<(std::ostream& os, const String& str)
	{
		return os << str.c_str();
	}

	class Type
	{
		friend class Cursor;
	public:
		// The followed function about objc are ignored
		// clang_getDeclObjCTypeEncoding
		// clang_Type_getObjCEncoding
		// clang_Type_getObjCObjectBaseType
		// clang_Type_getNumObjCProtocolRefs
		// clang_Type_getObjCProtocolDecl
		// clang_Type_getNumObjCTypeArgs
		// clang_Type_getObjCTypeArg

		[[nodiscard]] CXTypeKind typeKind() const
		{
			return _type.kind;
		}

		[[nodiscard]] bool isBuiltinType() const
		{
			return _type.kind < CXTypeKind::CXType_LastBuiltin
				&& _type.kind > CXTypeKind::CXType_FirstBuiltin;
		}

		[[nodiscard]] bool isRecordType() const
		{
			return _type.kind == CXTypeKind::CXType_Record;
		}

		[[nodiscard]] bool isEnum() const
		{
			return _type.kind == CXTypeKind::CXType_Enum;
		}

		bool operator==(Type other) const
		{
			// non-zero if the CXTypes represent the same type and
			// zero otherwise.
			return clang_equalTypes(_type, other._type);
		}

		[[nodiscard]] String typeSpelling() const
		{
			return String{ clang_getTypeSpelling(_type) };
		}
		
		[[nodiscard]] String typeKindSpelling() const
		{
			return String{ clang_getTypeKindSpelling(_type.kind) };
		}

		/**
		* Return the alignment of a type in bytes as per C++[expr.alignof]
		*   standard.
		*
		* If the type declaration is invalid, CXTypeLayoutError_Invalid is returned.
		* If the type declaration is an incomplete type, CXTypeLayoutError_Incomplete
		*   is returned.
		* If the type declaration is a dependent type, CXTypeLayoutError_Dependent is
		*   returned.
		* If the type declaration is not a constant size type,
		*   CXTypeLayoutError_NotConstantSize is returned.
		*/
		[[nodiscard]] size_t typeAlignof() const
		{
			return clang_Type_getAlignOf(_type);
		}

		/**
		 * Return the class type of an member pointer type.
		 *
		 * If a non-member-pointer type is passed in, an invalid type is returned.
		 */
		[[nodiscard]] Type classType() const
		{
			return Type(clang_Type_getClassType(_type));
		}

		/**
		 * Return the canonical type for a CXType.
		 *
		 * Clang's type system explicitly models typedefs and all the ways
		 * a specific type can be represented.  The canonical type is the underlying
		 * type with all the "sugar" removed.  For example, if 'T' is a typedef
		 * for 'int', the canonical type for 'T' would be 'int'.
		 */
		[[nodiscard]] Type canonicalType() const
		{
			return Type(clang_getCanonicalType(_type));
		}

		/**
		 * Return the size of a type in bytes as per C++[expr.sizeof] standard.
		 *
		 * If the type declaration is invalid, CXTypeLayoutError_Invalid is returned.
		 * If the type declaration is an incomplete type, CXTypeLayoutError_Incomplete
		 *   is returned.
		 * If the type declaration is a dependent type, CXTypeLayoutError_Dependent is
		 *   returned.
		 */
		[[nodiscard]] size_t typeSizeof() const
		{
			return clang_Type_getSizeOf(_type);
		}

		/**
		 * Return the offset of a field named S in a record of type T in bits
		 *   as it would be returned by __offsetof__ as per C++11[18.2p4]
		 *
		 * If the cursor is not a record field declaration, CXTypeLayoutError_Invalid
		 *   is returned.
		 * If the field's type declaration is an incomplete type,
		 *   CXTypeLayoutError_Incomplete is returned.
		 * If the field's type declaration is a dependent type,
		 *   CXTypeLayoutError_Dependent is returned.
		 * If the field's name S is not found,
		 *   CXTypeLayoutError_InvalidFieldName is returned.
		 */
		size_t typeOffset(const char* name) const
		{
			return clang_Type_getOffsetOf(_type, name);
		}

		/**
		 * Return the type that was modified by this attributed type.
		 *
		 * If the type is not an attributed type, an invalid type is returned.
		 */
		[[nodiscard]] Type modifiedType() const
		{
			return Type(clang_Type_getModifiedType(_type));
		}

		/**
		 * Gets the type contained by this atomic type.
		 *
		 * If a non-atomic type is passed in, an invalid type is returned.
		 */
		[[nodiscard]] Type valueType() const
		{
			return Type(clang_Type_getValueType(_type));
		}

		[[nodiscard]] bool isConstQualifiedType() const
		{
			return clang_isConstQualifiedType(_type);
		}

		[[nodiscard]] bool isVolatileQualifiedType() const
		{
			return clang_isVolatileQualifiedType(_type);
		}

		[[nodiscard]] bool isRestrictQualifiedType() const
		{
			return clang_isRestrictQualifiedType(_type);
		}

		[[nodiscard]] size_t getAddressSpace() const
		{
			return clang_getAddressSpace(_type);
		}

		[[nodiscard]] String getTypedefName() const
		{
			return String(clang_getTypedefName(_type));
		}

		[[nodiscard]] int getNumTemplateArguments() const
		{
			return clang_Type_getNumTemplateArguments(_type);
		}

		[[nodiscard]] Type getPointeeType() const
		{
			return Type(clang_getPointeeType(_type));
		}

		[[nodiscard]] CXCallingConv getFunctionTypeCallingConv() const
		{
			return clang_getFunctionTypeCallingConv(_type);
		}

		// function related
		[[nodiscard]] int getExceptionSpecificationType() const
		{
			return clang_getExceptionSpecificationType(_type);
		}

		/**
		 * Retrieve the argument cursor of a function or method.
		 *
		 * The argument cursor can be determined for calls as well as for declarations
		 * of functions or methods. For other cursors and for invalid indices, an
		 * invalid cursor is returned.
		 */
		[[nodiscard]] Type resultType() const
		{
			return Type(clang_getResultType(_type));
		}

		// function related
		[[nodiscard]] int getNumArgTypes() const
		{
			return clang_getNumArgTypes(_type);
		}

		// function related
		[[nodiscard]] Type getArgType(unsigned i) const
		{
			return Type(clang_getArgType(_type, i));
		}

		[[nodiscard]] bool isFunctionTypeVariadic() const
		{
			return clang_isFunctionTypeVariadic(_type);
		}

		[[nodiscard]] Type getTemplateArgumentAsType(unsigned i) const
		{
			return Type(clang_Type_getTemplateArgumentAsType(_type, i));
		}

		[[nodiscard]] size_t getNumElements() const
		{
			return clang_getNumElements(_type);
		}

		[[nodiscard]] bool isPODType() const
		{
			return clang_isPODType(_type);
		}

		[[nodiscard]] Type getElementType() const
		{
			return Type(clang_getElementType(_type));
		}

		[[nodiscard]] Type getArrayElementType() const
		{
			return Type(clang_getArrayElementType(_type));
		}

		[[nodiscard]] size_t getArraySize() const
		{
			return clang_getArraySize(_type);
		}

		[[nodiscard]] Type getNamedType() const
		{
			return Type(clang_Type_getNamedType(_type));
		}

		[[nodiscard]] CXTypeNullabilityKind getNullability() const
		{
			return clang_Type_getNullability(_type);
		}

		[[nodiscard]] size_t isTransparentTagTypedef() const
		{
			return clang_Type_isTransparentTagTypedef(_type);
		}

		[[nodiscard]] CXRefQualifierKind getCXXRefQualifier() const
		{
			return clang_Type_getCXXRefQualifier(_type);
		}

		using TypeVisitCallback = std::function<CXVisitorResult(const CXCursor&)>;

		// clang_Type_visitFields
		[[nodiscard]] size_t visitFields(TypeVisitCallback callback) const
		{
			auto raw_cb =
				[](CXCursor cursor, CXClientData client_data) -> CXVisitorResult
				{
					auto cb_ptr = static_cast<TypeVisitCallback*>(client_data);
					return (*cb_ptr)(cursor);
				};
			return clang_Type_visitFields(_type, raw_cb, (CXClientData)&callback);
		}

	private:
		explicit Type(CXType type) : _type(type) {}

		CXType _type;
	};

	class CursorKind
	{
		friend class Cursor;
	public:
		bool operator==(const CursorKind& other) const
		{
			return other._kind == _kind;
		}

		bool operator==(CXCursorKind other_enum) const
		{
			return other_enum == _kind;
		}

		[[nodiscard]] inline bool isDeclatation() const
		{
			return clang_isDeclaration(_kind);
		}

		[[nodiscard]] inline bool isReference() const
		{
			return clang_isReference(_kind);
		}

		/**
		 * Determine whether the given cursor kind represents an expression.
		 */
		[[nodiscard]] inline bool isExpression() const
		{
			return clang_isExpression(_kind);
		}

		/**
		 * Determine whether the given cursor kind represents a statement.
		 */
		[[nodiscard]] inline bool isStatement() const
		{
			return clang_isStatement(_kind);
		}

		/**
		 * Determine whether the given cursor kind represents an attribute.
		 */
		[[nodiscard]] inline bool isAttribute() const
		{
			return clang_isAttribute(_kind);
		}

		/**
		 * Determine whether the given cursor kind represents an invalid
		 * cursor.
		 */
		[[nodiscard]] inline bool isInvalid() const
		{
			return clang_isInvalid(_kind);
		}

		/**
		 * Determine whether the given cursor kind represents a translation
		 * unit.
		 */
		[[nodiscard]] inline bool isTranslationUnit() const
		{
			return clang_isTranslationUnit(_kind);
		}

		/***
		 * Determine whether the given cursor represents a preprocessing
		 * element, such as a preprocessor directive or macro instantiation.
		 */
		[[nodiscard]] inline bool isPreprocessing() const
		{
			return clang_isPreprocessing(_kind);
		}

		/**
		 * A declaration whose specific kind is not exposed via this
		 * interface.
		 *
		 * Unexposed declarations have the same operations as any other kind
		 * of declaration; one can extract their location information,
		 * spelling, find their definitions, etc. However, the specific kind
		 * of the declaration is not reported.
		 */
		[[nodiscard]] bool isUnexposedDeclaration() const
		{
			return _kind == CXCursor_UnexposedDecl;
		}

		[[nodiscard]] bool isStructDeclaration() const
		{
			return _kind == CXCursor_StructDecl;
		}

		[[nodiscard]] bool isUnionDeclaration() const
		{
			return _kind == CXCursor_UnionDecl;
		}

		[[nodiscard]] bool isClassDeclaration() const
		{
			return _kind == CXCursor_ClassDecl;
		}

		[[nodiscard]] bool isEnumDeclaration() const
		{
			return _kind == CXCursor_EnumDecl;
		}

		/**
		 * A field (in C) or non-static data member (in C++) in a
		 * struct, union, or C++ class.
		 */
		[[nodiscard]] bool isFieldDeclaration() const
		{
			return _kind == CXCursor_FieldDecl;
		}

		[[nodiscard]] bool isEnumConstantDeclaration() const
		{
			return _kind == CXCursor_EnumConstantDecl;
		}

		[[nodiscard]] bool isFunctionDeclaration() const
		{
			return _kind == CXCursor_FunctionDecl;
		}

		[[nodiscard]] bool isNamespace() const
		{
			return _kind == CXCursor_Namespace;
		}

		template<CXCursorKind KIND>
		[[nodiscard]] bool isKindOf() const
		{
			return _kind == KIND;
		}

		[[nodiscard]] bool isKindOf(CXCursorKind kind) const
		{
			return _kind == kind;
		}

		// wrapper of clang_getCursorKindSpelling, which convert enum CXCursorKind
		// to CXString
		[[nodiscard]] String cursorKindSpelling() const
		{
			return String{ clang_getCursorKindSpelling(_kind) };
		}

		[[nodiscard]] CXCursorKind kindEnum() const
		{
			return _kind;
		}

	private:
		
		explicit CursorKind(CXCursorKind kind)
			: _kind(kind) {}


		CXCursorKind _kind;
	};

	static bool operator==(Type l_type, Type r_type)
	{
		return l_type.operator==(r_type);
	}

	class Cursor
	{
	public:
		explicit Cursor(const TranslationUnit& unit)
			: _cursor(clang_getTranslationUnitCursor(unit._unit)) {}

		explicit Cursor(CXCursor cursor)
			: _cursor(cursor) {}

		Cursor(const Cursor& other) : _cursor(other._cursor){}

		Cursor()
		{
			_cursor.data[0] = nullptr;
			_cursor.data[1] = nullptr;
			_cursor.data[2] = nullptr;
		}

		[[nodiscard]] bool isValid() const
		{
			return _cursor.data[0] != nullptr;
		}

		bool operator==(const Cursor& other) const
		{
			return clang_equalCursors(_cursor, other._cursor);
		}

		[[nodiscard]] size_t hash() const
		{
			return clang_hashCursor(_cursor);
		}

		[[nodiscard]] CursorKind cursorKind() const
		{
			return CursorKind(clang_getCursorKind(_cursor));
		}

		[[nodiscard]] bool isInvalidDeclaration() const
		{
			return clang_isInvalidDeclaration(_cursor);
		}

		[[nodiscard]] bool hasAttrs() const
		{
			return clang_Cursor_hasAttrs(_cursor);
		}

		[[nodiscard]] Cursor getCursorSemanticParent() const
		{
			return Cursor{ clang_getCursorSemanticParent(_cursor) };
		}

		[[nodiscard]] Cursor getCursorLexicalParent() const
		{
			return Cursor{ clang_getCursorLexicalParent(_cursor) };
		}

		[[nodiscard]] Cursor getArgument(unsigned i) const
		{
			return Cursor{ clang_Cursor_getArgument(_cursor, i) };
		}

		/**
		* Describe the visibility of the entity referred to by a cursor.
		*
		* This returns the default visibility if not explicitly specified by
		* a visibility attribute. The default visibility may be changed by
		* commandline arguments.
		*
		* \returns The visibility of the cursor.
		*/
		[[nodiscard]] CXVisibilityKind visibility() const
		{
			return clang_getCursorVisibility(_cursor);
		}

		/**
		 * Determine the availability of the entity that this cursor refers to,
		 * taking the current target platform into account.
		 *
		 * \param cursor The cursor to query.
		 *
		 * \returns The availability of the cursor.
		 */
		[[nodiscard]] CXAvailabilityKind availability() const
		{
			return clang_getCursorAvailability(_cursor);
		}

		[[nodiscard]] std::int_least64_t enumConstantDeclValue() const
		{
			return clang_getEnumConstantDeclValue(_cursor);
		}

		[[nodiscard]] std::uint_least64_t enumConstantDeclUnsignedValue() const
		{
			return clang_getEnumConstantDeclUnsignedValue(_cursor);
		}

		[[nodiscard]] String displayName() const
		{
			return String(clang_getCursorDisplayName(_cursor));
		}

		[[nodiscard]] String mangling() const
		{
			return String(clang_Cursor_getMangling(_cursor));
		}

		[[nodiscard]] String cursorSpelling() const
		{
			return String(clang_getCursorSpelling(_cursor));
		}

		[[nodiscard]] Type cursorResultType() const
		{
			return Type(clang_getCursorResultType(_cursor));
		}

		[[nodiscard]] Type cursorType() const
		{
			return Type(clang_getCursorType(_cursor));
		}

		// typedef a b;
		// clang_getTypedefDeclUnderlyingType returns a
		[[nodiscard]] Type typedefDeclUnderlyingType() const
		{
			return Type(clang_getTypedefDeclUnderlyingType(_cursor));
		}

		[[nodiscard]] Type enumDeclIntegerType() const
		{
			return Type(clang_getEnumDeclIntegerType(_cursor));
		}

		static Cursor fromTypeDeclaration(Type type)
		{
			return Cursor{ clang_getTypeDeclaration(type._type) };
		}

		// TODO clang_Cursor_getCXXManglings

		/**
		 * @brief same as clang_getCursorDefinition
		 *
		 * @return Cursor
		 */
		[[nodiscard]] Cursor getReferenced() const
		{
			return Cursor{clang_getCursorReferenced(_cursor)};
		}

		/**
		 * @brief wrap for clang_getCursorDefinition
		 *
		 * @return Cursor
		 */
		[[nodiscard]] Cursor getDefinition() const
		{
			return Cursor{clang_getCursorDefinition(_cursor)};
		}

		[[nodiscard]] bool isDefinition() const
		{
			return clang_isCursorDefinition(_cursor);
		}

		[[nodiscard]] Cursor getCanonicalCursor() const
		{
			return Cursor{clang_getCanonicalCursor(_cursor)};
		}

		/**
		* Given a cursor pointing to a C++ method call or an Objective-C
		* message, returns non-zero if the method/message is "dynamic", meaning:
		*
		* For a C++ method: the call is virtual.
		* For an Objective-C message: the receiver is an object instance, not 'super'
		* or a specific class.
		*
		* If the method/message is "static" or the cursor does not point to a
		* method/message, it will return zero.
		*/
		[[nodiscard]] int isDynamicCall() const
		{
			return clang_Cursor_isDynamicCall(_cursor);
		}

		/**
		* Returns non-zero if the given cursor is a variadic function or method.
		*/
		[[nodiscard]] bool isVariadic() const
		{
			return clang_Cursor_isVariadic(_cursor);
		}

		// The number of arguments can be determined for calls as well as for
		// declarations of functions or methods
		[[nodiscard]] int numArguments() const
		{
			return clang_Cursor_getNumArguments(_cursor);
		}

		/**
		 * @brief VisitChildrenCallback
		 */
		using VisitChildrenCallback
			= std::function<CXChildVisitResult(Cursor cursor, Cursor parent)>;

		void visitChildren(VisitChildrenCallback callback) const
		{
			auto raw_cb = 
			[](CXCursor cursor, CXCursor parent, CXClientData client_data) -> enum CXChildVisitResult
			{
				auto cb_ptr = static_cast<VisitChildrenCallback*>(client_data);
				return (*cb_ptr)(Cursor(cursor), Cursor(parent));
			};
			clang_visitChildren(_cursor, raw_cb, (CXClientData)&callback);
		}

		[[nodiscard]] int getCursorExceptionSpecificationType() const
		{
			return clang_getCursorExceptionSpecificationType(_cursor);
		}

		// clang_Cursor_isMacroFunctionLike
		// clang_Cursor_isMacroBuiltin
		// clang_Cursor_isFunctionInlined

		// clang_Cursor_isExternalSymbol
		// clang_Cursor_getCommentRange

		[[nodiscard]] String rawCommentText() const
		{
			return String(clang_Cursor_getRawCommentText(_cursor));
		}
		
		[[nodiscard]] String briefCommentText() const
		{
			return String(clang_Cursor_getBriefCommentText(_cursor));
		}

		// clang_CXXConstructor_isConvertingConstructor
		// clang_CXXConstructor_isCopyConstructor
		// clang_CXXConstructor_isDefaultConstructor
		// clang_CXXConstructor_isMoveConstructor

		[[nodiscard]] bool isEnumDeclScoped() const
		{
			return clang_EnumDecl_isScoped(_cursor);
		}

		[[nodiscard]] CursorKind templateCursorKind() const
		{
			return CursorKind(clang_getTemplateCursorKind(_cursor));
		}

		// clang_getSpecializedCursorTemplate
		// clang_getCursorReferenceNameRange

		// begine 4106
		
		[[nodiscard]] CX_CXXAccessSpecifier accessSpecifier() const
		{
			return clang_getCXXAccessSpecifier(_cursor);
		}

		[[nodiscard]] CX_StorageClass storageClass() const
		{
			return clang_Cursor_getStorageClass(_cursor);
		}

		// clang_getNumOverloadedDecls
		// clang_getOverloadedDecl
		// clang_getIBOutletCollectionType seem like not a cxx attribute
		[[nodiscard]] std::uint_least64_t offsetOfField() const
		{
			return clang_Cursor_getOffsetOfField(_cursor);
		}

		[[nodiscard]] bool isAnonymous() const
		{
			return clang_Cursor_isAnonymous(_cursor);
		}

		// clang_Cursor_isAnonymousRecordDecl
		// clang_Cursor_isInlineNamespace

		[[nodiscard]] bool isBitField() const
		{
			return clang_Cursor_isBitField(_cursor);
		}

		[[nodiscard]] bool isVirtualBase() const
		{
			return clang_isVirtualBase(_cursor);
		}

		// method
		[[nodiscard]] bool isMethodConst() const
		{
			return clang_CXXMethod_isConst(_cursor);
		}

		[[nodiscard]] bool isMethodDefaulted() const
		{
			return clang_CXXMethod_isDefaulted(_cursor);
		}

		[[nodiscard]] bool isMethodPureVirtual() const
		{
			return clang_CXXMethod_isPureVirtual(_cursor);
		}

		[[nodiscard]] bool isMethodStatic() const
		{
			return clang_CXXMethod_isStatic(_cursor);
		}

		[[nodiscard]] bool isMethodVirtual() const
		{
			return clang_CXXMethod_isVirtual(_cursor);
		}

		// field
		[[nodiscard]] bool isMethodMutable() const
		{
			return clang_CXXField_isMutable(_cursor);
		}

		// constructor
		[[nodiscard]] bool isConvertingConstructor() const
		{
			return clang_CXXConstructor_isConvertingConstructor(_cursor);
		}

		[[nodiscard]] bool isCopyConstructor() const
		{
			return clang_CXXConstructor_isCopyConstructor(_cursor);
		}

		[[nodiscard]] bool isDefaultConstructor() const
		{
			return clang_CXXConstructor_isDefaultConstructor(_cursor);
		}

		[[nodiscard]] bool isMoveConstructor() const
		{
			return clang_CXXConstructor_isMoveConstructor(_cursor);
		}

	private:

		CXCursor _cursor{};
	};
}