#include "lux/cxx/dref/generator/GeneratorHelper.hpp"
#include "lux/cxx/dref/parser/CxxParser.hpp"
#include <iostream>
#include <inja/inja.hpp>
#include <fstream>
#include <filesystem>

using namespace ::lux::cxx::dref;

//---------------------------------------------------------------------
// validateFiles: Check that all necessary files exist
//
// This function verifies that every target file (to be parsed), the source file,
// and the compile commands file exist on the filesystem. It prints error messages
// if any of these files are missing. Note that the message for the source file also
// suggests that an absolute path is recommended (see the related helpers in GeneratorHelper).
//
// Returns:
//   true if all files exist, false otherwise.
//---------------------------------------------------------------------
static bool validateFiles(const GeneratorConfig& generator_config)
{
    // Iterate over each target file and verify that the file exists.
    for (const auto& file : generator_config.target_files)
    {
        if (!std::filesystem::exists(file))
        {
            std::cerr << "[Error] file_to_parse " << file << " does not exist.\n";
            return false;
        }
    }

    // Verify that the primary source file exists.
    if (!std::filesystem::exists(generator_config.source_file))
    {
        std::cerr << "[Error] source_file " << generator_config.source_file << " does not exist or not matched.\n";
        std::cerr << "[Error] You can check your build system, and try to make sure use absolute path for source file.\n";
        std::cerr << "[Error] If you are using cmake, the possible format is ${CMAKE_CURRENT_SOURCE_DIR}/to/source_file.\n";
        return false;
    }

    // Verify the existence of compile_commands file, which is necessary to extract include paths.
    if (!std::filesystem::exists(generator_config.compile_commands))
    {
        std::cerr << "[Error] compile_commands " << generator_config.compile_commands << " does not exist.\n";
        return false;
    }
    return true;
}

//---------------------------------------------------------------------
// buildCompileOptions: Construct the list of compile options
//
// This function builds a list of compilation options by fetching extra include paths from
// the compile_commands file using GeneratorHelper::fetchIncludePaths (which scans for "-I"
// and "-external:I" arguments) and by adding additional compile options provided in the config.
// It also adds default options such as the C++ standard and a custom preprocessor definition.
//
// Returns:
//   A vector of strings representing the compile options to be used by the parser.
//---------------------------------------------------------------------
static std::vector<std::string> buildCompileOptions(const GeneratorConfig& generator_config)
{
    // Fetch additional include paths from the compile commands based on the source file.
    auto extra_includes = GeneratorHelper::fetchIncludePaths(
        generator_config.compile_commands, generator_config.source_file
    );
    auto source_path = std::filesystem::path(generator_config.source_file);
    auto source_parent = source_path.parent_path();
    // Ensure that the parent directory of the source file is included.
    if (source_parent != std::filesystem::path("."))
    {
        extra_includes.push_back(source_parent.string());
    }
    // Convert the include paths into proper "-I" options.
    auto include_options = GeneratorHelper::convertToDashI(extra_includes);

    std::vector<std::string> options;
    // Preallocate the container's memory for efficiency.
    options.reserve(include_options.size() + generator_config.extra_compile_options.size() + 2);
    // Add a flag for C++20 standard.
    options.push_back("--std=c++20");
    // Define a macro to enable parse-time facilities (specific to this project).
    options.push_back("-D__LUX_PARSE_TIME__=1");

    // Append each include directory flag.
    for (const auto& inc : include_options) {
        options.push_back(inc);
    }
    // Append additional compile options provided in the configuration.
    for (const auto& opt : generator_config.extra_compile_options) {
        options.push_back(opt);
    }
    return options;
}

