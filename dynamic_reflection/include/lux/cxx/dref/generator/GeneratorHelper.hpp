#pragma once
#include <filesystem>
#include <optional>
#include <vector>
#include <lux/cxx/visibility.h>
#include <lux/cxx/dref/runtime/Declaration.hpp>

namespace lux::cxx::dref
{
	struct GeneratorConfig{
		// 用于标记c++声明的注解字符
		std::string              marker;
		// 对应的生成模板, 使用inja语法
		std::string              template_path;
		// 生成的文件存放目录
		std::string              out_dir;
		// 编译命令文件，一般可以在构建系统中生成，对于cmake系统一般在BINARY_DIR下
		std::string              compile_commands;
		// 需要生成元信息的文件列表
		std::vector<std::string> target_files;
		// 生成的元信息文件后缀
		std::string              meta_suffix;
		// 这个源文件不是需要生成元信息的文件，而是在compile_commands中的源文件，用于找到对应的编译命令以得到对应的编译选项
		// 如果没有提供，请在extra_compile_options中添加编译选项
		std::string              source_file;
		// 额外的编译选项，例如头文件的路径，平台特定的编译选项等
		std::vector<std::string> extra_compile_options;
		std::vector<std::string> custom_fields_json;
		// 是否将解析的元信息序列化到json文件中，保存在out_dir目录下
		bool					 serial_meta;
		// 是否不生成任何文件
		bool                     dry_run;
	};

	class LUX_CXX_PUBLIC GeneratorHelper
	{
	public:
		// -------------------------------------------------------------------------
		// Attempt to see if a file is inside one of the 'includeList' directories
		// If so, return the relative path, otherwise std::nullopt
		// -------------------------------------------------------------------------
		static std::optional<std::filesystem::path>
		findRelativeIncludePath(const std::filesystem::path& metaFile, const std::vector<std::filesystem::path>& includeList);

		// ------------------------------------------------------
		// Utility to parse the compile_commands "command"/"arguments" fields
		// and split them into tokens
		// ------------------------------------------------------
		static std::vector<std::string> 
		splitCommand(const std::string& cmd);

		// ------------------------------------------------------
		// Convert a baseDir + path to an absolute path
		// ------------------------------------------------------
		static std::filesystem::path 
		makeAbsolute(const std::filesystem::path& baseDir, const std::filesystem::path& p);

		// ------------------------------------------------------
		// Check if a string is a Windows-style absolute path
		// e.g., "C:\..." or "D:/..."
		// ------------------------------------------------------
		static bool isStandardAbsolute(const std::string& s);

		static std::string visibility2Str(lux::cxx::dref::EVisibility visibility);

		static std::string truncateAtLastParen(const std::string& funcName);

		static std::vector<std::filesystem::path> 
		fetchIncludePaths(const std::filesystem::path& compile_command_path, const std::filesystem::path& source_file_path);

		static std::vector<std::string> 
		convertToDashI(const std::vector<std::filesystem::path>& paths);

		static void 
		loadGeneratorConfig(const std::string& filename, GeneratorConfig& config);
	};
}

