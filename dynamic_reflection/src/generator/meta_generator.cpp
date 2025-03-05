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

struct GeneratorConfig
{
    std::filesystem::path target_dir;
	std::string 	      file_name;
	std::string           static_meta_suffix;
	std::string           dynamic_meta_suffix;
};

class MetaGenerator
{
public:
	MetaGenerator(GeneratorConfig config)
		: _config(std::move(config))
	{
	}

    enum class EGenerateError
    {
		SUCCESS,
		UNMARKED,
		FAILED
    };

	struct GenerationContext
	{
        nlohmann::json static_records   = nlohmann::json::array();
        nlohmann::json static_enums     = nlohmann::json::array();
        nlohmann::json dynamic_records  = nlohmann::json::array();
        nlohmann::json dynamic_enums    = nlohmann::json::array();
	};

    EGenerateError generate(const lux::cxx::dref::MetaUnit& unit)
    {
        // ---------------------------
        // 1) 收集静态所需信息
        // ---------------------------
        nlohmann::json static_data;

        // ---------------------------
        // 2) 收集动态所需信息
        // ---------------------------
        nlohmann::json dynamic_data;

		GenerationContext context;
		for (auto* decl : unit.markedDeclarations())
		{
			declarationDispatch(decl, context);
		}

        // 整理成 inja 模板最终需要的数据
        static_data["classes"] = context.static_records;
        static_data["enums"] = context.static_enums;

        dynamic_data["classes"] = context.dynamic_records;
        dynamic_data["enums"] = context.dynamic_enums;

        // ---------------------------
        // 3) 使用 Inja 渲染静态模板
        // ---------------------------
        try
        {
            inja::Environment env;
            inja::Template tpl = env.parse(static_meta_template.data());
            std::string result = env.render(tpl, static_data);

            // 输出到文件
            auto output_path = _config.target_dir / (_config.file_name + _config.static_meta_suffix);
            std::ofstream ofs(output_path);
            ofs << result;
            ofs.close();
            std::cout << "[MetaGenerator] Generated static meta file: " << output_path << std::endl;
        }
        catch (std::exception& e)
        {
            std::cerr << "[MetaGenerator] Error rendering static meta: " << e.what() << std::endl;
            return EGenerateError::FAILED;
        }

        // ---------------------------
        // 4) 使用 Inja 渲染动态模板
        // ---------------------------
        try
        {
            inja::Environment env;
            inja::Template tpl = env.parse(dynamic_meta_template.data());
            std::string result = env.render(tpl, dynamic_data);

            // 输出到文件
            auto output_path = _config.target_dir / (_config.file_name + _config.dynamic_meta_suffix);
            std::ofstream ofs(output_path);
            ofs << result;
            ofs.close();
            std::cout << "[MetaGenerator] Generated dynamic meta file: " << output_path << std::endl;
        }
        catch (std::exception& e)
        {
            std::cerr << "[MetaGenerator] Error rendering dynamic meta: " << e.what() << std::endl;
            return EGenerateError::FAILED;
        }

        return EGenerateError::SUCCESS;
    }

private:
	enum class EMarkedType : uint8_t
	{
		STATIC = 0,
		DYNAMIC,
        BOTH,
		NONE
	};

    EMarkedType markedType(lux::cxx::dref::NamedDecl* decl)
    {
		EMarkedType ret{ EMarkedType::NONE };
		static std::array<std::array<EMarkedType, 2>, 4> table{
            std::array<EMarkedType, 2>{EMarkedType::STATIC,     EMarkedType::BOTH},
            std::array<EMarkedType, 2>{EMarkedType::BOTH,       EMarkedType::DYNAMIC},
            std::array<EMarkedType, 2>{EMarkedType::BOTH,       EMarkedType::BOTH},
            std::array<EMarkedType, 2>{EMarkedType::STATIC,     EMarkedType::DYNAMIC}
		};

        const auto& attrs = decl->attributes;
		for (const auto& attr : attrs)
		{
			if (attr == "static_reflection")
			{
				ret = table[static_cast<uint8_t>(ret)][0];
			}
			
            if (attr == "dynamic_reflection")
			{
                ret = table[static_cast<uint8_t>(ret)][1];
			}
		}

		return ret;
    }

    bool generateStaticMeta(lux::cxx::dref::NamedDecl* decl, GenerationContext& context)
    {
        if (decl->kind == lux::cxx::dref::EDeclKind::CXX_RECORD_DECL)
        {
            return generateStaticMeta(dynamic_cast<lux::cxx::dref::CXXRecordDecl&>(*decl), context.static_records);
        }
        else if (decl->kind == lux::cxx::dref::EDeclKind::ENUM_DECL)
        {
            return generateStaticMeta(dynamic_cast<lux::cxx::dref::EnumDecl&>(*decl), context.static_enums);
        }
        return false;
    }

    bool generateDynamicMeta(lux::cxx::dref::NamedDecl* decl, GenerationContext& context)
    {
        if (decl->kind == lux::cxx::dref::EDeclKind::CXX_RECORD_DECL)
        {
			return generateDynamicMeta(dynamic_cast<lux::cxx::dref::CXXRecordDecl&>(*decl), context.dynamic_records);
        }
        else if (decl->kind == lux::cxx::dref::EDeclKind::ENUM_DECL)
        {
			return generateDynamicMeta(dynamic_cast<lux::cxx::dref::EnumDecl&>(*decl), context.dynamic_enums);
        }
        return false;
    }