//---------------------------------------------------------------------
// processTargetFile: Parse a single target file and generate its meta JSON
//
// This function takes a target file and uses a CxxParser instance to parse the file.
// It sets up parsing options that include command line options, a marker symbol, and other metadata.
// If parsing is successful, it serializes the resulting metadata into JSON form,
// optionally writing it out to a separate JSON file if serial_meta is enabled in the configuration.
// The meta unit (parsed structure) and its JSON representation are appended to the provided lists.
//
// Parameters:
//   file           - The file path to parse.
//   generator_config - The configuration settings for the generator.
//   options        - The compile options needed for parsing.
//   meta_list      - Vector to which the parsed MetaUnit will be added.
//   meta_json_list - Vector to which the JSON representation of the meta data is added.
//
// Returns:
//   true if the file is processed successfully, false otherwise.
//---------------------------------------------------------------------
static bool processTargetFile(const std::filesystem::path& file,
    const GeneratorConfig& generator_config,
    const std::vector<std::string>& options,
    std::vector<MetaUnit>& meta_list,
    std::vector<nlohmann::json>& meta_json_list)
{
    // Convert the current file path to a string.
    std::string file_path = file.string();
    // Obtain the parent directory of the file. This information is later stored in the meta JSON.
    auto source_parent = file.parent_path().string();

    // Set up the parsing options needed by the C++ parser.
    ParseOptions parse_options;
    parse_options.commands = options;                  // Command-line compile options.
    parse_options.marker_symbol = generator_config.marker;  // Marker used in the source to denote declarations.
    parse_options.name = file_path;                // The name of the file to parse.
    parse_options.pch_file = "";                       // Placeholder: PCH support to be added later.
    parse_options.version = "1.0.0";                  // Placeholder: version support to be added later.

    // Create a parser instance with the provided options.
    auto cxx_parser = std::make_unique<CxxParser>(parse_options);

    // Parse the target file.
    auto [parse_rst, data] = cxx_parser->parse(file_path);
    if (parse_rst != EParseResult::SUCCESS)
    {
        std::cerr << "[Error] Parsing of " << file_path << " failed.\n";
        return false;
    }

    // Convert the parsed data to JSON format.
    auto meta_json = data.toJson();
    // Augment the JSON with additional file-specific metadata.
    meta_json["source_path"] = file_path;
    meta_json["source_parent"] = source_parent;
    meta_json["parser_compile_options"] = options;

    // If the configuration requests serializing meta data into JSON files,
    // generate a JSON file for the current source file.
    if (generator_config.serial_meta)
    {
        // Convert the JSON data to a string.
        auto meta_json_str = nlohmann::to_string(meta_json);
        // Build the output file name based on the target file's stem (filename without extension).
        auto json_file_name = std::filesystem::path(file).stem().string() + ".json";
        // Combine with the output directory from the configuration.
        auto out_path = std::filesystem::path(generator_config.out_dir) / json_file_name;

        std::ofstream out_file(out_path, std::ios::binary);
        if (!out_file.is_open()) {
            std::cerr << "[Error] Failed to open file " << out_path << " for writing.\n";
            return false;
        }
        // Write the meta JSON string to the file.
        out_file << meta_json_str;
        out_file.close();
    }

    // Save the parsed meta unit and its JSON representation for later use in template rendering.
    meta_list.push_back(std::move(data));
    meta_json_list.push_back(std::move(meta_json));

    return true;
}

