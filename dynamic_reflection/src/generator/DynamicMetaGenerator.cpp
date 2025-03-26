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

static inline const char* typeKind2StrExtra(runtime::ETypeKinds kind)
{
	switch (kind)
	{
	case runtime::ETypeKinds::Imcomplete:             return "ETypeKinds::Imcomplete";
	case runtime::ETypeKinds::Void:                   return "ETypeKinds::Void";
	case runtime::ETypeKinds::Nullptr_t:              return "ETypeKinds::Nullptr_t";
	case runtime::ETypeKinds::Bool:                   return "ETypeKinds::Bool";
	case runtime::ETypeKinds::Char:                   return "ETypeKinds::Char";
	case runtime::ETypeKinds::SignedChar:             return "ETypeKinds::SignedChar";
	case runtime::ETypeKinds::UnsignedChar:           return "ETypeKinds::UnsignedChar";
	case runtime::ETypeKinds::Char8:                  return "ETypeKinds::Char8";
	case runtime::ETypeKinds::Char16:                 return "ETypeKinds::Char16";
	case runtime::ETypeKinds::Char32:                 return "ETypeKinds::Char32";
	case runtime::ETypeKinds::WChar:                  return "ETypeKinds::WChar";
	case runtime::ETypeKinds::Short:                  return "ETypeKinds::Short";
	case runtime::ETypeKinds::Int:                    return "ETypeKinds::Int";
	case runtime::ETypeKinds::Long:                   return "ETypeKinds::Long";
	case runtime::ETypeKinds::LongLong:               return "ETypeKinds::LongLong";
	case runtime::ETypeKinds::UnsignedShort:          return "ETypeKinds::UnsignedShort";
	case runtime::ETypeKinds::UnsignedInt:            return "ETypeKinds::UnsignedInt";
	case runtime::ETypeKinds::UnsignedLong:           return "ETypeKinds::UnsignedLong";
	case runtime::ETypeKinds::UnsignedLongLong:       return "ETypeKinds::UnsignedLongLong";
	case runtime::ETypeKinds::Float:                  return "ETypeKinds::Float";
	case runtime::ETypeKinds::Double:                 return "ETypeKinds::Double";
	case runtime::ETypeKinds::LongDouble:             return "ETypeKinds::LongDouble";
	case runtime::ETypeKinds::LvalueReference:        return "ETypeKinds::LvalueReference";
	case runtime::ETypeKinds::RvalueReference:        return "ETypeKinds::RvalueReference";
	case runtime::ETypeKinds::PointerToObject:        return "ETypeKinds::PointerToObject";
	case runtime::ETypeKinds::PointerToFunction:      return "ETypeKinds::PointerToFunction";
	case runtime::ETypeKinds::PointerToDataMember:    return "ETypeKinds::PointerToDataMember";
	case runtime::ETypeKinds::PointerToMemberFunction:return "ETypeKinds::PointerToMemberFunction";
	case runtime::ETypeKinds::Array:                  return "ETypeKinds::Array";
	case runtime::ETypeKinds::Function:               return "ETypeKinds::Function";
	case runtime::ETypeKinds::UnscopedEnum:           return "ETypeKinds::UnscopedEnum";
	case runtime::ETypeKinds::ScopedEnum:             return "ETypeKinds::ScopedEnum";
	case runtime::ETypeKinds::Class:                  return "ETypeKinds::Class";
	case runtime::ETypeKinds::Union:                  return "ETypeKinds::Union";
	default:										  throw std::runtime_error("Unknown ETypeKinds enum value");
	}
}

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
    case BuiltinType::EBuiltinKind::BFLOAT16:
        throw std::runtime_error("No corresponding ETypeKinds type for BFLOAT16");
    case BuiltinType::EBuiltinKind::IMB_128:
        throw std::runtime_error("No corresponding ETypeKinds type for IMB_128");
    default:
        throw std::runtime_error("Unknown EBuiltinKind enum value");
    }
}

