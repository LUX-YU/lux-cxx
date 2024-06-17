#pragma once
#include "entity.hpp"
#include <cstddef>

namespace lux::cxx::lan_model
{
	struct Enumerator : public Entity
	{
		const char* name;
		size_t		value;
	};
}
