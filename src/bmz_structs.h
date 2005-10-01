#ifndef __CMPH_BMZ_STRUCTS_H__
#define __CMPH_BMZ_STRUCTS_H__

#include "cmph.h"

typedef struct 
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	cmph_uint32 h1_seed;
	cmph_uint32 h2_seed;
} bmz_t;


typedef struct 
{
	CMPH_HASH hashfuncs[2];
	float c;
	cmph_bool custom_h1_seed;
	cmph_bool custom_h2_seed;
	cmph_uint32 h1_seed;
	cmph_uint32 h2_seed;
} bmz_config_t;

#endif
