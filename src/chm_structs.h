#ifndef __CMPH_CHM_STRUCTS_H__
#define __CMPH_CHM_STRUCTS_H__

#include "hash_state.h"

struct __chm_data_t
{
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 *g;
	cmph_uint32 k; //number of components
	hash_state_t *h0;
	jenkins_state_t *h1;
	jenkins_state_t *h2;
};

struct __chm_config_data_t
{
	CMPH_HASH hashfuncs[1];
	cmph_uint32 m; //edges (words) count
	cmph_uint32 n; //vertex count
	cmph_uint32 k; //number of components (graphs)
	graph_t **graph;
	cmph_uint32 *g;
	hash_state_t *h0;
	jenkins_state_t *h1;
	jenkins_state_t *h2;
};

#endif
