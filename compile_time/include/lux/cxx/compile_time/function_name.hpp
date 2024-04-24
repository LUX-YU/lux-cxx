#pragma once

#if defined __clang__ || defined __GNUC__
#    define LUX_PRETTY_FUNCTION __PRETTY_FUNCTION__
#    define LUX_PRETTY_FUNCTION_PREFIX '='
#    define LUX_PRETTY_FUNCTION_SUFFIX ']'
#elif defined _MSC_VER
#    define LUX_PRETTY_FUNCTION __FUNCSIG__
#    define LUX_PRETTY_FUNCTION_PREFIX '<'
#    define LUX_PRETTY_FUNCTION_SUFFIX '>'
#endif
