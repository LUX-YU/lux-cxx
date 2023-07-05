#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/runtime/hash.hpp>

namespace lux::cxx::dref::runtime
{
	MetaUnit::MetaUnit(std::string unit_name, std::string unit_version)
	: _name(std::move(unit_name)), _version(std::move(unit_version))
	{
		std::string id_str = _name + _version;
		_id = fnv1a(id_str);
	}

	MetaUnit::~MetaUnit()
	{
		for (TypeMeta* meta : _meta_list)
			delete meta;
	}

	bool MetaUnit::addMeta(TypeMeta* meta)
	{
		if (meta == nullptr) 
			return false;

		if (_meta_map.count(meta->type_id))
			return true;

		meta->meta_unit_id = _id;

		_meta_list.push_back(meta);
		_meta_map[meta->type_id] = meta;

		return true;
	}

	size_t MetaUnit::metaNumber() const
	{
		return _meta_list.size();
	}

	TypeMeta* MetaUnit::findMetaByTypeName(const std::string& name)
	{
		auto id = fnv1a(name);
		return _meta_map.count(id) ? _meta_map[id] : nullptr;
	}

	TypeMeta* MetaUnit::findMetaByTypeID(size_t id)
	{
		return _meta_map.count(id) ? _meta_map[id] : nullptr;
	}
}
