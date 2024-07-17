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

	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	auto rst = parser.parse(argc, argv);
	if(rst.has_value())
	{
		auto name					= parser.getArgument("name").value().get().to<std::string_view>();
		auto age					= parser.getArgument("a").value().get().to<int>();
		std::vector<double> vector  = parser["vector"];
		double height				= parser["height"];
		bool flag					= parser["flag"];

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		std::cout << "Time spent: " << time_span.count() * 1000 << " ms" << std::endl;

		std::cout << "Name: " << name << std::endl;
		std::cout << "Age: " << age << std::endl;
		std::cout << "Height: " << height << std::endl;
		std::cout << "Flag: " << flag << std::endl;

		std::cout << "Vector: ";
		for(auto& v : vector)
			std::cout << v << " ";
		std::cout << std::endl;
	}
	else
	{
		std::cout << "Error parsing arguments" << std::endl;
	}

	return 0;
}