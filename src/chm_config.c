#include "cmph_structs.h"
#include <string.h>

cmph_config_t *chm_config_new()
{
	cmph_config_t *config = (cmph_config_t *)malloc(sizeof(cmph_config_t));
	if (!config) return NULL;
	memset(config, 0, sizeof(cmph_config_t));
	config->impl.chm.hashfuncs[0] = CMPH_HASH_JENKINS;
	config->impl.chm.hashfuncs[1] = CMPH_HASH_JENKINS;
	config->impl.chm.c = 2.09;
	return config;
}
void chm_config_destroy(cmph_config_t *config)
{
	free(config);
}

cmph_bool chm_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; //chm only uses two hash functions
		config->impl.chm.hashfuncs[i] = *hashptr;
		++i, ++hashptr;
	}
	return 1;
}

cmph_bool chm_config_set_graphsize(cmph_config_t *config, float c)
{
	config->impl.chm.c = c;
	return 1;
}