//---------------------------------------------------------------------
// renderTemplates: Render output files using the provided template
//
// This function uses the inja template engine to render output files for each parsed meta JSON.
// It loads the template file from disk and sets up callback functions which are invoked from the template.
// These callbacks (decl_from_id, decl_from_index, type_from_id) let the template access specific parts
// of the metadata (such as declarations and types) by ID or index.
// Finally, for each meta JSON object, the rendered output is written to a new file in the designated output directory.
//
// Parameters:
//   generator_config  - The configuration settings.
//   meta_unit_list    - Vector of parsed meta units.
//   meta_json_list    - Vector of JSON representations for the parsed meta data.
//
// Returns:
//   true if all templates are rendered and written successfully, false otherwise.
//---------------------------------------------------------------------
static bool renderTemplates(const GeneratorConfig& generator_config,
    const std::vector<MetaUnit>& meta_unit_list,
    const std::vector<nlohmann::json>& meta_json_list)
{
    // Open the template file provided in the configuration.
    std::ifstream     template_file(generator_config.template_path);
    inja::Environment inja_env;
    inja::Template    inja_template;
    size_t i = 0; // Index used to correlate meta data with files

    if (!template_file.is_open()) {
        std::cerr << "[Error] Failed to open template file " << generator_config.template_path << "\n";
        return false;
    }
    // Read the entire template file into a string.
    std::string template_str(
        (std::istreambuf_iterator<char>(template_file)),
        std::istreambuf_iterator<char>()
    );

    // Callback to retrieve a declaration based on its unique ID from the meta data.
    // The meta_unit_list is used to locate the declaration, and then the corresponding JSON data is returned.
    inja_env.add_callback(
        "decl_from_id",
        [&meta_unit_list, &meta_json_list, &i](const inja::Arguments& args) -> nlohmann::json {
            auto& meta_json = meta_json_list[i];
            auto& meta_unit = meta_unit_list[i];
            auto id = args.at(0)->get<std::string>();
            auto decl = meta_unit.findDeclById(id);
            if (!decl) {
                std::cerr << "[Error] Declaration with id " << id << " not found.\n";
                throw std::runtime_error("Declaration not found");
            }
            return meta_json["declarations"][decl->index];
        }
    );

    // Callback to retrieve a declaration based solely on its index in the meta JSON array.
    inja_env.add_callback(
        "decl_from_index",
        [&meta_json_list, &i](const inja::Arguments& args) -> nlohmann::json {
            auto& meta_json = meta_json_list[i];
            auto  index = args.at(0)->get<size_t>();
            return meta_json["declarations"][index];
        }
    );

    // Callback to retrieve type information based on its unique ID.
    // It looks up the type from meta_unit_list and returns the associated JSON data.
    inja_env.add_callback(
        "type_from_id",
        [&meta_unit_list, &meta_json_list, &i](const inja::Arguments& args) -> nlohmann::json {
            auto& meta_json = meta_json_list[i];
            auto& meta_unit = meta_unit_list[i];
            auto index = args.at(0)->get<std::string>();
            auto type = meta_unit.findTypeById(index);
            if (!type) {
                std::cerr << "[Error] Type with id " << index << " not found.\n";
                throw std::runtime_error("Type not found");
            }
            return meta_json["types"][type->index];
        }
    );

    // Iterate over each meta JSON object.
    for (auto& meta_json : meta_json_list)
    {
        // Derive the source file's base name (without extension) to be used for the output file.
        auto source_file_name = std::filesystem::path(meta_json["source_path"].get<std::string>()).stem().string();
        // Construct the output file path by combining the output directory with the source filename and meta suffix.
        auto meta_file_path = std::filesystem::path(generator_config.out_dir) / (source_file_name + generator_config.meta_suffix);
        std::ofstream out_file(meta_file_path, std::ios::binary);
        if (!out_file.is_open()) {
            std::cerr << "[Error] Failed to open file " << meta_file_path << " for writing.\n";
            return false;
        }

        try {
            // Parse the template string.
            inja_template = inja_env.parse(template_str);
            // Render the template using the current meta JSON data.
            auto render_rst = inja_env.render(inja_template, meta_json);
            out_file << render_rst;
            if (out_file.fail()) {
                std::cerr << "[Error] Failed to write to file " << meta_file_path << "\n";
                return false;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[Error] Failed to write to file " << meta_file_path << ": " << e.what() << "\n";
            return false;
        }
        out_file.close();
        // Increment the index to process the next meta JSON object.
        i++;
    }
    return true;
}

//---------------------------------------------------------------------
// main: Program entry point
//
// This is the main function that orchestrates the entire metadata generation process.
// It expects a JSON configuration file as a command-line argument.
// The process includes loading the configuration, validating files, building compile options,
// parsing target source files, optionally writing out intermediate meta JSON files (dry run mode),
// and finally rendering the final output using template files.
//
// Returns:
//   0 on success, 1 on errors.
//---------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Check that a configuration file is provided.
    if (argc < 2) {
        std::cerr << "Usage: meta_generator <config_json>\n";
        return 1;
    }

    GeneratorConfig generator_config;
    try {
        // Load the generator configuration from the JSON file.
        // The helper function GeneratorHelper::loadGeneratorConfig reads various settings such as:
        // marker, template_path, out_dir, compile_commands, target_files, meta_suffix, source_file,
        // and extra compile options.
        GeneratorHelper::loadGeneratorConfig(argv[1], generator_config);
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to load config: " << e.what() << "\n";
        return 1;
    }

    // Ensure all required files exist.
    if (!validateFiles(generator_config)) {
        return 1;
    }

    // Check if the output directory exists, and attempt to create it if it does not.
    if (!std::filesystem::exists(generator_config.out_dir)) {
        if (!std::filesystem::create_directories(generator_config.out_dir))
        {
            std::cerr << "[Error] Failed to create output directory " << generator_config.out_dir << "\n";
            return 1;
        }
    }

    // Build compile options (including include paths) to be passed to the parser.
    auto options = buildCompileOptions(generator_config);
    std::vector<MetaUnit>       meta_unit_list;
    std::vector<nlohmann::json> meta_json_list;

    // Process each target file by parsing and collecting metadata.
    for (const auto& file : generator_config.target_files)
    {
        if (!processTargetFile(file, generator_config, options, meta_unit_list, meta_json_list)) {
            return 1;
        }
    }

    // If the configuration is set to dry_run, do not generate final output files.
    if (generator_config.dry_run)
    {
        std::cout << "[Info] Dry run, no output will be generated.\n";
        return 0;
    }

    // Render the final output files using the previously collected metadata and the specified template.
    if (!renderTemplates(generator_config, meta_unit_list, meta_json_list)) {
        return 1;
    }

    return 0;
}
