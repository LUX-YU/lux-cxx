#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <lux/cxx/dref/generator/GeneratorHelper.hpp>

namespace lux::cxx::dref
{
    std::optional<std::filesystem::path> GeneratorHelper::findRelativeIncludePath(
        const std::filesystem::path& metaFile,
        const std::vector<std::filesystem::path>& includeList)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        // Normalize the metaFile path
        fs::path absFile = fs::canonical(metaFile, ec);
        if (ec) {
            // fallback to absolute
            ec.clear();
            absFile = fs::absolute(metaFile, ec);
            if (ec) {
                return std::nullopt;
            }
        }

        for (const auto& incPath : includeList)
        {
            ec.clear();
            fs::path absInc = fs::canonical(incPath, ec);
            if (ec) {
                ec.clear();
                absInc = fs::absolute(incPath, ec);
                if (ec) {
                    // skip if we fail
                    continue;
                }
            }

            fs::path rel = fs::relative(absFile, absInc, ec);
            if (!ec && !rel.empty() && *rel.begin() != "..") {
                return rel; // e.g. "subdir/foo.hpp"
            }
        }
        return std::nullopt;
    }


    std::vector<std::string> GeneratorHelper::splitCommand(const std::string& cmd)
    {
        std::vector<std::string> tokens;
        std::istringstream iss(cmd);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
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
        for (const auto& entry : j)
        {
            if (!entry.is_object())
                continue;

            if (!entry.contains("file") || !entry.contains("directory"))
                continue;

            std::string fileStr = entry["file"].get<std::string>();
            if (fs::path(fileStr) != source_file_path)
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

        nlohmann::json j; ifs >> j;
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
    }
}
