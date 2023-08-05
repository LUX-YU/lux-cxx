#pragma once
#include <tuple>
#include <lux/cxx/compile_time/extended_type_traits.hpp>

namespace lux::cxx
{
	template<class T>
	struct is_tuple : public is_type_template_of<std::tuple, T>{};

	template<class T> constexpr bool is_tuple_v = is_tuple<T>::value;

	struct tuple_traits
	{
	private:
		template<size_t... I, typename T, typename FUNC>
		static inline constexpr void __handle(T&& tuple, FUNC&& func, std::index_sequence<I...>)
		{
			(func(std::get<I>(tuple)), ...);
		}

	public:
		// same as std::apply
		template<typename T, typename FUNC>
		static inline constexpr void for_each(T&& tuple, FUNC&& func)
		{
			using TupleType = std::remove_reference_t<std::remove_cv_t<T>>;
			__handle(
				std::forward<T>(tuple),
				std::forward<FUNC>(func),
				std::make_index_sequence<std::tuple_size<TupleType>::value>{}
			);
		}

		// FUNC: only accept template lambda and template function now ?
		template<typename T, typename FUNC>
		static inline constexpr void for_each_type(FUNC&& func)
		{
			auto  __hander = []<typename U, typename _FUNC, size_t... I>(FUNC && func, std::index_sequence<I...>) {
				(func.template operator() < std::tuple_element_t<I, T> > (), ...);
			};
			__hander.template operator() < T, FUNC > (
				std::forward<FUNC>(func),
				std::make_index_sequence<std::tuple_size<T>::value>{}
			);
		}
	};
}