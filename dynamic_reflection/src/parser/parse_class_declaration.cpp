#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	DestructorDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::DESTRUCTOR>(const Cursor& cursor, DestructorDeclaration* decl)
	{
		decl->visibility = visibiltyFromClangVisibility(cursor.accessSpecifier());
		decl->is_virtual = cursor.isMethodVirtual();
		callableDeclInit(decl, cursor);

		return decl;
	}

	template<>
	ConstructorDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::CONSTRUCTOR>(const Cursor& cursor, ConstructorDeclaration* decl)
	{
		decl->visibility = visibiltyFromClangVisibility(cursor.accessSpecifier());
		decl->attribute = annotationFromClangCursor(cursor);
		callableDeclInit(decl, cursor);

		return decl;
	}

	template<>
	FieldDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::MEMBER_DATA>(const Cursor& cursor, FieldDeclaration* decl)
	{
		decl->offset	 = cursor.offsetOfField();
		decl->visibility = visibiltyFromClangVisibility(cursor.accessSpecifier());
		decl->attribute  = annotationFromClangCursor(cursor);

		return decl;
	}

	template<>
	MemberFunctionDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::MEMBER_FUNCTION>(const Cursor& cursor, MemberFunctionDeclaration* declaration)
	{
		declaration->visibility = visibiltyFromClangVisibility(cursor.accessSpecifier());
		declaration->is_virtual = cursor.isMethodVirtual();
		declaration->is_static  = cursor.isMethodStatic();
		declaration->attribute  = annotationFromClangCursor(cursor);
		callableDeclInit(declaration, cursor);

		return declaration;
	}

	struct ParseContext
	{
		std::vector<FieldDeclaration*>			member_data_buffer;
		std::vector<MemberFunctionDeclaration*> member_function_buffer;
		std::vector<MemberFunctionDeclaration*>	static_member_function_buffer;
		std::vector<ConstructorDeclaration*>	constructor_buffer;
		std::vector<ClassDeclaration*>			base_class_buffer;
	};

	namespace {
		void fill_member_declaration_by_context(ClassDeclaration* class_decl, const ParseContext& context)
		{
			class_decl->base_class_num = context.base_class_buffer.size();
			class_decl->base_class_decls = class_decl->base_class_num > 0 ?
				new ClassDeclaration * [class_decl->base_class_num] : nullptr;
			for (size_t i = 0; i < class_decl->base_class_num; i++)
			{
				class_decl->base_class_decls[i] = context.base_class_buffer[i];
			}

			class_decl->constructor_num = context.constructor_buffer.size();
			class_decl->constructor_decls = class_decl->constructor_num > 0 ?
				new ConstructorDeclaration * [class_decl->constructor_num] : nullptr;
			for (size_t i = 0; i < class_decl->constructor_num; i++)
			{
				class_decl->constructor_decls[i] = context.constructor_buffer[i];
			}

			class_decl->field_num = context.member_data_buffer.size();
			class_decl->field_decls = class_decl->field_num > 0 ?
				new FieldDeclaration * [class_decl->field_num] : nullptr;
			for (size_t i = 0; i < class_decl->field_num; i++)
			{
				class_decl->field_decls[i] = context.member_data_buffer[i];
			}

			class_decl->member_function_num = context.member_function_buffer.size();
			class_decl->member_function_decls = class_decl->member_function_num > 0 ?
				new MemberFunctionDeclaration * [class_decl->member_function_num] : nullptr;
			for (size_t i = 0; i < class_decl->member_function_num; i++)
			{
				class_decl->member_function_decls[i] = context.member_function_buffer[i];
			}

			class_decl->static_member_function_num = context.static_member_function_buffer.size();
			class_decl->static_member_function_decls = class_decl->static_member_function_num > 0 ?
				new MemberFunctionDeclaration * [class_decl->static_member_function_num] : nullptr;
			for (size_t i = 0; i < class_decl->static_member_function_num; i++)
			{
				class_decl->static_member_function_decls[i] = context.static_member_function_buffer[i];
			}
		}
	}

	template<>
	ClassDeclaration* CxxParserImpl::TParseDeclaration<EDeclarationKind::CLASS>(const Cursor& cursor, ClassDeclaration* declaration)
	{
		ParseContext context;
		cursor.visitChildren(
			[this, declaration, &context](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				const auto cursor_kind = cursor.cursorKind();

				if (cursor_kind == CXCursorKind::CXCursor_FieldDecl)
				{
					auto decl = TParseDeclarationDecorator<EDeclarationKind::MEMBER_DATA>(cursor);
					decl->class_declaration = declaration;
					context.member_data_buffer.push_back(decl);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_CXXMethod)
				{
					const auto decl = TParseDeclarationDecorator<EDeclarationKind::MEMBER_FUNCTION>(cursor);
					decl->class_declaration = declaration;
					if (decl->is_static)
					{
						context.static_member_function_buffer.push_back(decl);
					}
					else
					{
						context.member_function_buffer.push_back(decl);
					}
				}
				else if (cursor_kind == CXCursorKind::CXCursor_CXXBaseSpecifier)
				{
					const auto decl = TParseDeclarationDecorator<EDeclarationKind::CLASS>(cursor.getDefinition());
					context.base_class_buffer.push_back(decl);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_Constructor) // implement
				{
					const auto decl = TParseDeclarationDecorator<EDeclarationKind::CONSTRUCTOR>(cursor);
					decl->class_declaration = declaration;
					context.constructor_buffer.push_back(decl);
				}
				else if (cursor_kind == CXCursorKind::CXCursor_Destructor) // implement
				{
					const auto decl = TParseDeclarationDecorator<EDeclarationKind::DESTRUCTOR>(cursor);
					decl->class_declaration = declaration;
					declaration->destructor_decl = decl;
				}

				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);

		fill_member_declaration_by_context(declaration, context);

		return declaration;
	}
}