#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <filesystem>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "static_meta.inja.hpp"
#include "dynamic_meta.inja.hpp"

static std::vector<std::string> fetchIncludePaths(const std::filesystem::path&, const std::filesystem::path&);

static std::string renderDeclaration(const std::vector<lux::cxx::dref::Decl*>&);
static bool renderCxxRecord(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& classes);
static bool renderEnumeration(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums);

/**
 * argv0: Path of meta_generator.exe
 * argv1: A file path needs to be parsed
 * argv2: Output path
 * argv3: A source file which needs meta-info (this file should in compile commands)
 * argv4: compile_commands.json path
 */
int main(const int argc, const char* argv[])
{
    if (argc < 5)
    {
        return 0;
    }  
 
    static auto target_file = argv[1];
    static auto output_file = argv[2];
    static auto source_file = argv[3];
    static auto compile_command_path = argv[4];

    inja::Environment env;

    if (!std::filesystem::exists(target_file))
    {
        std::cerr << "File " << target_file << " does not exist" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(source_file))
    {
        std::cerr << "Source file " << source_file << " does not exist" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(compile_command_path))
    {
        std::cerr << "Compile command file " << compile_command_path << " does not exist" << std::endl;
        return 1;
    }

    auto include_paths = fetchIncludePaths(compile_command_path, source_file);

    const lux::cxx::dref::CxxParser cxx_parser;

    // 构造编译选项
    std::vector<std::string> options;
    options.push_back("-resource-dir=/usr/lib/clang/19"); 
    options.emplace_back("--std=c++20");
    for (auto& path : include_paths)
    {
        options.emplace_back(std::move(path));
    }

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
        std::cerr << "Inja render error: \n" << e.what() << std::endl;
        return 1;
    }

    return 0;
}

