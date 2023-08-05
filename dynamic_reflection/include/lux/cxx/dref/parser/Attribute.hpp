#pragma once
#define LUX_REF_MARK_PREFIX "LUX::META;"

#if defined __LUX_PARSE_TIME__
#define LUX_REF_MARK(...) __attribute__((annotate(LUX_REF_MARK_PREFIX #__VA_ARGS__)))
#else
#define LUX_REF_MARK(...)
#endif
