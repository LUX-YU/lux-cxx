#pragma once
#include <type_traits>
#include "Class.hpp"
#include "Function.hpp"
#include <lux/cxx/lan_model/object.hpp>

namespace lux::cxx::dref
{
	using LuxDRefObject = lux::cxx::lan_model::Object;

<<<<<<< HEAD
	constexpr inline LuxDRefObject null_dref_object{
		lux::cxx::lan_model::null_object()
	};
=======
	static constexpr inline LuxDRefObject null_dref_object{nullptr, nullptr};
>>>>>>> 4d81ad501c51cc725108451331bc99a390d21107

	static constexpr bool is_dref_obj_vaild(const LuxDRefObject& obj)
	{
		return obj.type == nullptr || obj.name == nullptr;
	}

	class Instance
	{
	public:
		~Instance()
		{
			auto _meta = static_cast<DataTypeMeta*>(_dref_object.type_meta);
			Function destructor(_meta->destructor_meta);
			destructor.tInvoke(
				empty_func_arg,
				LUX_ARG(void*, _dref_object.object)
			);
		}

		DataTypeMeta* const metaType() const
		{
			return _dref_object.type_meta;
		}

	protected:
		Instance(const LuxDRefObject& object)
			: _dref_object(object) {}

		LuxDRefObject _dref_object;
	};
}
