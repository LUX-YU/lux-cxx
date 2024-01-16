#ifndef __LUX_C_TYPE_H__
#define __LUX_C_TYPE_H__
#include "type_kind.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
	E_LUX_C_TYPE_KIND		type_kind;
	const char*				name;
	uint8_t					is_const;
	uint8_t					is_volatile;
	// actually some type don't have size in standard, like reference
	// but different compiler will have different implementation
	size_t					size;
}S_LUX_C_Type;

typedef struct {
	S_LUX_C_Type			basic_type;
	S_LUX_C_Type*			element_type;
}S_LUX_C_ArrayType;

// compound type class
struct S_LUX_C_Declaration;
typedef struct {
	S_LUX_C_Type			basic_type;
	size_t					align;
	S_LUX_C_Declaration*	declaration;
}S_LUX_C_ClassType;

#ifdef __cplusplus
}
#endif

#endif
