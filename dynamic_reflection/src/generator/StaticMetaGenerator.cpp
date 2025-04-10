#include "lux/cxx/dref/generator/StaticMetaGenerator.hpp"
#include "lux/cxx/dref/generator/tools.hpp"
#include "lux/cxx/algorithm/hash.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace lux::cxx::dref {
	StaticMetaGenerator::StaticMetaGenerator() = default;

	StaticMetaGenerator::~StaticMetaGenerator() = default;

	void StaticMetaGenerator::visit(EnumDecl* decl)
	{
		// 只有当 decl 不为 nullptr 时才进行处理
		if (!decl) {
			return;
		}
		if (decl->is_anonymous) {
			return;
		}

		// 这里是你原先的 generateStaticMeta(EnumDecl&, ...) 的逻辑
		nlohmann::json enum_info = nlohmann::json::object();
		enum_info["name"] = decl->full_qualified_name;
		enum_info["hash"] = std::to_string(lux::cxx::algorithm::fnv1a(decl->full_qualified_name));
		enum_info["extended_name"] = lux::cxx::algorithm::replace(decl->full_qualified_name, "::", "_");
		enum_info["size"] = std::to_string(decl->enumerators.size());
		enum_info["is_scoped"] = decl->is_scoped;
		if (decl->underlying_type) {
			enum_info["underlying_type_name"] = decl->underlying_type->name;
		}
		else {
			enum_info["underlying_type_name"] = "int"; // fallback
		}

		// 收集keys
		nlohmann::json keys = nlohmann::json::array();
		for (auto& enumerator : decl->enumerators) {
			keys.push_back(enumerator.name);
		}
		enum_info["keys"] = keys;

		// 收集values
		nlohmann::json values = nlohmann::json::array();
		if (decl->underlying_type && decl->underlying_type->isSignedInteger()) {
			for (auto& enumerator : decl->enumerators) {
				values.push_back(std::to_string(enumerator.signed_value));
			}
		}
		else {
			for (auto& enumerator : decl->enumerators) {
				values.push_back(std::to_string(enumerator.unsigned_value));
			}
		}
		enum_info["values"] = values;

		// 收集 elements
		nlohmann::json elements = nlohmann::json::array();
		if (decl->underlying_type && decl->underlying_type->isSignedInteger()) {
			for (auto& enumerator : decl->enumerators) {
				nlohmann::json e;
				e["key"] = enumerator.name;
				e["value"] = std::to_string(enumerator.signed_value);
				elements.push_back(e);
			}
		}
		else {
			for (auto& enumerator : decl->enumerators) {
				nlohmann::json e;
				e["key"] = enumerator.name;
				e["value"] = std::to_string(enumerator.unsigned_value);
				elements.push_back(e);
			}
		}
		enum_info["elements"] = elements;

		// 枚举上的属性
		nlohmann::json attrs = nlohmann::json::array();
		for (auto& attr : decl->attributes) {
			attrs.push_back(attr);
		}
		enum_info["attributes"] = attrs;
		enum_info["attributes_count"] = std::to_string(attrs.size());

		// 推入 enums_
		enums_.push_back(enum_info);
	}

	void StaticMetaGenerator::visit(RecordDecl* decl)
	{
		// 如果你只关心 CXXRecordDecl 的话，这里可以留空
		// 或者做一些通用的 record 处理逻辑
	}

	void StaticMetaGenerator::visit(CXXRecordDecl* decl)
	{
		if (!decl) {
			return;
		}
		if (decl->is_anonymous) {
			return;
		}

		// 这里是你原先的 generateStaticMeta(CXXRecordDecl&, ...) 的逻辑
		nlohmann::json record_info = nlohmann::json::object();
		record_info["name"] = decl->full_qualified_name;
		record_info["hash"] = std::to_string(lux::cxx::algorithm::fnv1a(decl->full_qualified_name));
		record_info["extended_name"] = lux::cxx::algorithm::replace(decl->full_qualified_name, "::", "_");

		if (decl->type) {
			record_info["align"] = decl->type->align;
		}
		else {
			record_info["align"] = 0;
		}

		// 是 class 还是 struct
		if (decl->tag_kind == TagDecl::ETagKind::Class) {
			record_info["record_type"] = "ERecordType::Class";
		}
		else {
			record_info["record_type"] = "ERecordType::Struct";
		}

		// 收集字段
		nlohmann::json fields = nlohmann::json::array();
		nlohmann::json field_types = nlohmann::json::array();
		size_t count = 0;
		for (auto* field : decl->field_decls) {
			if (!field) continue;
			nlohmann::json field_obj;
			field_obj["name"] = field->name;
			field_obj["offset"] = std::to_string(field->offset / 8);
			if (field->type) {
				field_obj["size"] = std::to_string(field->type->size);
				field_obj["is_const"] = field->type->is_const;
			}
			else {
				field_obj["size"] = "0";
				field_obj["is_const"] = false;
			}
			field_obj["visibility"] = visibility2Str(field->visibility);
			field_obj["index"] = std::to_string(count);
			fields.push_back(field_obj);

			// 收集type信息
			nlohmann::json ft_obj;
			ft_obj["value"] = (field->type ? field->type->name : "UnknownType");
			ft_obj["last"] = (count == decl->field_decls.size() - 1);
			field_types.push_back(ft_obj);

			++count;
		}
		record_info["fields"] = fields;
		record_info["field_types"] = field_types;
		record_info["fields_count"] = static_cast<int>(fields.size());

		// 收集属性
		nlohmann::json attributes = nlohmann::json::array();
		for (auto& attr : decl->attributes) {
			attributes.push_back(attr);
		}
		record_info["attributes"] = attributes;
		record_info["attributes_count"] = std::to_string(attributes.size());

		// 收集方法（这里仅演示非静态）
		nlohmann::json func_names = nlohmann::json::array();
		nlohmann::json func_types = nlohmann::json::array();
		for (size_t i = 0; i < decl->method_decls.size(); ++i) {
			auto* method = decl->method_decls[i];
			if (!method) continue;

			nlohmann::json name_obj;
			name_obj["name"] = method->spelling;
			func_names.push_back(name_obj);

			nlohmann::json type_obj;
			if (method->result_type) {
				type_obj["return_type"] = method->result_type->name;
			}
			else {
				type_obj["return_type"] = "void";
			}
			type_obj["class_name"] = decl->full_qualified_name;
			type_obj["function_name"] = method->spelling;

			// 参数
			nlohmann::json params = nlohmann::json::array();
			for (size_t p = 0; p < method->params.size(); ++p) {
				auto* param = method->params[p];
				if (!param || !param->type) continue;
				nlohmann::json param_map;
				param_map["type"] = param->type->name;
				param_map["last"] = (p == method->params.size() - 1);
				params.push_back(param_map);
			}
			type_obj["parameters"] = params;
			type_obj["last"] = (i == decl->method_decls.size() - 1);
			type_obj["is_const"] = method->is_const;

			func_types.push_back(type_obj);
		}
		record_info["funcs"] = func_names;
		record_info["func_types"] = func_types;
		record_info["funcs_count"] = std::to_string(func_names.size());

		// 收集静态方法
		nlohmann::json static_names = nlohmann::json::array();
		nlohmann::json static_types = nlohmann::json::array();
		for (size_t i = 0; i < decl->static_method_decls.size(); ++i) {
			auto* method = decl->static_method_decls[i];
			if (!method) continue;

			nlohmann::json name_obj;
			name_obj["name"] = method->spelling;
			static_names.push_back(name_obj);

			nlohmann::json type_obj;
			if (method->result_type) {
				type_obj["return_type"] = method->result_type->name;
			}
			else {
				type_obj["return_type"] = "void";
			}
			type_obj["class_name"] = decl->full_qualified_name;
			type_obj["function_name"] = method->spelling;

			// 参数
			nlohmann::json params = nlohmann::json::array();
			for (size_t p = 0; p < method->params.size(); ++p) {
				auto* param = method->params[p];
				if (!param || !param->type) continue;
				nlohmann::json param_map;
				param_map["type"] = param->type->name;
				param_map["last"] = (p == method->params.size() - 1);
				params.push_back(param_map);
			}
			type_obj["parameters"] = params;
			type_obj["last"] = (i == decl->static_method_decls.size() - 1);
			type_obj["is_const"] = method->is_const;

			static_types.push_back(type_obj);
		}
		record_info["static_funcs"] = static_names;
		record_info["static_func_types"] = static_types;
		record_info["static_funcs_count"] = std::to_string(static_names.size());

		// 推入 m_classes
		classes_.push_back(record_info);
	}

	void StaticMetaGenerator::visit(FieldDecl* decl)
	{
		// 如果你不需要单独处理Field，可以留空
	}

	void StaticMetaGenerator::visit(FunctionDecl* decl)
	{
		// 静态反射中是否需要处理“自由函数”？
		// 如果原先不需要，则可留空
	}

	void StaticMetaGenerator::visit(CXXMethodDecl* decl)
	{
		// 如果不需要在此单独处理，可留空
	}

	void StaticMetaGenerator::visit(ParmVarDecl* decl)
	{
		// 如果不需要在此单独处理，可留空
	}

	void StaticMetaGenerator::toJsonFile(nlohmann::json& json)
	{
		json["classes"] = classes_;
		json["enums"] = enums_;
	}
}