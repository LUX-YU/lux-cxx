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

	std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
	auto rst = parser.parse(argc, argv);
	if(rst.has_value())
	{
		auto name	= parser.getArgument<std::string_view>("name");
		auto age	= parser.getArgument<int>("age");
		auto height = parser.getArgument<double>("height");
		auto vector = parser.getArgument<std::vector<double>>("vector");

		std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		std::cout << "Time spent: " << time_span.count() * 1000 << " ms" << std::endl;

		if(name.has_value())
			std::cout << "Name: " << name.value() << std::endl;
		if(age.has_value())
			std::cout << "Age: " << age.value() << std::endl;
		if(height.has_value())
			std::cout << "Height: " << height.value() << std::endl;
		if(vector.has_value())
		{
			std::cout << "Vector: ";
			for(auto& v : vector.value())
				std::cout << v << " ";
			std::cout << std::endl;
		}
	}
	else
	{
		std::cout << "Error parsing arguments" << std::endl;
	}

	return 0;
}