#include <lux/cxx/reflection/generator/GeneratorHelper.hpp>
#include <lux/cxx/reflection/parser/CxxParser.hpp>

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool verifyClangSystemIncludes(const std::filesystem::path& root)
    {
        using namespace lux::cxx::reflection;
        const auto regular = root / "regular";
        const auto system = root / "system headers";
        std::filesystem::create_directories(regular);
        std::filesystem::create_directories(system);
        const auto source = root / "clang.cpp";
        const auto header = root / "clang.hpp";
        const auto database = root / "clang_commands.json";
        std::ofstream(source) << "// clang-cl include extraction\n";
        std::ofstream(regular / "priority.hpp") << "#define INCLUDE_PRIORITY 1\n";
        std::ofstream(system / "priority.hpp") << "#error system header shadowed the regular header\n";
        std::ofstream(system / "dependency.hpp") << "struct Dependency { int value; };\n";
        std::ofstream(header) << R"cpp(
#include "priority.hpp"
#include "dependency.hpp"
static_assert(INCLUDE_PRIORITY == 1);
struct __attribute__((annotate("fixture::type"))) ClangLayout { Dependency value; };
)cpp";
        for (const bool joined : {false, true})
        {
            for (const bool string_command : {false, true})
            {
                nlohmann::json command{
                    {"directory", root.generic_string()}, {"file", source.generic_string()}
                };
                if (string_command)
                {
                    const auto option = joined ? "-imsvc\"" : "-imsvc \"";
                    command["command"] = std::string{"clang-cl.exe "} + option + system.generic_string() +
                        "\" -I\"" + regular.generic_string() + "\" -imsvc \"" + system.generic_string() +
                        "\" /c \"" + source.generic_string() + "\"";
                }
                else
                {
                    std::vector<std::string> arguments{"clang-cl.exe"};
                    if (joined)
                    {
                        arguments.push_back("-imsvc" + system.generic_string());
                    }
                    else
                    {
                        arguments.insert(arguments.end(), {"-imsvc", system.generic_string()});
                    }
                    arguments.insert(
                        arguments.end(),
                        {"-I" + regular.generic_string(), "-imsvc", system.generic_string(), "/c", source.generic_string()}
                    );
                    command["arguments"] = std::move(arguments);
                }
                std::ofstream(database) << nlohmann::json::array({command}).dump();
                const auto includes = GeneratorHelper::fetchIncludePaths(database, source);
                const std::vector<std::filesystem::path> expected{regular, system};
                if (!includes || *includes != expected)
                {
                    std::cerr << "clang-cl system include extraction failed: joined=" << joined
                              << ", command=" << string_command << '\n';
                    return false;
                }
                ParseOptions parse;
                parse.marker_symbol = "fixture";
                parse.commands = GeneratorHelper::convertToDashI(*includes);
                std::set<std::filesystem::path> dependencies;
                parse.on_included_file = [&](std::string_view file) {
                    dependencies.insert(std::filesystem::weakly_canonical(file));
                };
                CxxParser parser(std::move(parse));
                const auto result = parser.parse(header.generic_string());
                const bool parsed = result.first == EParseResult::SUCCESS;
                const auto dependency = std::filesystem::weakly_canonical(system / "dependency.hpp");
                const auto regular_header = std::filesystem::weakly_canonical(regular / "priority.hpp");
                const bool has_dependency = dependencies.contains(dependency);
                const bool has_regular = dependencies.contains(regular_header);
                const bool is_invalid_parse = !parsed || !has_dependency || !has_regular;
                if (is_invalid_parse)
                {
                    return false;
                }
            }
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    using namespace lux::cxx::reflection;
    if (argc != 2) return 1;
    const auto root = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(root / "included");
    const auto source = root / "consumer.cpp";
    const auto database = root / "compile_commands.json";
    const auto included = root / "included" / "layout.hpp";
    const auto header = root / "contract.hpp";
    std::ofstream(source) << "// compilation environment selection\n";
    std::ofstream(included) << "#define INCLUDED_SENTINEL 1\n";
    std::ofstream(header) << R"cpp(
#include "layout.hpp"
#if CODEGEN_SENTINEL != 73 || !INCLUDED_SENTINEL
#error compile environment lost
#endif
struct __attribute__((annotate("fixture::type"))) Layout { char tag; int value; };
static_assert(sizeof(Layout) == 5);
)cpp";
    nlohmann::json command{
        {"directory", root.generic_string()}, {"file", "consumer.cpp"},
        {"arguments", {"cl.exe", "/D", "CODEGEN_SENTINEL=73", "/Iincluded", "/Zp1", "/std:c++20", "/Zc:externConstexpr",
            "/c", "consumer.cpp", "/Founused.obj"}}
    };
    const auto save = [&](const nlohmann::json& entries) { std::ofstream(database) << entries.dump(); };
    save(nlohmann::json::array({command}));
    auto options = GeneratorHelper::fetchCompileOptions(database, source);
    auto includes = GeneratorHelper::fetchIncludePaths(database, source);
    if (!options || !includes) return 2;
    // Same relative-file resolution must be used for options and includes.
    command["file"] = source.generic_string();
    save(nlohmann::json::array({command}));
    includes = GeneratorHelper::fetchIncludePaths(database, source);
    if (!includes || includes->size() != 1U) return 3;
    if (std::ranges::find(*options, "-DCODEGEN_SENTINEL=73") == options->end() ||
        std::ranges::find(*options, "-fpack-struct=1") == options->end()) return 4;
    const auto paths = GeneratorHelper::convertToDashI(*includes);
    options->insert(options->end(), paths.begin(), paths.end());
    ParseOptions parse;
    parse.marker_symbol = "fixture";
    parse.commands = *options;
    std::set<std::filesystem::path> dependencies;
    parse.on_included_file = [&](std::string_view file) { dependencies.insert(std::filesystem::weakly_canonical(file)); };
    CxxParser parser(std::move(parse));
    parser.setOnParseError([](const std::string& message) { std::cerr << message << '\n'; });
    if (parser.parse(header.generic_string()).first != EParseResult::SUCCESS) return 5;
    if (!dependencies.contains(std::filesystem::weakly_canonical(included))) return 6;
    std::ofstream(included) << "#error deliberately invalid dependency\n";
    if (parser.parse(header.generic_string()).first != EParseResult::FAILED) return 7;
    if (GeneratorHelper::fetchCompileOptions(database, root / "missing.cpp")) return 8;
    save(nlohmann::json::array({command, command}));
    if (GeneratorHelper::fetchCompileOptions(database, source)) return 9;
    command["output"] = "CMakeFiles/wanted.dir/consumer.cpp.obj";
    auto other = command;
    other["output"] = "CMakeFiles/other.dir/consumer.cpp.obj";
    other["arguments"] = {"cl.exe", "/DCODEGEN_SENTINEL=11"};
    save(nlohmann::json::array({other, command}));
    const auto selected = GeneratorHelper::fetchCompileOptions(database, source, "wanted");
    if (!selected || std::ranges::find(*selected, "-DCODEGEN_SENTINEL=73") == selected->end()) return 11;
    if (GeneratorHelper::fetchCompileOptions(database, source, "absent")) return 12;
    command["arguments"] = {"cl.exe", "@hidden.rsp"};
    save(nlohmann::json::array({command}));
    if (GeneratorHelper::fetchCompileOptions(database, source)) return 10;
    if (!verifyClangSystemIncludes(root / "clang fixture")) return 13;
    std::cout << "compile options, transitive includes and failed parse: PASS\n";
}
