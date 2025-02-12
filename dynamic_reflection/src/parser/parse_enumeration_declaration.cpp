#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

#include <lux/cxx/lan_model/types/enumeration.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::ENUMERATION>(const Cursor& cursor, EnumerationDeclaration* declaration)
	{
		declaration->is_scope		 = cursor.isEnumDeclScoped();
		declaration->underlying_type = parseUncertainTypeMeta(cursor.enumDeclIntegerType());

		std::vector<Enumerator> context;

		cursor.visitChildren(
			[this, declaration, &context](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (auto cursor_kind = cursor.cursorKind(); cursor_kind == CXCursorKind::CXCursor_EnumConstantDecl)
				{
					Enumerator enumerator;
					enumerator.name  = cursor.displayName().to_std();
					enumerator.value = cursor.enumConstantDeclUnsignedValue();
					context.push_back(enumerator);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
				{
					declaration->attribute = cursor.displayName().to_std();
				}
				// else if(cursor_kind == CXCursorKind::Att)
				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);

		for (auto & enumerator : context)
		{
			declaration->enumerators.push_back(std::move(enumerator));
		}
	}
}