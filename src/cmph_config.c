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

void cmph_config_set_tmp_dir(cmph_config_t *config, const char *tmp_dir)
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
			//brz_config_set_tmp_dir(config->data, tmp_dir);
			break;
		default:
			assert(0);
	}
}

void cmph_config_set_memory_availability(cmph_config_t *config, cmph_uint32 memory_availability)
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
			//brz_config_set_memory_availability(config->data, memory_availability);
			break;
		default:
			assert(0);
	}

}

void cmph_config_set_verbosity(cmph_config_t *config, cmph_uint32 verbosity)
{
	config->verbosity = verbosity;
}

void cmph_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			chm_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BMZ: 
			//bmz_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BMZ8: 
			bmz8_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BRZ: 
			//brz_config_set_hashfuncs(config, hashfuncs);
			break;
		default:
			break;
	}
	return;
}
void cmph_config_set_graphsize(cmph_config_t *config, float c)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			chm_config_set_graphsize(config, c);
			break;
		case CMPH_BMZ: 
			//bmz_config_set_graphsize(config, c);
			break;
		case CMPH_BMZ8: 
			bmz8_config_set_graphsize(config, c);
			break;
		case CMPH_BRZ: 
			//brz_config_set_graphsize(config, c);
			break;
		default:
			break;
	}
	return;
}

