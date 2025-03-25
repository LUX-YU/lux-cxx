/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <cassert>

#include "libclang.hpp"
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/MetaUnitImpl.hpp>
#include <lux/cxx/dref/parser/Declaration.hpp>
#include <lux/cxx/dref/parser/Type.hpp>
#include <memory>

namespace lux::cxx::dref
{
    class MetaUnit;

    class CxxParserImpl
    {
    public:
        CxxParserImpl();

        ~CxxParserImpl();

        ParseResult parse(std::string_view file, std::vector<std::string> commands, std::string_view name, std::string_view version);

        static size_t                       type_meta_id(std::string_view name);

        static std::string                  fullQualifiedName(const Cursor& cursor);
        static std::string                  fullQualifiedParameterName(const Cursor& cursor, size_t index);

        static std::vector<std::string>     parseAnnotations(const Cursor&);
    private:

        [[nodiscard]] TranslationUnit       translate(std::string_view file, std::vector<std::string> commands) const;

        [[nodiscard]] std::vector<Cursor>   findMarkedCursors(const Cursor& root_cursor) const;
        bool                                parseMarkedDeclaration(const Cursor& cursor);

        [[nodiscard]] bool  hasDeclaration(const std::string& id) const;
        [[nodiscard]] Decl* getDeclaration(const std::string& id) const;

        [[nodiscard]] bool  hasType(const std::string& id) const;
        [[nodiscard]] Type* getType(const std::string& id) const;

        Type* registerType(std::unique_ptr<Type> type)
        {
            if (_meta_unit_data->type_map.contains(type->id))
            {
                return _meta_unit_data->type_map[type->id];
            }

            const auto raw_ptr = type.get();
            _meta_unit_data->type_map[type->id] = raw_ptr;
            _meta_unit_data->types.emplace_back(std::move(type));

            return raw_ptr;
        }

        Decl* registerDeclaration(std::unique_ptr<Decl> decl, const bool is_marked = false)
        {
            if (_meta_unit_data->declaration_map.contains(decl->id))
            {
                return _meta_unit_data->declaration_map[decl->id];
            }
            const auto raw_ptr = decl.get();
            _meta_unit_data->declaration_map[decl->id] = raw_ptr;
            _meta_unit_data->declarations.push_back(std::move(decl));

            if (is_marked)
            {
                _meta_unit_data->marked_declarations.push_back(raw_ptr);
				if (auto* named_decl = dynamic_cast<NamedDecl*>(raw_ptr); named_decl != nullptr)
				{
					_meta_unit_data->marked_types.push_back(named_decl->type);
				}
            }
            return raw_ptr;
        }

        static void parseBasicDecl(const Cursor& cursor, Decl& decl)
        {
            decl.id = cursor.USR().to_std();
        }

        void parseNamedDecl(const Cursor& cursor, NamedDecl& decl)
        {
            decl.name                = cursor.displayName().to_std();
            decl.full_qualified_name = fullQualifiedName(cursor);
            decl.spelling            = cursor.cursorSpelling().to_std();
            decl.attributes          = parseAnnotations(cursor);

            parseBasicDecl(cursor, decl);

        	decl.type = createOrFindType(cursor.cursorType());
        }

        void parseParamDecl(const Cursor& cursor, const int index, ParmVarDecl& decl)
        {
            decl.index = index;
        	decl.kind  = EDeclKind::PARAM_VAR_DECL;
            parseNamedDecl(cursor, decl);
        	decl.full_qualified_name = fullQualifiedParameterName(cursor, index);
        	decl.id = fullQualifiedParameterName(cursor, index);
        }

        // Decl -> NamedDecl -> ValueDecl -> DeclaratorDecl
        void parseFunctionDecl(const Cursor& cursor, FunctionDecl& decl)
        {
            decl.result_type = createOrFindType(cursor.cursorResultType());
            decl.mangling    = cursor.mangling().to_std();
			decl.is_variadic = cursor.isVariadic();
        	if(decl.kind != EDeclKind::CXX_CONSTRUCTOR_DECL &&
        		decl.kind != EDeclKind::CXX_CONVERSION_DECL &&
        		decl.kind != EDeclKind::CXX_DESTRUCTOR_DECL)
        	{
        		decl.kind = EDeclKind::FUNCTION_DECL;
        	}

            for(size_t i = 0; i < cursor.numArguments(); i++)
            {
                auto param_decl = std::make_unique<ParmVarDecl>();
                parseParamDecl(cursor.getArgument(i), i, *param_decl);
            	decl.params.push_back(param_decl.get());
                registerDeclaration(std::move(param_decl));
            }
            parseNamedDecl(cursor, decl);
        }

