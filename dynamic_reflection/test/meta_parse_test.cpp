#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>

using namespace lux::cxx::dref;

static std::filesystem::path file_dir(const std::string& filePath) {
    size_t pos = filePath.find_last_of("/\\");
    if (pos == std::string::npos) {
        return "";
    }
    std::string withoutFile = filePath.substr(0, pos);
    return withoutFile;
}

int main(int argc, char* argv[])
{
	std::filesystem::path test_file_dir    = file_dir(__FILE__);
	std::filesystem::path executable_path  = std::filesystem::current_path();
    std::filesystem::path executable_dir   = file_dir(argv[0]);
    std::filesystem::path target_file      = test_file_dir / "test_header.hpp";
    std::filesystem::path dref_project_dir = test_file_dir.parent_path();

	std::vector<std::string> compile_commands = {
		"-D__LUX_PARSE_TIME__=1",
		"-std=c++20",
		"-I" + (dref_project_dir / "include").string(),
	};

    ParseOptions options{
		.name          = "whatever",
		.version       = "whatever",
		.marker_symbol = "marked",
        .commands	   = std::move(compile_commands),
        .pch_file      = "Not Support now",
    };

	CxxParser parser(options);
	parser.setOnParseError(
		[](const std::string& error) {
			std::cerr << error << std::endl;
		}
	);
    auto[rst, meta] = parser.parse(target_file.string());
	if (rst != EParseResult::SUCCESS)
	{
		std::cerr << "Parse failed" << std::endl;
		return -1;
	}

    auto meta_json_str = meta.toJson().dump(4);
	auto out_file = executable_dir / "out.json";
	std::ofstream out(out_file);
	if (!out.is_open())
	{
		std::cerr << "Failed to open output file: " << out_file << std::endl;
		return -1;
	}

	out << meta_json_str;
	out.close();

	return 0;
}
