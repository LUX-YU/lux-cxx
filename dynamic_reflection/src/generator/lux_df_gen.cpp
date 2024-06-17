#include <lux/cxx/dref/parser/CxxParser.hpp>
#include "MetaUnitSerializer.hpp"
#include <iostream>
#include <vector>
#include <filesystem>
#include <sstream>

using FilePath		= std::filesystem::path;
using FilePathList	= std::vector<FilePath>;

#define __LUX_CXX_INCLUDE_DIR__	"-ID:/CodeBase/lux-install/include"
#define __LUX_CXX_STANDARD__	"-std=c++20"

static void parseArgument(int argc, char* argv[], FilePathList& file_list, FilePath& template_file_path)
{

}

static void show_declaration_info(lux::cxx::lan_model::Declaration* decl, uint8_t indent_num = 0)
{
	using namespace lux::cxx::lan_model;
	std::stringstream ss;
	for (uint8_t i = 0; i < indent_num; i++)
	{
		ss << "\t";
	}
	auto indent = ss.str();

	std::cout << indent << "Name: " << decl->name << std::endl;
	if(decl->attribute)
		std::cout << indent << "Attr: " << decl->attribute << std::endl;
	switch (decl->declaration_kind)
	{
		case EDeclarationKind::CLASS:
		{
			std::cout << indent << "Kind: " << "CLASS" << std::endl;
			auto declaration = static_cast<ClassDeclaration*>(decl);
			for (size_t i = 0; i < declaration->base_class_num; i++)
			{
				std::cout << indent << "Base Class " << i << std::endl;
				show_declaration_info(declaration->base_class_decls[i], indent_num + 1);
			}
			for (size_t i = 0; i < declaration->constructor_num; i++)
			{
				std::cout << indent << "Constructor " << i << std::endl;
				show_declaration_info(declaration->constructor_decls[i], indent_num + 1);
			}
			for (size_t i = 0; i < declaration->field_num; i++)
			{
				std::cout << indent << "Field " << i << std::endl;
				show_declaration_info(declaration->field_decls[i], indent_num + 1);
			}
			for (size_t i = 0; i < declaration->member_function_num; i++)
			{
				std::cout << indent << "Member function " << i << std::endl;
				show_declaration_info(declaration->member_function_decls[i], indent_num + 1);
			}
			for (size_t i = 0; i < declaration->static_member_function_num; i++)
			{
				std::cout << indent << "Static function " << i << std::endl;
				show_declaration_info(declaration->static_member_function_decls[i], indent_num + 1);
			}
			break;
		}
		case EDeclarationKind::ENUMERATION :
		{
			std::cout << indent << "Kind: " << "ENUMERATION" << std::endl;
			auto declaration = static_cast<EnumerationDeclaration*>(decl);
			for (size_t i = 0; i < declaration->enumerators_num; i++)
			{
				std::cout << "Enumerator " << i << std::endl;
				std::cout << indent << "Name " << declaration->enumerators[i]->name << std::endl;
				std::cout << indent << "Value " << declaration->enumerators[i]->value << std::endl;
			}
			break;
		}
		case EDeclarationKind::FUNCTION:
		{
			std::cout << indent << "Kind: " << "FUNCTION" << std::endl;
			auto declaration = static_cast<FunctionDeclaration*>(decl);
			for (size_t i = 0; i < declaration->parameter_number; i++)
			{
				std::cout << indent << "Parameter " << i << std::endl;
				show_declaration_info(declaration->parameter_decls[i], indent_num + 1);
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
			auto declaration = static_cast<FieldDeclaration*>(decl);
			std::cout << indent << "Type: " << decl->type->name << std::endl;
			std::cout << indent << "OFFSET: " << declaration->offset << std::endl;
			break;
		}
		case EDeclarationKind::MEMBER_FUNCTION:
		{
			std::cout << indent << "Kind: " << "MEMBER_FUNCTION" << std::endl;
			auto declaration = static_cast<MemberFunctionDeclaration*>(decl);
			std::cout << indent << "IS_STATIC: " << declaration->is_static << std::endl;
			std::cout << indent << "IS_VIRTUAL: " << declaration->is_virtual << std::endl;
			for (size_t i = 0; i < declaration->parameter_number; i++)
			{
				std::cout << indent << "Parameter " << i << std::endl;
				show_declaration_info(declaration->parameter_decls[i], indent_num + 1);
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
	}
}

int main(int argc, char* argv[])
{
	FilePathList file_path_list;
	FilePath	 template_file_path;
	// parse argument
	parseArgument(argc, argv, file_path_list, template_file_path);

	static const char* file_wait_for_parsing = "D:/CodeBase/lux-projects/lux-tests/cxx_test/dynamic_reflection/test_header.hpp";

	lux::cxx::dref::CxxParser		cxx_parser;
	std::vector<std::string_view>   options{ __LUX_CXX_STANDARD__, __LUX_CXX_INCLUDE_DIR__};
	auto [parse_rst, data] = cxx_parser.parse(file_wait_for_parsing, std::move(options), "test_unit", "1.0.0");

	if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS)
	{
		return 0;
	}

	using namespace lux::cxx::lan_model;
	const auto& decl_list = data.markedDeclarationList();

	for (auto decl : decl_list)
	{
		show_declaration_info(decl);
		std::cout << std::endl;
	}

	// lux::cxx::dref::MetaUnitSerializer serializer;
	// serializer.serialize(file_wait_for_parsing, meta_unit);

	return 0;
}
