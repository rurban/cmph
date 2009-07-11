#ifndef __CMPH_XCHMR_STRUCTS_H__
#define __CMPH_XCHMR_STRUCTS_H__

#include "hash_state.h"

struct __xchmr_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	hash_state_t **hashes;
};

struct __xchmr_config_data_t
{
	CMPH_HASH hashfuncs[2];
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	xgraph_t *graph;
	cmph_uint32 *g;
	hash_state_t **hashes;
};

#endif
