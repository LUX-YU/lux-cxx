#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	void CxxParserImpl::TParseTypeMeta<ETypeMetaKind::CLASS>(const Type& type, TypeMeta* class_meta)
	{

	}
}