runtime::ETypeKinds getETypeKinds(const Type* type)
{
	if (!type)
		throw std::runtime_error("Null type pointer");

	switch (type->kind)
	{
	case ETypeKind::BUILTIN: {
		const BuiltinType* bt = dynamic_cast<const BuiltinType*>(type);
		if (!bt)
			throw std::runtime_error("Type marked as BUILTIN is not a BuiltinType");
		return convertBuiltinKind(bt->builtin_type);
	}
	case ETypeKind::POINTER: {
		const PointerType* pt = dynamic_cast<const PointerType*>(type);
		if (!pt)
			throw std::runtime_error("Type marked as POINTER is not a PointerType");
		// 对于指针类型，如果是 pointer-to-member，则根据被指向类型区分数据成员和成员函数指针
		if (pt->is_pointer_to_member)
		{
			if (pt->pointee && pt->pointee->kind == ETypeKind::FUNCTION)
				return runtime::ETypeKinds::PointerToMemberFunction;
			else
				return runtime::ETypeKinds::PointerToDataMember;
		}
		else
		{
			if (pt->pointee && pt->pointee->kind == ETypeKind::FUNCTION)
				return runtime::ETypeKinds::PointerToFunction;
			else
				return runtime::ETypeKinds::PointerToObject;
		}
	}
	case ETypeKind::LVALUE_REFERENCE:
		return runtime::ETypeKinds::LvalueReference;
	case ETypeKind::RVALUE_REFERENCE:
		return runtime::ETypeKinds::RvalueReference;
	case ETypeKind::RECORD:
	case ETypeKind::CXX_RECORD:
		// 目前不区分 union 与 class，这里统一返回 Class
		return runtime::ETypeKinds::Class;
	case ETypeKind::ENUM:
		// 不区分 scoped 与 unscoped，此处默认返回 UnscopedEnum
		return runtime::ETypeKinds::UnscopedEnum;
	case ETypeKind::FUNCTION:
		return runtime::ETypeKinds::Function;
	case ETypeKind::ARRAY:
		return runtime::ETypeKinds::Array;
	case ETypeKind::UNSUPPORTED:
		throw std::runtime_error("Encountered unsupported type");
	default:
		throw std::runtime_error("Unknown type kind encountered");
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
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}

	json enum_json;
	json enum_json_basic_info;
	enum_json_basic_info["name"]		   = decl->name;
	enum_json_basic_info["qualified_name"] = decl->full_qualified_name;
	enum_json_basic_info["hash"]		   = std::to_string(fnv1a(decl->full_qualified_name));
	enum_json_basic_info["kind"]           =
		typeKind2StrExtra(
			decl->is_scoped ? runtime::ETypeKinds::ScopedEnum : runtime::ETypeKinds::UnscopedEnum
		);

	enum_json["basic_info"] = enum_json_basic_info;
	enum_json["is_scoped"] = decl->is_scoped;
	enum_json["underlying_type_hash"] = std::to_string(fnv1a(decl->underlying_type->name));

	for (auto& enumerator : decl->enumerators)
	{
		json enumerator_json;
		enumerator_json["name"] = enumerator.name;
		enumerator_json["signed_value"] = std::to_string(enumerator.signed_value);
		enumerator_json["unsigned_value"] = std::to_string(enumerator.unsigned_value);
		enum_json["enumerators"].push_back(enumerator_json);
	}

	enums_.push_back(enum_json);

	visit(decl->underlying_type);
}

void DynamicMetaGenerator::visit(RecordDecl* decl)
{
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}
}

