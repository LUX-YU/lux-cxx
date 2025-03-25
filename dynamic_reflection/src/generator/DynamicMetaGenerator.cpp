#include "lux/cxx/dref/generator/Generator.hpp"
#include "lux/cxx/dref/generator/tools.hpp"
#include "lux/cxx/dref/parser/Declaration.hpp"
#include "lux/cxx/dref/parser/Type.hpp"

#include "lux/cxx/algotithm/hash.hpp"
#include "lux/cxx/dref/runtime/RuntimeMeta.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

using namespace lux::cxx::algorithm;
using namespace lux::cxx::dref;
using namespace nlohmann;

#include <stdexcept>

static runtime::ETypeKinds convertBuiltinKind(BuiltinType::EBuiltinKind kind)
{
    switch (kind)
    {
    case BuiltinType::EBuiltinKind::VOID:
        return runtime::ETypeKinds::Void;
    case BuiltinType::EBuiltinKind::BOOL:
        return runtime::ETypeKinds::Bool;
    case BuiltinType::EBuiltinKind::CHAR_U:
        // Assuming CHAR_U represents the basic char type.
        return runtime::ETypeKinds::Char;
    case BuiltinType::EBuiltinKind::UCHAR:
        return runtime::ETypeKinds::UnsignedChar;
    case BuiltinType::EBuiltinKind::CHAR16_T:
        return runtime::ETypeKinds::Char16;
    case BuiltinType::EBuiltinKind::CHAR32_T:
        return runtime::ETypeKinds::Char32;
    case BuiltinType::EBuiltinKind::UNSIGNED_SHORT_INT:
        return runtime::ETypeKinds::UnsignedShort;
    case BuiltinType::EBuiltinKind::UNSIGNED_INT:
        return runtime::ETypeKinds::UnsignedInt;
    case BuiltinType::EBuiltinKind::UNSIGNED_LONG_INT:
        return runtime::ETypeKinds::UnsignedLong;
    case BuiltinType::EBuiltinKind::UNSIGNED_LONG_LONG_INT:
        return runtime::ETypeKinds::UnsignedLongLong;
    case BuiltinType::EBuiltinKind::EXTENDED_UNSIGNED:
        throw std::runtime_error("No corresponding ETypeKinds type for EXTENDED_UNSIGNED");
    case BuiltinType::EBuiltinKind::SIGNED_CHAR_S:
        return runtime::ETypeKinds::SignedChar;
    case BuiltinType::EBuiltinKind::SIGNED_SIGNED_CHAR:
        return runtime::ETypeKinds::SignedChar;
    case BuiltinType::EBuiltinKind::WCHAR_T:
        return runtime::ETypeKinds::WChar;
    case BuiltinType::EBuiltinKind::SHORT_INT:
        return runtime::ETypeKinds::Short;
    case BuiltinType::EBuiltinKind::INT:
        return runtime::ETypeKinds::Int;
    case BuiltinType::EBuiltinKind::LONG_INT:
        return runtime::ETypeKinds::Long;
    case BuiltinType::EBuiltinKind::LONG_LONG_INT:
        return runtime::ETypeKinds::LongLong;
    case BuiltinType::EBuiltinKind::EXTENDED_SIGNED:
        throw std::runtime_error("No corresponding ETypeKinds type for EXTENDED_SIGNED");
    case BuiltinType::EBuiltinKind::FLOAT:
        return runtime::ETypeKinds::Float;
    case BuiltinType::EBuiltinKind::DOUBLE:
        return runtime::ETypeKinds::Double;
    case BuiltinType::EBuiltinKind::LONG_DOUBLE:
        return runtime::ETypeKinds::LongDouble;
    case BuiltinType::EBuiltinKind::NULLPTR:
        return runtime::ETypeKinds::Nullptr_t;
    case BuiltinType::EBuiltinKind::OVERLOAD:
        throw std::runtime_error("No corresponding ETypeKinds type for OVERLOAD");
    case BuiltinType::EBuiltinKind::DEPENDENT:
        throw std::runtime_error("No corresponding ETypeKinds type for DEPENDENT");
    case BuiltinType::EBuiltinKind::OBJC_IDENTIFIER:
        throw std::runtime_error("No corresponding ETypeKinds type for OBJC_IDENTIFIER");
    case BuiltinType::EBuiltinKind::OBJC_CLASS:
        throw std::runtime_error("No corresponding ETypeKinds type for OBJC_CLASS");
    case BuiltinType::EBuiltinKind::OBJC_SEL:
        throw std::runtime_error("No corresponding ETypeKinds type for OBJC_SEL");
    case BuiltinType::EBuiltinKind::FLOAT_128:
        throw std::runtime_error("No corresponding ETypeKinds type for FLOAT_128");
    case BuiltinType::EBuiltinKind::HALF:
        throw std::runtime_error("No corresponding ETypeKinds type for HALF");
    case BuiltinType::EBuiltinKind::FLOAT16:
        throw std::runtime_error("No corresponding ETypeKinds type for FLOAT16");
    case BuiltinType::EBuiltinKind::SHORT_ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for SHORT_ACCUM");
    case BuiltinType::EBuiltinKind::ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for ACCUM");
    case BuiltinType::EBuiltinKind::LONG_ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for LONG_ACCUM");
    case BuiltinType::EBuiltinKind::UNSIGNED_SHORT_ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for UNSIGNED_SHORT_ACCUM");
    case BuiltinType::EBuiltinKind::UNSIGNED_ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for UNSIGNED_ACCUM");
    case BuiltinType::EBuiltinKind::UNSIGNED_LONG_ACCUM:
        throw std::runtime_error("No corresponding ETypeKinds type for UNSIGNED_LONG_ACCUM");
    case BuiltinType::EBuiltinKind::BFLOAT16:
        throw std::runtime_error("No corresponding ETypeKinds type for BFLOAT16");
    case BuiltinType::EBuiltinKind::IMB_128:
        throw std::runtime_error("No corresponding ETypeKinds type for IMB_128");
    default:
        throw std::runtime_error("Unknown EBuiltinKind enum value");
    }
}


