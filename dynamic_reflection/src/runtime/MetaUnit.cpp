#include <lux/cxx/dref/runtime/MetaUnit.hpp>
#include <lux/cxx/dref/runtime/MetaUnitImpl.hpp>

namespace lux::cxx::dref
{
	size_t MetaUnit::calculateTypeMetaId(std::string_view name)
	{
		return lux::cxx::algorithm::fnv1a(name);
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

	const std::vector<Decl*>&
	MetaUnit::markedDeclarations() const
	{
		return _impl->_data->marked_declarations;
	}
}