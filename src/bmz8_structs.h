#ifndef __CMPH_BMZ8_STRUCTS_H__
#define __CMPH_BMZ8_STRUCTS_H__

#include "cmph.h"

typedef struct
{
	cmph_uint8 m; //edges (words) count
	cmph_uint8 n; //vertex count
	cmph_uint8 *g;
	cmph_uint8 seed[2];
	CMPH_HASH hash[2];
} bmz8_t;


typedef struct
{
	CMPH_HASH hashfuncs[2];
	float c;
	cmph_bool custom_h1_seed;
	cmph_bool custom_h2_seed;
	cmph_uint32 h1_seed;
	cmph_uint32 h2_seed;
	cmph_uint32 iterations;
} bmz8_config_t;

#endif
