#pragma once
#include "function.hpp"

namespace lux::cxx::lan_model
{
	struct MemberFunction : public Function
	{
		bool	is_virtual;
	};
}
