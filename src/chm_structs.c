#include "chm_structs.h"
int chm_dump(cmph_t *mphf, FILE *fd)
{
#if 0
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 two = 2; //number of hash functions
	chm_t *mph = (chm_t *)mphf->;
	__cmph_dump(mphf, fd);

	fwrite(&two, sizeof(cmph_uint32), 1, fd);
	hash_state_dump(data->hashes[0], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	hash_state_dump(data->hashes[1], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	
	fwrite(data->g, sizeof(cmph_uint32)*data->n, 1, fd);
#endif
	return 1;
}

void chm_load(FILE *f, cmph_t *mphf)
{
#if 0
	cmph_uint32 nhashes;
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	chm_data_t *chm = (chm_data_t *)malloc(sizeof(chm_data_t));

	DEBUGP("Loading chm mphf\n");
	mphf->data = chm;
	fread(&nhashes, sizeof(cmph_uint32), 1, f);
	chm->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*(nhashes + 1));
	chm->hashes[nhashes] = NULL;
	DEBUGP("Reading %u hashes\n", nhashes);
	for (i = 0; i < nhashes; ++i)
	{
		hash_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = hash_state_load(buf, buflen);
		chm->hashes[i] = state;
		free(buf);
	}

	DEBUGP("Reading m and n\n");
	fread(&(chm->n), sizeof(cmph_uint32), 1, f);	
	fread(&(chm->m), sizeof(cmph_uint32), 1, f);	

	chm->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*chm->n);
	fread(chm->g, chm->n*sizeof(cmph_uint32), 1, f);
#endif
	return;
}
	