        void parseCxxMethodDecl(const Cursor& cursor,    CXXMethodDecl& decl);
        void parseFieldDecl(const Cursor& cursor, FieldDecl& decl) ;
        void parseCXXRecordDecl(const Cursor& cursor,    CXXRecordDecl& decl);
    	void parseEnumDecl(const Cursor& cursor,		 EnumDecl& decl);

        static void parseBasicType(const ClangType& clang_type, Type& type)
        {
            const auto canonical_type = clang_type.canonicalType();
            const auto canonical_name = canonical_type.typeSpelling().to_std();
            type.id           = canonical_name;
            type.is_const     = clang_type.isConstQualifiedType();
            type.is_volatile  = clang_type.isVolatileQualifiedType();
            type.name         = clang_type.typeSpelling().to_std();
            type.size         = clang_type.typeSizeof();
            type.align        = clang_type.typeAlignOf();
        }

        static void parseBuiltinType(const ClangType& clang_type, BuiltinType::EBuiltinKind kind,  BuiltinType& type)
        {
            type.builtin_type = kind;
            parseBasicType(clang_type, type);
        }

        static ClangType rootAnonicalType(const ClangType& type)
        {
            if (type.kind() == CXType_Typedef)
            {
                return rootAnonicalType(type.canonicalType());
            }
            return type;
        }

        void parsePointerType(const ClangType& clang_type, PointerType& type)
        {
            type.pointee = createOrFindType(clang_type.pointeeType());
        	type.kind    = ETypeKind::POINTER;
            parseBasicType(clang_type, type);
        }

        void parseReferenceType(const ClangType& clang_type, ReferenceType& type)
        {
            type.referred_type = createOrFindType(clang_type.pointeeType());
            parseBasicType(clang_type, type);
        }

        void parseRecordType(const ClangType& clang_type, RecordType& type)
        {
        	type.kind = ETypeKind::RECORD;
        	parseBasicType(clang_type, type);
        }

    	void parseEnumType(const ClangType& clang_type, EnumType& type)
        {
        	type.kind = ETypeKind::ENUM;
        	parseBasicType(clang_type, type);
        }

    	void parseFunctionType(const ClangType& clang_type, FunctionType& type)
        {
	        type.kind		= ETypeKind::FUNCTION;
        	type.result_type = createOrFindType(clang_type.resultType());
        	type.is_variadic = clang_type.isFunctionTypeVariadic();
        	const int num_args    = clang_type.getNumArgTypes();
        	for(int i = 0; i < num_args; i++)
        	{
        		auto arg_type = createOrFindType(clang_type.getArgType(i));
        		type.param_types.push_back(arg_type);
        	}
        	parseBasicType(clang_type, type);
        }

        void parseUnsupportedType(const ClangType& clang_type, UnsupportedType& type)
        {
            parseBasicType(clang_type, type);
        }

