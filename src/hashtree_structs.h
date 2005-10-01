#ifndef __CMPH_HASHTREE_STRUCTS_H__
#define __CMPH_HASHTREE_STRUCTS_H__

#include "cmph_types.h"
#include "cmph.h"
typedef struct 
{
	cmph_uint32 *offset; //offset[i] stores the sum size[0] + ... size[i - 1]
	cmph_uint32 k; //number of components
	cmph_uint8 h0_seed; //root hash function 
	CMPH_HASH hash[3];
	cmph_t **leaf; //leaf minimal perfect hash fuctions
} hashtree_t;

typedef struct 
{
	CMPH_ALGO leaf_algo;
	CMPH_HASH hash[3];
	cmph_float32 leaf_c; //constant c
	cmph_float32 root_c;
	cmph_uint32 memory;
	cmph_uint32 iterations;
} hashtree_config_t;

#endif
