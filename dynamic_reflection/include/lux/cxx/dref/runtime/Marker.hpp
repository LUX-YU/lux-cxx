#pragma once

#if defined __LUX_PARSE_TIME__
#	define LUX_REFL(...) __attribute__((annotate(#__VA_ARGS__)))
#else
#	define LUX_REFL(...)
#endif
