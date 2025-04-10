#pragma once
#include <cstddef>
#include "Declaration.hpp"

namespace lux::cxx::dref
{
	class FieldInfo
	{
	public:
		std::string_view	name;
		size_t				offset;
		EVisibility			visibility;
		size_t				index;
		bool				is_const;
	};
}

namespace lux::cxx::dref
{
	// For static reflection
	template <typename T> class type_meta;
}
