#pragma once

// <version> exposes the __cpp_lib_* feature-test macros — they are undefined until
// a standard header pulls them in, so this MUST precede the sniff below. Without it
// the check silently fails on a conforming C++23 toolchain (alias falls to tl::).
#include <version>

// Use the feature-test macro (+ __has_include) rather than __cplusplus: on MSVC
// __cplusplus stays 199711L unless /Zc:__cplusplus is set, so a raw __cplusplus
// check would never select std::expected even under /std:c++23.
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L && __has_include(<expected>)
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
