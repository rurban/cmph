#include "cmph.h"
#include "cmph_structs.h"
#include "chm.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
//#define DEBUG
#include "debug.h"

const char *cmph_names[] = { "bmz", "bmz8", "chm", "brz", NULL };


cmph_t *cmph_new(const cmph_config_t *config, cmph_io_adapter_t *key_source)
{
	cmph_t *mphf = NULL;

	DEBUGP("Creating mph with algorithm %s\n", cmph_names[mph->algo]);
	switch (config->algo)	
	{
		case CMPH_CHM:
			mphf->impl = chm_new(config, key_source);
			break;
		case CMPH_BMZ:
			//mphf = bmz_new(config, c);
			break;
		case CMPH_BMZ8: 
			//mphf = bmz8_new(config, c);
			break;
		case CMPH_BRZ: 
			//mphf = brz_new(config, c);
			break;
		default:
			assert(0);
	}
	return mphf;
}

cmph_uint32 cmph_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
   	DEBUGP("mphf algorithm: %u \n", mphf->algo);
	switch(mphf->algo)
	{
		case CMPH_CHM:
			return chm_search(mphf, key, keylen);
		case CMPH_BMZ: 
			//DEBUGP("bmz algorithm search\n");		         
			//return bmz_search(mphf->data, key, keylen);
		case CMPH_BMZ8: 
			//DEBUGP("bmz8 algorithm search\n");		         
			//return bmz8_search(mphf, key, keylen);
		case CMPH_BRZ: 
			//DEBUGP("brz algorithm search\n");		         
			//return brz_search(mphf, key, keylen);
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
			chm_size(mphf);
			return;
		case CMPH_BMZ: 
			//bmz_size(mphf->data);
			//return;
		case CMPH_BMZ8: 
			//bmz8_size(mphf->data);
			//return;
		case CMPH_BRZ: 
			//brz_size(mphf->data);
			//return;
		default: 
			assert(0);
	}
	assert(0);
	return;
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
			//bmz8_destroy(mphf);
			return;
		case CMPH_BRZ: 
			//brz_destroy(mphf);
			return;
		default: 
			assert(0);
	}
	assert(0);
	return;
}
