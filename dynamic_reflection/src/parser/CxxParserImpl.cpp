#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/parser/Attribute.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

namespace lux::cxx::dref
{
	size_t CxxParserImpl::declaration_id(const EDeclarationKind kind, std::string_view name)
	{
		return MetaUnit::calculateDeclarationId(kind, name);
	}

	size_t CxxParserImpl::type_meta_id(const std::string_view name)
	{
		return MetaUnit::calculateTypeMetaId(name);
	}

	size_t CxxParserImpl::declaration_id(const std::unique_ptr<Declaration>& decl)
	{
		return declaration_id(decl->kind, decl->name);
	}

	bool CxxParserImpl::hasDeclarationInContextById(const size_t id) const
	{
		return _meta_unit_data->decl_map.contains(id);
	}

	bool CxxParserImpl::hasDeclarationInContextByName(EDeclarationKind kind, const char* name) const
	{
		auto id = MetaUnit::calculateDeclarationId(kind, name);
		return hasDeclarationInContextById(id);
	}

	Declaration* CxxParserImpl::getDeclarationFromContextById(size_t id)
	{
		if (_meta_unit_data->decl_map.contains(id))
			return _meta_unit_data->decl_map[id];

		return nullptr;
	}

	Declaration* CxxParserImpl::getDeclarationFromContextByName(EDeclarationKind kind, const char* name)
	{
		auto id = MetaUnit::calculateDeclarationId(kind, name);
		return getDeclarationFromContextById(id);
	}

	bool CxxParserImpl::hasTypeMetaInContextById(size_t id) const
	{
		return _meta_unit_data->type_map.contains(id);
	}

	bool CxxParserImpl::hasTypeMetaInContextByName(const char* name) const
	{
		auto id = MetaUnit::calculateTypeMetaId(name);
		return hasTypeMetaInContextById(id);
	}

	TypeMeta* CxxParserImpl::getTypeMetaFromContextById(size_t id) const
	{
		return _meta_unit_data->type_map.contains(id) ?
			_meta_unit_data->type_map[id] : nullptr;
	}

	TypeMeta* CxxParserImpl::getTypeMetaFromContextByName(EDeclarationKind kind, std::string_view name)
	{
		const auto id = MetaUnit::calculateTypeMetaId(name);
		return getTypeMetaFromContextById(id);
	}

	TypeMeta* CxxParserImpl::registerTypeMeta(size_t id, const TypeMeta& type_meta)
	{
		_meta_unit_data->type_list.push_back(type_meta);
		_meta_unit_data->type_map[id] =
			&_meta_unit_data->type_list.back();
		return &_meta_unit_data->type_list.back();
	}

	lan_model::ClassDeclaration*
	CxxParserImpl::registerMarkedClassDeclaration(size_t id, const lan_model::ClassDeclaration& decl)
	{
		_meta_unit_data->marked_decl_lists.class_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->marked_decl_lists.class_decl_list.back();
		return &_meta_unit_data->marked_decl_lists.class_decl_list.back();
	}

	lan_model::FunctionDeclaration*
	CxxParserImpl::registerMarkedFunctionDeclaration(size_t id, const lan_model::FunctionDeclaration& decl)
	{
		_meta_unit_data->marked_decl_lists.function_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->marked_decl_lists.function_decl_list.back();
		return &_meta_unit_data->marked_decl_lists.function_decl_list.back();
	}

	lan_model::EnumerationDeclaration*
	CxxParserImpl::registerMarkedEnumerationDeclaration(size_t id, const lan_model::EnumerationDeclaration& decl)
	{
		_meta_unit_data->marked_decl_lists.enumeration_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->marked_decl_lists.enumeration_decl_list.back();
		return &_meta_unit_data->marked_decl_lists.enumeration_decl_list.back();
	}

	lan_model::ClassDeclaration*
	CxxParserImpl::registerUnmarkedClassDeclaration(size_t id, const lan_model::ClassDeclaration& decl)
	{
		_meta_unit_data->unmarked_decl_lists.class_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->unmarked_decl_lists.class_decl_list.back();
		return &_meta_unit_data->unmarked_decl_lists.class_decl_list.back();
	}

