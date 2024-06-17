#pragma once
#include "Instance.hpp"

namespace lux::cxx::dref
{
	class FundamentalTypeInstance : public Instance
	{
	public:
		explicit FundamentalTypeInstance(const LuxDRefObject& object)
			: Instance(object) {}

		template<typename T>
		T* as_ptr() requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}

		template<typename T>
		const T* const as_ptr() const requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}

		template<typename T>
		const T* const as_ptr_hard() const requires std::is_fundamental_v<T>
		{
			if (sizeof(T) == _dref_object.type_meta->size)
				return reinterpret_cast<T*>(_dref_object.object);

			return nullptr;
		}
	};
}
