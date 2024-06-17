#pragma once
#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <filesystem>

namespace lux::cxx::dref
{
	class MetaUnitSerializer
	{
	public:
		bool serialize(const std::filesystem::path& path, const MetaUnit& unit);
	};
}