	lan_model::FunctionDeclaration*
	CxxParserImpl::registerUnmarkedFunctionDeclaration(size_t id, const lan_model::FunctionDeclaration& decl)
	{
		_meta_unit_data->unmarked_decl_lists.function_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->unmarked_decl_lists.function_decl_list.back();
		return &_meta_unit_data->unmarked_decl_lists.function_decl_list.back();
	}

	lan_model::EnumerationDeclaration*
	CxxParserImpl::registerUnmarkedEnumerationDeclaration(size_t id, const lan_model::EnumerationDeclaration& decl)
	{
		_meta_unit_data->unmarked_decl_lists.enumeration_decl_list.push_back(decl);
		_meta_unit_data->decl_map[id] =
			&_meta_unit_data->unmarked_decl_lists.enumeration_decl_list.back();
		return &_meta_unit_data->unmarked_decl_lists.enumeration_decl_list.back();
	}

	lan_model::Visibility CxxParserImpl::visibilityFromClangVisibility(const CX_CXXAccessSpecifier specifier)
	{
		using namespace ::lux::cxx::lan_model;
		switch (specifier)
		{
		case CX_CXXInvalidAccessSpecifier:
			return Visibility::INVALID;
		case CX_CXXPublic:
			return Visibility::PUBLIC;
		case CX_CXXProtected:
			return Visibility::PROTECTED;
		case CX_CXXPrivate:
			return Visibility::PRIVATE;
		}
		return Visibility::INVALID;;
	}

