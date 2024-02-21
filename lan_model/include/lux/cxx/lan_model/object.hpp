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

		void*		value;

		// optional
		const char* name;
	};

	static constexpr inline Object null_object()
	{
		Object obj;
		obj.size	= 0;
		obj.align	= 0;
		obj.type	= nullptr;
		obj.value	= nullptr;
		obj.name	= nullptr;
		return obj;
	}
}
