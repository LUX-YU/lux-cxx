#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	EnumerationDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::ENUMERATION>(const Cursor& cursor, EnumerationDeclaration* declaration)
	{
		declaration->is_scope = cursor.isEnumDeclScoped();
		declaration->underlying_type = parseUncertainTypeMeta(cursor.enumDeclIntegerType());

		std::vector<Enumerator*> context;

		cursor.visitChildren(
			[this, declaration, &context](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				auto cursor_kind = cursor.cursorKind();

				if (cursor_kind == CXCursorKind::CXCursor_EnumConstantDecl)
				{
					auto* element = new Enumerator();
					element->name = nameFromClangString(cursor.displayName());
					element->value = cursor.enumConstantDeclUnsignedValue();
					context.push_back(element);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
				{
					declaration->attribute = nameFromClangString(cursor.displayName());
				}
				// else if(cursor_kind == CXCursorKind::Att)
				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);

		declaration->enumerators_num = context.size();
		declaration->enumerators = declaration->enumerators_num > 0 ? 
			new Enumerator* [declaration->enumerators_num] : nullptr;
		for (size_t i = 0 ; i < declaration->enumerators_num; i++)
		{
			declaration->enumerators[i] = context[i];
		}

		return declaration;
	}
}