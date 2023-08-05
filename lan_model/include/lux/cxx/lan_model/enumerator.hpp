#pragma once
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct Enumerator : public Entity
	{
		const char* name;
		size_t		value;
	};
}
