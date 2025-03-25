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

#include <lux/cxx/dref/generator/Generator.hpp>

#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/parser/Declaration.hpp>
#include <lux/cxx/dref/parser/Type.hpp>
#include <lux/cxx/algotithm/hash.hpp>
#include <lux/cxx/algotithm/string_operations.hpp>

// The text of your Inja templates:
#include "static_meta.inja.hpp"
#include "dynamic_meta.inja.hpp"
#include <lux/cxx/dref/generator/tools.hpp>

struct GeneratorConfig
{
    std::filesystem::path target_dir;
    std::string           file_name;
    std::string           file_hash;
    std::string           relative_include;
    std::string           static_meta_suffix;
    std::string           dynamic_meta_suffix;
};

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
		std::cerr << e.what() << std::endl;
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
        std::cerr << "[Warn] source_file " << config.source_file << " does not exist or not matched.\n";
        // not strictly an error, but we can't find the compile entry
    }
    if (!std::filesystem::exists(config.compile_commands)) {
        std::cerr << "[Error] compile_commands " << config.compile_commands << " does not exist.\n";
        return 1;
    }

    // gather additional includes from compile_commands
    auto extra_includes = fetchIncludePaths(config.compile_commands, config.source_file);
	auto include_options = convertToDashI(extra_includes);

	auto dynamic_meta_generator = std::make_unique<DynamicMetaGenerator>();
	auto static_meta_generator  = std::make_unique<StaticMetaGenerator>();

    // Build final clang parse options
    std::vector<std::string> options;
    options.push_back("--std=c++20");
    for (const auto& inc : include_options) {
        options.push_back(inc);  // e.g. "-ID:/some/include"
    }

    for (const auto& file : config.target_files)
    {
		std::filesystem::path file_path = file;
        auto maybeRel = findRelativeIncludePath(file_path, extra_includes);
        if (!maybeRel)
        {
            std::cerr << "[Error] Could not find a relative include path for " << file << "\n";
            return 1;
        }

        // Now parse the file
        lux::cxx::dref::CxxParser cxx_parser;
        auto [parse_rst, data] = cxx_parser.parse(
            file_path.string(),
            std::move(options),
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
			decl->accept(static_meta_generator.get());
			decl->accept(dynamic_meta_generator.get());
        }
    }


    /*
    // Setup generator config
    GeneratorConfig config;
    config.target_dir = out_dir;
    config.file_name = base_name;
    config.file_hash = file_hash;
    config.relative_include = lux::cxx::algorithm::replace((*maybeRel).string(), "\\", "/");
    config.static_meta_suffix = ".meta.static.hpp";
    config.dynamic_meta_suffix = ".meta.dynamic.cpp";

    // Create the meta generator
    MetaGenerator generator(config);
    auto gen_result = generator.generate(data);
    if (gen_result != MetaGenerator::EGenerateError::SUCCESS) {
        return 1;
    }
    */

    return 0;
}
