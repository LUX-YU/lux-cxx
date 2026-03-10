#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <algorithm>
#include <cctype>
#include <lux/cxx/dref/generator/GeneratorHelper.hpp>

namespace lux::cxx::dref
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

            if (c == '\\' && !in_single_quote)
            {
                escaped = true;
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

    std::string GeneratorHelper::visibility2Str(lux::cxx::dref::EVisibility visibility)
    {
        switch (visibility)
        {
        case lux::cxx::dref::EVisibility::PUBLIC:    return "EVisibility::PUBLIC";
        case lux::cxx::dref::EVisibility::PRIVATE:   return "EVisibility::PRIVATE";
        case lux::cxx::dref::EVisibility::PROTECTED: return "EVisibility::PROTECTED";
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

    std::vector<std::filesystem::path> GeneratorHelper::fetchIncludePaths(
        const std::filesystem::path& compile_command_path,
        const std::filesystem::path& source_file_path)
    {
        namespace fs = std::filesystem;

        std::ifstream ifs(compile_command_path);
        if (!ifs) {
            return {};
        }

        nlohmann::json j;
        try {
            ifs >> j;
        }
        catch (const nlohmann::json::parse_error& e) {
            return {};
        }

        if (!j.is_array()) {
            return {};
        }

        std::set<fs::path> includes;
        const std::string source_file_key = normalizedPathKey(source_file_path);
        for (const auto& entry : j)
        {
            if (!entry.is_object())
                continue;

            if (!entry.contains("file") || !entry.contains("directory"))
                continue;

            std::string fileStr = entry["file"].get<std::string>();
            if (normalizedPathKey(fs::path(fileStr)) != source_file_key)
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

                if (argStr.rfind("-I", 0) == 0)
                {
                    if (argStr == "-I") {
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
                    if (argStr == "-isystem") {
                        if (i + 1 < args.size()) {
                            pathStr = args[++i];
                        }
                    }
                    else {
                        pathStr = argStr.substr(std::string("-isystem").length());
                    }
                }
                else if (argStr.rfind("-external:I", 0) == 0)
                {
                    if (argStr == "-external:I") {
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
                    includes.insert(incP);
                }
            }
        }
        return std::vector<fs::path>(includes.begin(), includes.end());
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

    void GeneratorHelper::loadGeneratorConfig(const std::string& filename, GeneratorConfig& config)
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
		config.marker                   = j.at("marker").get<std::string>();
		config.template_path            = j.at("template_path").get<std::string>();
        config.out_dir                  = j.at("out_dir").get<std::string>();
        config.compile_commands         = j.at("compile_commands").get<std::string>();
        config.target_files             = j.at("target_files").get<std::vector<std::string>>();
        config.meta_suffix              = j.at("meta_suffix").get<std::string>();
        config.source_file              = j.at("source_file").get<std::string>();
        config.extra_compile_options    = j.at("extra_compile_options").get<std::vector<std::string>>();
        if (j.contains("custom_fields_json")) {
			for (auto& [key, val] : j["custom_fields_json"].items()) {
				if (val.is_string()) {
					config.custom_fields_json.push_back(val.get<std::string>());
				}
			}
        }
        else {
            config.custom_fields_json.clear();
        }
		config.serial_meta              = j.value("serial_meta", true);
		config.dry_run                  = j.value("dry_run", false);
		config.cxx_standard             = j.value("cxx_standard", std::string("c++20"));
		if (j.contains("preprocessor_defines") && j["preprocessor_defines"].is_array()) {
			config.preprocessor_defines = j["preprocessor_defines"].get<std::vector<std::string>>();
		}
    }
}
