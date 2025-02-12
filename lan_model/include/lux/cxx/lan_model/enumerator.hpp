#pragma once
#include "entity.hpp"
#include <cstddef>
#include <string>

namespace lux::cxx::lan_model
{
	struct Enumerator : Entity
	{
		std::string name;
		size_t		value;
	};
}