DynamicMetaGenerator::DynamicMetaGenerator()
{
}

DynamicMetaGenerator::~DynamicMetaGenerator()
{
}

// declaration visitor
void DynamicMetaGenerator::visit(EnumDecl* decl)
{
	json enum_json;
	json enum_json_basic_info;
	enum_json_basic_info["name"]		   = decl->name;
	enum_json_basic_info["qualified_name"] = decl->full_qualified_name;
	enum_json_basic_info["hash"]		   = fnv1a(decl->full_qualified_name);
	enum_json_basic_info["kind"]           =
		runtime::typeKind2Str(
			decl->is_scoped ? runtime::ETypeKinds::ScopedEnum : runtime::ETypeKinds::UnscopedEnum
		);

	enum_json["basic_info"]                = enum_json_basic_info;
	enum_json["is_scoped"]                 = decl->is_scoped;
	enum_json["underlying_type_hash"]      = fnv1a(decl->underlying_type->name);

	for (auto& enumerator : decl->enumerators)
	{
		json enumerator_json;
		enumerator_json["name"]            = enumerator.name;
		enumerator_json["signed_value"]    = std::to_string(enumerator.signed_value);
		enumerator_json["unsigned_value"]  = std::to_string(enumerator.unsigned_value);
		enum_json["enumerators"].push_back(enumerator_json);
	}

	enums_.push_back(enum_json);

	visit(decl->underlying_type);
}

void DynamicMetaGenerator::visit(RecordDecl* decl)
{
	// no use
}

void DynamicMetaGenerator::visit(CXXRecordDecl* decl)
{
    json record_json;
    json record_json_basic_info;
    json record_json_object_info;
    record_json_basic_info["name"] = decl->name;
    record_json_basic_info["qualified_name"] = decl->full_qualified_name;
    record_json_basic_info["hash"] = fnv1a(decl->full_qualified_name);
    record_json_basic_info["kind"] = runtime::ETypeKinds::Class;

    record_json["basic_info"] = record_json_basic_info;

    record_json_object_info["size"] = decl->type->size;
    record_json_object_info["alignment"] = decl->type->align;

    record_json["object_info"] = record_json_object_info;

    for (auto& base : decl->bases)
    {
        record_json["base_hashs"].push_back(fnv1a(base->full_qualified_name));
		visit(base);
    }

	for (auto& ctor : decl->constructor_decls)
	{
		record_json["ctor_hashs"].push_back(fnv1a(ctor->mangling));
		visit(ctor);
	}

    for (auto& field : decl->field_decls)
    {
		record_json["field_meta_hashs"].push_back(fnv1a(field->name));
		visit(field);
    }

	for (auto& method : decl->method_decls)
	{
		record_json["method_meta_hashs"].push_back(fnv1a(method->mangling));
		visit(method);
	}

	for (auto& static_method : decl->static_method_decls)
	{
		record_json["static_method_metax_hashs"].push_back(fnv1a(static_method->mangling));
		visit(static_method);
    }

	records_.push_back(record_json);
}

