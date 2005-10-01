#include <assert.h>

#include "cmph.h"
#include "cmph_structs.h"
#include "chm.h"

cmph_config_t *cmph_config_new(CMPH_ALGO algo)
{
	cmph_config_t *config = NULL;
	switch(algo)
	{
		case CMPH_BMZ:
			//config = bmz_config_new();
			break;
		case CMPH_BMZ8:
			config = bmz8_config_new();
			break;
		case CMPH_CHM:
			config = chm_config_new();
			break;
		case CMPH_BRZ:
			//config = brz_config_new();
			break;
		default:
			assert(0);
	}
	if (config) config->algo = algo;
	return config;
}

void cmph_config_destroy(cmph_config_t *config)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			chm_config_destroy(config);
			break;
		case CMPH_BMZ:
			//bmz_config_destroy(config);
			break;
		case CMPH_BMZ8: 
	       	bmz8_config_destroy(config);
			break;
		case CMPH_BRZ: 
	       	//brz_config_destroy(config);
			break;
		default:
			assert(0);
	}
	free(config);
}

cmph_bool cmph_config_set_tmp_dir(cmph_config_t *config, const char *tmp_dir)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			break;
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			break;
		case CMPH_BRZ: 
			//return brz_config_set_tmp_dir(config->data, tmp_dir);
			break;
		default:
			assert(0);
	}
	return 0;
}

cmph_bool cmph_config_set_memory_availability(cmph_config_t *config, cmph_uint32 memory_availability)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			break;
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			break;
		case CMPH_BRZ: 
			//return brz_config_set_memory_availability(config->data, memory_availability);
			break;
		default:
			assert(0);
	}
	return 0;
}
cmph_bool cmph_config_set_iterations(cmph_config_t *config, cmph_uint32 iterations)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			break;
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			return bmz8_config_set_iterations(config, iterations);
		case CMPH_BRZ: 
			//return brz_config_set_iterations(config->data, iterations);
			break;
		default:
			assert(0);
	}
	return 0;
}

cmph_bool cmph_config_set_seed1(cmph_config_t *config, cmph_uint32 seed1)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			break;
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			return bmz8_config_set_seed1(config, seed1);
		case CMPH_BRZ: 
			//return brz_config_set_seed1(config->data, seed1);
			break;
		default:
			assert(0);
	}
	return 0;
}
cmph_bool cmph_config_set_seed2(cmph_config_t *config, cmph_uint32 seed2)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			break;
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			return bmz8_config_set_seed2(config, seed2);
		case CMPH_BRZ: 
			//return brz_config_set_seed2(config->data, seed2);
			break;
		default:
			assert(0);
	}
	return 0;
}

cmph_bool cmph_config_set_verbosity(cmph_config_t *config, cmph_uint32 verbosity)
{
	config->verbosity = verbosity;
	return 1;
}

cmph_bool cmph_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			return chm_config_set_hashfuncs(config, hashfuncs);
		case CMPH_BMZ: 
			//bmz_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BMZ8: 
			return bmz8_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BRZ: 
			//brz_config_set_hashfuncs(config, hashfuncs);
			break;
		default:
			break;
	}
	return 0;
}
cmph_bool cmph_config_set_graphsize(cmph_config_t *config, float c)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			return chm_config_set_graphsize(config, c);
		case CMPH_BMZ: 
			//return bmz_config_set_graphsize(config, c);
		case CMPH_BMZ8: 
			return bmz8_config_set_graphsize(config, c);
		case CMPH_BRZ: 
			//return brz_config_set_graphsize(config, c);
		default:
			break;
	}
	return 0;
}

