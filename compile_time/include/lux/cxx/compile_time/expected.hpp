#pragma once

#if __cplusplus > 202002L
#include <expected>
#else
#include "expected_impl.hpp"
namespace std {
#pragma message ("Warning : std::expected is currently a alias to tl::expected, this will change in the future")
	template <typename T, typename E> using expected   = tl::expected<T, E>;
	template <typename T>			  using unexpected = tl::unexpected<T>;

	using unexpect_t = tl::unexpect_t;
	static constexpr unexpect_t unexpect{};

	template<typename T> using bad_expected_access	   = tl::bad_expected_access<T>;
}
#endif
