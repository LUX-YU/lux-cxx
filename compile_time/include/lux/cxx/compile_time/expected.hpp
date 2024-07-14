#pragma once

#if __cplusplus > 202002L
#include <expected>
namespace lux::cxx{
	template<typename T, typename E>
	using expected = std::expected<T, E>;

	template<typename T>
	using unexpected = std::unexpected<T>;

	using unexpect_t = std::unexpect_t;
	static constexpr unexpect_t unexpect{};

	template<typename T>
	using bad_expected_access = std::bad_expected_access<T>;
}
#else
#include "expected_impl.hpp"
namespace lux::cxx{
// #pragma message ("Warning : std::expected is currently a alias to tl::expected, this will change in the future")
	template <typename T, typename E> 
	using expected = tl::expected<T, E>;
	
	template <typename T> 
	using unexpected = tl::unexpected<T>;

	using unexpect_t = tl::unexpect_t;
	static constexpr unexpect_t unexpect{};

	template<typename T> 
	using bad_expected_access = tl::bad_expected_access<T>;
}
#endif
