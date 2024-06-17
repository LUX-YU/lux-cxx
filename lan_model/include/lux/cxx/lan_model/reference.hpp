#pragma once
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct ReferenceType;

	struct Reference : public Entity
	{
		ReferenceType*	type;

		// optional
		const char*		name;
	};
}
