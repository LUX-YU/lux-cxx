#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <lux/cxx/visibility.h>
#include <lux/cxx/reflection/runtime/Declaration.hpp>
#include <lux/cxx/reflection/Error.hpp>

namespace lux::cxx::reflection
{
	struct GeneratorTargetFile final
	{
		std::string physical_path;
		std::string logical_path;
	};

	struct GeneratorProjection final
	{
		std::string              name;
		std::string              template_path;
		std::string              output_root;
		std::string              output_suffix;
		std::vector<std::string> custom_fields_json;
		bool                     include_relative{true};
		bool                     serial_meta{false};
		bool                     validation{false};
	};

	struct GeneratorParseJob final
	{
		// 用于标记c++声明的注解字符
		std::string              marker;
		// 编译命令文件，一般可以在构建系统中生成，对于cmake系统一般在BINARY_DIR下
		std::string              compile_commands;
		// 需要生成元信息的物理文件及其稳定逻辑路径
		std::vector<GeneratorTargetFile> target_files;
		// 同一次 parse 复用的独立输出投影
		std::vector<GeneratorProjection> projections;
		// 这个源文件不是需要生成元信息的文件，而是在compile_commands中的源文件，用于找到对应的编译命令以得到对应的编译选项
		// 如果没有提供，请在extra_compile_options中添加编译选项
		std::string              source_file;
		// 额外的编译选项，例如头文件的路径，平台特定的编译选项等
		std::vector<std::string> extra_compile_options;
		// 是否不生成任何文件
		bool                     dry_run{false};
		// 是否放行「被 include 的头中的标记声明」(ParseOptions::parse_included_marked)。
		// 默认 false 保持仅主文件;注册型模板勿开(会按 TU 重复注册可见类型)。
		bool                     parse_included_marked = false;
		// C++ 标准版本，默认 c++20
		std::string              cxx_standard = "c++20";
		// 预处理宏定义，默认 __LUX_PARSE_TIME__=1
		std::vector<std::string> preprocessor_defines = {"__LUX_PARSE_TIME__=1"};
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

		static std::string visibility2Str(lux::cxx::reflection::EVisibility visibility);

		static std::string truncateAtLastParen(const std::string& funcName);

		static Result<std::vector<std::filesystem::path>>
		fetchIncludePaths(const std::filesystem::path& compile_command_path, const std::filesystem::path& source_file_path);

		static std::vector<std::string> 
		convertToDashI(const std::vector<std::filesystem::path>& paths);

		static void 
		loadGeneratorParseJob(const std::string& filename, GeneratorParseJob& config);
	};
}

