#ifndef __CMPH_BRZ_STRUCTS_H__
#define __CMPH_BRZ_STRUCTS_H__

typedef struct 
{
	cmph_uint32 m;       // edges (words) count
	cmph_float32 c;      // constant c
	cmph_uint8  *size;   // size[i] stores the number of edges represented by g[i][...]. 
	cmph_uint32 *offset; // offset[i] stores the sum: size[0] + size[1] + ... size[i-1].
	cmph_uint8 **g;      // g function. 
	cmph_uint32 k;       // number of components
	cmph_uint32 *h1_seed;
	cmph_uint32 *h2_seed;
	cmph_uint32 h3_seed;    
} brz_t;

typedef struct 
{
	CMPH_HASH hashfuncs[3];
	cmph_float32 c;      // constant c
	cmph_uint32 m;       // edges (words) count
	cmph_uint8  *size;   // size[i] stores the number of edges represented by g[i][...]. 
	cmph_uint32 *offset; // offset[i] stores the sum: size[0] + size[1] + ... size[i-1].
	cmph_uint8 **g;      // g function. 
	cmph_uint32 k;       // number of components
	cmph_uint32 *h1_seed;
	cmph_uint32 *h2_seed;
	cmph_uint32 h3_seed;    
	cmph_uint32 memory_availability; 
	cmph_uint8 * tmp_dir; // temporary directory 
} brz_config_t;

#endif
