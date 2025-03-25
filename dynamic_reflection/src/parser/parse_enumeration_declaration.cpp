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

namespace lux::cxx::dref
{
	void CxxParserImpl::parseEnumDecl(const Cursor& cursor, EnumDecl& decl)
	{
		decl.is_scoped = cursor.isEnumDeclScoped();
		cursor.visitChildren(
			[this, &decl](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (const auto cursor_kind = cursor.cursorKind(); cursor_kind == CXCursor_EnumConstantDecl)
				{
					Enumerator enumerator;
					enumerator.name			  = cursor.displayName().to_std();
					enumerator.signed_value   = cursor.enumConstantDeclValue();
					enumerator.unsigned_value = cursor.enumConstantDeclUnsignedValue();
					decl.enumerators.push_back(enumerator);
				}
				return CXChildVisit_Continue;
			}
		);

		decl.tag_kind = TagDecl::ETagKind::Enum;
		decl.kind	  = EDeclKind::ENUM_DECL;
		parseNamedDecl(cursor, decl);
		auto enum_type = dynamic_cast<EnumType*>(decl.type);

		auto underlying_type = createOrFindType(cursor.enumDeclIntegerType());
		assert(underlying_type->kind == ETypeKind::BUILTIN);
		enum_type->underlying_type = dynamic_cast<BuiltinType*>(underlying_type);
		assert(enum_type != nullptr);
		enum_type->decl = &decl;
	}
}