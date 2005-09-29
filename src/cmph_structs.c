#include "cmph_structs.h"

#include <string.h>
#include <assert.h>

//#define DEBUG
#include "debug.h"

void __cmph_dump(cmph_t *mphf, FILE *fd)
{
	fwrite(cmph_names[mphf->algo], (cmph_uint32)(strlen(cmph_names[mphf->algo]) + 1), 1, fd);
}
cmph_t *__cmph_load(FILE *f) 
{
	cmph_t *mphf = NULL;
	cmph_uint32 i;
	char algo_name[BUFSIZ];
	char *ptr = algo_name;
	CMPH_ALGO algo = CMPH_COUNT;

	DEBUGP("Loading mphf\n");
	while(1)
	{
		cmph_uint32 c = fread(ptr, 1, 1, f);
		if (c != 1) return NULL;
		if (*ptr == 0) break;
		++ptr;
	}
	for(i = 0; i < CMPH_COUNT; ++i)
	{
		if (strcmp(algo_name, cmph_names[i]) == 0)
		{
			algo = i;
		}
	}
	if (algo == CMPH_COUNT) 
	{
		DEBUGP("Algorithm %s not found\n", algo_name);
		return NULL;
	}
	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = algo;
	mphf->impl= NULL;
	DEBUGP("Algorithm is %s\n", cmph_names[algo]);
	return mphf;
}

int cmph_dump(cmph_t *mphf, FILE *f)
{
	switch (mphf->algo)
	{
		case CMPH_CHM:
			return chm_dump(mphf, f);
		case CMPH_BMZ: 
			//return bmz_dump(mphf, f);
		case CMPH_BMZ8: 
			//return bmz8_dump(mphf, f);
		case CMPH_BRZ: 
			//return brz_dump(mphf, f);
		default:
			assert(0);
	}
	assert(0);
	return 0;
}
cmph_t *cmph_load(FILE *f)
{
	cmph_t *mphf = NULL;
	DEBUGP("Loading mphf generic parts\n");
	mphf =  __cmph_load(f);
	if (mphf == NULL) return NULL;
	DEBUGP("Loading mphf algorithm dependent parts\n");

	switch (mphf->algo)
	{
		case CMPH_CHM:
			chm_load(f, mphf);
			break;
		case CMPH_BMZ: 
			//bmz_load(f, mphf);
			break;
		case CMPH_BMZ8: 
			//bmz8_load(f, mphf);
			break;
		case CMPH_BRZ: 
			//brz_load(f, mphf);
			break;
		default:
			assert(0);
	}
	DEBUGP("Loaded mphf\n");
	return mphf;
}

