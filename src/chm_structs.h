#ifndef __CMPH_CHM_STRUCTS_H__
#define __CMPH_CHM_STRUCTS_H__

#include "hash.h"
struct __chm_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	cmph_uint32 h1_seed;
	cmph_uint32 h2_seed;
	cmph_uint8 verbosity;
};

struct __chm_config_t
{
	cmph_hashfunc_t hashfuncs[2];
	float c;
};

#endif
