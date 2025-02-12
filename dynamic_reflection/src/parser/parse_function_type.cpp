#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <lux/cxx/lan_model/types/function_type.hpp>

#include <cstring>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::FUNCTION>(const Type& type, TypeMeta* type_meta)
	{
	}
}
