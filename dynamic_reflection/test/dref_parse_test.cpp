#include <clang_config.h>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <iostream>
#include <vector>
#include <filesystem>
#include <sstream>

// 使用 std::filesystem 定义文件路径类型
using FilePath = std::filesystem::path;
using FilePathList = std::vector<FilePath>;

// 与之前一致的辅助函数，用于输出声明信息
static void show_declaration_info(const lux::cxx::lan_model::Declaration* decl, uint8_t indent_num = 0)
{
    using namespace lux::cxx::lan_model;
    std::stringstream ss;
    for (uint8_t i = 0; i < indent_num; i++)
    {
        ss << "\t";
    }
    const auto indent = ss.str();

    std::cout << indent << "Name: " << decl->name << std::endl;
    std::cout << indent << "Spelling: " << decl->spelling << std::endl;
    std::cout << indent << "Full Qualified name: " << decl->full_qualified_name << std::endl;
    std::cout << indent << "USR: " << decl->usr << std::endl;
    std::cout << indent << "Type: " << decl->type->name << std::endl;
    std::cout << indent << "Type Kind Name: " << kind2str(decl->type->type_kind) << std::endl;
    std::cout << indent << "Type Size: " << decl->type->size << std::endl;
    std::cout << indent << "Type Align: " << decl->type->align << std::endl;
    if (!decl->attribute.empty())
        std::cout << indent << "Attr: " << decl->attribute << std::endl;
    switch (decl->declaration_kind)
    {
    case EDeclarationKind::CLASS:
    {
        std::cout << indent << "Kind: " << "CLASS" << std::endl;
        auto declaration = static_cast<const ClassDeclaration*>(decl);
        for (size_t i = 0; i < declaration->base_class_decls.size(); i++)
        {
            std::cout << indent << "Base Class " << i << std::endl;
            show_declaration_info(declaration->base_class_decls[i], indent_num + 1);
        }
        for (size_t i = 0; i < declaration->constructor_decls.size(); i++)
        {
            std::cout << indent << "Constructor " << i << std::endl;
            show_declaration_info(&declaration->constructor_decls[i], indent_num + 1);
        }
        for (size_t i = 0; i < declaration->field_decls.size(); i++)
        {
            std::cout << indent << "Field " << i << std::endl;
            show_declaration_info(&declaration->field_decls[i], indent_num + 1);
        }
        for (size_t i = 0; i < declaration->method_decls.size(); i++)
        {
            std::cout << indent << "Member function " << i << std::endl;
            show_declaration_info(&declaration->method_decls[i], indent_num + 1);
        }
        for (size_t i = 0; i < declaration->static_method_decls.size(); i++)
        {
            std::cout << indent << "Static function " << i << std::endl;
            show_declaration_info(&declaration->static_method_decls[i], indent_num + 1);
        }
        break;
    }
    case EDeclarationKind::ENUMERATION:
    {
        std::cout << indent << "Kind: " << "ENUMERATION" << std::endl;
        auto declaration = static_cast<const EnumerationDeclaration*>(decl);
        for (size_t i = 0; i < declaration->enumerators.size(); i++)
        {
            std::cout << indent << "Enumerator " << i << std::endl;
            std::cout << indent << "Name: " << declaration->enumerators[i].name << std::endl;
            std::cout << indent << "Value: " << declaration->enumerators[i].value << std::endl;
        }
        break;
    }
    case EDeclarationKind::FUNCTION:
    {
        std::cout << indent << "Kind: " << "FUNCTION" << std::endl;
        auto declaration = static_cast<const FunctionDeclaration*>(decl);
        std::cout << indent << "Result Type: " << declaration->result_type->name << std::endl;
        for (size_t i = 0; i < declaration->parameter_decls.size(); i++)
        {
            std::cout << indent << "Parameter " << i << std::endl;
            show_declaration_info(&declaration->parameter_decls[i], indent_num + 1);
        }
        break;
    }
    case EDeclarationKind::PARAMETER:
    {
        std::cout << indent << "Kind: " << "PARAMETER" << std::endl;
        std::cout << indent << "Type: " << decl->type->name << std::endl;
        break;
    }
    case EDeclarationKind::MEMBER_DATA:
    {
        std::cout << indent << "Kind: " << "MEMBER_DATA" << std::endl;
        auto declaration = static_cast<const FieldDeclaration*>(decl);
        std::cout << indent << "Type: " << decl->type->name << std::endl;
        std::cout << indent << "OFFSET: " << declaration->offset << std::endl;
        break;
    }
    case EDeclarationKind::MEMBER_FUNCTION:
    {
        std::cout << indent << "Kind: " << "MEMBER_FUNCTION" << std::endl;
        auto declaration = static_cast<const MemberFunctionDeclaration*>(decl);
        std::cout << indent << "IS_STATIC: " << declaration->is_static << std::endl;
        std::cout << indent << "IS_VIRTUAL: " << declaration->is_virtual << std::endl;
        for (size_t i = 0; i < declaration->parameter_decls.size(); i++)
        {
            std::cout << indent << "Parameter " << i << std::endl;
            show_declaration_info(&declaration->parameter_decls[i], indent_num + 1);
        }
        break;
    }
    case EDeclarationKind::CONSTRUCTOR:
    {
        std::cout << indent << "Kind: " << "CONSTRUCTOR" << std::endl;
        break;
    }
    case EDeclarationKind::DESTRUCTOR:
    {
        std::cout << indent << "Kind: " << "DESTRUCTOR" << std::endl;
        break;
    }
    default:
        break;
    }
}

int main(int argc, char* argv[])
{
    // 需要解析的文件路径
    static const char* file_wait_for_parsing = "D:/CodeBase/lux-projects/lux-cxx/dynamic_reflection/test/test_header.hpp";

    lux::cxx::dref::CxxParser cxx_parser;

    // 构造编译选项
    std::vector<std::string_view> options;
    // 添加预定义的 C++ 标准和 include 路径（这些宏由你的环境或配置文件定义）
    options.emplace_back(__LUX_CXX_INCLUDE_DIR_DEF__);

    // 根据不同平台添加目标三元组
#if defined(_WIN32)
    options.emplace_back("--driver-mode=cl");
    options.emplace_back("-fms-compatibility");
    options.emplace_back("/std:c++20");
    options.emplace_back("-fms-compatibility-version=19.33");
    options.emplace_back("-imsvc \"D:/Development/Mircosoft/VisualStudio/VC/Tools/MSVC/14.42.34433/include\"");
    options.emplace_back("-imsvc \"C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/ucrt\"");
    options.emplace_back("-imsvc \"C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um\"");
    options.emplace_back("-imsvc \"C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/shared\"");
    options.emplace_back("-target x86_64-pc-windows-msvc");
#elif defined(__linux__)
    options.push_back("-target x86_64-unknown-linux-gnu");
    options.emplace_back("--std=c++20");
#elif defined(__APPLE__)
    options.push_back("-target x86_64-apple-macosx10.15");
    options.emplace_back("--std=c++20");
#else
    options.push_back("-target x86_64-unknown");
    options.emplace_back("--std=c++20");
#endif

    // 输出编译选项，便于调试
    std::cout << "Compile options:" << std::endl;
    for (auto& opt : options)
    {
        std::cout << "\t" << opt << std::endl;
    }

    // 调用解析函数，传入文件和编译选项
    auto [parse_rst, data] = cxx_parser.parse(
        file_wait_for_parsing,
        std::move(options),
        "test_unit", "1.0.0"
    );

    if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS)
    {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }

    using namespace lux::cxx::lan_model;

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

    return 0;
}
