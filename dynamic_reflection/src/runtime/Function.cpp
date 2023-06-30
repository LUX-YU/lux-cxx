#include <lux/cxx/dref/runtime/Function.hpp>

namespace lux::cxx::dref::runtime
{
	bool Function::invoke()
	{
		if (_meta->parameters_list.size() != 0 || _meta->result_info->type_id != TFnv1a("void"))
			return false;

		auto func_ptr = static_cast<Function::InvokerPtr>(_meta->func_wrap_ptr);
		func_ptr(nullptr);
		return true;
	}

	bool Function::invoke(FuncArg ret)
	{
		if (_meta->parameters_list.size() != 0 || _meta->result_info->type_id != ret.type_id)
			return false;

		auto func_ptr = static_cast<Function::InvokerPtr>(_meta->func_wrap_ptr);

		func_ptr(&ret.data);
		return true;
	}
}
