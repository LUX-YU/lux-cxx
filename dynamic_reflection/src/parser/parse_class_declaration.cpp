#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::DESTRUCTOR>(const Cursor& cursor, DestructorDeclaration* decl)
	{
		decl->visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		decl->is_virtual = cursor.isMethodVirtual();
		callableDeclInit(decl, cursor);
	}

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::CONSTRUCTOR>(const Cursor& cursor, ConstructorDeclaration* decl)
	{
		decl->visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		decl->attribute = annotationFromClangCursor(cursor);
		callableDeclInit(decl, cursor);
	}

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::MEMBER_DATA>(const Cursor& cursor, FieldDeclaration* decl)
	{
		decl->offset	 = cursor.offsetOfField();
		decl->visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		decl->attribute  = annotationFromClangCursor(cursor);
	}

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::MEMBER_FUNCTION>(const Cursor& cursor, MemberFunctionDeclaration* declaration)
	{
		declaration->visibility = visibilityFromClangVisibility(cursor.accessSpecifier());
		declaration->is_virtual = cursor.isMethodVirtual();
		declaration->is_static  = cursor.isMethodStatic();
		declaration->attribute  = annotationFromClangCursor(cursor);
		callableDeclInit(declaration, cursor);
	}

	template<>
	void CxxParserImpl::TParseDeclaration<EDeclarationKind::CLASS>(const Cursor& cursor, ClassDeclaration* declaration)
	{
		cursor.visitChildren(
			[this, declaration](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				const auto cursor_kind = cursor.cursorKind();

				if (cursor_kind == CXCursorKind::CXCursor_FieldDecl)
				{
					declaration->field_decls.push_back({});
					TParseLocalDeclarationDecorator<EDeclarationKind::MEMBER_DATA>(
						cursor, declaration->field_decls.back()
					);
					declaration->field_decls.back().class_declaration = declaration;
				}
				else if (cursor_kind == CXCursorKind::CXCursor_CXXMethod)
				{
					MemberFunctionDeclaration decl;
					TParseLocalDeclarationDecorator<EDeclarationKind::MEMBER_FUNCTION>(
						cursor, decl
					);
					decl.class_declaration = declaration;
					if (decl.is_static)
					{
						declaration->static_method_decls.push_back(decl);
					}
					else
					{
						declaration->method_decls.push_back(decl);
					}
				}
				else if (cursor_kind == CXCursorKind::CXCursor_CXXBaseSpecifier)
				{
					auto* decl = TParseDeclarationDecorator<EDeclarationKind::CLASS>(cursor.getDefinition());
					declaration->base_class_decls.push_back(decl);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_Constructor) // implement
				{
					declaration->constructor_decls.push_back({});
					TParseLocalDeclarationDecorator<EDeclarationKind::CONSTRUCTOR>(
						cursor, declaration->constructor_decls.back()
					);
					declaration->constructor_decls.back().class_declaration = declaration;
				}
				else if (cursor_kind == CXCursorKind::CXCursor_Destructor) // implement
				{
					declaration->destructor_decl;
					TParseLocalDeclarationDecorator<EDeclarationKind::DESTRUCTOR>(
						cursor, declaration->destructor_decl
					);
					declaration->destructor_decl.class_declaration = declaration;
				}

				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);
	}
}