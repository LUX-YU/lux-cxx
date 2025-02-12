#pragma once
#include <cstdint>
#include <string>
#include "entity.hpp"

namespace lux::cxx::lan_model
{
	struct Type;

	struct Object : public Entity
	{
		size_t		size;
		size_t		align;
		Type*		type;

		void*		value;

		// optional
		std::string name;
	};

	static constexpr inline Object null_object()
	{
		Object obj;
		obj.size	= 0;
		obj.align	= 0;
		obj.type	= nullptr;
		obj.value	= nullptr;
		obj.name	= "";
		return obj;
	}
}
