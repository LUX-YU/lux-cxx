#include "lux/cxx/dref/generator/GeneratorHelper.hpp"
#include "lux/cxx/dref/parser/CxxParser.hpp"
#include "lux/cxx/dref/Error.hpp"
#include <iostream>
#include <inja/inja.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <string_view>

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
    bool ok = true;

    // Iterate over each target file and verify that the file exists.
    for (const auto& file : generator_config.target_files)
    {
        if (!std::filesystem::exists(file))
        {
            std::cerr << "[Error] file_to_parse " << file << " does not exist.\n";
            ok = false;
        }
    }

    // Verify that the primary source file exists.
    if (!std::filesystem::exists(generator_config.source_file))
    {
        std::cerr << "[Error] source_file " << generator_config.source_file << " does not exist or not matched.\n";
        std::cerr << "[Error] You can check your build system, and try to make sure use absolute path for source file.\n";
        std::cerr << "[Error] If you are using cmake, the possible format is ${CMAKE_CURRENT_SOURCE_DIR}/to/source_file.\n";
        ok = false;
    }

    // Verify the existence of compile_commands file, which is necessary to extract include paths.
    if (!std::filesystem::exists(generator_config.compile_commands))
    {
        std::cerr << "[Error] compile_commands " << generator_config.compile_commands << " does not exist.\n";
        ok = false;
    }

    return ok;
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
static std::vector<std::string> buildCompileOptions(const GeneratorConfig& generator_config, std::vector<std::filesystem::path>& includes)
{
    auto source_path = std::filesystem::path(generator_config.source_file);
    auto source_parent = source_path.parent_path();
    // Ensure that the parent directory of the source file is included.
    if (source_parent != std::filesystem::path("."))
    {
        includes.push_back(source_parent.string());
    }
    // Convert the include paths into proper "-I" options.
    auto include_options = GeneratorHelper::convertToDashI(includes);

    std::vector<std::string> options;
    // Preallocate the container's memory for efficiency.
    options.reserve(include_options.size() + generator_config.extra_compile_options.size() + generator_config.preprocessor_defines.size() + 2);
    // Add a flag for C++ standard (configurable, defaults to c++20).
    options.push_back("--std=" + generator_config.cxx_standard);
    // Add preprocessor defines from configuration.
    for (const auto& define : generator_config.preprocessor_defines) {
        options.push_back("-D" + define);
    }

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
    std::vector<nlohmann::json>& meta_json_list,
    std::vector<std::filesystem::path>& includes)
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
    // Route all libclang diagnostics and parser-level errors to stderr.
    cxx_parser->setOnParseError([&file_path](const std::string& msg) {
        std::cerr << "[Parser] " << file_path << ": " << msg << "\n";
    });

    // Parse the target file.
    auto [parse_rst, data] = cxx_parser->parse(file_path);
    if (parse_rst != EParseResult::SUCCESS)
    {
        std::cerr << "[Error] Parsing of '" << file_path << "' failed"
                  << (parse_rst == EParseResult::UNKNOWN_TYPE
                      ? " (one or more declarations have unsupported cursor kinds)" : "")
                  << ".\n";
        return false;
    }

    // Convert the parsed data to JSON format.
    auto meta_json = data.toJson();
    // Augment the JSON with additional file-specific metadata.
    meta_json["source_path"]            = file_path;
    meta_json["source_parent"]          = source_parent;
    meta_json["parser_compile_options"] = options;
	meta_json["include_dir"]            = GeneratorHelper::findRelativeIncludePath(file, includes).value_or(std::string(""));

    if (!generator_config.custom_fields_json.empty()) {
        for (const std::string& field : generator_config.custom_fields_json) {
            nlohmann::json extra;
            try {
                extra = nlohmann::json::parse(field);
            }
            catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[Error] Invalid custom_fields_json entry '" << field
                          << "': " << e.what() << "\n";
                return false;
            }
            for (auto& [key, val] : extra.items()) {
                meta_json[key] = val;
            }
		}
    }

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

