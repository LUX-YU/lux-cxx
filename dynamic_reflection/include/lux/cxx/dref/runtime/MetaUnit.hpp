#pragma once
#include <lux/cxx/visibility.h>
#include <unordered_map>
#include <vector>
#include "MetaType.hpp"

namespace lux::cxx::dref::runtime
{
	class MetaUnit
	{
		friend class MetaRegistry;	// runtime
		friend class CxxParserImpl; // parse time 
	public:
		MetaUnit(std::string unit_name, std::string unit_version);

		~MetaUnit();
	
		size_t metaNumber() const;

		TypeMeta* findMetaByTypeName(const std::string& name);

		TypeMeta* findMetaByTypeID(size_t id);

		bool addMeta(TypeMeta*);

	private:
		size_t										_id;
		std::string									_name;
		std::string									_version;
		std::vector<TypeMeta*>						_meta_list;
		std::unordered_map<std::size_t, TypeMeta*>	_meta_map;
	};
}
