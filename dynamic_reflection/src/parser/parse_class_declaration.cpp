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

#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>

namespace lux::cxx::dref
{
	static EVisibility visibilityFromClangVisibility(const CX_CXXAccessSpecifier specifier)
	{
		switch (specifier)
		{
		case CX_CXXInvalidAccessSpecifier:
			return EVisibility::INVALID;
		case CX_CXXPublic:
			return EVisibility::PUBLIC;
		case CX_CXXProtected:
			return EVisibility::PROTECTED;
		case CX_CXXPrivate:
			return EVisibility::PRIVATE;
		}
		return EVisibility::INVALID;;
	}

	void CxxParserImpl::parseCxxMethodDecl(const Cursor& cursor, CXXMethodDecl& decl)
	{
		decl.visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		decl.is_virtual = cursor.isMethodVirtual();
		decl.is_static  = cursor.isMethodStatic();
		decl.is_const   = cursor.isMethodConst();
		parseFunctionDecl(cursor, decl);
	}

	void CxxParserImpl::parseFieldDecl(const Cursor& cursor, FieldDecl& decl)
	{
		decl.visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		decl.offset		= cursor.offsetOfField();
		parseNamedDecl(cursor, decl);
	}

	void CxxParserImpl::parseCXXRecordDecl(const Cursor& cursor, CXXRecordDecl& decl)
	{
		cursor.visitChildren(
			[this, &decl](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (const auto cursor_kind = cursor.cursorKind(); cursor_kind == CXCursor_FieldDecl)
				{
					auto field_decl = std::make_unique<FieldDecl>();
					parseFieldDecl(cursor, *field_decl);
					field_decl->kind = EDeclKind::FIELD_DECL;
					decl.field_decls.push_back(field_decl.get());
					registerDeclaration(std::move(field_decl));
				}
				else if (cursor_kind == CXCursor_CXXMethod)
				{
					auto method_decl = std::make_unique<CXXMethodDecl>();
					method_decl->kind = EDeclKind::CXX_METHOD_DECL;
					parseCxxMethodDecl(cursor, *method_decl);
					if (method_decl->is_static)
					{
						decl.static_method_decls.push_back(method_decl.get());
					}
					else
					{
						decl.method_decls.push_back(method_decl.get());
					}
					registerDeclaration(std::move(method_decl));
				}
				else if (cursor_kind == CXCursor_Constructor)
				{
					auto method_decl = std::make_unique<CXXConstructorDecl>();
					method_decl->kind = EDeclKind::CXX_CONSTRUCTOR_DECL;
					parseCxxMethodDecl(cursor, *method_decl);
					decl.constructor_decls.push_back(method_decl.get());
					registerDeclaration(std::move(method_decl));
				}
				else if (cursor_kind == CXCursor_Destructor)
				{
					auto method_decl = std::make_unique<CXXDestructorDecl>();
					method_decl->kind = EDeclKind::CXX_DESTRUCTOR_DECL;
					parseCxxMethodDecl(cursor, *method_decl);
					decl.destructor_decl = method_decl.get();
					registerDeclaration(std::move(method_decl));
				}
				else if (cursor_kind == CXCursor_CXXBaseSpecifier)
				{
					const auto base_cursor = cursor.getDefinition();
					if (const auto cursor_id = base_cursor.USR().to_std(); hasDeclaration(cursor_id))
					{
						const auto base_decl = dynamic_cast<CXXRecordDecl*>(getDeclaration(cursor_id));
						assert(base_decl != nullptr);
						decl.bases.push_back(base_decl);
					}
					else
					{
						auto base_class_decl = std::make_unique<CXXRecordDecl>();
						parseCXXRecordDecl(base_cursor, *base_class_decl);
						decl.bases.push_back(base_class_decl.get());
						registerDeclaration(std::move(base_class_decl));
					}
				}

				return CXChildVisit_Continue;
			}
		);

		auto cursor_kind_enum = cursor.cursorKind().kindEnum();
		if (cursor_kind_enum == CXCursor_StructDecl)
		{
			decl.tag_kind = TagDecl::ETagKind::Struct;
		}
		else if (cursor_kind_enum == CXCursor_UnionDecl)
		{
			decl.tag_kind = TagDecl::ETagKind::Union;
		}
		else if (cursor_kind_enum == CXCursor_ClassDecl)
		{
			decl.tag_kind = TagDecl::ETagKind::Class;
		}

		parseNamedDecl(cursor, decl);
		decl.kind = EDeclKind::CXX_RECORD_DECL;
		auto record_type = dynamic_cast<RecordType*>(decl.type);
		assert(record_type != nullptr);
		record_type->decl = &decl;
	}
}