void DynamicMetaGenerator::visit(CXXRecordDecl* decl)
{
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}

	json record_json;
	json record_json_basic_info;
	json record_json_object_info;
	record_json_basic_info["name"] = decl->name;
	record_json_basic_info["qualified_name"] = decl->full_qualified_name;
	record_json_basic_info["hash"] = std::to_string(fnv1a(decl->full_qualified_name));
	record_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::Class);

	record_json["basic_info"] = record_json_basic_info;

	record_json_object_info["size"] = decl->type->size;
	record_json_object_info["alignment"] = decl->type->align;

	record_json["object_info"] = record_json_object_info;

	record_json["base_hashs"] = nlohmann::json::array();
	for (auto& base : decl->bases)
	{
		record_json["base_hashs"].push_back(std::to_string(fnv1a(base->full_qualified_name)));
		visit(base);
	}

	record_json["ctor_hashs"] = nlohmann::json::array();
	for (auto& ctor : decl->constructor_decls)
	{
		record_json["ctor_hashs"].push_back(std::to_string(fnv1a(ctor->mangling)));
		visit(ctor);
	}

	record_json["field_meta_hashs"] = nlohmann::json::array();
	for (auto& field : decl->field_decls)
	{
		record_json["field_meta_hashs"].push_back(std::to_string(fnv1a(field->full_qualified_name)));
		visit(field);
	}

	record_json["method_meta_hashs"] = nlohmann::json::array();
	for (auto& method : decl->method_decls)
	{
		record_json["method_meta_hashs"].push_back(std::to_string(fnv1a(method->mangling)));
		visit(method);
	}

	record_json["static_method_metax_hashs"] = nlohmann::json::array();
	for (auto& static_method : decl->static_method_decls)
	{
		record_json["static_method_metax_hashs"].push_back(std::to_string(fnv1a(static_method->mangling)));
		visit(static_method);
	}

	records_.push_back(record_json);
}

void DynamicMetaGenerator::visit(FieldDecl* decl)
{
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}

	json field_json;
	json field_json_basic_info;
	json field_json_object_info;
	json field_json_cv_qualifier;

	field_json_basic_info["name"] = decl->name;
	field_json_basic_info["qualified_name"] = decl->full_qualified_name;
	field_json_basic_info["hash"] = std::to_string(fnv1a(decl->full_qualified_name));
	field_json_basic_info["kind"] = typeKind2StrExtra(getETypeKinds(decl->type));

	field_json_object_info["size"] = decl->type->size;
	field_json_object_info["alignment"] = decl->type->align;

	field_json_cv_qualifier["is_const"] = decl->type->is_const;
	field_json_cv_qualifier["is_volatile"] = decl->type->is_volatile;
	
	field_json["basic_info"] = field_json_basic_info;
	field_json["cv_qualifier"] = field_json_cv_qualifier;
	field_json["object_info"] = field_json_object_info;
	field_json["offset"] = decl->offset;
	field_json["visibility"] = visibility2Str(decl->visibility);
	field_.push_back(field_json);
}

void DynamicMetaGenerator::visit(FunctionDecl* decl)
{
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}

	if (decl->is_variadic)
	{
		std::cerr << "Variadic functions are not supported yet" << std::endl;
		return;
	}

	json function_json;
	json function_json_basic_info;
	json function_json_invokable_info;

	function_json_basic_info["name"] = decl->name;
	function_json_basic_info["qualified_name"] = decl->full_qualified_name;
	function_json_basic_info["hash"] = std::to_string(fnv1a(decl->full_qualified_name));
	function_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::Function);

	function_json_invokable_info["mangling"] = decl->mangling;
	function_json_invokable_info["return_type_hash"] = std::to_string(fnv1a(decl->result_type->id));
	function_json_invokable_info["return_type"] = decl->result_type->name;
	function_json_invokable_info["invokable_name"] = truncateAtLastParen(decl->full_qualified_name);
	function_json_invokable_info["parameters"] = nlohmann::json::array();
	function_json_invokable_info["param_type_hashs"] = nlohmann::json::array();
	function_json_invokable_info["is_variadic"] = decl->is_variadic;
	int i = 0;
	for (auto& param : decl->params)
	{
		function_json_invokable_info["param_type_hashs"].push_back(std::to_string(fnv1a(param->type->id)));
		json parameter;
		parameter["type_name"] = param->type->name;
		parameter["index"] = i++;
		function_json_invokable_info["parameters"].push_back(parameter);
		visit(param);
	}
	visit(decl->result_type);

	function_json["basic_info"] = function_json_basic_info;
	function_json["invokable_info"] = function_json_invokable_info;
	std::string extended_name = lux::cxx::algorithm::replace(decl->full_qualified_name, "::", "_");
	std::string bridge_name = extended_name + "_" + decl->spelling + "_invoker";
	function_json["extended_name"] = extended_name;
	function_json["bridge_name"] = bridge_name;

	function_.push_back(function_json);
}

