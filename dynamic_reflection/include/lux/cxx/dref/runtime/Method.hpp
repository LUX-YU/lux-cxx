#pragma once
#include "CallableHelper.hpp"
#include "MetaType.hpp"

namespace lux::cxx::dref::runtime
{
	class ClassMeta;
	class ClassInstance;

	struct MethodMeta : public FunctionTypeMeta
	{
		static constexpr MetaType self_meta_type = MetaType::METHOD;

		bool				is_const;
		bool				is_virtual;
		bool				is_pure_virtual;
		ClassMeta*			owner_info;
	};

	class Method : public CallableHelper
	{
	public:
		using InvokerPtr = bool (*)(void* object, void** args);

		Method(MethodMeta* const meta)
			:_meta(meta){}

		MethodMeta* const methodMeta()
		{
			return static_cast<MethodMeta* const>(_meta);
		}

		template<typename T, typename... Args>
		bool tInvoke(ClassInstance* instance, T&& ret, Args&&... args)
		{
			static_assert(std::is_same_v<T, FuncArg>, "Argument type must be FuncArg");

			if (_meta->result_info->type_id != ret.type_id)
				return false;

			if (!tArgsCheck(_meta->parameters_list, std::forward<Args>(args)...))
				return false;

			auto func_ptr =  reinterpret_cast<InvokerPtr>(_meta->func_wrap_ptr);

			std::array<void*, sizeof...(Args) + 1> _args{ret.data, args.data...};

			func_ptr((void*)instance, (void**)&_args);
			return true;
		}
	
	private:
		MethodMeta* const _meta;
	};
}

