#pragma once
#include <cstdint>
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct Type;

	struct Object : public Entity
	{
		size_t		size;
		size_t		align;
		Type*		type;

		// optional
		const char* name;
	};
}
