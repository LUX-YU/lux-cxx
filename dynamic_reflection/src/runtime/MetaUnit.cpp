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
			default:
				break;
		}
		return "DCL_KIND_UNKNOWN";
	}

	size_t MetaUnit::calculateTypeMetaId(std::string_view name)
	{
		return lux::cxx::algorithm::fnv1a(name);
	}

	size_t MetaUnit::calculateTypeMetaId(TypeMeta* const meta)
	{
		return calculateTypeMetaId(meta->name);
	}

	size_t MetaUnit::calculateDeclarationId(const EDeclarationKind kind, std::string_view name)
	{
		return lux::cxx::algorithm::fnv1a(std::string(name) + declKind2Str(kind));
	}

	size_t MetaUnit::calculateDeclarationId(const Declaration* const decl)
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

	const std::deque<lan_model::ClassDeclaration>&
	MetaUnit::markedClassDeclarationList() const
	{
		return _impl->_data->marked_decl_lists.class_decl_list;
	}

	const std::deque<lan_model::FunctionDeclaration>&
	MetaUnit::markedFunctionDeclarationList() const
	{
		return _impl->_data->marked_decl_lists.function_decl_list;
	}

	const std::deque<lan_model::EnumerationDeclaration>&
	MetaUnit::markedEnumerationDeclarationList() const
	{
		return _impl->_data->marked_decl_lists.enumeration_decl_list;
	}

	const std::deque<lan_model::ClassDeclaration>&
	MetaUnit::unmarkedClassDeclarationList() const
	{
		return _impl->_data->unmarked_decl_lists.class_decl_list;
	}

	const std::deque<lan_model::FunctionDeclaration>&
	MetaUnit::unmarkedFunctionDeclarationList() const
	{
		return _impl->_data->unmarked_decl_lists.function_decl_list;
	}

	const std::deque<lan_model::EnumerationDeclaration>&
	MetaUnit::unmarkedEnumerationDeclarationList() const
	{
		return _impl->_data->unmarked_decl_lists.enumeration_decl_list;
	}

	const Declaration* MetaUnit::findDeclarationByName(EDeclarationKind kind, std::string_view name) const
	{
		if (_impl->_data == nullptr) 
			return nullptr;

		auto& map = _impl->_data->decl_map;

		const size_t id = calculateDeclarationId(kind, name);
		return map.contains(id) ? map[id] : nullptr;
	}

	const std::deque<TypeMeta>& MetaUnit::typeMetaList() const
	{
		return _impl->_data->type_list;
	}

	const TypeMeta* MetaUnit::findTypeMetaByName(std::string_view name) const
	{
		if (_impl->_data == nullptr)
			return nullptr;

		auto& map = _impl->_data->type_map;

		const size_t id = calculateTypeMetaId(name.data());
		return map.contains(id) ? map[id] : nullptr;
	}
}