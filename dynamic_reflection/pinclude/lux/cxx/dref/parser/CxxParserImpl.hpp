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
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <memory>

namespace lux::cxx::dref
{
    class MetaUnit;

    class CxxParserImpl
    {
    public:
        CxxParserImpl(ParseOptions option);

        ~CxxParserImpl();

        ParseResult parse(std::string_view file);

        void setOnParseError(std::function<void(const std::string&)> callback);
    private:
        static size_t type_meta_id(std::string_view name);
        static std::string fullQualifiedName(const Cursor& cursor);
        static std::string fullQualifiedParameterName(const Cursor& cursor, size_t index);
        static std::vector<std::string> parseAnnotations(const Cursor&);

        TranslationUnit translate(std::string_view file, const std::vector<std::string>& commands) const;
        std::vector<Cursor> findMarkedCursors(const Cursor& root_cursor) const;
        bool parseMarkedDeclaration(const Cursor& cursor);
        bool hasDeclaration(const std::string& id) const;
        Decl* getDeclaration(const std::string& id) const;
        bool  hasType(const std::string& id) const;
        Type* getType(const std::string& id) const;
        Type* registerType(std::unique_ptr<Type> type);
        Decl* registerDeclaration(std::unique_ptr<Decl> decl, const bool is_marked = false);
        static void parseBasicDecl(const Cursor& cursor, Decl& decl);
        void parseNamedDecl(const Cursor& cursor, NamedDecl& decl);
        void parseParamDecl(const Cursor& cursor, const int index, ParmVarDecl& decl);
        void parseFunctionDecl(const Cursor& cursor, FunctionDecl& decl);
        void parseCxxMethodDecl(const Cursor& cursor,    CXXMethodDecl& decl);
        void parseFieldDecl(const Cursor& cursor, FieldDecl& decl) ;
        void parseCXXRecordDecl(const Cursor& cursor, CXXRecordDecl& decl);
    	void parseEnumDecl(const Cursor& cursor, EnumDecl& decl);
        static void parseBasicType(const ClangType& clang_type, Type& type);
        static void parseBuiltinType(const ClangType& clang_type, BuiltinType::EBuiltinKind kind, BuiltinType& type);
		void parsePointerType(const ClangType& clang_type, PointerType& type);
		void parseReferenceType(const ClangType& clang_type, ReferenceType& type);
		void parseRecordType(const ClangType& clang_type, RecordType& type);
		void parseEnumType(const ClangType& clang_type, EnumType& type);
		void parseFunctionType(const ClangType& clang_type, FunctionType& type);
		void parseUnsupportedType(const ClangType& clang_type, UnsupportedType& type);
        Type* createOrFindType(const ClangType& clang_type);

        // shared context for creating translation units
		ParseOptions                            _options;
        std::function<void(const std::string&)> _callback;
        std::unique_ptr<MetaUnitData>           _meta_unit_data;
        CXIndex                                 _clang_index;
    };
} // namespace lux::reflection
