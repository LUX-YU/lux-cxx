#pragma once
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/algotithm/hash.hpp>

#include <unordered_map>
#include <memory>

namespace lux::cxx::dref
{
	struct MetaUnitData
	{
		// All declaration and types
		std::vector<std::unique_ptr<Decl>> declarations;
		std::vector<std::unique_ptr<Type>> types;

		std::unordered_map<std::string, Decl*>  declaration_map;
		std::unordered_map<std::string, Type*>  type_map;

		// Marked declarations
		std::vector<Decl*>				   marked_declarations;
	};

	class MetaUnitImpl
	{
	public:
		friend class MetaUnit; // parse time 

		MetaUnitImpl(std::unique_ptr<MetaUnitData> data, std::string unit_name, std::string unit_version)
			: _data(std::move(data)), _name(std::move(unit_name)), _version(std::move(unit_version))
		{
			const std::string id_str = _name + _version;
			_id = algorithm::fnv1a(id_str);
		}

		~MetaUnitImpl() = default;

	private:
		size_t		 _id;
		std::unique_ptr<MetaUnitData> _data;
		std::string	 _name;
		std::string	 _version;
	};
}
