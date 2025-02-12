#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/enumeration.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <cstring>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::ENUMERATION>(const Type& type, TypeMeta* type_meta)
	{
	}
}
