#include <lux/cxx/dref/parser/CxxParserImpl.hpp>

#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/algorithm/string_operations.hpp>

#include <numeric>
#include <iostream>
#include <sstream>

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

	Type* CxxParserImpl::registerType(std::unique_ptr<Type> type)
	{
		if (_meta_unit_data->type_map.contains(type->id))
		{
			return _meta_unit_data->type_map[type->id];
		}

		const auto raw_ptr = type.get();
		_meta_unit_data->type_map[type->id] = raw_ptr;
		_meta_unit_data->types.emplace_back(std::move(type));

		return raw_ptr;
	}

	Decl* CxxParserImpl::registerDeclaration(std::unique_ptr<Decl> decl, const bool is_marked)
	{
		if (_meta_unit_data->declaration_map.contains(decl->id))
		{
			return _meta_unit_data->declaration_map[decl->id];
		}
		const auto raw_ptr = decl.get();
		_meta_unit_data->declaration_map[decl->id] = raw_ptr;
		_meta_unit_data->declarations.push_back(std::move(decl));

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
		decl.id = fullQualifiedParameterName(cursor, index);
		decl.hash = algorithm::fnv1a(decl.id);
	}

	// Decl -> NamedDecl -> ValueDecl -> DeclaratorDecl
	void CxxParserImpl::parseFunctionDecl(const Cursor& cursor, FunctionDecl& decl)
	{
		decl.result_type = createOrFindType(cursor.cursorResultType());
		decl.mangling = cursor.mangling().to_std();
		decl.is_variadic = cursor.isVariadic();


		for (size_t i = 0; i < cursor.numArguments(); i++)
		{
			auto param_decl = std::make_unique<ParmVarDecl>();
			parseParamDecl(cursor.getArgument(i), i, *param_decl);
			decl.params.push_back(param_decl.get());
			registerDeclaration(std::move(param_decl));
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
	CxxParserImpl::CxxParserImpl(ParseOptions option)
		: _options(std::move(option))
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

	ParseResult CxxParserImpl::parse(std::string_view file)
	{
		auto translate_unit = translate(file, _options.commands);
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

		for (size_t i = 0; i < _meta_unit_data->types.size(); i++)
		{
			auto& type  = _meta_unit_data->types[i];
			type->index = i;
		}

		for (size_t i = 0; i < _meta_unit_data->declarations.size(); i++)
		{
			auto& decl = _meta_unit_data->declarations[i];
			decl->index = i;
		}

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
						if(std::string_view(attr_c_name).find(_options.marker_symbol) == std::string_view::npos)
						{
							return CXChildVisit_Continue;
						}

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
		const std::vector<std::string>& commands) const
	{
		CXTranslationUnit translation_unit;

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

		if (type.is_const)
		{
			clang_type;
		}
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
				? ETypeKinds::PointerToFunction : ETypeKinds::PointerToObject;
		}
		else
		{
			type.kind = type.pointee->kind == ETypeKinds::Function
				? ETypeKinds::PointerToMemberFunction : ETypeKinds::PointerToDataMember;
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
			parseBuiltinType(clang_type, static_cast<BuiltinType::EBuiltinKind>(type_kind), *builtin_type);
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
			return createOrFindType(clang_type);
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
			return createOrFindType(clang_type);
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
