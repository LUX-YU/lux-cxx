#pragma once
#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/algotithm/hash.hpp>
#include <lux/cxx/lan_model/types.hpp>

#include <unordered_map>
#include <deque>

namespace lux::cxx::dref
{
	struct DeclarationLists
	{
		std::deque<lan_model::ClassDeclaration>			class_decl_list;
		std::deque<lan_model::FunctionDeclaration>		function_decl_list; // global functions
		std::deque<lan_model::EnumerationDeclaration>	enumeration_decl_list;
	};

	struct MetaUnitData
	{
		// marked declarations
		DeclarationLists								 marked_decl_lists;
		DeclarationLists								 unmarked_decl_lists;

		std::unordered_map<std::size_t, Declaration*>    decl_map;

		// type meta
		std::deque<lan_model::Type>					     type_list;
		std::unordered_map<std::size_t, TypeMeta*>       type_map;
	};

	class MetaUnitImpl
	{
	public:
		friend class MetaUnit; // parse time 

		MetaUnitImpl(std::unique_ptr<MetaUnitData> data, std::string unit_name, std::string unit_version)
			: _data(std::move(data)), _name(std::move(unit_name)), _version(std::move(unit_version))
		{
			const std::string id_str = _name + _version;
			_id = lux::cxx::algorithm::fnv1a(id_str);
		}

		~MetaUnitImpl() = default;

	private:
		size_t							_id;
		std::unique_ptr<MetaUnitData>	_data;
		std::string						_name;
		std::string						_version;
	};
}