	std::string CxxParserImpl::annotationFromClangCursor(const Cursor& cursor)
	{
		if (!cursor.hasAttrs())
		{
			return "";
		}
		std::string ret;
		cursor.visitChildren(
			[&ret](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (auto cursor_kind = cursor.cursorKind(); cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
				{
					ret = cursor.displayName().to_std();
					return CXChildVisitResult::CXChildVisit_Break;
				}
				// else if(cursor_kind == CXCursorKind::Att)
				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);
		return ret;
	}

	std::string CxxParserImpl::fullQualifiedName(const Cursor& cursor)
	{
		if (cursor.cursorKind().kindEnum() == CXCursor_TranslationUnit)
		{
			return std::string{};
		}
		auto spelling = cursor.displayName().to_std();
		if (const std::string res = fullQualifiedName(cursor.getCursorSemanticParent()); !res.empty())
		{
			return res + "::" + spelling;
		}
		return spelling;
	}

	std::string CxxParserImpl::fullQualifiedParameterName(const Cursor& cursor, size_t index)
	{
		if (cursor.cursorKind().kindEnum() == CXCursor_TranslationUnit)
		{
			return std::string{};
		}
		auto spelling = "arg" + std::to_string(index);
		std::string res = fullQualifiedName(cursor.getCursorSemanticParent());
		if (!res.empty())
		{
			return res + "::" + spelling;
		}
		return spelling;
	}
}

namespace lux::cxx::dref
{
	CxxParserImpl::CxxParserImpl()
	{
		/**
		 * @brief information about clang_createIndex(int excludeDeclarationsFromPCH, int displayDiagnostics)
		 * @param excludeDeclarationsFromPCH
		 */
		_clang_index = clang_createIndex(0, 1);
	}

	CxxParserImpl::~CxxParserImpl()
	{
		clang_disposeIndex(_clang_index);
	}

	ParseResult CxxParserImpl::parse(std::string_view file, std::vector<std::string_view> commands, std::string_view name, std::string_view version)
	{
		auto translate_unit = translate(file, std::move(commands));
		auto error_list = translate_unit.diagnostics();
		for (auto& str : error_list)
		{
			std::cerr << str << std::endl;
		}

		const Cursor cursor(translate_unit);
		const auto marked_cursors = findMarkedCursors(cursor);

		// set global parse context
		_meta_unit_data = std::make_unique<MetaUnitData>();

		for (auto& marked_cursor : marked_cursors)
		{
			Declaration* decl = parseDeclaration(marked_cursor);
			if (decl == nullptr)
			{
				return std::make_pair(EParseResult::FAILED, MetaUnit{});
			}
		}

		auto meta_unit_impl = std::make_unique<MetaUnitImpl>(
			std::move(_meta_unit_data),
			std::string(name), 
			std::string(version)
		);

		MetaUnit return_meta_unit(std::move(meta_unit_impl));
		return std::make_pair(EParseResult::SUCCESS, std::move(return_meta_unit));
	}

	Declaration* CxxParserImpl::parseDeclaration(const Cursor& cursor)
	{
		int cursor_kind = cursor.cursorKind().kindEnum();
		switch(cursor_kind)
		{
			case CXCursorKind::CXCursor_StructDecl:
			case CXCursorKind::CXCursor_ClassDecl: // implement
			{
				return TParseDeclarationDecorator<EDeclarationKind::CLASS>(cursor, true);
			}
			case CXCursorKind::CXCursor_EnumDecl: // implement
			{
				return TParseDeclarationDecorator<EDeclarationKind::ENUMERATION>(cursor, true);
			}
			case CXCursorKind::CXCursor_FunctionDecl:
			{
				return TParseDeclarationDecorator<EDeclarationKind::FUNCTION>(cursor, true);
			}
            default:
                return nullptr;
		}
		return nullptr;
	}

	std::vector<Cursor> CxxParserImpl::findMarkedCursors(const Cursor& root_cursor) const
	{
		std::vector<Cursor> cursor_list;

		root_cursor.visitChildren(
			[this, &cursor_list](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				CursorKind cursor_kind = cursor.cursorKind();

				if (cursor_kind.isNamespace())
				{
					auto ns_name = cursor.cursorSpelling().to_std();
					if (ns_name == "std")
						return CXChildVisitResult::CXChildVisit_Continue;

					return CXChildVisitResult::CXChildVisit_Recurse;
				}

				if (!cursor.hasAttrs())
					return CXChildVisitResult::CXChildVisit_Continue;

				cursor.visitChildren(
					[this, &cursor_list](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
					{
						CursorKind cursor_kind = cursor.cursorKind();
						if (!cursor_kind.isAttribute())
							return CXChildVisitResult::CXChildVisit_Continue;

						ClangString attr_name = cursor.displayName();
						const char* attr_c_name = attr_name.c_str();
						if (std::strncmp(attr_c_name, LUX_REF_MARK_PREFIX, 10) != 0)
							return CXChildVisitResult::CXChildVisit_Continue;

						std::string mark_name(attr_c_name + 10);

						cursor_list.push_back(parent_cursor);

						return CXChildVisitResult::CXChildVisit_Break;
					}
				);

				return CXChildVisitResult::CXChildVisit_Continue;
			}
		);

		return cursor_list;
	}

	TranslationUnit CxxParserImpl::translate(
		std::string_view file_path,
		std::vector<std::string_view> commands) const
	{
		CXTranslationUnit translation_unit;
		commands.emplace_back("-D__LUX_PARSE_TIME__=1");

		int commands_size = static_cast<int>(commands.size());
		// +1 for add definition
		const char** _c_commands = new const char*[commands_size];

		for (size_t i = 0; i < commands_size; i++)
		{
			_c_commands[i] = commands[i].data();
		}

		CXErrorCode error_code = clang_parseTranslationUnit2(
			_clang_index,           // CXIndex
			file_path.data(),       // source_filename
			_c_commands,            // command line args
			commands_size,          // num_command_line_args
			nullptr,                // unsaved_files
			0,                      // num_unsaved_files
			CXTranslationUnit_None, // option
			&translation_unit       // CXTranslationUnit
		);

		delete[] _c_commands;

		return TranslationUnit(error_code == CXErrorCode::CXError_Success ? translation_unit : nullptr);
	}
} // namespace lux::engine::core
