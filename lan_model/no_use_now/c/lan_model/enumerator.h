#ifndef __ENUMERATOR_H__
#define __ENUMERATOR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
	const char* name;

	// may be signed or unsigned
	size_t		value;
}S_LUX_C_Enumerator;

#ifdef __cplusplus
}
#endif

#endif
