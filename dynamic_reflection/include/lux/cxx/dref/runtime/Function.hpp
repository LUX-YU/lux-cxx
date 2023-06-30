#pragma once
#include "MetaType.hpp"

#include "Instance.hpp"
#include "CallableHelper.hpp"
#include <nameof.hpp>

namespace lux::cxx::dref::runtime
{
	class Function : public CallableHelper
	{
	public:
		using InvokerPtr = void (*)(void** args);

		explicit Function(FunctionTypeMeta* const meta): _meta(meta){}

		LUX_CXX_PUBLIC bool invoke();
		LUX_CXX_PUBLIC bool invoke(FuncArg ret);

		template<typename T, typename... Args>
		bool tInvoke(T&& ret, Args&&... args)
		{
			if (_meta->result_info->type_id != ret.type_id)
				return false;

			if (!tArgsCheck(_meta->parameters_list, std::forward<Args>(args)...))
				return false;

			auto func_ptr = reinterpret_cast<InvokerPtr>(_meta->func_wrap_ptr);

			std::array<void*, sizeof...(Args) + 1> _args{ret.data, args.data...};

			func_ptr((void**)&_args);
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
