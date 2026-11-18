#pragma once

// clang-cl can diagnose the standard spelling on the Microsoft ABI.
#if __has_cpp_attribute(no_unique_address)
#define LUX_CXX_ARGS_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define LUX_CXX_ARGS_NO_UNIQUE_ADDRESS
#endif
