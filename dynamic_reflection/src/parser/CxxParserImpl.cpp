#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/parser/Attribute.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>

// types
#include <lux/cxx/lan_model/types/arithmetic.hpp>
#include <lux/cxx/lan_model/types/array.hpp>

#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <lux/cxx/lan_model/types/function_type.hpp>
#include <lux/cxx/lan_model/types/member_pointer_type.hpp>
#include <lux/cxx/lan_model/types/pointer_type.hpp>
#include <lux/cxx/lan_model/types/reference_type.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

namespace lux::cxx::dref
{
	size_t CxxParserImpl::declaration_id(EDeclarationKind kind, const char* name)
	{
		return MetaUnit::calculateDeclarationId(kind, name);
	}

	size_t CxxParserImpl::type_meta_id(const char* name)
	{
		return MetaUnit::calculateTypeMetaId(name);
	}

	size_t CxxParserImpl::declaration_id(Declaration* decl)
	{
		return declaration_id(decl->kind, decl->name);
	}

	bool CxxParserImpl::hasDeclarationInContextById(size_t id)
	{
		return _meta_unit_data->_markable_decl_map.count(id) > 0
			|| _meta_unit_data->_unmarkable_decl_map.count(id) > 0;
	}

	bool CxxParserImpl::hasDeclarationInContextByName(EDeclarationKind kind, const char* name)
	{
		auto id = MetaUnit::calculateDeclarationId(kind, name);
		return hasDeclarationInContextById(id);
	}

	Declaration* CxxParserImpl::getDeclarationFromContextById(size_t id)
	{
		if (_meta_unit_data->_markable_decl_map.count(id) > 0)
			return _meta_unit_data->_markable_decl_map[id];

		return _meta_unit_data->_unmarkable_decl_map.count(id) > 0 ?
			_meta_unit_data->_unmarkable_decl_map[id] : nullptr;
	}

	Declaration* CxxParserImpl::getDeclarationFromContextByName(EDeclarationKind kind, const char* name)
	{
		auto id = MetaUnit::calculateDeclarationId(kind, name);
		return getDeclarationFromContextById(id);
	}

	bool CxxParserImpl::hasTypeMetaInContextById(size_t id)
	{
		return _meta_unit_data->_meta_type_map.count(id) > 0;
	}

	bool CxxParserImpl::hasTypeMetaInContextByName(const char* name)
	{
		auto id = MetaUnit::calculateTypeMetaId(name);
		return hasTypeMetaInContextById(id);
	}

	TypeMeta* CxxParserImpl::getTypeMetaFromContextById(size_t id)
	{
		return _meta_unit_data->_meta_type_map.count(id) > 0 ?
			_meta_unit_data->_meta_type_map[id] : nullptr;
	}

	TypeMeta* CxxParserImpl::getTypeMetaFromContextByName(EDeclarationKind kind, const char* name)
	{
		auto id = MetaUnit::calculateTypeMetaId(name);
		return getTypeMetaFromContextById(id);
	}

	void CxxParserImpl::registTypeMeta(size_t id, TypeMeta* type_meta)
	{
		_meta_unit_data->_meta_type_list.push_back(type_meta);
		_meta_unit_data->_meta_type_map[id] = type_meta;
	}

	void CxxParserImpl::registUnmarkableDeclaration(Declaration* decl)
	{
		_meta_unit_data->_unmarkable_decl_list.push_back(decl);
		_meta_unit_data->_unmarkable_decl_map[declaration_id(decl)] = decl;
	}
	void CxxParserImpl::registMarkableDeclaration(Declaration* decl)
	{
		_meta_unit_data->_markable_decl_list.push_back(decl);
		_meta_unit_data->_markable_decl_map[declaration_id(decl)] = decl;
	}

	void CxxParserImpl::registUnmarkableDeclaration(size_t id, Declaration* decl)
	{
		_meta_unit_data->_unmarkable_decl_list.push_back(decl);
		_meta_unit_data->_unmarkable_decl_map[id] = decl;
	}
	void CxxParserImpl::registMarkableDeclaration(size_t id, Declaration* decl)
	{
		_meta_unit_data->_markable_decl_list.push_back(decl);
		_meta_unit_data->_markable_decl_map[id] = decl;
	}

	char* CxxParserImpl::nameFromClangString(const String& string)
	{
		auto string_len = string.size();
		char* ret = new char[string_len + 1];
		std::strcpy(ret, string.c_str());
		
		return ret;
	}

	char* CxxParserImpl::nameFromStdString(const std::string& str)
	{
		auto string_len = str.size();
		char* ret = new char[string_len + 1];
		std::strcpy(ret, str.c_str());

		return ret;
	}

	::lux::cxx::lan_model::Visibility CxxParserImpl::visibiltyFromClangVisibility(CX_CXXAccessSpecifier specifier)
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

	char* CxxParserImpl::annotationFromClangCursor(const Cursor& cursor)
	{
		if (!cursor.hasAttrs())
		{
			return nullptr;
		}
		char* ret{nullptr};
		cursor.visitChildren(
			[&ret](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				auto cursor_kind = cursor.cursorKind();

				if (cursor_kind == CXCursorKind::CXCursor_AnnotateAttr)
				{
					ret = nameFromClangString(cursor.displayName());
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
		std::string res = fullQualifiedName(cursor.getCursorSemanticParent());
		if (!res.empty())
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

	ParseResult CxxParserImpl::parse(const std::string& file, std::vector<std::string> commands, std::string name, std::string version)
	{
		auto translate_unit = translate(file, std::move(commands));
		auto error_list = translate_unit.diagnostics();
		for (auto& str : error_list)
		{
			std::cout << str << std::endl;
		}

		Cursor cursor(translate_unit);

		auto marked_cursors = findMarkedCursors(cursor);

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
			std::move(name), std::move(version)
		);

		meta_unit_impl->setDeleteFlat(true);

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

	std::vector<Cursor> CxxParserImpl::findMarkedCursors(const Cursor& root_cursor)
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

						String attr_name = cursor.displayName();
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
		const std::string& file_path, 
		std::vector<std::string> commands)
	{
		CXTranslationUnit translation_unit;
		commands.emplace_back("-D__LUX_PARSE_TIME__=1");

		int commands_size = static_cast<int>(commands.size());
		// +1 for add definition
		const char** _c_commands = new const char*[commands_size];

        for (size_t i = 0; i < commands_size; i++)
		{
			_c_commands[i] = commands[i].c_str();
		}

		CXErrorCode error_code = clang_parseTranslationUnit2(
			_clang_index,           // CXIndex
			file_path.c_str(),      // source_filename
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
