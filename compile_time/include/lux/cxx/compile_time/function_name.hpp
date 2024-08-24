#pragma once

#if defined __clang__ || defined __GNUC__
#   define LUX_FUNCTION_NAME __PRETTY_FUNCTION__
#   define LUX_FUNCTION_NAME_PREFIX     '='
#   define LUX_FUNCTION_NAME_SUFFIX     ']'
#   define LUX_FUNCTION_MEM_NAME_PREFIX "::"
#   define LUX_FUNCTION_MEM_NAME_SUFFIX ')' 
#elif defined _MSC_VER
#   define LUX_FUNCTION_NAME __FUNCSIG__
#   define LUX_FUNCTION_NAME_PREFIX     '<'
#   define LUX_FUNCTION_NAME_SUFFIX     '>'
#   define LUX_FUNCTION_MEM_NAME_PREFIX "->"
#   define LUX_FUNCTION_MEM_NAME_SUFFIX ''
#endif
