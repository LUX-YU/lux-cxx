#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/Attribute.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/algotithm/string_operations.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

namespace lux::cxx::dref
{
	[[nodiscard]] bool CxxParserImpl::hasDeclaration(const std::string& id) const
	{
		return _meta_unit_data->declaration_map.contains(id);
	}

	[[nodiscard]] Decl* CxxParserImpl::getDeclaration(const std::string& id) const
	{
		return _meta_unit_data->declaration_map[id];
	}

	[[nodiscard]] bool CxxParserImpl::hasType(const std::string& id) const
	{
		return _meta_unit_data->type_map.contains(id);
	}
	[[nodiscard]] Type* CxxParserImpl::getType(const std::string& id) const
	{
		return _meta_unit_data->type_map[id];
	}

	std::vector<std::string> CxxParserImpl::parseAnnotations(const Cursor& cursor)
	{
		if (!cursor.hasAttrs())
		{
			return std::vector<std::string>{};
		}
		std::vector<std::string> ret;
		cursor.visitChildren(
			[&ret](const Cursor& current_cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (const auto cursor_kind = current_cursor.cursorKind(); cursor_kind == CXCursor_AnnotateAttr)
				{
					std::string annotation = current_cursor.displayName().to_std();

					// 分割注解字符串
					std::stringstream ss(annotation);
					std::string part;
					while (std::getline(ss, part, ';'))
					{
						if (!part.empty())
						{
							std::string trimed_part(lux::cxx::algorithm::trim(part));
							ret.push_back(trimed_part);
						}
					}

					return CXChildVisit_Break;
				}
				return CXChildVisit_Continue;
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
		auto cursor_spelling = cursor.cursorSpelling();
		std::string spelling = cursor_spelling.size() > 0 ? cursor_spelling.to_std() : "arg" + std::to_string(index);
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

	ParseResult CxxParserImpl::parse(std::string_view file, std::vector<std::string> commands, std::string_view name, std::string_view version)
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
			if (!parseMarkedDeclaration(marked_cursor))
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

	bool CxxParserImpl::parseMarkedDeclaration(const Cursor& cursor)
	{
		std::unique_ptr<Decl> decl = nullptr;
		switch(cursor.cursorKind().kindEnum())
		{
			case CXCursor_StructDecl: [[fallthrough]];
			case CXCursor_ClassDecl: // implement
			{
				auto class_decl = std::make_unique<CXXRecordDecl>();
				parseCXXRecordDecl(cursor, *class_decl);
				decl = std::move(class_decl);
				break;
			}
			case CXCursor_EnumDecl: // implement
			{
				auto enum_decl = std::make_unique<EnumDecl>();
				parseEnumDecl(cursor, *enum_decl);
				decl = std::move(enum_decl);
				break;
			}
			case CXCursor_FunctionDecl:
			{
				auto func_decl = std::make_unique<FunctionDecl>();
				parseFunctionDecl(cursor, *func_decl);
				decl = std::move(func_decl);
				break;
			}
			default:
				return false;
		}

		if (cursor.isFromMainFile()) {
			registerDeclaration(std::move(decl), true);
		}

		return true;
	}

	std::vector<Cursor> CxxParserImpl::findMarkedCursors(const Cursor& root_cursor) const
	{
		std::vector<Cursor> cursor_list;

		root_cursor.visitChildren(
			[this, &cursor_list](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (const CursorKind cursor_kind = cursor.cursorKind(); cursor_kind.isNamespace())
				{
					if (const auto ns_name = cursor.cursorSpelling().to_std(); ns_name == "std")
						return CXChildVisit_Continue;

					return CXChildVisit_Recurse;
				}

				if (!cursor.hasAttrs())
					return CXChildVisit_Continue;

				cursor.visitChildren(
					[this, &cursor_list](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
					{
						if (CursorKind cursor_kind = cursor.cursorKind(); !cursor_kind.isAttribute())
							return CXChildVisit_Continue;

						const ClangString attr_name = cursor.displayName();
						const char* attr_c_name = attr_name.c_str();
						if (std::strncmp(attr_c_name, LUX_REF_MARK_PREFIX, 10) != 0)
							return CXChildVisit_Continue;

						std::string mark_name(attr_c_name + 10);

						cursor_list.push_back(parent_cursor);

						return CXChildVisit_Break;
					}
				);

				return CXChildVisit_Continue;
			}
		);

		return cursor_list;
	}

	TranslationUnit CxxParserImpl::translate(
		std::string_view file_path,
		std::vector<std::string> commands) const
	{
		CXTranslationUnit translation_unit;
		commands.emplace_back("-D__LUX_PARSE_TIME__=1");

		const int commands_size = static_cast<int>(commands.size());
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
