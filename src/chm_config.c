#include "chm.h"

chm_config_t *chm_config_new()
{
	chm_config_t *chm_config = NULL;
	chm_config = (chm_config_t *)malloc(sizeof(chm_config_t));
	if (!chm_config) return NULL;
	memset(chm_config, 0, sizeof(chm_config_t));
	chm_config->hashfuncs[0] = CMPH_HASH_JENKINS;
	chm_config->hashfuncs[1] = CMPH_HASH_JENKINS;
	chm_config->c = 2.09;
	return chm_config;
}
void chm_config_destroy(cmph_config_t *config)
{
	chm_config_t *chm_config = (chm_config_data_t *)config->data;
	free(chm_config);
}

void chm_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	chm_config_t *chm_config = (chm_config_data_t *)config->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; //chm only uses two hash functions
		chm_config->hashfuncs[i] = cmph_hashfuncs[*hashptr];	
		++i, ++hashptr;
	}
}

void chm_config_set_graphsize(cmph_config_t *config, float c)
{
	chm_config_t *chm_config = (chm_config_data_t *)config->data;
	chm_config->c = c;
}
