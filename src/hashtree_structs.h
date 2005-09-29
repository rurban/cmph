#ifndef __CMPH_HASHTREE_STRUCTS_H__
#define __CMPH_HASHTREE_STRUCTS_H__

typedef struct 
{
	cmph_uint32 *offset; //offset[i] stores the sum size[0] + ... size[i - 1]
	cmph_uint32 k; //number of components
	cmph_uint8 h0_seed; //root hash function 
	cmph_uint8 *h1_seed; //Seed used by leaf hash functions
	cmph_uint8 *h2_seed; //Seed used by leaf hash functions
	cmph_uint32 **g; //graph g for each leaf
} hashtree_t;

typedef struct 
{
	CMPH_ALGO leaf_algo;
	CMPH_HASH hashfuncs[3];
	cmph_float32 c;
	cmph_float32 leaf_c; //constant c
	cmph_float32 root_c;
	cmph_uint32 memory;
} hashtree_config_t;

#endif