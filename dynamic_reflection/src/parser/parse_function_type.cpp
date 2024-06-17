#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/dref/runtime/MetaUnit.hpp>

#include <lux/cxx/lan_model/types/function_type.hpp>

#include <cstring>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	FunctionType* CxxParserImpl::TParseTypeMeta<ETypeMetaKind::FUNCTION>(const Type& type, FunctionType* type_meta)
	{
		return type_meta;
	}
}
