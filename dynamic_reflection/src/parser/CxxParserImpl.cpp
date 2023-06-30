#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/parser/ParserResultImpl.hpp>
#include <lux/cxx/dref/parser/Attribute.hpp>

#include <lux/cxx/dref/runtime/hash.hpp>
#include <lux/cxx/dref/runtime/Class.hpp>
#include <lux/cxx/dref/runtime/Method.hpp>

#include <numeric>
#include <stack>
#include <iostream>

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

	TranslationUnit CxxParserImpl::translate(const std::string& file_path, std::vector<std::string> commands)
	{
		CXTranslationUnit translation_unit;
		commands.push_back("-D__LUX_PARSE_TIME__=1");

		const int commands_size = static_cast<int>(commands.size());
		// +1 for add definition
		const char** _c_commands = (const char**)malloc((commands_size) * sizeof(char*));
		if (!_c_commands)
		{
			return TranslationUnit(nullptr);
		}

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

		free(_c_commands);

		return TranslationUnit(error_code == CXErrorCode::CXError_Success ? translation_unit : nullptr);
	}

	std::vector<Cursor> CxxParserImpl::findMarkedCursors(Cursor& root_cursor)
	{
		std::vector<Cursor> cursor_list;

		root_cursor.visitChildren(
			[this, &cursor_list](Cursor cursor, Cursor parent_cursor) -> CXChildVisitResult
			{
				CursorKind cursor_kind = cursor.cursorKind();

				if (cursor_kind.isNamespace())
				{
					auto ns_name = cursor.cursorSpelling().c_str();
					if (std::strcmp(ns_name, "std"))
						return CXChildVisitResult::CXChildVisit_Continue;

					return CXChildVisitResult::CXChildVisit_Recurse;
				}

				if (!cursor.hasAttrs())
					return CXChildVisitResult::CXChildVisit_Continue;

				cursor.visitChildren(
					[this, &cursor_list](Cursor cursor, Cursor parent_cursor) -> CXChildVisitResult
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

	runtime::TypeMeta* CxxParserImpl::cursorTypeDispatch(Cursor& cursor, ParserResultImpl* rst_impl)
	{
		using namespace runtime;
		TypeMeta* ret = nullptr;

		Type cursor_type = cursor.cursorType();
		std::string type_spell_name = cursor_type.typeSpelling().c_str();

		auto kind_enum = cursor.cursorKind().kindEnumValue();

		switch (kind_enum)
		{
			case CXCursorKind::CXCursor_CXXBaseSpecifier:
			{
				if (rst_impl->hasBufferedMeta(type_spell_name))
				{
					ret = rst_impl->getBufferedMeta(type_spell_name);
					break;
				}
			}
			case CXCursorKind::CXCursor_StructDecl:
			case CXCursorKind::CXCursor_ClassDecl: // implement
			{
				auto meta = new ClassMeta;

				meta->type_id = runtime::fnv1a(cursor_type.typeSpelling().c_str());
				meta->type_name = cursor_type.typeSpelling().c_str();
				meta->type_underlying_name = cursor.typedefDeclUnderlyingType().typeSpelling().c_str();

				meta->meta_type = runtime::ClassMeta::self_meta_type;
				// data type meta
				meta->size = cursor_type.typeSizeof();
				meta->is_const = cursor_type.isConstQualifiedType();
				meta->is_volatile = cursor_type.isVolatileQualifiedType();

				// class meta
				meta->align = cursor_type.typeAlignof();

				cursor.visitChildren(
					[this, meta, ret, rst_impl](Cursor cursor, Cursor parent_cursor) -> CXChildVisitResult
					{
						auto type_meta = this->cursorTypeDispatch(cursor, rst_impl);

						if(type_meta == nullptr) 
							return CXChildVisitResult::CXChildVisit_Continue;

						if (type_meta->meta_type == FieldMeta::self_meta_type)
						{
							auto field_meta = static_cast<FieldMeta*>(type_meta);
							field_meta->owner_info = meta;
							meta->field_meta_list.push_back(static_cast<FieldMeta*>(type_meta));
						}
						else if (type_meta->meta_type == ClassMeta::self_meta_type)
						{
							meta->parents_list.push_back(static_cast<ClassMeta*>(type_meta));
						}
						else if (type_meta->meta_type == MethodMeta::self_meta_type)
						{
							auto method_meta = static_cast<MethodMeta*>(type_meta);
							method_meta->owner_info = meta;
							meta->method_meta_list.push_back(method_meta);
						}

						return CXChildVisitResult::CXChildVisit_Continue;
					}
				);

				ret = meta;
				break;
			}
			case CXCursorKind::CXCursor_CXXAccessSpecifier: // skip
			{
				// public protected private
				break;
			}
			case CXCursorKind::CXCursor_EnumDecl: // implement
			{
				// enum declaration
				break;
			}
			case CXCursorKind::CXCursor_FieldDecl: // implement
			{
				auto meta = new FieldMeta;

				meta->type_id = runtime::fnv1a(cursor_type.typeSpelling().c_str());
				meta->type_name = cursor_type.typeSpelling().c_str();
				meta->type_underlying_name = cursor.typedefDeclUnderlyingType().typeSpelling().c_str();
				meta->meta_type = FieldMeta::self_meta_type;

				// field data meta
				meta->field_name = cursor.cursorSpelling().c_str();
				meta->offset	 = cursor.offsetOfField();
				
				// data type
				meta->size		 = cursor_type.typeSizeof();
				meta->is_const	 = cursor_type.isConstQualifiedType();
				meta->is_volatile = cursor_type.isVolatileQualifiedType();
				// field declaration
				ret = meta;

				break;
			}
			case CXCursorKind::CXCursor_ParmDecl:
			{
				break;
			}
			case CXCursorKind::CXCursor_CXXMethod: // implement
			{
				auto meta = new MethodMeta;

				meta->type_id = runtime::fnv1a(cursor_type.typeSpelling().c_str());
				meta->type_name			= cursor_type.typeSpelling().c_str();
				meta->type_underlying_name = cursor.typedefDeclUnderlyingType().typeSpelling().c_str();
				meta->meta_type			= MethodMeta::self_meta_type;

				meta->is_const			= cursor.isMethodConst();
				meta->is_virtual		= cursor.isMethodVirtual();
				meta->is_pure_virtual	= cursor.isMethodPureVirtual();
				// meta->is_pure_virtual init in class
				meta->is_static			= cursor.isMethodStatic();

				meta->func_name			= cursor.cursorSpelling().c_str();

				auto rst_type = cursor_type.resultType();
				auto rst_spelling_name	= rst_type.typeSpelling().c_str();
				auto rst_type_cursor	= Cursor::fromTypeDeclaration(rst_type);
				meta->result_info		= cursorTypeDispatch(rst_type_cursor, rst_impl);

				break;
			}
			case CXCursorKind::CXCursor_FunctionDecl:
			{
				break;
			}
			case CXCursorKind::CXCursor_Constructor: // implement
			{
				break;
			}
			case CXCursorKind::CXCursor_Destructor: // implement
			{
				break;
			}
			case CXCursorKind::CXCursor_TypeAliasDecl:
			{
				// type alias, be like using or typedef
				break;
			}
		}

		if (ret)
		{
			std::cout << ret->type_name << std::endl;

			if (!rst_impl->hasBufferedMeta(type_spell_name))
			{
				rst_impl->bufferMeta(type_spell_name, ret);
			}
		}

		return ret;
	}

	std::unique_ptr<ParserResultImpl> CxxParserImpl::extractCursor(Cursor& root_cursor)
	{
		CursorKind cursor_kind = root_cursor.cursorKind();

		auto result_impl = std::make_unique<ParserResultImpl>();

		std::vector<Cursor> cursor_list = findMarkedCursors(root_cursor);

		for(auto& cursor : cursor_list)
		{
			runtime::TypeMeta* type_meta = cursorTypeDispatch(cursor, result_impl.get());
		}

		return result_impl;
	}
} // namespace lux::engine::core
