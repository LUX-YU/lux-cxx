#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <tuple>
#include <lux/cxx/compile_time/extended_type_traits.hpp>

namespace lux::cxx
{
	template <class T>
    struct is_tuple : std::false_type {};

    template <class... Args>
    struct is_tuple<std::tuple<Args...>> : std::true_type {};

    template <class T>
    inline constexpr bool is_tuple_v = is_tuple<T>::value;

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
			auto handler_ = []<typename U, typename FUNC_, size_t... I>(FUNC_ && func_, U && tuple_, std::index_sequence<I...>) {
				(func_.template operator() < std::tuple_element_t<I, TupleType>, I > (std::get<I>(tuple_)), ...);
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
			auto handler_ = []<typename U, typename FUNC_, size_t... I>(FUNC_ && func_, std::index_sequence<I...>) {
				(func_.template operator() < std::tuple_element_t<I, TupleType>, I > (), ...);
			};
			handler_.template operator() < T, FUNC > (
				std::forward<FUNC>(func),
				std::make_index_sequence<std::tuple_size_v<TupleType>>{}
			);
		}
	};

	// tuple has type
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

	// tuple type index
	template<typename Tuple, typename T> struct tuple_type_index;
	template<typename T, typename... Types>
	struct tuple_type_index<std::tuple<T, Types...>, T> {
		static constexpr size_t value = 0;
	};

	template<typename U, typename T, typename... Types>
	struct tuple_type_index<std::tuple<U, Types...>, T> {
		static constexpr size_t value = 1 + tuple_type_index<std::tuple<Types...>, T>::value;
	};

	template<typename Tuple, typename T>
	static inline constexpr size_t tuple_type_index_v = tuple_type_index<Tuple, T>::value;

	// remove repeated types
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

	// merge_and_remove_duplicates
	template <typename... Tuples>
	struct merge_and_remove_duplicates;

	template <typename... Ts>
	struct merge_and_remove_duplicates<std::tuple<Ts...>>
	{
		using type = typename remove_repeated_types<Ts...>::tuple_type;
	};

	template <typename Tuple1, typename Tuple2, typename... Rest>
	struct merge_and_remove_duplicates<Tuple1, Tuple2, Rest...>
	{
		using merged_tuple = decltype(std::tuple_cat(std::declval<Tuple1>(), std::declval<Tuple2>()));
		using type = typename merge_and_remove_duplicates<merged_tuple, Rest...>::type;
	};

	template <typename... Tuples>
	using merge_and_remove_duplicates_t = typename merge_and_remove_duplicates<Tuples...>::type;

	template<typename Tuple, typename T>
	struct tuple_remove;

	template <typename T>
    struct tuple_remove<std::tuple<>, T>
    {
        using type = std::tuple<>;
    };

    template <typename Head, typename... Tail, typename T>
    struct tuple_remove<std::tuple<Head, Tail...>, T>
    {
    private:
        using tail_removed = typename tuple_remove<std::tuple<Tail...>, T>::type;
    public:
        using type = std::conditional_t<
            std::is_same_v<Head, T>,
            tail_removed,
            decltype(std::tuple_cat(std::declval<std::tuple<Head>>(),
                                    std::declval<tail_removed>()))
        >;
    };

    template <typename Tup, typename T>
    using tuple_remove_t = typename tuple_remove<Tup,T>::type;

	template<typename Tuple, typename T>
	struct tuple_append;

	template <typename... Ts, typename T>
    struct tuple_append<std::tuple<Ts...>, T>
    {
        using type = std::tuple<Ts..., T>;
    };

    template <typename Tup, typename T>
    using tuple_append_t = typename tuple_append<Tup, T>::type;
}
