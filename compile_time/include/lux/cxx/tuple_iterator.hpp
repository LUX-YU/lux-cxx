#pragma once
#include <tuple>
#include <lux/cxx/extended_type_traits.hpp>

namespace lux::cxx
{
	template<class T>
	struct is_tuple : public is_template_of<std::tuple, T>{};

	template<class T> constexpr bool is_tuple_v = is_tuple<T>::value;

	template<>
	struct TupleIterator
	{

	};
}