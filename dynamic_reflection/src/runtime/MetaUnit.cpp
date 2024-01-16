#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>

namespace lux::cxx::dref
{
#define CASE_ITEM(name)\
	case EDeclarationKind::name:\
		return "DCL_KIND_" #name;

	std::string declKind2Str(EDeclarationKind kind)
	{
		switch (kind)
		{
			CASE_ITEM(CLASS)
			CASE_ITEM(ENUMERATION)
			CASE_ITEM(FUNCTION)
			CASE_ITEM(PARAMETER)

			CASE_ITEM(MEMBER_DATA)
			CASE_ITEM(MEMBER_FUNCTION)
			CASE_ITEM(CONSTRUCTOR)
			CASE_ITEM(DESTRUCTOR)
		}
		return "DCL_KIND_UNKNOWN";
	}

	size_t MetaUnit::calculateTypeMetaId(const char* name)
	{
		return hash(name);
	}

	size_t MetaUnit::calculateTypeMetaId(TypeMeta* const meta)
	{
		return calculateTypeMetaId(meta->name);
	}

	size_t MetaUnit::calculateDeclarationId(EDeclarationKind kind, const char* name)
	{
		return hash(name + declKind2Str(kind));
	}

	size_t MetaUnit::calculateDeclarationId(Declaration* const decl)
	{
		return calculateDeclarationId(decl->kind, decl->name);
	}

	MetaUnit::MetaUnit()
		:_impl(nullptr)
	{
		
	}

	MetaUnit::MetaUnit(MetaUnit&& other) noexcept
	{
		_impl = std::move(other._impl);
	}

	MetaUnit& MetaUnit::operator=(MetaUnit&& other) noexcept
	{
		_impl = std::move(other._impl);
		return *this;
	}

	MetaUnit::MetaUnit(std::unique_ptr<MetaUnitImpl> impl)
	{
		_impl = std::move(impl);
	}

	MetaUnit::~MetaUnit() = default;

	bool MetaUnit::isValid() const
	{
		if (_impl == nullptr) return false;

		return _impl->_data != nullptr;
	}

	size_t MetaUnit::id() const
	{
		return _impl->_id;
	}

	const std::string& MetaUnit::name() const
	{
		return _impl->_name;
	}

	const std::string& MetaUnit::version() const
	{
		return _impl->_version;
	}

	const std::vector<Declaration*>& MetaUnit::markedDeclarationList() const
	{
		return _impl->_data->_markable_decl_list;
	}

	const Declaration* MetaUnit::findDeclarationByName(EDeclarationKind kind, const std::string& name) const
	{
		if (_impl->_data == nullptr) 
			return nullptr;

		auto& map = _impl->_data->_markable_decl_map;

		size_t id = calculateDeclarationId(kind, name.c_str());
		return map.count(id) > 0 ? map[id] : nullptr;
	}

	const std::vector<TypeMeta*>& MetaUnit::typeMetaList() const
	{
		return _impl->_data->_meta_type_list;
	}

	const TypeMeta* MetaUnit::findTypeMetaByName(std::string_view name) const
	{
		if (_impl->_data == nullptr)
			return nullptr;

		auto& map = _impl->_data->_meta_type_map;

		size_t id = calculateTypeMetaId(name.data());
		return map.count(id) > 0 ? map[id] : nullptr;
	}
}