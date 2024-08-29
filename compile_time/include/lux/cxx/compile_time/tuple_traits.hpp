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
		static constexpr void handle_(T&& tuple, FUNC&& func, std::index_sequence<I...>)
		{
			(func(std::get<I>(tuple)), ...);
		}

	public:
		// same as std::apply
		template<typename T, typename FUNC>
		static constexpr void for_each(T&& tuple, FUNC&& func)
		{
			using TupleType = std::remove_reference_t<std::remove_cv_t<T>>;
			handle_(
				std::forward<T>(tuple),
				std::forward<FUNC>(func),
				std::make_index_sequence<std::tuple_size_v<TupleType>>{}
			);
		}

		/* FUNC: only accept template lambda and template function now ?
		 * example:
		 * lux::cxx::tuple_traits::for_each_type<test_tuple>(
		 *     []<typename T, size_t I>
		 *     {
		 *			// ...
		 *     },
		 *	   tuple_install
		 * );
		*/
		template<typename T, typename FUNC>
		static constexpr void for_each_type(FUNC&& func, T&& tuple)
		{
			using TupleType = std::remove_reference_t<std::remove_cv_t<T>>;
			auto handler_ = []<typename U, typename FUNC_, size_t... I>(FUNC_&& func_, U&& tuple_, std::index_sequence<I...>) {
				(func_.template operator() < std::tuple_element_t<I, TupleType>, I >(std::get<I>(tuple_)), ...);
			};
			handler_.template operator() < T, FUNC > (
				std::forward<FUNC>(func),
				std::forward<T>(tuple),
				std::make_index_sequence<std::tuple_size_v<TupleType>>{}
			);
		}

		/* FUNC: only accept template lambda and template function now ?
		 * example:
		 * lux::cxx::tuple_traits::for_each_type<test_tuple>(
		 *     []<typename T, size_t I>
		 *     {
		 *			// ...
		 *     }
		 * );
		*/
		template<typename T, typename FUNC>
		static constexpr void for_each_type(FUNC&& func)
		{
			using TupleType = std::remove_reference_t<std::remove_cv_t<T>>;
			auto handler_ = []<typename U, typename FUNC_, size_t... I>(FUNC_&& func_, std::index_sequence<I...>) {
				(func_.template operator() < std::tuple_element_t<I, TupleType>, I >(), ...);
			};
			handler_.template operator() < T, FUNC > (
				std::forward<FUNC>(func),
				std::make_index_sequence<std::tuple_size_v<TupleType>>{}
			);
		}
	};

	template<typename TupleType, typename U>
	struct tuple_has_type
	{
	private:
		template<size_t... I>
		static constexpr bool func_(std::index_sequence<I...>)
		{
			return (std::is_same_v<std::tuple_element_t<I, TupleType>, U> || ...);
		}

		static constexpr bool func_()
		{
			return func_(
				std::make_index_sequence<std::tuple_size_v<TupleType>>()
			);
		}

	public:
		static constexpr bool value = func_();
	};

	template<typename TupleType, typename U> 
	static inline constexpr bool tuple_has_type_v = tuple_has_type<TupleType, U>::value;

	template<typename... T>
	struct remove_repeated_types
	{
	private:
		template<typename ... input_t>
		using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

		template<typename CurrentTupleType, typename U>
		static constexpr auto strategy(U&&)
		{
			if constexpr (tuple_has_type_v<CurrentTupleType, U>)
			{
				return CurrentTupleType{};
			}
			else
			{
				return tuple_cat_t<CurrentTupleType, std::tuple<U>>{};
			}
		}

		template<typename CurrentTupleType, typename U, typename... Args>
		static constexpr auto strategy(U&&, Args&&...)
		{
			if constexpr (tuple_has_type_v<CurrentTupleType, U>)
			{
				return strategy<CurrentTupleType, Args...>(Args{}...);
			}
			else
			{
				using next_tuple_type = tuple_cat_t<CurrentTupleType, std::tuple<U>>;
				return strategy<next_tuple_type, Args...>(Args{}...);
			}
		}
	public:

		using tuple_type = decltype(strategy<std::tuple<>, T...>(T{}...));
	};
}
	