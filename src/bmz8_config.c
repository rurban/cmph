#include "cmph_structs.h"

#define DEBUG
#include "debug.h"

cmph_config_t *bmz8_config_new()
{
	cmph_config_t *config = (cmph_config_t *)malloc(sizeof(cmph_config_t));
	if (!config) return NULL;
	memset(config, 0, sizeof(cmph_config_t));
	config->impl.bmz8.hashfuncs[0] = CMPH_HASH_JENKINS;
	config->impl.bmz8.hashfuncs[1] = CMPH_HASH_JENKINS;
	config->impl.bmz8.c = 0.93;
	config->impl.bmz8.custom_h1_seed = 0;
	config->impl.bmz8.custom_h2_seed = 0;
	return config;
}
void bmz8_config_destroy(cmph_config_t *config)
{
	free(config);
}

void bmz8_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; //bmz8 only uses two hash functions
		config->impl.bmz8.hashfuncs[i] = *hashptr;
		++i, ++hashptr;
	}
}

void bmz8_config_set_graphsize(cmph_config_t *config, float c)
{
	DEBUGP("Setting graphsize to %f\n", c);
	config->impl.bmz8.c = c;
}
cmph_bool bmz8_config_set_seed1(cmph_config_t *config, cmph_uint32 seed)
{
	config->impl.bmz8.custom_h1_seed = 1;
	config->impl.bmz8.h1_seed = seed;
	return 1;
}
cmph_bool bmz8_config_set_seed2(cmph_config_t *config, cmph_uint32 seed)
{
	config->impl.bmz8.custom_h2_seed = 1;
	config->impl.bmz8.h2_seed = seed;
	return 1;
}
cmph_bool bmz8_config_set_iterations(cmph_config_t *config, cmph_uint32 iterations)
{
	config->impl.bmz8.iterations = iterations;
	return 1;
}
