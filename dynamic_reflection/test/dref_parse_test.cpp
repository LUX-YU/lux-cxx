#include <clang_config.h>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/runtime/Attribute.hpp>
#include <iostream>
#include <vector>
#include <filesystem>
#include <array>

// 使用 std::filesystem 定义文件路径类型
using FilePath = std::filesystem::path;
using FilePathList = std::vector<FilePath>;

using namespace lux::cxx::dref;

enum class LUX_REFL(static_reflection) TestEnum
{
    FIRST_ENUMERATOR = 100,
    SECOND_ENUMERATOR = 200,
    THIRD_ENUMERATOR = 4,
};

namespace lux::cxx::dref{
template<>
class type_meta<TestEnum>
{
public:
    static constexpr size_t size = 3;
    using value_type   = std::underlying_type_t<TestEnum>;
    using element_type = std::pair<const char*, std::underlying_type_t<TestEnum>>;

    static constexpr std::array<const char*, size> keys{
        "FIRST_ENUMERATOR",
        "SECOND_ENUMERATOR",
        "THIRD_ENUMERATOR",
    };

    static constexpr std::array<value_type, size> values{
        100,
        200,
        4,
    };

    static constexpr std::array<element_type, size> elements{
        element_type{"FIRST_ENUMERATOR", 100},
        element_type{"SECOND_ENUMERATOR", 200},
        element_type{"THIRD_ENUMERATOR", 4},
    };

    static constexpr const char* toString(TestEnum value)
    {
        switch (value)
        {
        case TestEnum::FIRST_ENUMERATOR:
            return "FIRST_ENUMERATOR";
        case TestEnum::SECOND_ENUMERATOR:
            return "SECOND_ENUMERATOR";
        case TestEnum::THIRD_ENUMERATOR:
            return "THIRD_ENUMERATOR";
        }
        return "";
    }
};
}

int main(int argc, char* argv[])
{
    // 需要解析的文件路径
    static auto file_wait_for_parsing = "D:/CodeBase/lux-projects/lux-cxx/dynamic_reflection/test/test_header.hpp";

    const CxxParser cxx_parser;

    // 构造编译选项
    std::vector<std::string> options;
    // 添加预定义的 C++ 标准和 include 路径（这些宏由你的环境或配置文件定义）
    options.emplace_back(__LUX_CXX_INCLUDE_DIR_DEF__);
    options.emplace_back("--std=c++20");

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

    /*
    const auto& type_meta_list = data.typeMetaList();
    std::cout << "-----------------------Type Meta List-----------------------" << std::endl;
    for (const auto& type_meta : type_meta_list)
    {
        std::cout << "Type:          " << type_meta.name << std::endl;
        std::cout << "Kind:          " << kind2str(type_meta.type_kind) << std::endl;
        std::cout << "Size:          " << type_meta.size << std::endl;
        std::cout << "Align:         " << type_meta.align << std::endl;
        std::cout << std::endl;
    }

    std::cout << "----------------------- Marked Declaration List -----------------------" << std::endl;
    for (const auto& decl : data.markedClassDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }

    for (const auto& decl : data.markedEnumerationDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }

    for (const auto& decl : data.markedFunctionDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }

    std::cout << "---------------------- Unmarked Declaration List ----------------------" << std::endl;
    for (const auto& decl : data.unmarkedClassDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }

    for (const auto& decl : data.unmarkedEnumerationDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }

    for (const auto& decl : data.unmarkedFunctionDeclarationList())
    {
        show_declaration_info(&decl);
        std::cout << std::endl;
    }
    */

    return 0;
}