void DynamicMetaGenerator::visit(CXXMethodDecl* decl)
{
	if (checkAndMarkVisited(decl->full_qualified_name)) {
		return;
	}

	json method_json;
	json method_json_basic_info;
	json method_json_invokable_info;
	json method_json_cv_qualifier;

	method_json_basic_info["name"] = decl->name;
	method_json_basic_info["qualified_name"] = decl->full_qualified_name;
	method_json_basic_info["hash"] = std::to_string(fnv1a(decl->mangling));
	method_json_basic_info["kind"] = runtime::ETypeKinds::Function;

	method_json_invokable_info["mangling"] = decl->mangling;
	method_json_invokable_info["return_type_hash"] = std::to_string(fnv1a(decl->result_type->id));
	method_json_invokable_info["return_type"] = decl->result_type->name;
	method_json_invokable_info["invokable_name"] = truncateAtLastParen(decl->full_qualified_name);
	method_json_invokable_info["parameters"] = nlohmann::json::array();
	method_json_invokable_info["param_type_hashs"] = nlohmann::json::array();
	method_json_invokable_info["is_variadic"] = decl->is_variadic;
	int i = 0;
	for (auto& param : decl->params)
	{
		method_json_invokable_info["param_type_hashs"].push_back(std::to_string(fnv1a(param->type->id)));
		json parameter;
		parameter["type_name"] = param->type->name;
		parameter["index"]     = i++;
		method_json_invokable_info["parameters"].push_back(parameter);
		visit(param);
	}
	visit(decl->result_type);
	
	method_json_cv_qualifier["is_const"] = decl->is_const;
	method_json_cv_qualifier["is_volatile"] = decl->is_volatile;

	method_json["basic_info"] = method_json_basic_info;
	method_json["invokable_info"] = method_json_invokable_info;
	method_json["cv_qualifier"] = method_json_cv_qualifier;

	std::string extended_name = lux::cxx::algorithm::replace(decl->full_qualified_name, "::", "_");
	std::string bridge_name   = extended_name + "_" + decl->spelling + "_invoker";
	method_json["extended_name"] = extended_name;
	method_json["bridge_name"] = bridge_name;
	method_json["visibility"] = visibility2Str(decl->visibility);
	method_json["is_virtual"] = decl->is_virtual;

	method_.push_back(method_json);
}

void DynamicMetaGenerator::visit(ParmVarDecl* decl)
{
	// No use
	if (type_map_.contains(decl->full_qualified_name))
	{
		return;
	}
}

// type visitor
void DynamicMetaGenerator::visit(BuiltinType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	json fundamental_json;
	json fundamental_json_basic_info;
	json fundamental_json_object_info;
	json fundamental_json_cv_qualifier;
	auto kind = convertBuiltinKind(type->builtin_type);
	fundamental_json_basic_info["name"] = type->name;
	fundamental_json_basic_info["qualified_name"] = type->name;
	fundamental_json_basic_info["hash"] = std::to_string(fnv1a(type->id));
	fundamental_json_basic_info["kind"] = typeKind2StrExtra(kind);

	fundamental_json["basic_info"] = fundamental_json_basic_info;

	fundamental_json_object_info["size"] = type->size;
	fundamental_json_object_info["alignment"] = type->align;

	fundamental_json["object_info"] = fundamental_json_object_info;

	fundamental_json_cv_qualifier["is_const"] = type->is_const;
	fundamental_json_cv_qualifier["is_volatile"] = type->is_volatile;

	fundamental_json["cv_qualifier"] = fundamental_json_cv_qualifier;

	fundamental_.push_back(fundamental_json);
}