static bool dfsFindChain(const CXXRecordDecl* current,
    std::string_view            target_name,
    std::vector<const CXXRecordDecl*>& path,
    std::unordered_set<const CXXRecordDecl*>& visited,
    const MetaUnit& meta_unit)
{
    if (!current || visited.count(current)) return false;
    visited.insert(current);

    path.push_back(current);
    if (current->name == target_name ||
        current->full_qualified_name == target_name) {
        return true;
    }

    for (auto base_idx : current->bases) {
        auto* base = meta_unit.getDeclAs<CXXRecordDecl>(base_idx);
        if (dfsFindChain(base, target_name, path, visited, meta_unit)) {
            return true;
        }
    }

    path.pop_back();
    return false;
}

/**
 * find_parent_chain(E, "A")  ⇒  {E, D, C, A}
 *
 * @param start
 * @param target_name
 * @return
 */
static std::vector<const CXXRecordDecl*> find_parent_chain(const CXXRecordDecl* start, std::string_view target_name, const MetaUnit& meta_unit)
{
    std::vector<const CXXRecordDecl*> path;
    std::unordered_set<const CXXRecordDecl*> visited;

    if (dfsFindChain(start, target_name, path, visited, meta_unit)) {
        return path; 
    }
    return {};
}

struct ParsedAnnotation
{
    std::string raw{};
    std::string head{};
    nlohmann::json args = nlohmann::json::object();
};

static std::string trim_copy(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }
    return std::string(text);
}

