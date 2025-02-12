#pragma once
#include <string>
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct ReferenceType;

	struct Reference : public Entity
	{
		ReferenceType*	type;

		// optional
		std::string		name;
	};
}
