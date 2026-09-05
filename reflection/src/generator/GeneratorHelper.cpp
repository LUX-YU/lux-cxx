#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <algorithm>
#include <cctype>
#include <lux/cxx/reflection/generator/GeneratorHelper.hpp>

namespace lux::cxx::reflection
{
    static std::filesystem::path normalizedPath(const std::filesystem::path& in)
    {
        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path p = fs::weakly_canonical(in, ec);
        if (ec)
        {
            ec.clear();
            p = fs::absolute(in, ec);
            if (ec)
            {
                p = in;
            }
        }

        return p.lexically_normal();
    }

    static std::string normalizedPathKey(const std::filesystem::path& in)
    {
        auto p = normalizedPath(in);

        std::string key = p.generic_string();

#ifdef _WIN32
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif

        return key;
    }

    std::optional<std::filesystem::path> GeneratorHelper::findRelativeIncludePath(
        const std::filesystem::path& metaFile,
        const std::vector<std::filesystem::path>& includeList)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path absFile = normalizedPath(metaFile);
        const std::string absFileKey = normalizedPathKey(absFile);

        for (const auto& incPath : includeList)
        {
            const fs::path absInc = normalizedPath(incPath);
            std::string absIncKey = normalizedPathKey(absInc);

            // Ensure a strict directory boundary: "x/y" should not match "x/yz".
            if (!absIncKey.empty() && absIncKey.back() != '/')
            {
                absIncKey.push_back('/');
            }

            const bool is_same_path = absFileKey == normalizedPathKey(absInc);
            const bool is_child_path = !absIncKey.empty() && absFileKey.rfind(absIncKey, 0) == 0;
            if (!is_same_path && !is_child_path)
            {
                continue;
            }

            fs::path rel = fs::relative(absFile, absInc, ec);
            if (!ec && !rel.empty()) {
                return rel.lexically_normal(); // e.g. "subdir/foo.hpp"
            }
        }
        return std::nullopt;
    }


    std::vector<std::string> GeneratorHelper::splitCommand(const std::string& cmd)
    {
        std::vector<std::string> tokens;
        std::string current;
        bool in_single_quote = false;
        bool in_double_quote = false;
        bool escaped = false;

        for (size_t i = 0; i < cmd.size(); ++i)
        {
            char c = cmd[i];

            if (escaped)
            {
                current += c;
                escaped = false;
                continue;
            }

            // Only treat '\\' as an escape character when it is followed by a
            // whitespace or quote. Windows paths (e.g. -IC:\foo\bar) use '\\'
            // as a plain path separator; treating them as escapes would mangle
            // include paths collected from compile_commands.
            if (c == '\\' && !in_single_quote)
            {
                if (i + 1 < cmd.size())
                {
                    char next = cmd[i + 1];
                    if (next == ' ' || next == '\t' || next == '"')
                    {
                        escaped = true;
                        continue;
                    }
                }

                // Not a shell escape sequence; keep the literal separator.
                current += c;
                continue;
            }

            if (c == '\'' && !in_double_quote)
            {
                in_single_quote = !in_single_quote;
                continue;
            }

            if (c == '"' && !in_single_quote)
            {
                in_double_quote = !in_double_quote;
                continue;
            }

            if (std::isspace(static_cast<unsigned char>(c)) && !in_single_quote && !in_double_quote)
            {
                if (!current.empty())
                {
                    tokens.push_back(std::move(current));
                    current.clear();
                }
                continue;
            }

            current += c;
        }

        if (!current.empty())
        {
            tokens.push_back(std::move(current));
        }

        return tokens;
    }

    // ------------------------------------------------------
    // Convert a baseDir + path to an absolute path
    // ------------------------------------------------------
    std::filesystem::path GeneratorHelper::makeAbsolute(
        const std::filesystem::path& baseDir,
        const std::filesystem::path& p)
    {
        if (p.is_absolute())
            return p;
        return std::filesystem::absolute(baseDir / p);
    }

    // ------------------------------------------------------
    // Check if a string is a Windows-style absolute path
    // e.g., "C:\..." or "D:/..."
    // ------------------------------------------------------
    bool GeneratorHelper::isStandardAbsolute(const std::string& s)
    {
        return (s.size() >= 3 && std::isalpha(s[0]) && s[1] == ':' &&
            (s[2] == '\\' || s[2] == '/'));
    }

    std::string GeneratorHelper::visibility2Str(lux::cxx::reflection::EVisibility visibility)
    {
        switch (visibility)
        {
        case lux::cxx::reflection::EVisibility::PUBLIC:    return "EVisibility::PUBLIC";
        case lux::cxx::reflection::EVisibility::PRIVATE:   return "EVisibility::PRIVATE";
        case lux::cxx::reflection::EVisibility::PROTECTED: return "EVisibility::PROTECTED";
        default:
            return "EVisibility::INVALID";
        }
    }

    std::string GeneratorHelper::truncateAtLastParen(const std::string& funcName) {

        size_t pos = funcName.rfind('(');
        if (pos != std::string::npos) {
            return funcName.substr(0, pos);
        }
        return funcName;
    }

    Result<std::vector<std::filesystem::path>> GeneratorHelper::fetchIncludePaths(
        const std::filesystem::path& compile_command_path,
        const std::filesystem::path& source_file_path)
    {
        namespace fs = std::filesystem;

        std::ifstream ifs(compile_command_path);
        if (!ifs) {
            return make_error(EErrorCode::CompileCommandsNotFound,
                "[GeneratorHelper] Cannot open compile_commands: " + compile_command_path.string());
        }

        nlohmann::json j;
        try {
            ifs >> j;
        }
        catch (const nlohmann::json::parse_error& e) {
            return make_error(EErrorCode::CompileCommandsParseError,
                "[GeneratorHelper] Failed to parse compile_commands '"
                + compile_command_path.string() + "': " + e.what());
        }

        if (!j.is_array()) {
            return make_error(EErrorCode::CompileCommandsParseError,
                "[GeneratorHelper] compile_commands is not a JSON array: "
                + compile_command_path.string());
        }

        // Include search order is semantic: clang probes -I directories in the
        // order given, and MSVC searches /external:I directories only AFTER all
        // regular -I paths. Regular -I paths must keep their command-line order
        // and precede -isystem/-external:I paths, otherwise stale install-tree
        // headers can shadow the source tree during parsing.
        std::vector<fs::path> regular_includes;
        std::vector<fs::path> system_includes;
        std::set<fs::path>    seen_includes;
        const std::string source_file_key = normalizedPathKey(source_file_path);
        for (const auto& entry : j)
        {
            if (!entry.is_object())
                continue;

            if (!entry.contains("file") || !entry.contains("directory"))
                continue;

            std::string fileStr = entry["file"].get<std::string>();
            if (normalizedPathKey(makeAbsolute(entry["directory"].get<std::string>(), fileStr)) != source_file_key)
                continue;

            fs::path baseDir = entry["directory"].get<std::string>();
            std::vector<std::string> args;

            if (entry.contains("arguments") && entry["arguments"].is_array())
            {
                for (const auto& arg : entry["arguments"])
                {
                    args.push_back(arg.get<std::string>());
                }
            }

            else if (entry.contains("command") && entry["command"].is_string())
            {
                std::string cmdStr = entry["command"].get<std::string>();
                args = splitCommand(cmdStr);
            }

            for (size_t i = 0; i < args.size(); ++i)
            {
                std::string argStr = args[i];
                std::string pathStr;
                bool is_system_include = false;

                if (argStr.rfind("-I", 0) == 0 || argStr.rfind("/I", 0) == 0)
                {
                    if (argStr == "-I" || argStr == "/I") {
                        if (i + 1 < args.size()) {
                            pathStr = args[++i];
                        }
                    }
                    else {
                        pathStr = argStr.substr(2);
                    }
                }
                else if (argStr.rfind("-isystem", 0) == 0)
                {
                    is_system_include = true;
                    if (argStr == "-isystem") {
                        if (i + 1 < args.size()) {
                            pathStr = args[++i];
                        }
                    }
                    else {
                        pathStr = argStr.substr(std::string("-isystem").length());
                    }
                }
                else if (argStr.rfind("-external:I", 0) == 0 || argStr.rfind("/external:I", 0) == 0)
                {
                    is_system_include = true;
                    if (argStr == "-external:I" || argStr == "/external:I") {
                        if (i + 1 < args.size()) {
                            pathStr = args[++i];
                        }
                    }
                    else {
                        pathStr = argStr.substr(std::string("-external:I").length());
                    }
                }

                if (!pathStr.empty())
                {
                    fs::path incP;
                    if (isStandardAbsolute(pathStr)) {
                        incP = fs::path(pathStr);
                    }
                    else {
                        incP = makeAbsolute(baseDir, pathStr);
                    }
                    if (seen_includes.insert(incP).second) {
                        (is_system_include ? system_includes : regular_includes).push_back(incP);
                    }
                }
            }
        }
        std::vector<fs::path> includes = std::move(regular_includes);
        includes.insert(includes.end(), system_includes.begin(), system_includes.end());
        return includes;
    }

    Result<std::vector<std::string>> GeneratorHelper::fetchCompileOptions(
        const std::filesystem::path& database, const std::filesystem::path& source)
    {
        std::ifstream input(database);
        if (!input)
            return make_error(EErrorCode::CompileCommandsNotFound, "Cannot open " + database.string());
        nlohmann::json commands;
        try { input >> commands; }
        catch (const nlohmann::json::exception& error)
        {
            return make_error(EErrorCode::CompileCommandsParseError, error.what());
        }
        if (!commands.is_array())
            return make_error(EErrorCode::CompileCommandsParseError, "Compilation database must be an array");
        std::vector<std::string> result;
        bool found{};
        for (const auto& entry : commands)
        {
            if (!entry.is_object() || !entry.contains("directory") || !entry.contains("file"))
                continue;
            const std::filesystem::path directory = entry.at("directory").get<std::string>();
            const auto file = makeAbsolute(directory, entry.at("file").get<std::string>());
            if (normalizedPathKey(file) != normalizedPathKey(source))
                continue;
            if (found)
                return make_error(EErrorCode::CompileCommandsParseError, "Ambiguous compile command for " + source.string());
            found = true;
            const auto args = entry.contains("arguments")
                ? entry.at("arguments").get<std::vector<std::string>>()
                : splitCommand(entry.at("command").get<std::string>());
            for (std::size_t index{1U}; index < args.size(); ++index)
            {
                const auto& arg = args[index];
                const auto missing = [&]() {
                    return make_error(EErrorCode::CompileCommandsParseError, "Missing compile option value: " + arg);
                };
                if (arg.starts_with("@"))
                    return make_error(EErrorCode::CompileCommandsParseError,
                        "Response-file compile commands require expanded arguments: " + arg);
                const bool definition = arg.starts_with("/D") || arg.starts_with("-D") ||
                    arg.starts_with("/U") || arg.starts_with("-U");
                if (definition)
                {
                    auto value = arg.substr(2);
                    if (value.empty())
                    {
                        if (++index == args.size()) return missing();
                        value = args[index];
                    }
                    result.push_back(std::string{"-"} + arg[1] + value);
                }
                else if (arg.starts_with("/std:") || arg.starts_with("-std:"))
                    result.push_back("-std=" + arg.substr(5));
                else if (arg.starts_with("/Zp") || arg.starts_with("-Zp"))
                    result.push_back("-fpack-struct=" + (arg.size() == 3U ? std::string{"8"} : arg.substr(3)));
                else if (arg.starts_with("/FI") || arg.starts_with("-FI"))
                {
                    auto value = arg.substr(3);
                    if (value.empty())
                    {
                        if (++index == args.size()) return missing();
                        value = args[index];
                    }
                    result.emplace_back("-include");
                    result.push_back(makeAbsolute(directory, value).string());
                }
                else if (arg == "-target" || arg == "--target" || arg == "-isysroot" ||
                    arg == "--sysroot" || arg == "-Xclang" || arg == "-x" || arg == "-include" || arg == "-imacros")
                {
                    if (index + 1U == args.size()) return missing();
                    result.push_back(arg);
                    const auto& value = args[++index];
                    const bool path = arg == "-include" || arg == "-imacros" || arg == "-isysroot" || arg == "--sysroot";
                    result.push_back(path ? makeAbsolute(directory, value).string() : value);
                }
                else if (arg.starts_with("-std=") || arg.starts_with("--target=") || arg.starts_with("--sysroot=") ||
                    arg.starts_with("-f") || arg.starts_with("-m") || arg.starts_with("-O"))
                    result.push_back(arg);
                else if (arg == "/J" || arg == "-J") result.emplace_back("-funsigned-char");
                else if (arg == "/GR-" || arg == "-GR-") result.emplace_back("-fno-rtti");
                else if (arg == "/O2" || arg == "/Ox") result.emplace_back("-O2");
                else if (arg == "/Od") result.emplace_back("-O0");
                else if (arg == "/MD" || arg == "/MDd")
                {
                    result.emplace_back("-D_MT");
                    result.emplace_back("-D_DLL");
                    if (arg == "/MDd") result.emplace_back("-D_DEBUG");
                }
                else if (arg == "/MT" || arg == "/MTd")
                {
                    result.emplace_back("-D_MT");
                    if (arg == "/MTd") result.emplace_back("-D_DEBUG");
                }
                else if (arg.starts_with("/Zc:") || arg.starts_with("-Zc:"))
                {
                    if (arg.substr(4) == "wchar_t-") result.emplace_back("-fno-wchar");
                    else if (arg.substr(4) != "__cplusplus" && arg.substr(4) != "inline" &&
                        arg.substr(4) != "preprocessor" && arg.substr(4) != "wchar_t" && arg.substr(4) != "sizedDealloc" &&
                        arg.substr(4) != "externConstexpr")
                        return make_error(EErrorCode::CompileCommandsParseError, "Unsupported semantic option: " + arg);
                }
            }
        }
        if (!found)
            return make_error(EErrorCode::SourceFileNotFound, "No compile command for " + source.string());
        return result;
    }

    std::vector<std::string> GeneratorHelper::convertToDashI(const std::vector<std::filesystem::path>& paths)
    {
        std::vector<std::string> result;
        result.reserve(paths.size());
        for (const auto& p : paths)
        {
            result.push_back("-I" + p.string());
        }
        return result;
    }

    void GeneratorHelper::loadGeneratorParseJob(
        const std::string& filename,
        GeneratorParseJob& config
    )
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            throw std::runtime_error("Failed to open config file: " + filename);
        }

        nlohmann::json j;
        try {
            ifs >> j;
        }
        catch (const nlohmann::json::parse_error& e) {
            throw std::runtime_error("Failed to parse config file '" + filename + "': " + e.what());
        }
        // Helper: throws with clear field name when a required key is absent.
        auto require_field = [&](const char* key) -> const nlohmann::json& {
            if (!j.contains(key)) {
                throw std::runtime_error(
                    std::string("[Config] Missing required field '") + key + "' in '" + filename + "'");
            }
            return j[key];
        };

        config.marker           = require_field("marker").get<std::string>();
        config.compile_commands = require_field("compile_commands").get<std::string>();
        config.depfile = j.value("depfile", std::string{});
        config.source_file      = require_field("source_file").get<std::string>();
        config.extra_compile_options =
            require_field("extra_compile_options").get<std::vector<std::string>>();

        const auto& target_files = require_field("target_files");
        if (!target_files.is_array() || target_files.empty())
        {
            throw std::runtime_error("[Config] 'target_files' must be a non-empty array");
        }
        config.target_files.clear();
        for (const auto& target : target_files)
        {
            GeneratorTargetFile file;
            file.physical_path = target.at("physical_path").get<std::string>();
            file.logical_path  = target.at("logical_path").get<std::string>();
            if (file.logical_path.empty() || std::filesystem::path(file.logical_path).is_absolute())
            {
                throw std::runtime_error(
                    "[Config] target logical_path must be non-empty and relative"
                );
            }
            config.target_files.push_back(std::move(file));
        }

        const auto& projections = require_field("projections");
        if (!projections.is_array() || projections.empty())
        {
            throw std::runtime_error("[Config] 'projections' must be a non-empty array");
        }
        config.projections.clear();
        for (const auto& item : projections)
        {
            GeneratorProjection projection;
            projection.name            = item.at("name").get<std::string>();
            projection.template_path   = item.at("template_path").get<std::string>();
            projection.output_root     = item.at("output_root").get<std::string>();
            projection.output_suffix   = item.at("output_suffix").get<std::string>();
            projection.include_relative = item.value("include_relative", true);
            projection.serial_meta      = item.value("serial_meta", false);
            projection.validation       = item.value("validation", false);
            if (item.contains("custom_fields_json"))
            {
                projection.custom_fields_json =
                    item["custom_fields_json"].get<std::vector<std::string>>();
            }
            if (projection.name.empty() ||
                (!projection.validation && projection.output_suffix.empty()))
            {
                throw std::runtime_error(
                    "[Config] projection name and output_suffix must be non-empty"
                );
            }
            config.projections.push_back(std::move(projection));
        }

		config.dry_run                  = j.value("dry_run", false);
		config.parse_included_marked    = j.value("parse_included_marked", false);
		config.cxx_standard             = j.value("cxx_standard", std::string("c++20"));
		if (j.contains("preprocessor_defines") && j["preprocessor_defines"].is_array()) {
			config.preprocessor_defines = j["preprocessor_defines"].get<std::vector<std::string>>();
		}
    }
}
