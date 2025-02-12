#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/declaration.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::FUNCTION>(const Cursor& cursor, FunctionDeclaration* declaration)
	{
		callableDeclInit(declaration, cursor);

		auto function_handler =
		[this, declaration](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
		{
			auto cursor_kind = cursor.cursorKind();

			if (cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
			{
				declaration->attribute = cursor.displayName().to_std();
			}
			return CXChildVisitResult::CXChildVisit_Continue;
		};

		cursor.visitChildren(std::move(function_handler));
	}
}