void DynamicMetaGenerator::visit(FieldDecl* decl)
{
	json field_json;
	json field_json_basic_info;
	json field_json_object_info;
	json field_json_cv_qualifier;

	field_json_basic_info["name"] = decl->name;
	field_json_basic_info["qualified_name"] = decl->full_qualified_name;
	field_json_basic_info["hash"] = fnv1a(decl->full_qualified_name);
	field_json_basic_info["kind"] = decl->kind;

	field_json["basic_info"] = field_json_basic_info;

	field_json_object_info["size"] = decl->type->size;
	field_json_object_info["alignment"] = decl->type->align;

	field_json["object_info"] = field_json_object_info;
	field_json["offset"] = decl->offset;

	field_json_cv_qualifier["is_const"] = decl->type->is_const;
	field_json_cv_qualifier["is_volatile"] = decl->type->is_volatile;

	field_json["cv_qualifier"] = field_json_cv_qualifier;

	field_.push_back(field_json);
}

void DynamicMetaGenerator::visit(FunctionDecl* decl)
{
    if (decl->is_variadic)
    {
		std::cerr << "Variadic functions are not supported yet" << std::endl;
        return;
    }

	json function_json;
	json function_json_basic_info;
	json function_json_invokable_info;

	function_json_basic_info["name"]           = decl->name;
	function_json_basic_info["qualified_name"] = decl->full_qualified_name;
	function_json_basic_info["hash"]           = fnv1a(decl->full_qualified_name);
	function_json_basic_info["kind"]           = runtime::typeKind2Str(runtime::ETypeKinds::Function);

	function_json_invokable_info["mangling"]         = decl->mangling;
	function_json_invokable_info["return_type_hash"] = fnv1a(decl->result_type->name);

    visit(decl->result_type);

	for (auto& param : decl->params)
	{
		function_json_invokable_info["param_type_hashs"].push_back(fnv1a(param->type->name));
		visit(param);
	}

	function_json_invokable_info["is_variadic"] = decl->is_variadic;

    function_json["basic_info"] = function_json_basic_info;
	function_json["invokable_info"] = function_json_invokable_info;

	function_.push_back(function_json);
}

void DynamicMetaGenerator::visit(CXXMethodDecl* decl)
{

}

void DynamicMetaGenerator::visit(ParmVarDecl* decl)
{

}

// type visitor
void DynamicMetaGenerator::visit(BuiltinType* type)
{
	json fundamental_json;
	json fundamental_json_basic_info;
	json fundamental_json_object_info;
	json fundamental_json_cv_qualifier;
    auto kind = convertBuiltinKind(type->builtin_type);
	fundamental_json_basic_info["name"] = type->name;
	fundamental_json_basic_info["qualified_name"] = type->name;
	fundamental_json_basic_info["hash"] = fnv1a(type->name);
	fundamental_json_basic_info["kind"] = runtime::typeKind2Str(kind);

	fundamental_json["basic_info"] = fundamental_json_basic_info;

	fundamental_json_object_info["size"] = type->size;
	fundamental_json_object_info["alignment"] = type->align;

	fundamental_json["object_info"] = fundamental_json_object_info;

	fundamental_json_cv_qualifier["is_const"]    = type->is_const;
	fundamental_json_cv_qualifier["is_volatile"] = type->is_volatile;

	fundamental_json["cv_qualifier"] = fundamental_json_cv_qualifier;

	fundamental_.push_back(fundamental_json);
}

void DynamicMetaGenerator::visit(PointerType* type)
{

}

void DynamicMetaGenerator::visit(LValueReferenceType* type)
{

}

void DynamicMetaGenerator::visit(RValueReferenceType* type)
{

}

void DynamicMetaGenerator::visit(RecordType* type)
{

}

void DynamicMetaGenerator::visit(EnumType* type)
{

}

void DynamicMetaGenerator::visit(FunctionType* type)
{

}