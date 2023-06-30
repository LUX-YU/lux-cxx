#pragma once
#include "MetaType.hpp"
#include "hash.hpp"

namespace lux::cxx::dref::runtime
{
	class Instance
	{
		friend class Class;
	public:
		LUX_CXX_PUBLIC explicit Instance(DataTypeMeta* const meta);

		LUX_CXX_PUBLIC ~Instance();

		LUX_CXX_PUBLIC DataTypeMeta* const typeMeta() const;

		LUX_CXX_PUBLIC bool isValid() const;

		// cast the instance to a specified instance type
		template<typename T> 
		std::enable_if_t<std::is_base_of_v<Instance, T>, T*> castTo()
		{
			return static_cast<T*>(this);
		}

	protected:
		DataTypeMeta* const	_type_meta;
		void*				_instance;
	};
}
