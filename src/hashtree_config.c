#include "cmph_structs.h"

cmph_config_t *hashtree_config_new()
{
	cmph_config_t *config = (cmph_config_t *)malloc(sizeof(cmph_config_t));
	if (!config) return NULL;
	memset(config, 0, sizeof(cmph_config_t));
	config->impl.hashtree.hash[0] = CMPH_HASH_JENKINS;
	config->impl.hashtree.hash[1] = CMPH_HASH_JENKINS;
	config->impl.hashtree.hash[2] = CMPH_HASH_JENKINS;
	config->impl.hashtree.leaf_c = 0.93;
	config->impl.hashtree.root_c = 1.5;
	return config;
}
void hashtree_config_destroy(cmph_config_t *config)
{
	free(config);
}

void hashtree_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 3) break; //hashtree only uses three hash functions
		config->impl.hashtree.hash[i] = *hashptr;
		++i, ++hashptr;
	}
}

void hashtree_config_set_root_c(cmph_config_t *config, float c)
{
	config->impl.hashtree.root_c = c;
}
void hashtree_config_set_leaf_c(cmph_config_t *config, float c)
{
	config->impl.hashtree.leaf_c = c;
}
