#pragma once
#include <lux/cxx/algotithm/hash.hpp>
#include "MetaType.hpp"
#include <nameof.hpp>
#include <vector>
#include <array>

namespace lux::cxx::dref
{
	struct FunctionCallArgument
	{
		size_t	type_id{};
		void*	data{ nullptr };
	};

	using FuncArg = FunctionCallArgument;

	constexpr inline FuncArg empty_func_arg = FuncArg{ 0, nullptr };

	class CallableHelper
	{
	public:
		template<typename T>
		static inline bool TTypeCheck(size_t hash)
		{
			return THash(nameof::nameof_full_type<T>()) == hash;
		}

	private:
		template<size_t I, typename T>
		inline bool _parameter_check(const std::vector<TypeMeta*>& parameters_list)
		{
			return TTypeCheck<T>(parameters_list[I]->type_id);
		}

		template<size_t I, typename T, typename... Args>
		inline bool _parameter_check(const std::vector<TypeMeta*>& parameters_list)
		{
			constexpr size_t _type_id = THash(nameof::nameof_full_type<T>());
			return  TTypeCheck<T>(parameters_list[I]->type_id) &&
				_parameter_check<I + 1, Args...>();
		}

		template<size_t I>
		inline bool _args_check(const std::vector<TypeMeta*>& parameters_list)
		{
			return true;
		}

		template<size_t I, typename T>
		inline bool _args_check(const std::vector<TypeMeta*>& parameters_list, T&& arg)
		{
			using ArgType = std::remove_reference_t<T>;
			static_assert(std::is_same_v<ArgType, FuncArg>, "Argument type must be FuncArg");

			return arg.type_id == parameters_list[I]->type_id;
		}

		template<size_t I = 0, typename T, typename... Args>
		inline bool _args_check(const std::vector<TypeMeta*>& parameters_list, T&& arg, Args&&... args)
		{
			using ArgType = std::remove_reference_t<T>;
			static_assert(std::is_same_v<ArgType, FuncArg>, "Argument type must be FuncArg");

			return arg.type_id == parameters_list[I]->type_id &&
				_args_check<I + 1>(parameters_list, std::forward<Args>(args)...);
		}

	public:
		template<typename... Args>
		constexpr bool tParameterCheck(const std::vector<TypeMeta*>& parameters_list)
		{
			return _parameter_check<0, Args...>(parameters_list);
		}

		template<typename Ret, typename... Args>
		inline bool tFuncTypeCheck(const std::vector<TypeMeta*>& parameters_list, size_t ret_id)
		{
			return sizeof...(Args) == parameters_list.size() && // paraneter number check
				tParameterCheck<Args...>(parameters_list) && // parameter check
				TTypeCheck<Ret>(ret_id); // return type check
		}

		template<typename... Args>
		inline bool tArgsCheck(const std::vector<TypeMeta*>& parameters_list, Args&&... args)
		{
			if (parameters_list.size() == 0 && sizeof...(Args) == 0)
				return true;

			if (parameters_list.size() != sizeof...(Args))
				return false;

			return _args_check<0>(parameters_list, std::forward<Args>(args)...);
		}
	};
}

#define LUX_ARG(type_name, value)\
	::lux::cxx::dref::FunctionCallArgument{::lux::cxx::dref::THash(#type_name), std::addressof(value)}
