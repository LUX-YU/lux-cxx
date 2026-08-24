#include "lux/cxx/reflection/generator/GeneratorHelper.hpp"
#include "lux/cxx/reflection/parser/CxxParser.hpp"
#include "lux/cxx/reflection/Error.hpp"
#include "lux/cxx/reflection/runtime/MetaIrJson.hpp"
#include <iostream>
#include <inja/inja.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <set>
#include <string_view>

using namespace ::lux::cxx::reflection;

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
struct PendingOutput final
{
    std::filesystem::path path;
    std::string contents;
};

static std::filesystem::path projectionOutputPath(
    const GeneratorTargetFile& target,
    const GeneratorProjection& projection
)
{
    std::filesystem::path relative = target.logical_path;
    if (!projection.include_relative)
    {
        relative = relative.filename();
    }
    relative.replace_extension(projection.output_suffix);
    return std::filesystem::path(projection.output_root) / relative;
}

static bool validateFiles(const GeneratorParseJob& generator_config)
{
    bool ok = true;

    // Iterate over each target file and verify that the file exists.
    for (const auto& file : generator_config.target_files)
    {
        if (!std::filesystem::exists(file.physical_path))
        {
            std::cerr << "[Error] file_to_parse " << file.physical_path
                      << " does not exist.\n";
            ok = false;
        }
        const auto logical = std::filesystem::path(file.logical_path).lexically_normal();
        if (logical.empty() || logical.is_absolute() ||
            (!logical.empty() && *logical.begin() == ".."))
        {
            std::cerr << "[Error] logical_path must remain relative: "
                      << file.logical_path << "\n";
            ok = false;
        }
    }

    std::set<std::filesystem::path> output_paths;
    for (const auto& projection : generator_config.projections)
    {
        if (!std::filesystem::exists(projection.template_path))
        {
            std::cerr << "[Error] template " << projection.template_path
                      << " does not exist.\n";
            ok = false;
        }
        for (const auto& file : generator_config.target_files)
        {
            const auto output = projectionOutputPath(file, projection).lexically_normal();
            if (!output_paths.emplace(output).second)
            {
                std::cerr << "[Error] projection output collision: " << output << "\n";
                ok = false;
            }
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
static std::vector<std::string> buildCompileOptions(
    const GeneratorParseJob& generator_config,
    std::vector<std::filesystem::path>& includes
)
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
static bool processTargetFile(const GeneratorTargetFile& target,
    const GeneratorParseJob& generator_config,
    const std::vector<std::string>& options,
    std::vector<MetaUnit>& meta_list,
    std::vector<nlohmann::json>& meta_json_list,
    std::vector<std::filesystem::path>& includes)
{
    const std::filesystem::path file = target.physical_path;
    // Convert the current file path to a string.
    std::string file_path = file.string();
    // Obtain the parent directory of the file. This information is later stored in the meta JSON.
    auto source_parent = file.parent_path().string();

    // Set up the parsing options needed by the C++ parser.
    ParseOptions parse_options;
    parse_options.commands = options;                  // Command-line compile options.
    parse_options.marker_symbol = generator_config.marker;  // Marker used in the source to denote declarations.
    parse_options.parse_included_marked = generator_config.parse_included_marked;
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
    auto [parse_rst, compact_data] = cxx_parser->parse(file_path);
    if (parse_rst != EParseResult::SUCCESS)
    {
        std::cerr << "[Error] Parsing of '" << file_path << "' failed"
                  << (parse_rst == EParseResult::UNKNOWN_TYPE
                      ? " (one or more declarations have unsupported cursor kinds)" : "")
                  << ".\n";
        return false;
    }

    // Project the canonical compact IR into the stable template JSON view.
    auto template_json = ir::templateJson(compact_data);
    if (!template_json)
    {
        std::cerr << "[Error] Compact reflection IR has no template projection for '"
                  << file_path << "'.\n";
        return false;
    }
    nlohmann::json meta_json = nlohmann::json::parse(*template_json);
    // Parent-chain callbacks still use the private legacy view during this
    // generator migration. It is reconstructed from the canonical IR and is
    // never the parser/generator transport representation.
    MetaUnit data = MetaUnit::fromJson(*template_json);
    // Augment the JSON with additional file-specific metadata.
    meta_json["source_path"]            = file_path;
    meta_json["logical_path"]           = target.logical_path;
    meta_json["source_parent"]          = source_parent;
    meta_json["parser_compile_options"] = options;
	meta_json["include_dir"] = GeneratorHelper::findRelativeIncludePath(
        file,
        includes
    ).value_or(std::string(""));

    // Save the parsed meta unit and its JSON representation for later use in template rendering.
    meta_list.push_back(std::move(data));
    meta_json_list.push_back(std::move(meta_json));

    return true;
}

static bool dfsFindChain(const CXXRecordDecl* current,
    std::string_view                          target_name,
    std::vector<const CXXRecordDecl*>&        path,
    std::unordered_set<const CXXRecordDecl*>& visited,
    const MetaUnit&                           meta_unit)
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
static bool renderProjection(
    const GeneratorParseJob& generator_config,
    const GeneratorProjection& projection,
    const std::vector<MetaUnit>& meta_unit_list,
    const std::vector<nlohmann::json>& meta_json_list,
    std::vector<PendingOutput>& outputs
)
{
    // Open the template file provided in the configuration.
    std::ifstream     template_file(projection.template_path);
    inja::Environment inja_env;
    inja::Template    inja_template;

    if (!template_file.is_open()) {
        std::cerr << "[Error] Failed to open template file "
                  << projection.template_path << "\n";
        return false;
    }
    // Read the entire template file into a string.
    std::string template_str(
        (std::istreambuf_iterator<char>(template_file)),
        std::istreambuf_iterator<char>()
    );

    // Per-iteration context the file-aware callbacks read from. Updated once at
    // the top of each per-file render below instead of being driven by a shared
    // mutable index — this keeps the dependency on iteration state explicit and
    // is robust against future async/cached rendering.
    struct RenderContext {
        const MetaUnit*       meta_unit = nullptr;
        const nlohmann::json* meta_json = nullptr;
    } current{};

    // Escape a value so it is safe to embed inside a generated C++ string literal.
    // Values originate from libclang spellings and user annotations and may contain
    // backslashes (Windows paths), quotes, newlines or other control characters;
    // emitting them raw produces malformed or injectable generated code. Templates
    // must wrap any value that lands inside "..." with cxx_str(...).
    inja_env.add_callback(
        "cxx_str",
        [](const inja::Arguments& args) -> std::string {
            const std::string s = args.at(0)->get<std::string>();
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string out;
            out.reserve(s.size() + 8);
            for (unsigned char c : s)
            {
                switch (c)
                {
                    case '\\': out += "\\\\"; break;
                    case '\"': out += "\\\""; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (c < 0x20 || c == 0x7F)
                        {
                            out += "\\x";
                            out += hex[(c >> 4) & 0xF];
                            out += hex[c & 0xF];
                            out += "\"\""; // terminate the \x run so following chars aren't absorbed
                        }
                        else
                        {
                            out += static_cast<char>(c);
                        }
                        break;
                }
            }
            return out;
        }
    );

    // Callback to retrieve a declaration based on its unique ID from the meta data.
    // The meta_unit_list is used to locate the declaration, and then the corresponding JSON data is returned.
    inja_env.add_callback(
        "decl_from_id",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            auto id = args.at(0)->get<std::string>();
            auto decl = current.meta_unit->findDeclById(id);
            if (!decl) {
                std::cerr << "[Error] Declaration with id " << id << " not found.\n";
                throw std::runtime_error("Declaration not found");
            }
            return (*current.meta_json)["declarations"][decl->index];
        }
    );

    // Callback to retrieve a declaration based solely on its index in the meta JSON array.
    inja_env.add_callback(
        "decl_from_index",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            auto  index = args.at(0)->get<size_t>();
            return (*current.meta_json)["declarations"][index];
        }
    );

    // Callback to retrieve type information based on its unique ID.
    // It looks up the type from meta_unit_list and returns the associated JSON data.
    inja_env.add_callback(
        "type_from_id",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            auto id = args.at(0)->get<std::string>();
            auto type = current.meta_unit->findTypeById(id);
            if (!type) {
                std::cerr << "[Error] Type with id " << id << " not found.\n";
                throw std::runtime_error("Type not found");
            }
            return (*current.meta_json)["types"][type->index];
        }
    );

    // Generic canonical template queries. Downstream generators can identify
    // their own domain templates without teaching lux-cxx those domain names
    // or parsing a C++ spelling themselves.
    inja_env.add_callback(
        "type_is_specialization",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            if (args.size() < 2)
                return false;
            const auto id = args.at(0)->get<std::string>();
            auto expected = trim_copy(args.at(1)->get<std::string>());
            if (expected.starts_with("::"))
                expected.erase(0, 2);
            const auto* type = current.meta_unit->findTypeById(id);
            const auto* record = dynamic_cast<const RecordType*>(type);
            if (!record || !record->isTemplateSpecialization())
                return false;
            auto actual = record->template_name;
            if (actual.starts_with("::"))
                actual.erase(0, 2);
            return actual == expected;
        }
    );

    inja_env.add_callback(
        "type_argument_type_id",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            if (args.size() < 2)
                return nullptr;
            const auto id = args.at(0)->get<std::string>();
            const auto index = args.at(1)->get<std::size_t>();
            const auto* type = current.meta_unit->findTypeById(id);
            const auto* record = dynamic_cast<const RecordType*>(type);
            if (!record || index >= record->template_arguments.size())
                return nullptr;
            const auto& argument = record->template_arguments[index];
            if (argument.kind != TemplateArgument::Kind::Type || !argument.type)
                return nullptr;
            return argument.type->id;
        }
    );

    inja_env.add_callback(
        "type_template_name",
        [&current](const inja::Arguments& args) -> nlohmann::json {
            if (args.empty())
                return nullptr;
            const auto id = args.at(0)->get<std::string>();
            const auto* type = current.meta_unit->findTypeById(id);
            const auto* record = dynamic_cast<const RecordType*>(type);
            if (!record || record->template_name.empty())
                return nullptr;
            auto name = trim_copy(record->template_name);
            if (name.starts_with("::"))
                name.erase(0, 2);
            return name;
        }
    );

    // Compute a stable prefix layout for static members selected by a generic
    // template primary.  The generator is deliberately unaware of the
    // downstream meanings of either template: callers provide both the
    // lineage wrapper and member template names.
    const auto lineage_base =
        [&current](const CXXRecordDecl* record,
                   std::string_view wrapper_template,
                   std::size_t base_argument_index) -> const CXXRecordDecl* {
        if (!record)
            return nullptr;

        auto normalized_wrapper = trim_copy(wrapper_template);
        if (normalized_wrapper.starts_with("::"))
            normalized_wrapper.erase(0, 2);

        const RecordType* wrapped_base_type = nullptr;
        for (const auto base_index : record->bases)
        {
            const auto* base = current.meta_unit->getDeclAs<CXXRecordDecl>(base_index);
            const auto* base_type = base
                ? dynamic_cast<const RecordType*>(base->type)
                : nullptr;
            if (!base_type)
                continue;
            auto actual_wrapper = trim_copy(base_type->template_name);
            if (actual_wrapper.starts_with("::"))
                actual_wrapper.erase(0, 2);
            if (actual_wrapper != normalized_wrapper)
                continue;
            if (wrapped_base_type)
                throw std::runtime_error(
                    "template lineage: multiple direct lineage wrappers on "
                    + record->full_qualified_name
                );
            wrapped_base_type = base_type;
        }

        if (!wrapped_base_type)
            return nullptr;
        if (base_argument_index >= wrapped_base_type->template_arguments.size())
            throw std::runtime_error(
                "template lineage: base type argument index is out of range on "
                + record->full_qualified_name
            );
        const auto& base_argument =
            wrapped_base_type->template_arguments[base_argument_index];
        const auto* actual_base_type = base_argument.type
            ? dynamic_cast<const RecordType*>(base_argument.type)
            : nullptr;
        return actual_base_type
            ? decl_cast<CXXRecordDecl>(actual_base_type->decl)
            : nullptr;
    };

    const auto template_lineage_static_count =
        [&current, &lineage_base](const CXXRecordDecl* record,
                   std::string_view wrapper_template,
                   std::size_t base_argument_index,
                   std::string_view member_template,
                   const auto& self) -> std::size_t {
        if (!record)
            return 0;

        std::size_t inherited_count = 0;
        if (const auto* actual_base =
                lineage_base(record, wrapper_template, base_argument_index))
        {
            inherited_count = self(
                actual_base,
                wrapper_template,
                base_argument_index,
                member_template,
                self
            );
        }

        std::size_t local_count = 0;
        for (const auto member_index : record->static_var_decls)
        {
            const auto* member = current.meta_unit->getDeclAs<VarDecl>(member_index);
            const auto* member_type = member
                ? dynamic_cast<const RecordType*>(member->type)
                : nullptr;
            if (member_type && member_type->template_name == member_template)
                ++local_count;
        }
        return inherited_count + local_count;
    };

    inja_env.add_callback(
        "template_lineage_static_count",
        [&current, &template_lineage_static_count](const inja::Arguments& args) -> nlohmann::json {
            if (args.size() < 4)
                return nullptr;
            const auto* decl = current.meta_unit->findDeclById(
                args.at(0)->get<std::string>()
            );
            const auto* record = decl_cast<CXXRecordDecl>(decl);
            if (!record)
                throw std::runtime_error(
                    "template_lineage_static_count: record declaration not found"
                );
            return template_lineage_static_count(
                record,
                trim_copy(args.at(1)->get<std::string>()),
                args.at(2)->get<std::size_t>(),
                trim_copy(args.at(3)->get<std::string>()),
                template_lineage_static_count
            );
        }
    );

    inja_env.add_callback(
        "template_lineage_static_index",
        [&current, &lineage_base, &template_lineage_static_count](const inja::Arguments& args) -> nlohmann::json {
            if (args.size() < 5)
                return nullptr;
            const auto* decl = current.meta_unit->findDeclById(
                args.at(0)->get<std::string>()
            );
            const auto* record = decl_cast<CXXRecordDecl>(decl);
            if (!record)
                throw std::runtime_error(
                    "template_lineage_static_index: record declaration not found"
                );

            const auto wrapper_template = trim_copy(args.at(2)->get<std::string>());
            const auto base_argument_index = args.at(3)->get<std::size_t>();
            const auto member_template = trim_copy(args.at(4)->get<std::string>());
            std::size_t inherited_count = 0;
            if (const auto* actual_base =
                    lineage_base(record, wrapper_template, base_argument_index))
            {
                inherited_count = template_lineage_static_count(
                    actual_base,
                    wrapper_template,
                    base_argument_index,
                    member_template,
                    template_lineage_static_count
                );
            }

            const auto member_id = args.at(1)->get<std::string>();
            std::size_t local_ordinal = 0;
            for (const auto member_index : record->static_var_decls)
            {
                const auto* member = current.meta_unit->getDeclAs<VarDecl>(member_index);
                const auto* member_type = member
                    ? dynamic_cast<const RecordType*>(member->type)
                    : nullptr;
                if (!member_type || member_type->template_name != member_template)
                    continue;
                if (member->id == member_id)
                    return inherited_count + local_ordinal;
                ++local_ordinal;
            }
            throw std::runtime_error(
                "template_lineage_static_index: selected member is not a matching static member"
            );
        }
    );

    inja_env.add_callback(
        "template_lineage_record_hashes",
        [&current, &lineage_base](const inja::Arguments& args) -> nlohmann::json {
            if (args.size() < 3)
                return nlohmann::json::array();
            const auto* decl = current.meta_unit->findDeclById(
                args.at(0)->get<std::string>()
            );
            const auto* record = decl_cast<CXXRecordDecl>(decl);
            if (!record)
                throw std::runtime_error(
                    "template_lineage_record_hashes: record declaration not found"
                );

            const auto wrapper_template = trim_copy(args.at(1)->get<std::string>());
            const auto base_argument_index = args.at(2)->get<std::size_t>();
            nlohmann::json result = nlohmann::json::array();
            auto* base = lineage_base(
                record,
                wrapper_template,
                base_argument_index
            );
            while (base)
            {
                result.push_back({
                    {"id", base->id},
                    {"hash", std::to_string(base->hash)}
                });
                base = lineage_base(
                    base,
                    wrapper_template,
                    base_argument_index
                );
            }
            return result;
        }
    );

    inja_env.add_callback(
        "parent_chain",
        [&current](const inja::Arguments& args) -> nlohmann::json
        {
            auto id     = args.at(0)->get<std::string>();
            auto target = args.at(1)->get<std::string>();
            auto decl = current.meta_unit->findDeclById(id);
            if (!decl) {
                std::cerr << "[Error] parent_chain: declaration id='" << id
                          << "' not found in '" << (*current.meta_json)["source_path"] << "'.\n";
                throw std::runtime_error("parent_chain: declaration not found: " + id);
            }
            if (decl->kind != EDeclKind::CXX_RECORD_DECL)
            {
                std::cerr << "[Error] parent_chain: id='" << id
                          << "' is not a CXX_RECORD_DECL (kind="
                          << static_cast<int>(decl->kind) << ") in '"
                          << (*current.meta_json)["source_path"] << "'.\n";
                throw std::runtime_error("parent_chain: not a CXX_RECORD_DECL");
            }
            auto decl_list = find_parent_chain(static_cast<const CXXRecordDecl*>(decl), target, *current.meta_unit);
            nlohmann::json ret = nlohmann::json::array();
            for (auto d : decl_list)
            {
                nlohmann::json info = {
                    {"id", d->id},
                    {"hash", std::to_string(d->hash)}
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

    // Parse the template exactly once — the string never changes across files.
    try {
        inja_template = inja_env.parse(template_str);
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to parse template "
                  << projection.template_path << ": " << e.what() << "\n";
        return false;
    }

    // Iterate over each meta JSON object.
    for (size_t i = 0; i < meta_json_list.size(); ++i)
    {
        auto meta_json = meta_json_list[i];
        meta_json["projection_name"] = projection.name;
        for (const std::string& field : projection.custom_fields_json)
        {
            try
            {
                const auto extra = nlohmann::json::parse(field);
                for (const auto& [key, value] : extra.items())
                {
                    meta_json[key] = value;
                }
            }
            catch (const nlohmann::json::parse_error& error)
            {
                std::cerr << "[Error] Invalid custom_fields_json entry for projection '"
                          << projection.name << "': " << error.what() << "\n";
                return false;
            }
        }
        // Update the per-iteration context that the file-aware callbacks
        // capture. Must be set before render() because inja invokes callbacks
        // synchronously during render.
        current.meta_unit = &meta_unit_list[i];
        current.meta_json = &meta_json;

        try {
            // Render the pre-parsed template using the current meta JSON data.
            outputs.push_back(PendingOutput{
                projectionOutputPath(generator_config.target_files[i], projection),
                inja_env.render(inja_template, meta_json)
            });
            if (projection.serial_meta)
            {
                auto json_path = projectionOutputPath(
                    generator_config.target_files[i],
                    projection
                );
                json_path += ".json";
                outputs.push_back(PendingOutput{
                    std::move(json_path),
                    nlohmann::to_string(meta_json)
                });
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[Error] Failed to render projection " << projection.name
                      << ": " << e.what() << "\n";
            return false;
        }
    }
    return true;
}

static bool readExisting(
    const std::filesystem::path& path,
    std::string& contents
)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
    {
        return false;
    }
    contents.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()
    );
    return !stream.bad();
}

static bool publishOutputs(const std::vector<PendingOutput>& outputs)
{
    struct Publication final
    {
        std::filesystem::path destination;
        std::filesystem::path temporary;
        std::filesystem::path backup;
        bool had_destination{};
        bool published{};
    };

    std::vector<Publication> publications;
    publications.reserve(outputs.size());

    std::set<std::filesystem::path> unique;
    for (const auto& output : outputs)
    {
        const auto normalized = output.path.lexically_normal();
        if (!unique.emplace(normalized).second)
        {
            std::cerr << "[Error] duplicate rendered output: " << normalized << "\n";
            return false;
        }

        std::string existing;
        if (readExisting(normalized, existing) && existing == output.contents)
        {
            continue;
        }

        std::error_code error;
        std::filesystem::create_directories(normalized.parent_path(), error);
        if (error)
        {
            std::cerr << "[Error] failed to create output directory for "
                      << normalized << ": " << error.message() << "\n";
            return false;
        }

        Publication publication;
        publication.destination = normalized;
        publication.temporary = normalized;
        publication.temporary += ".luxgen.tmp";
        publication.backup = normalized;
        publication.backup += ".luxgen.bak";
        publication.had_destination = std::filesystem::exists(normalized);

        std::filesystem::remove(publication.temporary, error);
        error.clear();
        std::ofstream stream(publication.temporary, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            std::cerr << "[Error] failed to open temporary output "
                      << publication.temporary << "\n";
            return false;
        }
        stream.write(output.contents.data(), static_cast<std::streamsize>(output.contents.size()));
        stream.close();
        if (!stream)
        {
            std::cerr << "[Error] failed to write temporary output "
                      << publication.temporary << "\n";
            return false;
        }
        publications.push_back(std::move(publication));
    }

    const auto rollback = [&publications]() noexcept
    {
        for (auto iterator = publications.rbegin(); iterator != publications.rend(); ++iterator)
        {
            std::error_code ignored;
            std::filesystem::remove(iterator->temporary, ignored);
            if (iterator->published)
            {
                std::filesystem::remove(iterator->destination, ignored);
            }
            if (iterator->had_destination && std::filesystem::exists(iterator->backup))
            {
                std::filesystem::rename(iterator->backup, iterator->destination, ignored);
            }
        }
    };

    for (auto& publication : publications)
    {
        std::error_code error;
        std::filesystem::remove(publication.backup, error);
        error.clear();
        if (publication.had_destination)
        {
            std::filesystem::rename(
                publication.destination,
                publication.backup,
                error
            );
            if (error)
            {
                rollback();
                return false;
            }
        }
        std::filesystem::rename(
            publication.temporary,
            publication.destination,
            error
        );
        if (error)
        {
            rollback();
            return false;
        }
        publication.published = true;
    }

    for (const auto& publication : publications)
    {
        std::error_code ignored;
        std::filesystem::remove(publication.backup, ignored);
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

    GeneratorParseJob generator_config;
    try {
        // Load the generator configuration from the JSON file.
        // The helper function GeneratorHelper::loadGeneratorConfig reads various settings such as:
        // marker, template_path, out_dir, compile_commands, target_files, meta_suffix, source_file,
        // and extra compile options.
        GeneratorHelper::loadGeneratorParseJob(argv[1], generator_config);
    }
    catch (const std::exception& e) {
        std::cerr << "[Error] Failed to load config: " << e.what() << "\n";
        return 1;
    }

    // Ensure all required files exist.
    if (!validateFiles(generator_config)) {
        return 1;
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

    std::vector<PendingOutput> outputs;
    outputs.reserve(generator_config.target_files.size() * generator_config.projections.size());
    for (const auto& projection : generator_config.projections)
    {
        if (!renderProjection(
                generator_config,
                projection,
                meta_unit_list,
                meta_json_list,
                outputs
            ))
        {
            return 1;
        }
    }
    if (!publishOutputs(outputs))
    {
        return 1;
    }

    std::cout << "[Info] Parsed " << generator_config.target_files.size()
              << " target file(s) for " << generator_config.projections.size()
              << " projection(s).\n";

    return 0;
}
