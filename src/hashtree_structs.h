#ifndef __CMPH_HASHTREE_STRUCTS_H__
#define __CMPH_HASHTREE_STRUCTS_H__

#include "hash_state.h"

struct __hashtree_data_t
{
	cmph_float32 leaf_c; //constant c
	cmph_float32 root_c;
	cmph_uint8 *size; //size[i] stores the number of edges represented by g[i]
	cmph_uint32 *offset; //offset[i] stores the sum size[0] + ... size[i - 1]
	cmph_uint32 k; //number of components
	hash_state_t *h0; //root hash function 
	uint8 *seeds; //Seed used by leaf hash functions
	cmph_uint32 **g; //graph g for each leaf
};

struct __hashtree_config_data_t
{
	CMPH_ALGO leaf_algo;
	CMPH_HASH hashfuncs[3];
	cmph_uint8 *size; //size[i] stores the number of edges represented by g[i]
	cmph_float32 c;
	cmph_uint32 memory;
};

#endif
