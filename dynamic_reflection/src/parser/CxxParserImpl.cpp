#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/algorithm/string_operations.hpp>

#include <numeric>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace lux::cxx::dref
{
	static std::string strip_param_list(std::string_view sig)
	{
		const auto paren = sig.find('(');
		if (paren == std::string_view::npos)
			return std::string{ sig };
		std::size_t last = paren;
		while (last > 0 && std::isspace(static_cast<unsigned char>(sig[last - 1])))
			--last;
		return std::string{ sig.substr(0, last) };
	}

	[[nodiscard]] bool CxxParserImpl::hasDeclaration(const std::string& id) const
	{
		return _meta_unit_data->declaration_map.contains(id);
	}

	[[nodiscard]] Decl* CxxParserImpl::getDeclaration(const std::string& id) const
	{
		auto it = _meta_unit_data->declaration_map.find(id);
		return it != _meta_unit_data->declaration_map.end() ? it->second : nullptr;
	}

	[[nodiscard]] bool CxxParserImpl::hasType(const std::string& id) const
	{
		return _meta_unit_data->type_map.contains(id);
	}
	[[nodiscard]] Type* CxxParserImpl::getType(const std::string& id) const
	{
		auto it = _meta_unit_data->type_map.find(id);
		return it != _meta_unit_data->type_map.end() ? it->second : nullptr;
	}

	Type* CxxParserImpl::registerType(std::unique_ptr<Type> type)
	{
		auto [it, inserted] = _meta_unit_data->type_map.try_emplace(type->id, type.get());
		if (!inserted)
		{
			return it->second;
		}
		// Assign the index immediately (before moving into the vector).
		type->index = _meta_unit_data->types.size();
		const auto raw_ptr = type.get();
		_meta_unit_data->types.emplace_back(std::move(type));
		return raw_ptr;
	}

	Decl* CxxParserImpl::registerDeclaration(std::unique_ptr<Decl> decl, const bool is_marked)
	{
		auto [it, inserted] = _meta_unit_data->declaration_map.try_emplace(decl->id, decl.get());
		if (!inserted)
		{
			return it->second;
		}
		// Assign the stable index before transferring ownership.
		decl->index = _meta_unit_data->declarations.size();
		const auto raw_ptr = decl.get();
		const auto own_index = raw_ptr->index;
		_meta_unit_data->declarations.push_back(std::move(decl));

		// Back-patch parent_class on every child of a CXXRecordDecl so that
		// child entries (fields, methods) that were registered earlier can
		// resolve their parent's index now that it is known.
		if (raw_ptr->kind == EDeclKind::CXX_RECORD_DECL)
		{
			auto* cxxr = static_cast<CXXRecordDecl*>(raw_ptr);
			auto back_patch = [&](size_t child_idx)
			{
				auto* child = _meta_unit_data->declarations[child_idx].get();
				if (auto* f = dynamic_cast<FieldDecl*>(child))
					f->parent_class = own_index;
				else if (auto* m = dynamic_cast<CXXMethodDecl*>(child))
					m->parent_class = own_index;
			};
			for (auto idx : cxxr->field_decls)        back_patch(idx);
			for (auto idx : cxxr->method_decls)       back_patch(idx);
			for (auto idx : cxxr->static_method_decls) back_patch(idx);
			for (auto idx : cxxr->constructor_decls)  back_patch(idx);
			if (cxxr->destructor_decl != INVALID_DECL_INDEX)
				back_patch(cxxr->destructor_decl);
		}

		if (is_marked)
		{
			if (raw_ptr->kind == EDeclKind::CXX_RECORD_DECL)
			{
				auto record_decl = static_cast<CXXRecordDecl*>(raw_ptr);
				_meta_unit_data->marked_record_decls.push_back(record_decl);
			}
			else if (raw_ptr->kind == EDeclKind::FUNCTION_DECL)
			{
				auto function_decl = static_cast<FunctionDecl*>(raw_ptr);
				_meta_unit_data->marked_function_decls.push_back(function_decl);
			}
			else if (raw_ptr->kind == EDeclKind::ENUM_DECL)
			{
				auto enum_decl = static_cast<EnumDecl*>(raw_ptr);
				_meta_unit_data->marked_enum_decls.push_back(enum_decl);
			}
		}
		return raw_ptr;
	}

	void CxxParserImpl::parseBasicDecl(const Cursor& cursor, Decl& decl)
	{
		decl.id	  = cursor.USR().to_std();
		decl.hash = algorithm::fnv1a(decl.id);
	}

	void CxxParserImpl::parseNamedDecl(const Cursor& cursor, NamedDecl& decl)
	{
		decl.name = cursor.displayName().to_std();
		decl.full_qualified_name = fullQualifiedName(cursor);
		decl.spelling = cursor.cursorSpelling().to_std();
		decl.attributes = parseAnnotations(cursor);
		decl.is_anonymous = cursor.isAnonymous();

		parseBasicDecl(cursor, decl);

		decl.type = createOrFindType(cursor.cursorType());
	}

	void CxxParserImpl::parseParamDecl(const Cursor& cursor, const int index, ParmVarDecl& decl)
	{
		decl.arg_index = index;
		decl.kind = EDeclKind::PARAM_VAR_DECL;
		parseNamedDecl(cursor, decl);
		decl.full_qualified_name = fullQualifiedParameterName(cursor, index);
		decl.id = decl.full_qualified_name; // fullQualifiedParameterName(cursor, index);
		decl.hash = algorithm::fnv1a(decl.id);
	}

	// Decl -> NamedDecl -> ValueDecl -> DeclaratorDecl
	void CxxParserImpl::parseFunctionDecl(const Cursor& cursor, FunctionDecl& decl)
	{
		decl.result_type = createOrFindType(cursor.cursorResultType());
		decl.mangling    = cursor.mangling().to_std();
		decl.is_variadic = cursor.isVariadic();

		for (size_t i = 0; i < cursor.numArguments(); i++)
		{
			auto param_decl = std::make_unique<ParmVarDecl>();
			parseParamDecl(cursor.getArgument(i), static_cast<int>(i), *param_decl);
			auto* param_raw = registerDeclaration(std::move(param_decl));
			decl.params.push_back(param_raw->index);
		}
		parseNamedDecl(cursor, decl); // type is set here

		if (decl.kind != EDeclKind::CXX_CONSTRUCTOR_DECL &&
			decl.kind != EDeclKind::CXX_CONVERSION_DECL &&
			decl.kind != EDeclKind::CXX_DESTRUCTOR_DECL &&
			decl.kind != EDeclKind::CXX_METHOD_DECL)
		{
			decl.kind = EDeclKind::FUNCTION_DECL;
			decl.invoke_name = strip_param_list(decl.full_qualified_name);
		}
		else
		{
			decl.invoke_name = decl.spelling;
		}

		// Phase 3: collect template parameters for function templates
		if (cursor.cursorKind().kindEnum() == CXCursor_FunctionTemplate)
		{
			decl.is_template = true;
			cursor.visitChildren(
				[&decl](const Cursor& child, const Cursor&) -> CXChildVisitResult
				{
					const auto ck = child.cursorKind().kindEnum();
					if (ck == CXCursor_TemplateTypeParameter)
					{
						TemplateParam tp;
						tp.kind     = TemplateParam::Kind::Type;
						tp.name     = child.cursorSpelling().to_std();
						tp.spelling = child.displayName().to_std();
						decl.template_params.push_back(std::move(tp));
					}
					else if (ck == CXCursor_NonTypeTemplateParameter)
					{
						TemplateParam tp;
						tp.kind     = TemplateParam::Kind::NonType;
						tp.name     = child.cursorSpelling().to_std();
						tp.spelling = child.displayName().to_std();
						decl.template_params.push_back(std::move(tp));
					}
					else if (ck == CXCursor_TemplateTemplateParameter)
					{
						TemplateParam tp;
						tp.kind     = TemplateParam::Kind::TemplateTemplate;
						tp.name     = child.cursorSpelling().to_std();
						tp.spelling = child.displayName().to_std();
						decl.template_params.push_back(std::move(tp));
					}
					// Params and body children are visited fine; we only collect
					// template parameter cursors at the top level of the template.
					return CXChildVisit_Continue;
				}
			);
		}
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
		// Iterative implementation: walk up the semantic parent chain collecting
		// display names, then concatenate in reverse order.  This avoids N
		// heap-allocations + string-copies that the recursive version produced
		// for a namespace-depth of N.
		std::vector<std::string> parts;
		parts.reserve(8);
		Cursor current = cursor;
		while (true)
		{
			const auto kind = current.cursorKind().kindEnum();
			if (kind == CXCursor_TranslationUnit)
				break;
			if (kind == CXCursor_LinkageSpec)
			{
				current = current.getCursorSemanticParent();
				continue;
			}
			parts.push_back(current.displayName().to_std());
			current = current.getCursorSemanticParent();
		}
		if (parts.empty())
			return {};

		std::reverse(parts.begin(), parts.end());

		// Pre-compute total length to avoid repeated reallocation.
		std::size_t total = 0;
		for (const auto& p : parts)
			total += p.size() + 2; // "::" separator (slightly over-allocated but harmless)

		std::string result;
		result.reserve(total);
		for (std::size_t i = 0; i < parts.size(); ++i)
		{
			if (i > 0)
				result += "::";
			result += parts[i];
		}
		return result;
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
	CxxParserImpl::CxxParserImpl(ParseOptions option)
		: _options(std::move(option))
	{
		/**
		 * @brief information about clang_createIndex(int excludeDeclarationsFromPCH, int displayDiagnostics)
		 * @param excludeDeclarationsFromPCH
		 */
		_clang_index = clang_createIndex(0, 1);

		// Automatically inject MSVC STL compatibility flags so that libclang
		// (which may report an older Clang version) can parse MSVC STL headers
		// without triggering the STL1000 static_assert version check.
#ifdef _MSC_VER
		auto& cmds = _options.commands;
		const std::string mismatch_flag = "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH";
		if (std::find(cmds.begin(), cmds.end(), mismatch_flag) == cmds.end())
		{
			cmds.push_back(mismatch_flag);
		}
#endif
	}

	CxxParserImpl::~CxxParserImpl()
	{
		clang_disposeIndex(_clang_index);
	}

	ParseResult CxxParserImpl::parse(std::string_view file)
	{
		auto translate_unit = translate(file, _options.commands);
		if (!translate_unit.isValid())
		{
			if (_callback) _callback("Failed to parse translation unit: " + std::string(file));
			return std::make_pair(EParseResult::FAILED, MetaUnit{});
		}

		auto error_list = translate_unit.diagnostics();
		for (auto& str : error_list)
		{
			if (_callback) _callback(str);
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

		// Indices are assigned eagerly in registerDeclaration/registerType, so
		// no separate index-setting loops are needed here.

		auto meta_unit_impl = std::make_unique<MetaUnitImpl>(
			std::move(_meta_unit_data),
			std::string(_options.name),
			std::string(_options.version)
		);

		MetaUnit return_meta_unit(std::move(meta_unit_impl));
		return std::make_pair(EParseResult::SUCCESS, std::move(return_meta_unit));
	}

	void CxxParserImpl::setOnParseError(std::function<void(const std::string&)> callback)
	{
		_callback = std::move(callback);
	}

	bool CxxParserImpl::parseMarkedDeclaration(const Cursor& cursor)
	{
		std::unique_ptr<Decl> decl = nullptr;
		if (!cursor.isFromMainFile()) {
			auto cursor_spell = cursor.cursorSpelling().to_std();
			if (_callback) {
				_callback("Cursor is not from main file: " + cursor_spell);
			}
			return true;
		}

		switch(cursor.cursorKind().kindEnum())
		{
			case CXCursor_StructDecl: [[fallthrough]];
			case CXCursor_UnionDecl: [[fallthrough]];
			case CXCursor_ClassTemplate: [[fallthrough]]; // Phase 3: class/struct templates
			case CXCursor_ClassDecl:
			{
				auto class_decl = std::make_unique<CXXRecordDecl>();
				parseCXXRecordDecl(cursor, *class_decl);
				decl = std::move(class_decl);
				break;
			}
			case CXCursor_EnumDecl:
			{
				auto enum_decl = std::make_unique<EnumDecl>();
				parseEnumDecl(cursor, *enum_decl);
				decl = std::move(enum_decl);
				break;
			}
			case CXCursor_FunctionTemplate: [[fallthrough]]; // Phase 3: function templates
			case CXCursor_FunctionDecl:
			{
				auto func_decl = std::make_unique<FunctionDecl>();
				parseFunctionDecl(cursor, *func_decl);
				decl = std::move(func_decl);
				break;
			}
			default:
			{
				const auto kind_int = static_cast<int>(cursor.cursorKind().kindEnum());
				const auto spelling = cursor.cursorSpelling().to_std();
				if (_callback)
					_callback("Unsupported declaration kind " + std::to_string(kind_int)
						+ " for cursor '" + spelling + "' — skipped");
				// Return true (not false) so one unrecognised declaration does
				// not abort the entire parse.
				return true;
			}
		}

		registerDeclaration(std::move(decl), true);

		return true;
	}

	std::vector<Cursor> CxxParserImpl::findMarkedCursors(const Cursor& root_cursor) const
	{
		std::vector<Cursor> cursor_list;

		root_cursor.visitChildren(
			[this, &cursor_list](const Cursor& cursor, const Cursor& parent_cursor) -> CXChildVisitResult
			{
				if (!cursor.isFromMainFile())
				{
					return CXChildVisit_Continue;
				}

				// CXCursor_LinkageSpec for some thing like extern "C"
				if (const CursorKind cursor_kind = cursor.cursorKind(); cursor_kind.isNamespace() || cursor_kind == CXCursor_LinkageSpec)
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
						if(std::string_view(attr_c_name).find(_options.marker_symbol) == std::string_view::npos)
						{
							return CXChildVisit_Continue;
						}

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
		const std::vector<std::string>& commands) const
	{
		CXTranslationUnit translation_unit;

		const int commands_size = static_cast<int>(commands.size());
		std::vector<const char*> c_commands(commands_size);
		for (size_t i = 0; i < commands_size; i++)
		{
			c_commands[i] = commands[i].data();
		}

		CXErrorCode error_code = clang_parseTranslationUnit2(
			_clang_index,           // CXIndex
			file_path.data(),       // source_filename
			c_commands.data(),      // command line args
			commands_size,          // num_command_line_args
			nullptr,                // unsaved_files
			0,                      // num_unsaved_files
			CXTranslationUnit_None, // option
			&translation_unit       // CXTranslationUnit
		);

		return TranslationUnit(error_code == CXErrorCode::CXError_Success ? translation_unit : nullptr);
	}

	void CxxParserImpl::parseBasicType(const ClangType& clang_type, Type& type)
	{
		const auto canonical_type = clang_type.canonicalType();
		const auto canonical_name = canonical_type.typeSpelling().to_std();
		type.id = canonical_name;
		type.is_const = clang_type.isConstQualifiedType();
		type.is_volatile = clang_type.isVolatileQualifiedType();
		type.name = clang_type.typeSpelling().to_std();
		type.size = clang_type.typeSizeof();
		type.align = clang_type.typeAlignOf();
	}

	void CxxParserImpl::parseBuiltinType(const ClangType& clang_type, BuiltinType::EBuiltinKind kind, BuiltinType& type)
	{
		type.builtin_type = kind;
		type.kind = ETypeKinds::Builtin;
		parseBasicType(clang_type, type);
	}

	void CxxParserImpl::parsePointerType(const ClangType& clang_type, PointerType& type)
	{
		type.pointee = createOrFindType(clang_type.pointeeType());
		if (type.is_pointer_to_member)
		{
			type.kind = type.pointee->kind == ETypeKinds::Function
				? ETypeKinds::PointerToMemberFunction : ETypeKinds::PointerToDataMember;
		}
		else
		{
			type.kind = type.pointee->kind == ETypeKinds::Function
				? ETypeKinds::PointerToFunction : ETypeKinds::PointerToObject;
		}
		parseBasicType(clang_type, type);
	}

	void CxxParserImpl::parseReferenceType(const ClangType& clang_type, ReferenceType& type)
	{
		type.referred_type = createOrFindType(clang_type.pointeeType());
		parseBasicType(clang_type, type);
	}

	void CxxParserImpl::parseRecordType(const ClangType& clang_type, RecordType& type)
	{
		type.kind = ETypeKinds::Record;
		parseBasicType(clang_type, type);

		// Extract template argument information
		const int num_template_args = clang_type.canonicalType().getNumTemplateArguments();
		if (num_template_args > 0)
		{
			// Extract template name by stripping "<...>" from the type spelling
			const auto& type_name = type.name;
			auto angle_pos = type_name.find('<');
			if (angle_pos != std::string::npos)
			{
				type.template_name = type_name.substr(0, angle_pos);
			}
			else
			{
				// Canonical name might have the angle brackets
				const auto& canonical_name = type.id;
				auto cangle_pos = canonical_name.find('<');
				type.template_name = (cangle_pos != std::string::npos)
					? canonical_name.substr(0, cangle_pos)
					: type_name;
			}

			const auto canonical_type = clang_type.canonicalType();
			for (int i = 0; i < num_template_args; i++)
			{
				TemplateArgument targ;
				auto arg_type = canonical_type.getTemplateArgumentAsType(i);
				if (arg_type.kind() != CXType_Invalid)
				{
					// Type template argument
					targ.kind = TemplateArgument::Kind::Type;
					targ.type = createOrFindType(arg_type);
					targ.spelling = arg_type.typeSpelling().to_std();
				}
				else
				{
					// Non-type template argument (e.g., integral value)
					// libclang C API doesn't expose non-type arg values directly,
					// so we extract from the type spelling
					targ.kind = TemplateArgument::Kind::Integral;
					targ.spelling = "";
					targ.integral_value = 0;

					// Try to extract the integral value from the canonical spelling
					// e.g., "std::array<int, 5>" -> find "5" after the last comma in template args
					const auto& canonical_spelling = type.id;
					// Parse the i-th non-type argument from the spelling
					size_t depth = 0;
					int current_arg = 0;
					size_t arg_start = 0;
					bool found_start = false;
					for (size_t ci = 0; ci < canonical_spelling.size(); ci++)
					{
						char ch = canonical_spelling[ci];
						if (ch == '<')
						{
							if (depth == 0 && !found_start)
							{
								arg_start = ci + 1;
								found_start = true;
							}
							depth++;
						}
						else if (ch == '>')
						{
							depth--;
							if (depth == 0 && current_arg == i)
							{
								auto arg_str = canonical_spelling.substr(arg_start, ci - arg_start);
								// Trim whitespace
								targ.spelling = std::string(lux::cxx::algorithm::trim(arg_str));
								break;
							}
						}
						else if (ch == ',' && depth == 1)
						{
							if (current_arg == i)
							{
								auto arg_str = canonical_spelling.substr(arg_start, ci - arg_start);
								targ.spelling = std::string(lux::cxx::algorithm::trim(arg_str));
								break;
							}
							current_arg++;
							arg_start = ci + 1;
						}
					}

					// Try to parse as integer
					if (!targ.spelling.empty())
					{
						const char* begin = targ.spelling.data();
						const char* end = begin + targ.spelling.size();
						auto [ptr, ec] = std::from_chars(begin, end, targ.integral_value);
						(void)ptr; (void)ec; // If parsing fails, integral_value stays 0
					}
				}
				type.template_arguments.push_back(std::move(targ));
			}
		}
	}

	void CxxParserImpl::parseEnumType(const ClangType& clang_type, EnumType& type)
	{
		type.kind = ETypeKinds::Enum;
		parseBasicType(clang_type, type);
	}

	void CxxParserImpl::parseFunctionType(const ClangType& clang_type, FunctionType& type)
	{
		type.kind = ETypeKinds::Function;
		type.result_type = createOrFindType(clang_type.resultType());
		type.is_variadic = clang_type.isFunctionTypeVariadic();
		const int num_args = clang_type.getNumArgTypes();
		for (int i = 0; i < num_args; i++)
		{
			auto arg_type = createOrFindType(clang_type.getArgType(i));
			type.param_types.push_back(arg_type);
		}
		parseBasicType(clang_type, type);
	}

	void CxxParserImpl::parseUnsupportedType(const ClangType& clang_type, UnsupportedType& type)
	{
		parseBasicType(clang_type, type);
	}

	static BuiltinType::EBuiltinKind clang_type2builtin_type(CXTypeKind kind)
	{
		switch (kind)
		{
		case CXType_Void: return BuiltinType::EBuiltinKind::VOID;
		case CXType_Bool: return BuiltinType::EBuiltinKind::BOOL;
		case CXType_Char_U: return BuiltinType::EBuiltinKind::CHAR_U;
		case CXType_UChar: return BuiltinType::EBuiltinKind::UCHAR;
		case CXType_Char16: return BuiltinType::EBuiltinKind::CHAR16_T;
		case CXType_Char32: return BuiltinType::EBuiltinKind::CHAR32_T;
		case CXType_UShort: return BuiltinType::EBuiltinKind::UNSIGNED_SHORT_INT;
		case CXType_UInt: return BuiltinType::EBuiltinKind::UNSIGNED_INT;
		case CXType_ULong: return BuiltinType::EBuiltinKind::UNSIGNED_LONG_INT;
		case CXType_ULongLong: return BuiltinType::EBuiltinKind::UNSIGNED_LONG_LONG_INT;
		case CXType_UInt128: return BuiltinType::EBuiltinKind::EXTENDED_UNSIGNED;
		case CXType_Char_S: return BuiltinType::EBuiltinKind::SIGNED_CHAR_S;
		case CXType_SChar: return BuiltinType::EBuiltinKind::SIGNED_SIGNED_CHAR;
		case CXType_WChar: return BuiltinType::EBuiltinKind::WCHAR_T;
		case CXType_Short: return BuiltinType::EBuiltinKind::SHORT_INT;
		case CXType_Int: return BuiltinType::EBuiltinKind::INT;
		case CXType_Long: return BuiltinType::EBuiltinKind::LONG_INT;
		case CXType_LongLong: return BuiltinType::EBuiltinKind::LONG_LONG_INT;
		case CXType_Int128: return BuiltinType::EBuiltinKind::EXTENDED_SIGNED;
		case CXType_Float: return BuiltinType::EBuiltinKind::FLOAT;
		case CXType_Double: return BuiltinType::EBuiltinKind::DOUBLE;
		case CXType_LongDouble: return BuiltinType::EBuiltinKind::LONG_DOUBLE;
		case CXType_NullPtr: return BuiltinType::EBuiltinKind::NULLPTR;
		case CXType_Overload: return BuiltinType::EBuiltinKind::OVERLOAD;
		case CXType_Dependent: return BuiltinType::EBuiltinKind::DEPENDENT;
		case CXType_ObjCId: return BuiltinType::EBuiltinKind::OBJC_IDENTIFIER;
		case CXType_ObjCClass: return BuiltinType::EBuiltinKind::OBJC_CLASS;
		case CXType_ObjCSel: return BuiltinType::EBuiltinKind::OBJC_SEL;
		case CXType_Float128: return BuiltinType::EBuiltinKind::FLOAT_128;
		case CXType_Half: return BuiltinType::EBuiltinKind::HALF;
		case CXType_Float16: return BuiltinType::EBuiltinKind::FLOAT16;
		case CXType_ShortAccum: return BuiltinType::EBuiltinKind::SHORT_ACCUM;
		case CXType_Accum: return BuiltinType::EBuiltinKind::ACCUM;
		case CXType_LongAccum: return BuiltinType::EBuiltinKind::LONG_ACCUM;
		case CXType_UShortAccum: return BuiltinType::EBuiltinKind::UNSIGNED_SHORT_ACCUM;
		case CXType_UAccum: return BuiltinType::EBuiltinKind::UNSIGNED_ACCUM;
		case CXType_ULongAccum: return BuiltinType::EBuiltinKind::UNSIGNED_LONG_ACCUM;
		case CXType_BFloat16: return BuiltinType::EBuiltinKind::BFLOAT16;
		case CXType_Ibm128: return BuiltinType::EBuiltinKind::IMB_128;
		default:
			break;
		}
		return static_cast<BuiltinType::EBuiltinKind>(0); // Invalid type
	}

	Type* CxxParserImpl::createOrFindType(const ClangType& clang_type)
	{
		const auto canonical_type = clang_type.canonicalType();
		const auto origin_type_name = clang_type.typeSpelling().to_std();
		if (const auto canonical_name = canonical_type.typeSpelling().to_std(); hasType(canonical_name))
		{
			// if the type is a typedef, we need to register it in the type alias map
			auto type_ptr = getType(canonical_name);
			if (!hasType(origin_type_name))
			{
				_meta_unit_data->type_alias_map[origin_type_name] = type_ptr;
			}

			return type_ptr;
		}

		switch (const auto type_kind = canonical_type.kind())
		{
		case CXType_Invalid:
			return nullptr;
		case CXType_Unexposed:
			return nullptr;
		case CXType_Void: [[fallthrough]];
		case CXType_Bool: [[fallthrough]];
		case CXType_Char_U: [[fallthrough]];
		case CXType_UChar: [[fallthrough]];
		case CXType_Char16: [[fallthrough]];
		case CXType_Char32: [[fallthrough]];
		case CXType_UShort: [[fallthrough]];
		case CXType_UInt: [[fallthrough]];
		case CXType_ULong: [[fallthrough]];
		case CXType_ULongLong: [[fallthrough]];
		case CXType_UInt128: [[fallthrough]];
		case CXType_Char_S: [[fallthrough]];
		case CXType_SChar: [[fallthrough]];
		case CXType_WChar: [[fallthrough]];
		case CXType_Short: [[fallthrough]];
		case CXType_Int: [[fallthrough]];
		case CXType_Long: [[fallthrough]];
		case CXType_LongLong: [[fallthrough]];
		case CXType_Int128: [[fallthrough]];
		case CXType_Float: [[fallthrough]];
		case CXType_Double: [[fallthrough]];
		case CXType_LongDouble: [[fallthrough]];
		case CXType_NullPtr: [[fallthrough]];
		case CXType_Overload: [[fallthrough]];
		case CXType_Dependent: [[fallthrough]];
		case CXType_Float128: [[fallthrough]];
		case CXType_Half: [[fallthrough]];
		case CXType_Float16: [[fallthrough]];
		case CXType_ShortAccum: [[fallthrough]];
		case CXType_Accum: [[fallthrough]];
		case CXType_LongAccum: [[fallthrough]];
		case CXType_UShortAccum: [[fallthrough]];
		case CXType_UAccum: [[fallthrough]];
		case CXType_ULongAccum: [[fallthrough]];
		case CXType_BFloat16: [[fallthrough]];
		case CXType_Ibm128:
		{
			auto builtin_type = std::make_unique<BuiltinType>();
			parseBuiltinType(clang_type, clang_type2builtin_type(type_kind), *builtin_type);
			return registerType(std::move(builtin_type));
		}
		// case CXType_Complex:
		case CXType_Pointer:
		{
			auto pointer_type = std::make_unique<PointerType>();
			parsePointerType(clang_type, *pointer_type);
			return registerType(std::move(pointer_type));
		}
		/* for obj - c
		case CXType_BlockPointer:
			break;
		*/
		case CXType_LValueReference:
		{
			auto l_reference_type = std::make_unique<LValueReferenceType>();
			parseReferenceType(clang_type, *l_reference_type);
			l_reference_type->kind = ETypeKinds::LvalueReference;
			return registerType(std::move(l_reference_type));
		}
		case CXType_RValueReference:
		{
			auto r_reference_type = std::make_unique<RValueReferenceType>();
			parseReferenceType(clang_type, *r_reference_type);
			r_reference_type->kind = ETypeKinds::RvalueReference;
			return registerType(std::move(r_reference_type));
		}
		case CXType_Record:
		{
			auto record_type = std::make_unique<RecordType>();
			parseRecordType(clang_type, *record_type);
			return registerType(std::move(record_type));
		}
		case CXType_Enum:
		{
			auto enum_type = std::make_unique<EnumType>();
			parseEnumType(clang_type, *enum_type);
			return registerType(std::move(enum_type));
		}
		case CXType_Typedef:
		{
			return createOrFindType(canonical_type);
		}
		/* for obj - c
		// case CXType_ObjCInterface:
		// case CXType_ObjCObjectPointer:
		// case CXType_FunctionNoProto:
		case CXType_FunctionProto:
		// case CXType_ConstantArray:
		// case CXType_Vector:
		// case CXType_VariableArray:
		// case CXType_DependentSizedArray:
		*/
		case CXType_FunctionProto:
		{
			auto function_type = std::make_unique<FunctionType>();
			parseFunctionType(clang_type, *function_type);
			return registerType(std::move(function_type));
		}
		case CXType_MemberPointer:
		{
			auto pointer_type = std::make_unique<PointerType>();
			pointer_type->is_pointer_to_member = true;
			parsePointerType(clang_type, *pointer_type);
			return registerType(std::move(pointer_type));
		}
		// case CXType_Auto:
		case CXType_Elaborated:
			return createOrFindType(canonical_type);
			// case CXType_Attributed:
		default:
		{
			auto unsupported_type = std::make_unique<UnsupportedType>();
			parseUnsupportedType(clang_type, *unsupported_type);
			return registerType(std::move(unsupported_type));
		}
		}
		return nullptr;
	}
} // namespace lux::engine::core
