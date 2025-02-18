#include <clang_config.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <bustache/format.hpp>
#include <bustache/model.hpp>

#include "static_meta.mustache.hpp"

static std::string renderDeclaration(const std::vector<lux::cxx::dref::Decl*>&);
static bool renderCxxRecord(lux::cxx::dref::CXXRecordDecl& decl, bustache::array& classes);
static bool renderEnumeration(lux::cxx::dref::EnumDecl& decl, bustache::array& enums);

int main(const int argc, const char* argv[])
{
    if (argc < 3)
    {
        return 0;
    }

    static auto target_file = argv[1];
    static auto output_file = argv[2];

    if (!std::filesystem::exists(target_file))
    {
        std::cerr << "File " << target_file << " does not exist" << std::endl;
        return 1;
    }

    const lux::cxx::dref::CxxParser cxx_parser;

    // 构造编译选项
    std::vector<std::string_view> options;
    // 添加预定义的 C++ 标准和 include 路径（这些宏由你的环境或配置文件定义）
    options.emplace_back(__LUX_CXX_INCLUDE_DIR_DEF__);
    options.emplace_back("--std=c++20");

    // 输出编译选项，便于调试
    std::cout << "Compile options:" << std::endl;
    for (const auto& opt : options)
    {
        std::cout << "\t" << opt << std::endl;
    }

    // 调用解析函数，传入文件和编译选项
    auto [parse_rst, data] = cxx_parser.parse(
        target_file,
        std::move(options),
        "test_unit", "1.0.0"
    );

    if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS)
    {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }

    try
    {
        std::ofstream output(output_file, std::ios::binary);
        output << renderDeclaration(data.markedDeclarations());
        output.flush();
        output.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "MSTCH render error: \n" << e.what() << std::endl;
    }

    return 0;
}

static std::string renderDeclaration(const std::vector<lux::cxx::dref::Decl*>& declarations)
{
    bustache::object context;
    bustache::array  classes;
    bustache::array  enums;

    bustache::format format(static_meta_template);
    // 渲染模板

    for (const auto decl : declarations)
    {
        switch (decl->kind)
        {
        case lux::cxx::dref::EDeclKind::CXX_RECORD_DECL:
            {
                auto cxx_record_decl = static_cast<lux::cxx::dref::CXXRecordDecl*>(decl);
                renderCxxRecord(*cxx_record_decl, classes);
                break;
            }
        case lux::cxx::dref::EDeclKind::ENUM_DECL:
            {
                auto cxx_record_decl = static_cast<lux::cxx::dref::EnumDecl*>(decl);
                renderEnumeration(*cxx_record_decl, enums);
                break;
            }
        default:
            continue;
        }
    }

    context["classes"] = classes;
    context["enums"]   = enums;

    return bustache::to_string(format(context));
}

static const char* visibility2Str(lux::cxx::dref::EVisibility visibility)
{
    switch (visibility)
    {
    case lux::cxx::dref::EVisibility::PUBLIC:
        return "EVisibility::PUBLIC";
    case lux::cxx::dref::EVisibility::PRIVATE:
        return "EVisibility::PRIVATE";
    case lux::cxx::dref::EVisibility::PROTECTED:
        return "EVisibility::PROTECTED";
    default:
        return "EVisibility::INVALID";
    }
}