    	Type* createOrFindType(const ClangType& clang_type)
		{
			const auto canonical_type = clang_type.canonicalType();
			if (const auto canonical_name = canonical_type.typeSpelling().to_std(); hasType(canonical_name))
			{
				return getType(canonical_name);
			}

			switch (const auto type_kind = clang_type.kind())
			{
				case CXType_Invalid:
					return nullptr;
				case CXType_Unexposed:
					return nullptr;
				case CXType_Void:	[[fallthrough]];
				case CXType_Bool:	[[fallthrough]];
				case CXType_Char_U:	[[fallthrough]];
				case CXType_UChar:	[[fallthrough]];
				case CXType_Char16:	[[fallthrough]];
				case CXType_Char32:	[[fallthrough]];
				case CXType_UShort:	[[fallthrough]];
				case CXType_UInt:	[[fallthrough]];
				case CXType_ULong:	[[fallthrough]];
				case CXType_ULongLong: [[fallthrough]];
				case CXType_UInt128:[[fallthrough]];
				case CXType_Char_S:	[[fallthrough]];
				case CXType_SChar:	[[fallthrough]];
				case CXType_WChar:	[[fallthrough]];
				case CXType_Short:	[[fallthrough]];
				case CXType_Int:	[[fallthrough]];
				case CXType_Long:	[[fallthrough]];
				case CXType_LongLong: [[fallthrough]];
				case CXType_Int128:	[[fallthrough]];
				case CXType_Float:	[[fallthrough]];
				case CXType_Double:	[[fallthrough]];
				case CXType_LongDouble: [[fallthrough]];
				case CXType_NullPtr:[[fallthrough]];
				case CXType_Overload:[[fallthrough]];
				case CXType_Dependent:[[fallthrough]];
				case CXType_Float128:[[fallthrough]];
				case CXType_Half:[[fallthrough]];
				case CXType_Float16:[[fallthrough]];
				case CXType_ShortAccum:[[fallthrough]];
				case CXType_Accum:[[fallthrough]];
				case CXType_LongAccum:[[fallthrough]];
				case CXType_UShortAccum:[[fallthrough]];
				case CXType_UAccum:[[fallthrough]];
				case CXType_ULongAccum:[[fallthrough]];
				case CXType_BFloat16:[[fallthrough]];
				case CXType_Ibm128:
				{
					auto builtin_type = std::make_unique<BuiltinType>();
					parseBuiltinType(clang_type, static_cast<BuiltinType::EBuiltinKind>(type_kind), *builtin_type);
					return registerType(std::move(builtin_type));
				}
				// case CXType_Complex:
				case CXType_Pointer:
				{
					auto pointer_type = std::make_unique<PointerType>();
					parsePointerType(clang_type, *pointer_type);
					return registerType(std::move(pointer_type));
				}
				/* for obj - c
				case CXType_BlockPointer:
					break;
				*/
				case CXType_LValueReference:
				{
					auto l_reference_type = std::make_unique<LValueReferenceType>();
					parseReferenceType(clang_type, *l_reference_type);
					l_reference_type->kind = ETypeKind::LVALUE_REFERENCE;
					return registerType(std::move(l_reference_type));
				}
				case CXType_RValueReference:
				{
					auto r_reference_type = std::make_unique<RValueReferenceType>();
					parseReferenceType(clang_type, *r_reference_type);
					r_reference_type->kind = ETypeKind::RVALUE_REFERENCE;
					return registerType(std::move(r_reference_type));
				}
				case CXType_Record:
				{
					auto record_type = std::make_unique<RecordType>();
					parseRecordType(clang_type, *record_type);
					return registerType(std::move(record_type));
				}
				case CXType_Enum:
				{
					auto enum_type = std::make_unique<EnumType>();
					parseEnumType(clang_type, *enum_type);
					return registerType(std::move(enum_type));
				}
				case CXType_Typedef:
				{
					return createOrFindType(clang_type.canonicalType());
				}
				/* for obj - c
				// case CXType_ObjCInterface:
				// case CXType_ObjCObjectPointer:
				// case CXType_FunctionNoProto:
				case CXType_FunctionProto:
				// case CXType_ConstantArray:
				// case CXType_Vector:
				// case CXType_VariableArray:
				// case CXType_DependentSizedArray:
				*/
				case CXType_FunctionProto:
				{
					auto function_type = std::make_unique<FunctionType>();
					parseFunctionType(clang_type, *function_type);
					return registerType(std::move(function_type));
				}
				case CXType_MemberPointer:
				{
					auto pointer_type = std::make_unique<PointerType>();
					pointer_type->is_pointer_to_member = true;
					parsePointerType(clang_type, *pointer_type);
					return registerType(std::move(pointer_type));
				}
				// case CXType_Auto:
				case CXType_Elaborated:
					return createOrFindType(clang_type.canonicalType());
				// case CXType_Attributed:
				default:
				{
					auto unsupported_type = std::make_unique<UnsupportedType>();
					parseUnsupportedType(clang_type, *unsupported_type);
					return registerType(std::move(unsupported_type));
				}
			}
			return nullptr;
		}

        // shared context for creating translation units
        std::string                     _pch_file;
        std::unique_ptr<MetaUnitData>   _meta_unit_data;
        CXIndex                         _clang_index;
    };
} // namespace lux::reflection
