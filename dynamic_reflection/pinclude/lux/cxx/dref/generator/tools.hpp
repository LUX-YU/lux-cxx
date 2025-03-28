#pragma once
#include <vector>
#include <filesystem>
#include <string>
#include <optional>
#include <sstream>
#include <lux/cxx/dref/runtime/Attribute.hpp>
#include <lux/cxx/algotithm/string_operations.hpp>
#include <lux/cxx/algotithm/hash.hpp>

#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>


// -------------------------------------------------------------------------
// Attempt to see if a file is inside one of the 'includeList' directories
// If so, return the relative path, otherwise std::nullopt
// -------------------------------------------------------------------------
std::optional<std::filesystem::path> findRelativeIncludePath(
    const std::filesystem::path& metaFile,
    const std::vector<std::filesystem::path>& includeList
);

// ------------------------------------------------------
// Utility to parse the compile_commands "command"/"arguments" fields
// and split them into tokens
// ------------------------------------------------------
std::vector<std::string> splitCommand(const std::string& cmd);

// ------------------------------------------------------
// Convert a baseDir + path to an absolute path
// ------------------------------------------------------
std::filesystem::path makeAbsolute(
    const std::filesystem::path& baseDir,
    const std::filesystem::path& p
);

// ------------------------------------------------------
// Check if a string is a Windows-style absolute path
// e.g., "C:\..." or "D:/..."
// ------------------------------------------------------
bool isStandardAbsolute(const std::string& s);

std::string visibility2Str(lux::cxx::dref::runtime::EVisibility visibility);

std::string truncateAtLastParen(const std::string& funcName);

std::vector<std::filesystem::path> fetchIncludePaths(
    const std::filesystem::path& compile_command_path,
    const std::filesystem::path& source_file_path
);

std::vector<std::string> convertToDashI(const std::vector<std::filesystem::path>& paths);

struct Config {
    std::string              out_dir;
    std::string              compile_commands;
    std::vector<std::string> target_files;
    std::string              register_header_name;
    std::string              register_function_name;
    std::string              source_file;
};

void loadConfig(const std::string& filename, Config& config);