static std::vector<std::string> splitCommand(const std::string &cmd) {
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// 使用 std::filesystem::path 生成绝对路径
static std::filesystem::path makeAbsolute(const std::filesystem::path &baseDir, const std::filesystem::path &p) {
    if (p.is_absolute())
        return p;
    return std::filesystem::absolute(baseDir / p);
}

// 判断字符串是否符合 Windows 标准绝对路径（例如 "D:\..."）
static bool isStandardAbsolute(const std::string &s) {
    return (s.size() >= 3 && std::isalpha(s[0]) && s[1] == ':' && (s[2] == '\\' || s[2] == '/'));
}

std::vector<std::string> fetchIncludePaths(const std::filesystem::path& compile_command_path, const std::filesystem::path& source_file_path)
{
    std::ifstream ifs(compile_command_path, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open " << compile_command_path << std::endl;
        return {};
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string jsonContent = buffer.str();
    ifs.close();

    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    if (doc.HasParseError() || !doc.IsArray()) {
        std::cerr << "Failed to parse compile_commands.json or root is not an array." << std::endl;
        return {};
    }

    std::set<std::string> includePaths;

    // 遍历所有编译条目
    for (auto& entry : doc.GetArray()) {
        if (!entry.IsObject())
            continue;

        if (!entry.HasMember("file") || !entry.HasMember("directory"))
            continue;

        std::string fileStr = entry["file"].GetString();
        // 简单匹配目标文件（实际情况可能需要归一化路径）
        if (std::filesystem::path(fileStr) != source_file_path)
            continue;

        std::filesystem::path baseDir = entry["directory"].GetString();
        std::vector<std::string> args;

        // 如果存在 "arguments" 字段，则直接作为数组处理
        if (entry.HasMember("arguments") && entry["arguments"].IsArray()) {
            for (auto& arg : entry["arguments"].GetArray()) {
                args.push_back(arg.GetString());
            }
        } else if (entry.HasMember("command") && entry["command"].IsString()) {
            std::string cmdStr = entry["command"].GetString();
            args = splitCommand(cmdStr);
        }

        // 遍历参数，查找以 "-I" 或 "-external:" 开头的参数
        for (size_t i = 0; i < args.size(); ++i)
        {
            std::string arg = args[i];
            std::string pathStr;
            if (arg.rfind("-I", 0) == 0) {
                // 处理 -I 参数
                if (arg == "-I") {
                    if (i + 1 < args.size()) {
                        pathStr = args[++i];
                    }
                } else {
                    pathStr = arg.substr(2);
                }
            } else if (arg.rfind("-external:I", 0) == 0) {
                // 处理 -external: 参数，将其转换为 -I
                if (arg == "-external:I") {
                    if (i + 1 < args.size()) {
                        pathStr = args[++i];
                    }
                } else {
                    pathStr = arg.substr(std::string("-external:I").length());
                }
                // 此时我们希望返回的仍然是 "-I" + 路径
            }
            if (!pathStr.empty()) {
                std::filesystem::path incPath;
                // 如果 pathStr 已经是标准的绝对路径，则直接使用
                if (isStandardAbsolute(pathStr))
                    incPath = std::filesystem::path(pathStr);
                else
                    incPath = makeAbsolute(baseDir, pathStr);
                includePaths.insert("-I" + incPath.string());
            }
        }
    }

    return std::vector(includePaths.begin(), includePaths.end());
}

static std::string renderDeclaration(const std::vector<lux::cxx::dref::Decl*>& declarations)
{
    nlohmann::json context;
    nlohmann::json classes = nlohmann::json::array();
    nlohmann::json enums = nlohmann::json::array();

    // 渲染各个声明，构造模板上下文
    for (const auto decl : declarations)
    {
        switch (decl->kind)
        {
        case lux::cxx::dref::EDeclKind::CXX_RECORD_DECL:
            {
                auto cxx_record_decl = static_cast<lux::cxx::dref::CXXRecordDecl*>(decl);
                if (!renderCxxRecord(*cxx_record_decl, classes))
                {
                    std::cerr << "Render CXX Record Failed!" << std::endl;
                }
                break;
            }
        case lux::cxx::dref::EDeclKind::ENUM_DECL:
            {
                auto enum_decl = static_cast<lux::cxx::dref::EnumDecl*>(decl);
                if (!renderEnumeration(*enum_decl, enums))
                {
                    std::cerr << "Render Enumeration Failed!" << std::endl;
                }
                break;
            }
        default:
            continue;
        }
    }

    context["classes"] = classes;
    context["enums"]   = enums;

    inja::Environment env;
    return env.render(static_meta_template, context);
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

bool renderCxxRecord(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& classes)
{
    nlohmann::json record_info = nlohmann::json::object();
    record_info["name"]  = decl.full_qualified_name;
    record_info["align"] = decl.type->align;
    record_info["record_type"] = (decl.tag_kind == lux::cxx::dref::TagDecl::ETagKind::Class)
                                    ? "EMetaType::CLASS" : "EMetaType::STRUCT";
    record_info["fields_count"] = static_cast<int>(decl.field_decls.size());

    // lambda 用于构造参数列表
    auto build_params = [&](const auto& params_vector) -> nlohmann::json
    {
        nlohmann::json params = nlohmann::json::array();
        size_t index = 0;
        for (const auto& param : params_vector)
        {
            nlohmann::json param_map;
            param_map["type"] = param->type->name;
            param_map["last"] = (index == params_vector.size() - 1);
            params.push_back(param_map);
            ++index;
        }
        return params;
    };

    // 构造成员函数和静态函数上下文（名称列表和类型信息列表）
    auto build_function_context = [&](const auto& methods)
        -> std::pair<nlohmann::json, nlohmann::json>
    {
        nlohmann::json names = nlohmann::json::array();
        nlohmann::json types = nlohmann::json::array();
        size_t count = 0;
        for (const auto& method : methods)
        {
            nlohmann::json name_obj;
            name_obj["name"] = method->spelling;
            names.push_back(name_obj);

            nlohmann::json type_obj;
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

    // 渲染字段
    {
        nlohmann::json fields = nlohmann::json::array();
        nlohmann::json field_types = nlohmann::json::array();
        size_t count = 0;
        for (auto* field : decl.field_decls)
        {
            nlohmann::json field_obj;
            field_obj["name"] = field->name;
            field_obj["offset"] = std::to_string(field->offset / 8);
            field_obj["visibility"] = visibility2Str(field->visibility);
            field_obj["index"] = std::to_string(count);
            fields.push_back(field_obj);

            nlohmann::json ft_obj;
            ft_obj["value"] = field->type->name;
            ft_obj["last"] = (count == decl.field_decls.size() - 1);
            field_types.push_back(ft_obj);
            ++count;
        }
        record_info["fields"] = fields;
        record_info["field_types"] = field_types;
    }

    // 渲染 attributes
    {
        nlohmann::json attributes = nlohmann::json::array();
        for (const auto& attr : decl.attributes)
        {
            attributes.push_back(attr);
        }
        record_info["attributes"] = attributes;
        record_info["attributes_count"] = std::to_string(attributes.size());
    }

    // 渲染成员函数（非静态）
    {
        auto context_pair = build_function_context(decl.method_decls);
        record_info["funcs"] = context_pair.first;
        record_info["func_types"] = context_pair.second;
        record_info["funcs_count"] = std::to_string(context_pair.first.size());
    }

    // 渲染静态函数
    {
        auto context_pair = build_function_context(decl.static_method_decls);
        record_info["static_funcs"] = context_pair.first;
        record_info["static_func_types"] = context_pair.second;
        record_info["static_funcs_count"] = std::to_string(context_pair.first.size());
    }

    classes.push_back(record_info);
    return true;
}

bool renderEnumeration(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums)
{
    nlohmann::json enum_info = nlohmann::json::object();
    enum_info["name"] = decl.full_qualified_name;
    enum_info["size"] = std::to_string(decl.enumerators.size());

    nlohmann::json keys = nlohmann::json::array();
    for (const auto& enumerator : decl.enumerators)
    {
        keys.push_back(enumerator.name);
    }
    enum_info["keys"] = keys;

    nlohmann::json values = nlohmann::json::array();
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

    nlohmann::json elements = nlohmann::json::array();
    if (decl.underlying_type->isSignedInteger())
    {
        for (const auto& enumerator : decl.enumerators)
        {
            nlohmann::json e;
            e["key"] = enumerator.name;
            e["value"] = std::to_string(enumerator.signed_value);
            elements.push_back(e);
        }
    }
    else
    {
        for (const auto& enumerator : decl.enumerators)
        {
            nlohmann::json e;
            e["key"] = enumerator.name;
            e["value"] = std::to_string(enumerator.unsigned_value);
            elements.push_back(e);
        }
    }
    enum_info["elements"] = elements;

    // 渲染 attributes
    nlohmann::json attributes = nlohmann::json::array();
    for (const auto& attr : decl.attributes)
    {
        attributes.push_back(attr);
    }
    enum_info["attributes"] = attributes;
    enum_info["attributes_count"] = std::to_string(attributes.size());

    nlohmann::json cases = nlohmann::json::array();
    for (const auto& enumerator : decl.enumerators)
    {
        cases.push_back(enumerator.name);
    }
    enum_info["cases"] = cases;

    enums.push_back(enum_info);
    return true;
}
