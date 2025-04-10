#include <clang_config.h>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/runtime/Attribute.hpp>
#include <iostream>


int main(int argc, char* argv[])
{
    using namespace lux::cxx::dref;

    // 需要解析的文件路径
    static auto file_wait_for_parsing = "D:/CodeBase/lux-projects/lux-cxx/dynamic_reflection/test/test_header.hpp";

    const CxxParser cxx_parser;

    // 构造编译选项
    std::vector<std::string> options;
    // 添加预定义的 C++ 标准和 include 路径（这些宏由你的环境或配置文件定义）
    options.emplace_back(__LUX_CXX_INCLUDE_DIR_DEF__);
    options.emplace_back("--std=c++20");
    options.emplace_back("-D__LUX_PARSE_TIME__=1");

    // 输出编译选项，便于调试
    std::cout << "Compile options:" << std::endl;
    for (const auto& opt : options)
    {
        std::cout << "\t" << opt << std::endl;
    }

    // 调用解析函数，传入文件和编译选项
    auto [parse_rst, data] = cxx_parser.parse(
        file_wait_for_parsing,
        std::move(options),
        "test_unit", "1.0.0"
    );

    if (parse_rst != EParseResult::SUCCESS)
    {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }

    std::string json_data = data.toJson();
	std::cout << "Parsed JSON data:\n"<< json_data << std::endl;

    MetaUnit unit;
	MetaUnit::fromJson(json_data, unit);

	std::cout << "Done!" << std::endl;
	
	return 0;
}