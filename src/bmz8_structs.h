#ifndef __CMPH_BMZ8_STRUCTS_H__
#define __CMPH_BMZ8_STRUCTS_H__

typedef struct
{
	cmph_uint8 m; //edges (words) count
	cmph_uint8 n; //vertex count
	cmph_uint8 *g;
	cmph_uint8 h1_seed;
	cmph_uint8 h2_seed;
} bmz8_t;


typedef struct
{
	CMPH_HASH hashfuncs[2];
	float c;
} bmz8_config_t;

#endif
