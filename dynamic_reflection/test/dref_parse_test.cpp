/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