bool renderCxxRecord(lux::cxx::dref::CXXRecordDecl& decl, bustache::array& classes)
{
    // 构造 record 的基础信息
    bustache::object record_info;
    record_info["name"]  = decl.full_qualified_name;
    record_info["align"] = decl.type->align;
    record_info["record_type"] = decl.tag_kind ==
        lux::cxx::dref::TagDecl::ETagKind::Class ?  "EMetaType::CLASS" : "EMetaType::STRUCT";
    record_info["fields_count"] = static_cast<int>(decl.field_decls.size());

    // 用 lambda 构造参数列表
    auto build_params = [&](const auto& params_vector) -> bustache::array
    {
        bustache::array params;
        size_t index = 0;
        for (const auto& param : params_vector)
        {
            bustache::object param_map;
            param_map["type"] = param->type->name;
            param_map["last"] = (index == params_vector.size() - 1);
            params.push_back(param_map);
            ++index;
        }
        return params;
    };

    // 构造成员函数和静态函数通用的上下文（名称列表和类型信息列表）
    auto build_function_context = [&](const auto& methods)
        -> std::pair<bustache::array, bustache::array>
    {
        bustache::array names;
        bustache::array types;
        size_t count = 0;
        for (const auto& method : methods)
        {
            // 构造函数名称对象
            bustache::object name_obj;
            name_obj["name"] = method->spelling;
            names.push_back(name_obj);

            // 构造函数类型对象
            bustache::object type_obj;
            type_obj["return_type"] = method->result_type->name;
            type_obj["class_name"] = decl.name;
            type_obj["function_name"] = method->spelling;
            type_obj["parameters"] = build_params(method->params);
            type_obj["last"] = (count == methods.size() - 1);
            type_obj["is_const"] = method->is_const;

            types.push_back(type_obj);
            ++count;
        }
        return std::make_pair(names, types);
    };

    // render fields
    {
        bustache::array fields;
        bustache::array field_types;
        size_t count = 0;
        for (auto* field : decl.field_decls)
        {
            bustache::object field_obj;
            field_obj["name"] = field->name;
            field_obj["offset"] = std::to_string(field->offset / 8);
            field_obj["visibility"] = visibility2Str(field->visibility);
            field_obj["index"] = std::to_string(count);
            fields.push_back(field_obj);

            bustache::object ft_obj;
            ft_obj["value"] = field->type->name;
            ft_obj["last"] = (count == decl.field_decls.size() - 1);
            field_types.push_back(ft_obj);
            ++count;
        }
        record_info["fields"] = fields;
        record_info["field_types"] = field_types;
    }

    // render attributes
    {
        bustache::array attributes;
        for (const auto& attr : decl.attributes)
        {
            // 假设 attr 为 std::string 类型
            attributes.push_back(attr);
        }
        record_info["attributes"] = attributes;
        record_info["attributes_count"] = std::to_string(attributes.size());
    }

    // render member functions（非静态）
    {
        auto context_pair = build_function_context(decl.method_decls);
        record_info["funcs"] = context_pair.first;
        record_info["func_types"] = context_pair.second;
        record_info["funcs_count"] = std::to_string(context_pair.first.size());
    }

    // render static functions
    {
        auto context_pair = build_function_context(decl.static_method_decls);
        record_info["static_funcs"] = context_pair.first;
        record_info["static_func_types"] = context_pair.second;
        record_info["static_funcs_count"] = std::to_string(context_pair.first.size());
    }

    // 将该类型加入到 classes 数组中
    classes.push_back(record_info);

    return true;
}

bool renderEnumeration(lux::cxx::dref::EnumDecl& decl, bustache::array& enums)
{
    // 创建一个对象用于存放单个枚举类型的数据
    bustache::object enum_info;
    enum_info["name"] = decl.full_qualified_name;
    enum_info["size"] = std::to_string(decl.enumerators.size()); // 枚举项个数

    bustache::array keys;
    for (const auto& enumerator : decl.enumerators)
    {
        keys.push_back(enumerator.name);
    }
    enum_info["keys"] = keys;

    bustache::array values;
    if (decl.underlying_type->isSignedInteger())
    {
        for (const auto& enumerator : decl.enumerators)
        {
            values.push_back(std::to_string(enumerator.signed_value));
        }
    }
    else
    {
        for (const auto& enumerator : decl.enumerators)
        {
            values.push_back(std::to_string(enumerator.unsigned_value));
        }
    }
    enum_info["values"] = values;

    // 构造 elements 数组，每项是一个对象，包含 key 和 value
    bustache::array elements;
    if (decl.underlying_type->isSignedInteger())
    {
        for (const auto& enumerator : decl.enumerators)
        {
            bustache::object e;
            e["key"] = enumerator.name;
            e["value"] = std::to_string(enumerator.signed_value);
            elements.push_back(e);
        }
    }
    else
    {
        for (const auto& enumerator : decl.enumerators)
        {
            bustache::object e;
            e["key"] = enumerator.name;
            e["value"] = std::to_string(enumerator.unsigned_value);
            elements.push_back(e);
        }
    }
    enum_info["elements"] = elements;

    // 构造 switch-case 部分（枚举项名称）
    bustache::array cases;
    for (const auto& enumerator : decl.enumerators)
    {
        cases.push_back(enumerator.name);
    }
    enum_info["cases"] = cases;

    enums.push_back(enum_info);

    return true;
}