	EGenerateError declarationDispatch(lux::cxx::dref::Decl* decl, GenerationContext& context)
	{
		using namespace lux::cxx::dref;
        auto* rec = dynamic_cast<NamedDecl*>(decl);
        auto marked_type = markedType(rec);

        if (marked_type == EMarkedType::STATIC)
        {
			generateStaticMeta(rec, context);
		}
		else if (marked_type == EMarkedType::DYNAMIC)
		{
			generateDynamicMeta(rec, context);
		}
        else if (marked_type == EMarkedType::BOTH)
        {
            generateStaticMeta(rec, context);
            generateDynamicMeta(rec, context);
        }
        else
        {
            EGenerateError::UNMARKED;
        }

		return EGenerateError::SUCCESS;
	}

    bool generateStaticMeta(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& classes)
    {
        nlohmann::json record_info = nlohmann::json::object();
        record_info["name"] = decl.full_qualified_name;
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

    bool generateDynamicMeta(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& records)
    {
        using namespace lux::cxx::dref;

        // 构造一个 JSON 对象存放此类的动态信息
        nlohmann::json record_info = nlohmann::json::object();
        record_info["name"] = decl.full_qualified_name;
        // 或者 decl.name，看你想用全限定名还是简写

        // 1) 收集“实例方法”的动态调用信息
        {
            nlohmann::json methods_json = nlohmann::json::array();
            for (auto* method : decl.method_decls)
            {
                nlohmann::json method_info;
                method_info["name"] = method->spelling;
                method_info["return_type"] = method->result_type->name;
                method_info["is_const"] = method->is_const;
                method_info["is_virtual"] = method->is_virtual;
                method_info["is_static"] = false;

                // collect parameters
                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < method->params.size(); ++i)
                {
                    auto* param_decl = method->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                method_info["parameters"] = params;

                // 生成桥接函数名
                std::string bridge_name = decl.name + std::string("_") + method->spelling + "_invoker";
                method_info["bridge_name"] = bridge_name;

                methods_json.push_back(method_info);
            }
            record_info["methods"] = methods_json;
        }

        // 2) 收集“静态方法”的动态调用信息
        {
            nlohmann::json static_methods_json = nlohmann::json::array();
            for (auto* method : decl.static_method_decls)
            {
                nlohmann::json method_info;
                method_info["name"] = method->spelling;
                method_info["return_type"] = method->result_type->name;

                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < method->params.size(); ++i)
                {
                    auto* param_decl = method->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                method_info["parameters"] = params;

                std::string bridge_name = decl.name + std::string("_") + method->spelling + "_static_invoker";
                method_info["bridge_name"] = bridge_name;

                static_methods_json.push_back(method_info);
            }
            record_info["static_methods"] = static_methods_json;
        }

        // 3) 收集构造函数信息(如果需要)
        //    在 Clang 中构造函数也是 CXXConstructorDecl
        //    你可以在 decl.constructor_decls 里收集
        {
            nlohmann::json ctors_json = nlohmann::json::array();
            for (auto* ctor : decl.constructor_decls)
            {
                nlohmann::json ctor_info;
                ctor_info["ctor_name"] = ctor->spelling; // or "Foo" typically
                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < ctor->params.size(); ++i)
                {
                    auto* param_decl = ctor->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                ctor_info["parameters"] = params;

                std::string bridge_name = decl.name + "_ctor_" + std::to_string(reinterpret_cast<uintptr_t>(ctor));
                ctor_info["bridge_name"] = bridge_name;

                ctors_json.push_back(ctor_info);
            }
            record_info["constructors"] = ctors_json;
        }

        // 5) 可收集字段信息
        {
            nlohmann::json fields_json = nlohmann::json::array();
            for (auto* field : decl.field_decls)
            {
                nlohmann::json f;
                f["name"] = field->name;
                f["type_name"] = field->type->name;
                f["offset"] = static_cast<int>(field->offset / 8);
                f["visibility"] = visibility2Str(field->visibility);
                f["is_static"] = false; // 如果想区分静态成员变量可以再判断

                fields_json.push_back(f);
            }
            record_info["fields"] = fields_json;
        }

        // 最后放入 records 数组
        records.push_back(record_info);
        return true;
    }

    bool generateStaticMeta(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums)
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

    bool generateDynamicMeta(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums)
    {
        nlohmann::json enum_info;
        enum_info["name"] = decl.full_qualified_name;  // 或 decl.name

        // 这里可以收集枚举项
        nlohmann::json enumerators_json = nlohmann::json::array();
        for (auto& enumerator : decl.enumerators)
        {
            nlohmann::json e;
            e["name"] = enumerator.name;
            e["value"] = (decl.underlying_type->isSignedInteger())
                ? std::to_string(enumerator.signed_value)
                : std::to_string(enumerator.unsigned_value);
            enumerators_json.push_back(e);
        }
        enum_info["enumerators"] = enumerators_json;

        // 如果需要是否是 scoped enum
        enum_info["is_scoped"] = decl.is_scoped;
        enum_info["underlying_type_name"] = decl.underlying_type->name;

        // push to array
        enums.push_back(enum_info);
        return true;
    }

	GeneratorConfig _config;
};

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
    static auto output_path = argv[2];
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
	std::cout << "target_dir:" << output_path << std::endl;
	std::cout << "file_name:" << target_file << std::endl;
	std::cout << "static_meta_suffix:" << ".meta.static.hpp" << std::endl;
	std::cout << "dynamic_meta_suffix:" << ".meta.dynamic.hpp" << std::endl;
	auto file_name = std::filesystem::path(target_file).filename().replace_extension("").string();
    GeneratorConfig config{
        .target_dir = std::filesystem::path(output_path),
		.file_name = file_name,
        .static_meta_suffix = ".meta.static.hpp",
        .dynamic_meta_suffix = ".meta.dynamic.hpp"
    };

    MetaGenerator generator(config);

	if (generator.generate(data) != MetaGenerator::EGenerateError::SUCCESS)
	{
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