static std::vector<std::string> split_csv_like(std::string_view text, char delimiter)
{
    std::vector<std::string> tokens;
    std::string current;
    current.reserve(text.size());

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (char ch : text)
    {
        if (escaped)
        {
            current.push_back(ch);
            escaped = false;
            continue;
        }

        if (ch == '\\')
        {
            escaped = true;
            current.push_back(ch);
            continue;
        }

        if (ch == '\'' && !in_double_quote)
        {
            in_single_quote = !in_single_quote;
            current.push_back(ch);
            continue;
        }

        if (ch == '"' && !in_single_quote)
        {
            in_double_quote = !in_double_quote;
            current.push_back(ch);
            continue;
        }

        if (ch == delimiter && !in_single_quote && !in_double_quote)
        {
            auto token = trim_copy(current);
            if (!token.empty())
            {
                tokens.push_back(std::move(token));
            }
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    auto token = trim_copy(current);
    if (!token.empty())
    {
        tokens.push_back(std::move(token));
    }

    return tokens;
}

static nlohmann::json parse_annotation_value(std::string_view raw)
{
    std::string value = trim_copy(raw);
    if (value.empty())
    {
        return "";
    }

    if (value.size() >= 2)
    {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
        {
            return value.substr(1, value.size() - 2);
        }
    }

    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "true")
    {
        return true;
    }
    if (lower == "false")
    {
        return false;
    }

    errno = 0;
    char* end_i = nullptr;
    const long long as_int = std::strtoll(value.c_str(), &end_i, 10);
    if (end_i != value.c_str() && end_i && *end_i == '\0' && errno != ERANGE)
    {
        return as_int;
    }

    errno = 0;
    char* end_d = nullptr;
    const double as_double = std::strtod(value.c_str(), &end_d);
    if (end_d != value.c_str() && end_d && *end_d == '\0' && errno != ERANGE)
    {
        return as_double;
    }

    return value;
}

static ParsedAnnotation parse_annotation(std::string_view annotation_text)
{
    ParsedAnnotation result{};
    result.raw = trim_copy(annotation_text);
    if (result.raw.empty())
    {
        return result;
    }

    auto tokens = split_csv_like(result.raw, ',');
    if (tokens.empty())
    {
        return result;
    }

    auto parse_kv_token = [&result](const std::string& token, bool is_head) {
        const auto eq_pos = token.find('=');
        if (eq_pos == std::string::npos)
        {
            if (is_head)
            {
                result.head = trim_copy(token);
            }
            else
            {
                const std::string key = trim_copy(token);
                if (!key.empty())
                {
                    result.args[key] = true;
                }
            }
            return;
        }

        const std::string key = trim_copy(token.substr(0, eq_pos));
        const std::string value = trim_copy(token.substr(eq_pos + 1));
        if (key.empty())
        {
            return;
        }
        if (is_head)
        {
            result.head = key;
        }
        result.args[key] = parse_annotation_value(value);
    };

    parse_kv_token(tokens[0], true);
    for (size_t i = 1; i < tokens.size(); ++i)
    {
        parse_kv_token(tokens[i], false);
    }

    if (result.head.empty())
    {
        result.head = result.raw;
    }
    return result;
}

static std::vector<std::string> extract_annotation_texts(const nlohmann::json& input)
{
    std::vector<std::string> annotations{};
    if (input.is_null())
    {
        return annotations;
    }

    if (input.is_object())
    {
        auto it = input.find("attributes");
        if (it == input.end() || !it->is_array())
        {
            return annotations;
        }
        for (const auto& item : *it)
        {
            if (item.is_string())
            {
                annotations.push_back(item.get<std::string>());
            }
        }
        return annotations;
    }

    if (input.is_array())
    {
        for (const auto& item : input)
        {
            if (item.is_string())
            {
                annotations.push_back(item.get<std::string>());
            }
        }
        return annotations;
    }

    if (input.is_string())
    {
        annotations.push_back(input.get<std::string>());
    }

    return annotations;
}

static std::vector<ParsedAnnotation> parse_annotation_set(const nlohmann::json& input)
{
    auto raw_annotations = extract_annotation_texts(input);
    std::vector<ParsedAnnotation> parsed{};
    parsed.reserve(raw_annotations.size());
    for (const auto& raw : raw_annotations)
    {
        parsed.push_back(parse_annotation(raw));
    }
    return parsed;
}

static nlohmann::json annotation_map_all(const std::vector<ParsedAnnotation>& parsed)
{
    nlohmann::json map = nlohmann::json::object();
    for (const auto& ann : parsed)
    {
        for (auto it = ann.args.begin(); it != ann.args.end(); ++it)
        {
            map[it.key()] = it.value();
        }
    }
    return map;
}

static nlohmann::json annotation_map_for_head(
    const std::vector<ParsedAnnotation>& parsed,
    std::string_view target_head)
{
    nlohmann::json map = nlohmann::json::object();
    for (const auto& ann : parsed)
    {
        if (ann.head != target_head)
        {
            continue;
        }
        for (auto it = ann.args.begin(); it != ann.args.end(); ++it)
        {
            map[it.key()] = it.value();
        }
    }
    return map;
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
            auto id = args.at(0)->get<std::string>();
            auto type = meta_unit.findTypeById(id);
            if (!type) {
                std::cerr << "[Error] Type with id " << id << " not found.\n";
                throw std::runtime_error("Type not found");
            }
            return meta_json["types"][type->index];
        }
    );

    inja_env.add_callback(
        "parent_chain",
        [&meta_unit_list, &meta_json_list, &i](const inja::Arguments& args) -> nlohmann::json
        {
            auto& meta_json = meta_json_list[i];
            auto& meta_unit = meta_unit_list[i];
            auto id     = args.at(0)->get<std::string>();
			auto target = args.at(1)->get<std::string>();
            auto decl = meta_unit.findDeclById(id);
            if (!decl) {
                std::cerr << "[Error] parent_chain: declaration id='" << id
                          << "' not found in '" << meta_json["source_path"] << "'.\n";
                throw std::runtime_error("parent_chain: declaration not found: " + id);
            }
            if (decl->kind != EDeclKind::CXX_RECORD_DECL)
            {
                std::cerr << "[Error] parent_chain: id='" << id
                          << "' is not a CXX_RECORD_DECL (kind="
                          << static_cast<int>(decl->kind) << ") in '"
                          << meta_json["source_path"] << "'.\n";
                throw std::runtime_error("parent_chain: not a CXX_RECORD_DECL");
            }
            auto decl_list = find_parent_chain(static_cast<const CXXRecordDecl*>(decl), target, meta_unit);
			nlohmann::json ret = nlohmann::json::array();
            for (auto decl : decl_list)
            {
				nlohmann::json info = {
					{"id", decl->id},
					{"hash", std::to_string(decl->hash)}
				};
                ret.push_back(info);
            }
			return ret;
        }
    );

    // Returns annotation strings from a declaration (or raw attributes input).
    inja_env.add_callback(
        "annotation_list",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.empty())
            {
                return nlohmann::json::array();
            }
            const auto raw = extract_annotation_texts(*args.at(0));
            nlohmann::json out = nlohmann::json::array();
            for (const auto& attr : raw)
            {
                out.push_back(attr);
            }
            return out;
        }
    );

    // Returns annotation heads (first token before comma).
    inja_env.add_callback(
        "annotation_heads",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.empty())
            {
                return nlohmann::json::array();
            }
            const auto parsed = parse_annotation_set(*args.at(0));
            nlohmann::json out = nlohmann::json::array();
            for (const auto& ann : parsed)
            {
                out.push_back(ann.head);
            }
            return out;
        }
    );

    // Checks whether a declaration has a given annotation symbol.
    inja_env.add_callback(
        "annotation_has",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 2)
            {
                return false;
            }
            const auto target = trim_copy(args.at(1)->get<std::string>());
            const auto parsed = parse_annotation_set(*args.at(0));
            for (const auto& ann : parsed)
            {
                if (ann.raw == target || ann.head == target || ann.args.contains(target))
                {
                    return true;
                }
            }
            return false;
        }
    );

    // Returns merged key/value map of all annotations.
    inja_env.add_callback(
        "annotation_map",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.empty())
            {
                return nlohmann::json::object();
            }
            const auto parsed = parse_annotation_set(*args.at(0));
            return annotation_map_all(parsed);
        }
    );

    // Returns merged key/value map for a specific annotation head.
    inja_env.add_callback(
        "annotation_map_for",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 2)
            {
                return nlohmann::json::object();
            }
            const auto parsed = parse_annotation_set(*args.at(0));
            const auto head = trim_copy(args.at(1)->get<std::string>());
            return annotation_map_for_head(parsed, head);
        }
    );

    // Returns a value for key across all annotations; null if not found.
    inja_env.add_callback(
        "annotation_get",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 2)
            {
                return nullptr;
            }
            const auto key = trim_copy(args.at(1)->get<std::string>());
            const auto map = annotation_map_all(parse_annotation_set(*args.at(0)));
            auto it = map.find(key);
            if (it == map.end())
            {
                return nullptr;
            }
            return *it;
        }
    );

    // Returns a value for key across all annotations; default when missing.
    inja_env.add_callback(
        "annotation_get_or",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 3)
            {
                return nullptr;
            }
            const auto key = trim_copy(args.at(1)->get<std::string>());
            const auto map = annotation_map_all(parse_annotation_set(*args.at(0)));
            auto it = map.find(key);
            if (it == map.end())
            {
                return *args.at(2);
            }
            return *it;
        }
    );

    // Returns a value for key inside a specific annotation head; null if missing.
    inja_env.add_callback(
        "annotation_get_for",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 3)
            {
                return nullptr;
            }
            const auto head = trim_copy(args.at(1)->get<std::string>());
            const auto key = trim_copy(args.at(2)->get<std::string>());
            const auto map = annotation_map_for_head(parse_annotation_set(*args.at(0)), head);
            auto it = map.find(key);
            if (it == map.end())
            {
                return nullptr;
            }
            return *it;
        }
    );

    // Returns a value for key inside a specific annotation head; default when missing.
    inja_env.add_callback(
        "annotation_get_for_or",
        [](const inja::Arguments& args) -> nlohmann::json
        {
            if (args.size() < 4)
            {
                return nullptr;
            }
            const auto head = trim_copy(args.at(1)->get<std::string>());
            const auto key = trim_copy(args.at(2)->get<std::string>());
            const auto map = annotation_map_for_head(parse_annotation_set(*args.at(0)), head);
            auto it = map.find(key);
            if (it == map.end())
            {
                return *args.at(3);
            }
            return *it;
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
    // Fetch additional include paths from the compile commands based on the source file.
    auto includes_result = GeneratorHelper::fetchIncludePaths(
        generator_config.compile_commands, generator_config.source_file
    );
    if (!includes_result) {
        std::cerr << includes_result.error().message << "\n";
        return 1;
    }
    auto extra_includes = std::move(includes_result.value());
    auto options = buildCompileOptions(generator_config, extra_includes);
    std::vector<MetaUnit>       meta_unit_list;
    std::vector<nlohmann::json> meta_json_list;

    // Process each target file by parsing and collecting metadata.
    for (const auto& file : generator_config.target_files)
    {
        if (!processTargetFile(file, generator_config, options, meta_unit_list, meta_json_list, extra_includes)) {
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
