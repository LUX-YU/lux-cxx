#include "lux/cxx/arguments/Arguments.hpp"

#include <iostream>
#include <chrono>

int main(int argc, char* argv[])
{
	using namespace lux::cxx;

	// calculate all time spent in the program

	ArgumentParser parser;
	parser.addArgument<std::string_view>("n", "name").setRequired(true).setDescription("Name of the user").setDefaultValue("John Doe");
	parser.addArgument<int>("a", "age").setRequired(true).setDescription("Age of the user").setDefaultValue(18);
	parser.addArgument<double>("h", "height").setRequired(false).setDescription("Height of the user").setDefaultValue(165.5f);
	parser.addArgument<std::vector<double>>("v", "vector").setRequired(false).setDescription("Vector of doubles");
	parser.addArgument<bool>("f", "flag").setDescription("Flag test");

	const std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	if(const auto rst = parser.parse(argc, argv); !rst.has_value())
	{
		std::cout << "Error parsing arguments" << std::endl;
		return EXIT_FAILURE;
	}

	std::string_view name;
	int age;
	parser.getArgument("name", name);
	parser.getArgument("a", age);
	const auto& vector			= parser["vector"].as<std::vector<double>>();
	const double height			= parser["height"].as<double>();
	const bool flag				= parser["flag"].as<bool>();

	const std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	const auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
	std::cout << "Time spent: " << time_span.count() * 1000 << " ms" << std::endl;

	std::cout << "Name: " << name << std::endl;
	std::cout << "Age: " << age << std::endl;
	std::cout << "Height: " << height << std::endl;
	std::cout << "Flag: " << flag << std::endl;

	std::cout << "Vector: ";
	for(auto& v : vector)
		std::cout << v << " ";
	std::cout << std::endl;

	return 0;
}