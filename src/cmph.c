#include "cmph.h"
#include "cmph_structs.h"
#include "chm.h"
#include "bmz8.h"
#include "hashtree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
//#define DEBUG
#include "debug.h"

const char *cmph_names[] = { "bmz", "bmz8", "chm", "brz", "hashtree", NULL };


cmph_t *cmph_new(const cmph_config_t *config, cmph_io_adapter_t *key_source)
{
	DEBUGP("Creating mph with algorithm %s\n", cmph_names[config->algo]);
	switch (config->algo)	
	{
		case CMPH_CHM:
			return chm_new(config, key_source);
		case CMPH_BMZ:
			break;
		case CMPH_BMZ8: 
			return bmz8_new(config, key_source);
			break;
		case CMPH_BRZ: 
			break;
		case CMPH_HASHTREE:
			return hashtree_new(config, key_source);
		default:
		assert(0);
	}
	return NULL;
}

cmph_uint32 cmph_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
   	DEBUGP("mphf algorithm: %u \n", mphf->algo);
	switch(mphf->algo)
	{
		case CMPH_CHM:
			return chm_search(mphf, key, keylen);
		case CMPH_BMZ: 
			break;
		case CMPH_BMZ8: 
			return bmz8_search(mphf, key, keylen);
		case CMPH_BRZ: 
			break;
		case CMPH_HASHTREE:
			return hashtree_search(mphf, key, keylen);
		default:
			assert(0);
	}
	assert(0);
	return 0;
}

cmph_uint32 cmph_size(const cmph_t *mphf)
{
	switch(mphf->algo)
	{
		case CMPH_CHM:
			return chm_size(mphf);
		case CMPH_BMZ: 
			//return bmz_size(mphf->data);
		case CMPH_BMZ8: 
			return bmz8_size(mphf);
		case CMPH_BRZ: 
			//return brz_size(mphf->data);
		case CMPH_HASHTREE:
			return hashtree_size(mphf);
		default: 
			assert(0);
	}
	assert(0);
	return 1;
}
	
void cmph_destroy(cmph_t *mphf)
{
	switch(mphf->algo)
	{
		case CMPH_CHM:
			chm_destroy(mphf);
			return;
		case CMPH_BMZ: 
			//bmz_destroy(mphf);
			return;
		case CMPH_BMZ8: 
			DEBUGP("Destroy bmz8\n");
			bmz8_destroy(mphf);
			return;
		case CMPH_BRZ: 
			//brz_destroy(mphf);
			return;
		case CMPH_HASHTREE:
			return hashtree_destroy(mphf);
		default: 
			assert(0);
	}
	assert(0);
	return;
}
