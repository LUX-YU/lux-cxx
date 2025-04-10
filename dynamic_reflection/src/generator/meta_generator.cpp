/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <optional>
#include <set>
#include <array>
#include <unordered_set>

#include <lux/cxx/dref/generator/Generator.hpp>

#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/algorithm/hash.hpp>
#include <lux/cxx/algorithm/string_operations.hpp>

// The text of your Inja templates:
#include "lux/cxx/dref/generator/static_meta.inja.hpp"
#include "lux/cxx/dref/generator/dynamic_meta.inja.hpp"
#include <lux/cxx/dref/generator/tools.hpp>

using inja_template_array = std::array<inja::Template, dynamic_meta_template_strs.size()>;

static std::string generateDynamicReflectionCode(inja::Environment& env, inja_template_array& dynamic_meta_template, nlohmann::json& data)
{
    std::stringstream ss;
	for (auto& meta_template : dynamic_meta_template)
	{
        ss << env.render(meta_template, data);
	}
	return ss.str();
}

static std::string generateStaticReflectionCode(inja::Environment& env, inja::Template& static_meta_template, nlohmann::json& data)
{
	return env.render(static_meta_template, data);
}

// -------------------------------------------------------------------------
// meta_generator()
// Example usage: 
//   meta_generator <config_json>
// -------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: meta_generator <config_json>\n";
        return 1;
    }

    Config config;
    try {
        loadConfig(argv[1], config);
    }
    catch (std::exception& e){
		std::cerr << "Parse config file failed!\n" << e.what() << std::endl;
        return 1;
    }

    for (const auto& file : config.target_files)
    {
        if (!std::filesystem::exists(file)) {
            std::cerr << "[Error] file_to_parse " << file << " does not exist.\n";
            return 1;
        }
    }
	
    if (!std::filesystem::exists(config.source_file)) {
        std::cerr << "[Error] source_file " << config.source_file << " does not exist or not matched.\n";
        std::cerr << "[Error] You can check your build system, and try to make sure use absolute path for source file.\n"
            "If you are using cmake, the possible format is ${CMAKE_CURRENT_SOURCE_DIR}/to/source_file.";

        return 1;
        // not strictly an error, but we can't find the compile entry
    }
    if (!std::filesystem::exists(config.compile_commands)) {
        std::cerr << "[Error] compile_commands " << config.compile_commands << " does not exist.\n";
        return 1;
    }

    // gather additional includes from compile_commands
    auto extra_includes     = fetchIncludePaths(config.compile_commands, config.source_file);
    auto source_file_path   = std::filesystem::path(config.source_file);
    auto source_file_parent = source_file_path.parent_path();
    if (source_file_parent != std::filesystem::path("."))
    {
        extra_includes.push_back(source_file_parent.string());
    }

	auto include_options = convertToDashI(extra_includes);

    // Build final clang parse options
    std::vector<std::string> options;
    options.push_back("--std=c++20");
    for (const auto& inc : include_options) {
        options.push_back(inc);  // e.g. "-ID:/some/include"
    }
    options.push_back("-D__LUX_PARSE_TIME__=1");

    inja::Environment   static_env;
	inja::Environment   dynamic_env;
    inja::Environment   register_file_env;
	inja::Template      static_meta_template;
    inja_template_array dynamic_meta_templates;
    inja::Template      register_file_template;

    try {
        static_meta_template  = static_env.parse(static_meta_template_str.data());
		for (size_t i = 0; i < dynamic_meta_templates.size(); i++)
		{
            dynamic_meta_templates[i] = dynamic_env.parse(dynamic_meta_template_strs[i].data());
		}
		register_file_template = register_file_env.parse(register_file_header.data());
	}
	catch (const std::exception& e) {
		std::cerr << "[Error] Parsing of template failed.\n" << e.what() << std::endl;
		return 1;
	}

    std::vector<std::string> file_hashs;
    std::unordered_set<std::string> global_filter;

    for (const auto& file : config.target_files)
    {
        auto dynamic_meta_generator = std::make_unique<DynamicMetaGenerator>();
        auto static_meta_generator  = std::make_unique<StaticMetaGenerator>();

		std::filesystem::path file_path = file;
        auto maybe_rel = findRelativeIncludePath(file_path, extra_includes);
        if (!maybe_rel.has_value())
        {
			std::cerr << "[Error] Cannot find relative path for " << file << "\n";
			return 1;
        }

        // Now parse the file
        lux::cxx::dref::CxxParser cxx_parser;
        auto [parse_rst, data] = cxx_parser.parse(
            file_path.string(),
            options,
            "test_unit",
            "1.0.0"
        );

        if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS) {
            std::cerr << "[Error] Parsing of " << file  << " failed.\n";
            return 1;
        }

        // Construct base filename from the file to parse
        auto base_name = file_path.filename().replace_extension("").string();

        for (auto& decl : data.markedDeclarations())
        {
			if (global_filter.find(decl->id) != global_filter.end()) {
				continue;
			}
			global_filter.insert(decl->id);

			decl->accept(static_meta_generator.get());
			decl->accept(dynamic_meta_generator.get());
        }

		nlohmann::json dyn_json;
		nlohmann::json static_json;
        auto file_hash = std::to_string(lux::cxx::algorithm::fnv1a(file_path.string()));
        file_hashs.push_back(std::move(file_hash));

		dynamic_meta_generator->toJsonFile(dyn_json);
        dyn_json["include_path"] = (*maybe_rel).string();
        dyn_json["file_hash"]    = file_hashs.back();
        
		static_meta_generator->toJsonFile(static_json);
        try {
			auto dynamic_render_rst = generateDynamicReflectionCode(dynamic_env, dynamic_meta_templates, dyn_json);
			auto static_render_rst  = generateStaticReflectionCode(static_env, static_meta_template, static_json);
            auto dyn_output_path    = std::filesystem::path(config.out_dir) / (base_name + ".meta.dynamic.cpp");
			auto static_output_path = std::filesystem::path(config.out_dir) / (base_name + ".meta.static.hpp");

            std::ofstream dyn_output(dyn_output_path, std::ios::binary);
			std::ofstream static_output(static_output_path, std::ios::binary);

            dyn_output << dynamic_render_rst;
            dyn_output.close();

			static_output << static_render_rst;
			static_output.close();
		}
		catch (std::exception& e) {
			std::cerr << "[Error] Rendering of template failed.\n" << e.what() << std::endl;
			return 1;
		}
    }

    try {
        nlohmann::json register_file_json;
        register_file_json["file_hashs"] = file_hashs;
        register_file_json["register_function_name"] = config.register_function_name;
        std::string rst = register_file_env.render(register_file_template, register_file_json);

        std::ofstream output_register_file(config.out_dir + "/" + config.register_header_name, std::ios::binary);
        output_register_file << rst;
        output_register_file.close();
	}
	catch (std::exception& e) {
		std::cerr << "[Error] Rendering of register file template failed.\n" << e.what() << std::endl;
		return 1;
	}

    return 0;
}
