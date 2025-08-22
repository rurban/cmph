#include "cmph_structs.h"

#include <string.h>
#include <limits.h>

//#define DEBUG
#include "debug.h"

cmph_config_t *__config_new(cmph_io_adapter_t *key_source)
{
	cmph_config_t *mph = (cmph_config_t *)malloc(sizeof(cmph_config_t));
	if (mph == NULL) return NULL;
	memset(mph, 0, sizeof(cmph_config_t));
	mph->key_source = key_source;
	mph->verbosity = 0;
	mph->data = NULL;
	mph->c = 0;
	mph->seed = UINT_MAX;
	mph->hashfuncs[0] = CMPH_HASH_JENKINS;
	mph->hashfuncs[1] = CMPH_HASH_JENKINS;
	mph->hashfuncs[2] = CMPH_HASH_JENKINS;
	mph->nhashfuncs = 1;
	return mph;
}

void __config_destroy(cmph_config_t *mph)
{
	free(mph);
}

void __cmph_dump(cmph_t *mphf, FILE *fd)
{
	CHK_FWRITE(cmph_names[mphf->algo], (size_t)(strlen(cmph_names[mphf->algo]) + 1), (size_t)1, fd);
	CHK_FWRITE(&(mphf->size), sizeof(mphf->size), (size_t)1, fd);
}
cmph_t *__cmph_load(FILE *f)
{
	cmph_t *mphf = NULL;
	cmph_uint32 i;
	char algo_name[BUFSIZ];
	char *ptr = algo_name;
	CMPH_ALGO algo = CMPH_COUNT;

	DEBUGP("Loading mphf\n");
	for(i = 0; i < BUFSIZ; i++)
	{
		size_t c = fread(ptr, (size_t)1, (size_t)1, f);
		if (c != 1) return NULL;
		if (*ptr == 0) break;
		++ptr;
	}
	if(algo_name[i] != 0)
	{
		DEBUGP("Attempted buffer overflow while loading mph file\n");
		return NULL;
	}
	for(i = 0; i < CMPH_COUNT; ++i)
	{
		if (strcmp(algo_name, cmph_names[i]) == 0)
		{
			algo = (CMPH_ALGO)(i);
		}
	}
	if (algo == CMPH_COUNT)
	{
		DEBUGP("Algorithm %s not found\n", algo_name);
		return NULL;
	}
	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = algo;
	CHK_FREAD(&(mphf->size), sizeof(mphf->size), (size_t)1, f);
	mphf->data = NULL;
	DEBUGP("Algorithm is %s and mphf is sized %u\n", cmph_names[algo],  mphf->size);

	return mphf;
}
