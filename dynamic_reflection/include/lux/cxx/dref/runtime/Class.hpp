#pragma once
#include <lux/cxx/dref/meta/MetaType.hpp>
#include "Method.hpp"
#include <vector>
#include <cstdlib>

namespace lux::cxx::dref
{
	class Class
	{
	public:
		Class(ClassMeta* const meta)
			: _meta(meta){}

		size_t align() const
		{
			return _meta->align;
		}

		size_t size() const
		{
			return _meta->size;
		}

		ClassMeta* const classMeta(const volatile int&) const
		{
			return _meta;
		}

	private:
		ClassMeta* const _meta;
	};
}
