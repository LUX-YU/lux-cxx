#include <lux/cxx/dref/runtime/Instance.hpp>

namespace lux::cxx::dref::runtime
{
	Instance::Instance(DataTypeMeta* const meta)
	: _type_meta(meta){
		
	}

	Instance::~Instance()
	{
		
	}

	DataTypeMeta* const Instance::typeMeta() const
	{
		return _type_meta;
	}

	bool Instance::isValid() const
	{
		return _type_meta != nullptr;
	}
}
