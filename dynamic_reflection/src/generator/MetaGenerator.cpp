#include "lux/cxx/dref/generator/GeneratorHelper.hpp"
#include "lux/cxx/algorithm/hash.hpp"
#include "lux/cxx/dref/parser/CxxParser.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <inja/inja.hpp>
#include <fstream>
#include <filesystem>

using namespace ::lux::cxx::dref;

//-----------------------------------------------------------------------
// 检查配置中的所有文件是否存在
static bool validateFiles(const GeneratorConfig &generator_config)
{
    // 检查所有目标文件
    for (const auto &file : generator_config.target_files)
    {
        if (!std::filesystem::exists(file))
        {
            std::cerr << "[Error] file_to_parse " << file << " does not exist.\n";
            return false;
        }
    }
    // 检查源文件
    if (!std::filesystem::exists(generator_config.source_file))
    {
        std::cerr << "[Error] source_file " << generator_config.source_file << " does not exist or not matched.\n";
        std::cerr << "[Error] You can check your build system, and try to make sure use absolute path for source file.\n";
        std::cerr << "[Error] If you are using cmake, the possible format is ${CMAKE_CURRENT_SOURCE_DIR}/to/source_file.\n";
        return false;
    }
    // 检查 compile_commands 文件
    if (!std::filesystem::exists(generator_config.compile_commands))
    {
        std::cerr << "[Error] compile_commands " << generator_config.compile_commands << " does not exist.\n";
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------
// 根据配置构建编译选项（包含 -I 选项及其他预定义）
static std::vector<std::string> buildCompileOptions(const GeneratorConfig &generator_config)
{
    // 从 compile_commands 中收集额外包含目录
    auto extra_includes = GeneratorHelper::fetchIncludePaths(
        generator_config.compile_commands, generator_config.source_file
    );
    auto source_path = std::filesystem::path(generator_config.source_file);
    auto source_parent = source_path.parent_path();
    if (source_parent != std::filesystem::path("."))
    {
        extra_includes.push_back(source_parent.string());
    }
    auto include_options = GeneratorHelper::convertToDashI(extra_includes);
    
    std::vector<std::string> options;
    options.reserve(include_options.size() + generator_config.extra_compile_options.size() + 2);
    options.push_back("--std=c++20");
    options.push_back("-D__LUX_PARSE_TIME__=1");
    
    // 添加包含目录选项
    for (const auto &inc : include_options) {
        options.push_back(inc);  // 例如 "-ID:/some/include"
    }
    // 如果有额外的编译选项（extra_compile_options）也加入
    for (const auto &opt : generator_config.extra_compile_options) {
        options.push_back(opt);
    }
    return options;
}

//-----------------------------------------------------------------------
// 处理单个目标文件：调用解析器、生成 meta data 并（可选）序列化输出 JSON 文件
static bool processTargetFile(const std::string &file,
    const GeneratorConfig &generator_config,
    const std::vector<std::string> &options,
    const std::string &source_parent_str,
    std::vector<nlohmann::json> &meta_json_list)
{
    ParseOptions parse_options;
    parse_options.commands        = options;
    parse_options.marker_symbol   = generator_config.marker;
    parse_options.name            = file;
    parse_options.pch_file        = "";      // TODO: add pch file support
    parse_options.version         = "1.0.0"; // TODO: add version support
    auto cxx_parser = std::make_unique<CxxParser>(parse_options);

    auto [parse_rst, data] = cxx_parser->parse(file);
    if (parse_rst != EParseResult::SUCCESS)
    {
        std::cerr << "[Error] Parsing of " << file << " failed.\n";
        return false;
    }

    auto meta_json = data.toJson();
    meta_json["source_path"] = file;
    meta_json["source_parent"] = source_parent_str;
    meta_json["parser_compile_options"] = options;

    // 如果配置中要求序列化 meta data，则输出对应的 JSON 文件
    if (generator_config.serial_meta)
    {
        auto meta_json_str  = nlohmann::to_string(meta_json);
        auto json_file_name = std::filesystem::path(file).stem().string() + ".json";
        auto out_path       = std::filesystem::path(generator_config.out_dir) / json_file_name;

        std::ofstream out_file(out_path, std::ios::binary);
        if (!out_file.is_open()) {
            std::cerr << "[Error] Failed to open file " << out_path << " for writing.\n";
            return false;
        }
        out_file << meta_json_str;
        out_file.close();
    }
    
    meta_json_list.push_back(meta_json);
    return true;
}

//-----------------------------------------------------------------------
// 加载模板并渲染所有 meta data ，生成最终输出文件
static bool renderTemplates(const GeneratorConfig &generator_config,
    const std::vector<nlohmann::json> &meta_json_list)
{
    inja::Environment inja_env;
    inja::Template    inja_template;
    
    // 加载模板文件
    std::ifstream template_file(generator_config.template_path);
    if (!template_file.is_open()) {
        std::cerr << "[Error] Failed to open template file " << generator_config.template_path << "\n";
        return false;
    }
    try {
        std::string template_str((std::istreambuf_iterator<char>(template_file)),
                                 std::istreambuf_iterator<char>());
        inja_template = inja_env.parse(template_str);
    }
    catch (const std::exception &e) {
        std::cerr << "[Error] Failed to parse template: " << e.what() << "\n";
        return false;
    }

    // 如果是 dry run，则不生成输出文件
    if (generator_config.dry_run)
    {
        std::cout << "[Info] Dry run, no output will be generated.\n";
        return true;
    }

    // 对每个 meta data 渲染模板并写入输出文件
    for (const auto &meta_json : meta_json_list)
    {
        auto source_file_name = std::filesystem::path(meta_json["source_path"].get<std::string>()).stem().string();
        auto meta_file_path   = std::filesystem::path(generator_config.out_dir) / (source_file_name + generator_config.meta_suffix);
        std::ofstream out_file(meta_file_path, std::ios::binary);
        if (!out_file.is_open()) {
            std::cerr << "[Error] Failed to open file " << meta_file_path << " for writing.\n";
            return false;
        }
        
        try {
            auto render_rst = inja_env.render(inja_template, meta_json);    
            out_file << render_rst;
        }
        catch (const std::exception &e) {
            std::cerr << "[Error] Failed to write to file " << meta_file_path << ": " << e.what() << "\n";
            return false;
        }
        out_file.close();
    }
    return true;
}

//-----------------------------------------------------------------------
// 主函数：依次调用各个独立的处理函数
int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: meta_generator <config_json>\n";
        return 1;
    }

    GeneratorConfig generator_config;
    try {
        GeneratorHelper::loadGeneratorConfig(argv[1], generator_config);
    }
    catch (const std::exception &e) {
        std::cerr << "[Error] Failed to load config: " << e.what() << "\n";
        return 1;
    }

    if (!validateFiles(generator_config)) {
        return 1;
    }

    auto options = buildCompileOptions(generator_config);
    auto source_path = std::filesystem::path(generator_config.source_file);
    auto source_parent = source_path.parent_path().string();

    std::vector<nlohmann::json> meta_json_list;
    for (const auto &file : generator_config.target_files)
    {
        if (!processTargetFile(file, generator_config, options, source_parent, meta_json_list)) {
            return 1;
        }
    }

    if (!renderTemplates(generator_config, meta_json_list)) {
        return 1;
    }

    return 0;
}
