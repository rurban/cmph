#include "bmz8_structs.h"
int bmz8_dump(cmph_t *mphf, FILE *fd)
{
#if 0
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint8 two = 2; //number of hash functions
	bmz8_data_t *data = (bmz8_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	fwrite(&two, sizeof(cmph_uint8), 1, fd);

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

	fwrite(&(data->n), sizeof(cmph_uint8), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint8), 1, fd);
	
	fwrite(data->g, sizeof(cmph_uint8)*(data->n), 1, fd);
#endif
	return 1;
}

void bmz8_load(FILE *f, cmph_t *mphf)
{
#if 0
	cmph_uint8 nhashes;
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint8 i;
	bmz8_data_t *bmz8 = (bmz8_data_t *)malloc(sizeof(bmz8_data_t));

	DEBUGP("Loading bmz8 mphf\n");
	mphf->data = bmz8;
	fread(&nhashes, sizeof(cmph_uint8), 1, f);
	bmz8->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*(nhashes + 1));
	bmz8->hashes[nhashes] = NULL;
	DEBUGP("Reading %u hashes\n", nhashes);
	for (i = 0; i < nhashes; ++i)
	{
		hash_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = hash_state_load(buf, buflen);
		bmz8->hashes[i] = state;
		free(buf);
	}

	DEBUGP("Reading m and n\n");
	fread(&(bmz8->n), sizeof(cmph_uint8), 1, f);	
	fread(&(bmz8->m), sizeof(cmph_uint8), 1, f);	

	bmz8->g = (cmph_uint8 *)malloc(sizeof(cmph_uint8)*bmz8->n);
	fread(bmz8->g, bmz8->n*sizeof(cmph_uint8), 1, f);
#endif
	return;
}
		


