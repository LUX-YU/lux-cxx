#pragma once
#include <lux/cxx/dref/meta/MetaType.hpp>

#include "Instance.hpp"
#include "CallableHelper.hpp"
#include <nameof.hpp>

namespace lux::cxx::dref
{
	class Function : public CallableHelper
	{
	public:
		using InvokerPtr = void (*)(void** args);

		explicit Function(FunctionTypeMeta* const meta): _meta(meta){}

		template<typename T, typename... Args>
		void tInvokeNoCheck(T&& ret, Args&&... args)
		{
			auto func_ptr = reinterpret_cast<InvokerPtr>(_meta->func_wrap_ptr);

			std::array<void*, sizeof...(Args) + 1> _args{ret.data, args.data...};

			func_ptr((void**)&_args);
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
		
	protected:

		FunctionTypeMeta* const func_mata() const
		{
			return _meta;
		}

		FunctionTypeMeta* const _meta;
	};
}
