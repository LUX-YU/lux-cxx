#include <lux/cxx/dref/parser/libclang_for_test.hpp>
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <clang_config.h>
#include <filesystem>
#include <iostream>

using namespace lux::cxx::dref;

int main(int argc, char* argv[])
{
    std::filesystem::path current_path = std::filesystem::path(__FILE__).parent_path();
    std::filesystem::path test_header_path = current_path / "test_header.hpp";

    std::vector<std::string_view> options{ __LUX_CXX_STANDARD__, __LUX_CXX_INCLUDE_DIR_DEF__};

    CxxParser parser;
    auto [result, meta] = parser.parse(
        test_header_path.string(),  // filename
        std::move(options), // commands
        "test_meta",
        "0.0.1"
    );

    const auto& decl_list = meta.markedDeclarationList();
    const auto& type_list = meta.typeMetaList();

    for (const auto& decl : decl_list)
    {
        std::cout << "Declaration name: " << decl->name
    		<< " Declaration type:" << static_cast<int>(decl->declaration_kind) << std::endl;
        
        std::cout << "Type name:" << decl->type->name
    		<< " Kind type:" << static_cast<int>(decl->type->type_kind)
    		<< " Is const:" << decl->type->is_const << std::endl;
    }

    std::cout << type_list.size() << std::endl;

    return 0;
}
