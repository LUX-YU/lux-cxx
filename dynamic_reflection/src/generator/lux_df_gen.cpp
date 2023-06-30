#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <vector>
#include <filesystem>

using FilePath = std::filesystem::path;
using FilePathList = std::vector<FilePath>;

static void parseArgument(int argc, char* argv[], FilePathList& file_list, FilePath& template_file_path)
{

}

int main(int argc, char* argv[])
{
	FilePathList file_path_list;
	FilePath	 template_file_path;
	// parse argument
	parseArgument(argc, argv, file_path_list, template_file_path);



	return 0;
}
