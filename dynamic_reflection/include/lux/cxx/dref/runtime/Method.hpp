#pragma once
#include <lux/cxx/dref/meta/MetaType.hpp>

#include "CallableHelper.hpp"

namespace lux::cxx::dref
{
	class ClassInstance;

	class Method : public CallableHelper
	{
		friend class ClassInstance;
	public:
		using InvokerPtr = bool (*)(void* object, void** args);

		MethodMeta* const methodMeta()
		{
			return static_cast<MethodMeta* const>(_meta);
		}

		template<typename T, typename... Args>
		void tInvokeNoCheck(T&& ret, Args&&... args)
		{
			auto func_ptr = reinterpret_cast<InvokerPtr>(_meta->func_wrap_ptr);

			std::array<void*, sizeof...(Args) + 1> _args{ret.data, args.data...};

			func_ptr((void*)_object, (void**)&_args);
		}

		template<typename T, typename... Args>
		bool tInvoke(T&& ret, Args&&... args)
		{
			using TType = std::remove_cvref_t<T>;
			static_assert(std::is_same_v<TType, FuncArg>, "Argument type must be FuncArg");

			if (_meta->result_info->type_id != ret.type_id)
				return false;

			if (!tArgsCheck(_meta->parameters_list, args...))
				return false;

			tInvokeNoCheck(std::forward<T>(ret), std::forward<Args>(args)...);
			return true;
		}
	
	private:

		Method(MethodMeta* const meta, void* object)
			:_meta(meta), _object(object) {}

		void*			  _object;
		MethodMeta* const _meta;
	};
}

