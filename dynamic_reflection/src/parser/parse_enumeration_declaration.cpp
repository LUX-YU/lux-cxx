#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

namespace lux::cxx::dref
{
	void CxxParserImpl::parseEnumDecl(const Cursor& cursor, EnumDecl& decl)
	{
		decl.is_scoped		 = cursor.isEnumDeclScoped();
		auto underlying_type = createOrFindType(cursor.enumDeclIntegerType());
		assert(underlying_type->kind == ETypeKind::BUILTIN);
		decl.underlying_type = static_cast<BuiltinType*>(underlying_type);

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
		assert(enum_type != nullptr);
		enum_type->decl = &decl;
	}
}