void DynamicMetaGenerator::visit(PointerType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	json ptr_json;
	json ptr_json_basic_info;

	ptr_json_basic_info["name"] = type->name;
	ptr_json_basic_info["qualified_name"] = type->name;
	ptr_json_basic_info["hash"] = std::to_string(fnv1a(type->id));
	if (type->is_pointer_to_member)
	{
		if (type->pointee->kind == ETypeKind::FUNCTION)
		{
			ptr_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::PointerToMemberFunction);
		}
		else
		{
			ptr_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::PointerToDataMember);
		}
	}
	else
	{
		if (type->pointee->kind == ETypeKind::FUNCTION)
		{
			ptr_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::PointerToFunction);
		}
		else
		{
			ptr_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::PointerToObject);
		}
	}
	ptr_json["basic_info"] = ptr_json_basic_info;

	visit(type->pointee);
}

void DynamicMetaGenerator::visit(LValueReferenceType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	json ref_json;
	json ref_json_basic_info;

	ref_json_basic_info["name"] = type->name;
	ref_json_basic_info["qualified_name"] = type->name;
	ref_json_basic_info["hash"] = std::to_string(fnv1a(type->id));
	ref_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::LvalueReference);

	ref_json["basic_info"] = ref_json_basic_info;
	ref_json["pointee_hash"] = std::to_string(fnv1a(type->referred_type->id));

	ref_.push_back(ref_json);

	visit(type->referred_type);
}

void DynamicMetaGenerator::visit(RValueReferenceType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	json ref_json;
	json ref_json_basic_info;

	ref_json_basic_info["name"] = type->name;
	ref_json_basic_info["qualified_name"] = type->name;
	ref_json_basic_info["hash"] = std::to_string(fnv1a(type->id));
	ref_json_basic_info["kind"] = typeKind2StrExtra(runtime::ETypeKinds::RvalueReference);

	ref_json["basic_info"] = ref_json_basic_info;
	ref_json["pointee_hash"] = std::to_string(fnv1a(type->referred_type->id));

	ref_.push_back(ref_json);

	visit(type->referred_type);
}

void DynamicMetaGenerator::visit(RecordType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	auto decl = dynamic_cast<CXXRecordDecl*>(type->decl);
	if (decl) {
		visit(decl);
	}
}

void DynamicMetaGenerator::visit(EnumType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	auto decl = dynamic_cast<EnumDecl*>(type->decl);
	if (decl) {
		visit(decl);
	}
}

void DynamicMetaGenerator::visit(FunctionType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}

	visit(type->decl);
}

void DynamicMetaGenerator::visit(lux::cxx::dref::UnsupportedType* type)
{
	if (checkAndMarkVisited(type->id)) {
		return;
	}
}

void DynamicMetaGenerator::visit(lux::cxx::dref::Type* type)
{
    switch (type->kind)
    {
    case ETypeKind::BUILTIN:
		visit(static_cast<BuiltinType*>(type));
		break;
	case ETypeKind::POINTER:
		visit(static_cast<PointerType*>(type));
		break;
	case ETypeKind::LVALUE_REFERENCE:
		visit(static_cast<LValueReferenceType*>(type));
		break;
	case ETypeKind::RVALUE_REFERENCE:
		visit(static_cast<RValueReferenceType*>(type));
		break;
	case ETypeKind::RECORD:
		visit(static_cast<RecordType*>(type));
		break;
	case ETypeKind::ENUM:
		visit(static_cast<EnumType*>(type));
		break;
	case ETypeKind::FUNCTION:
		visit(static_cast<FunctionType*>(type));
		break;
	default:
		throw std::runtime_error("Unknown ETypeKind enum value");
    }
}

void DynamicMetaGenerator::toJsonFile(nlohmann::json& json)
{
	json["records"] = records_;
	json["enums"] = enums_;
	json["free_functions"] = function_;

	json["fundamentals"] = fundamental_;
	json["pointers"] = ptr_;
	json["pointer_to_data_members"] = ptr_to_memb_data_;
	json["pointer_to_methods"] = ptr_to_memb_func_;
	json["references"] = ref_;
	json["arrays"] = array_;
	
	json["methods"] = method_;
	json["fields"] = field_;
	json["ctor"] = ctor_;
	json["dtor"] = dtor_;
}