#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <cstring>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	EnumerationType* CxxParserImpl::TParseTypeMeta<ETypeMetaKind::ENUMERATION>(const Type& type, EnumerationType* type_meta)
	{
		return type_meta;
	}
}
