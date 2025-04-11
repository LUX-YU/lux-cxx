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

// argv[1] is a json file which contains the meta unit
int main(int argc, char* argv[])
{
	std::filesystem::path executable_path = std::filesystem::current_path();
	std::filesystem::path executable_dir = file_dir(argv[0]);

	std::ifstream in_file(argv[1]);
	if (!in_file.is_open())
	{
		std::cerr << "Failed to open input file: " << argv[1] << std::endl;
		return -1;
	}
	std::string json_str((std::istreambuf_iterator<char>(in_file)), std::istreambuf_iterator<char>());

	MetaUnit meta;
	MetaUnit::fromJson(json_str, meta);

	auto meta_json_str = meta.toJson().dump(4);
	auto out_file = executable_dir / "deserialize_out.json";
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
