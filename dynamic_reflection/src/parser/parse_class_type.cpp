#include <lux/cxx/dref/parser/CxxParserImpl.hpp>
#include <lux/cxx/lan_model/types/class_type.hpp>

namespace lux::cxx::dref
{
	using namespace ::lux::cxx::lan_model;

	template<>
	ClassType* CxxParserImpl::TParseTypeMeta<ETypeMetaKind::CLASS>(const Type& type, ClassType* class_meta)
	{
		class_meta->size		= type.typeSizeof();
		class_meta->align		= type.typeAlignof();

		return class_meta;
	}
}
