#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <cstring>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	FunctionDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::FUNCTION>(const Cursor& cursor, FunctionDeclaration* declaration)
	{
		callableDeclInit(declaration, cursor);
		cursor.visitChildren(
			[this, declaration](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				auto cursor_kind = cursor.cursorKind();

				if (cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
				{
					declaration->attribute = nameFromClangString(cursor.displayName());
				}
				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);

		return declaration;
	}
}
