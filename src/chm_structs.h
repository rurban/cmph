#ifndef __CMPH_CHM_STRUCTS_H__
#define __CMPH_CHM_STRUCTS_H__

#include "cmph.h"
typedef struct 
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	cmph_uint32 h1_seed;
	cmph_uint32 h2_seed;
	CMPH_HASH hashfuncs[2];
} chm_t;

typedef struct 
{
	CMPH_HASH hashfuncs[2];
	float c;
} chm_config_t;

#endif
