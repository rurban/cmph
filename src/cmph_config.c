#include "cmph.h"
#include "cmph_structs.h"
#include "chm.h"
#include "bmz.h"
#include "bmz8.h"
#include "brz.h"

cmph_config_t *cmph_config_new(CMPH_ALGO algo)
{
	cmph_config_t *config = (cmph_config_t *)malloc(sizeof(cmph_config_t));
	if (!config) return NULL;
	memset(config, 0, sizeof(cmph_config_t));
	config->algo = algo; 
	switch(algo)
	{
		case CMPH_CHM:
			config->data = chm_config_new();
			break;
		case CMPH_BMZ:
			config->data = bmz_config_new();
			break;
		case CMPH_BMZ8:
			config->data = bmz8_config_new();
			break;
		case CMPH_BRZ:
			config->data = brz_config_new();
			break;
		default:
			assert(0);
	}
	return config;
}

void cmph_config_destroy(cmph_config_t *config)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			chm_config_destroy(config->data);
			break;
		case CMPH_BMZ:
			bmz_config_destroy(config->data);
			break;
		case CMPH_BMZ8: 
	       	bmz8_config_destroy(config->data);
			break;
		case CMPH_BRZ: 
	       	brz_config_destroy(config->data);
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
}

void cmph_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs)
{
	switch (config->algo)
	{
		case CMPH_CHM:
			chm_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BMZ: 
			bmz_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BMZ8: 
			bmz8_config_set_hashfuncs(config, hashfuncs);
			break;
		case CMPH_BRZ: 
			brz_config_set_hashfuncs(config, hashfuncs);
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
			chm_config_set_graphsize(config->data, c);
			break;
		case CMPH_BMZ: 
			bmz_config_set_graphsize(config->data, c);
			break;
		case CMPH_BMZ8: 
			bmz8_config_set_graphsize(config->data, c);
			break;
		case CMPH_BRZ: 
			brz_config_set_graphsize(config->data, c);
			break;
		default:
			break;
	}
	return;
}

