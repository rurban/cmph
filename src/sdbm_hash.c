#include "sdbm_hash.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "hash.h"
#include "debug.h"

hash_state_t *sdbm_state_new(cmph_uint32 size)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
        if (!state) return NULL;
	state->hashfunc = CMPH_HASH_SDBM;
	if (size > 0) state->seed = (cmph_uint32)rand();
	else state->seed = 0;
	DEBUGP("Initializing sdbm hash with seed %u\n", state->seed);
	return state;
}

void sdbm_state_destroy(hash_state_t *state)
{
	free(state);
}

cmph_uint32 sdbm_hash(hash_state_t *state, const char *k, const cmph_uint32 keylen)
{
	register cmph_uint32 hash = state->seed;
	const unsigned char *ptr = (unsigned char *)k;
	cmph_uint32 i = 0;

	while(i < keylen) {
		hash = *ptr + (hash << 6) + (hash << 16) - hash;
		++ptr, ++i;
	}
	return hash;
}

static cmph_uint32 sdbm_hash_seed(const cmph_uint32 seed, const char *k, const cmph_uint32 keylen)
{
	register cmph_uint32 hash = seed;
	const unsigned char *ptr = (unsigned char *)k;
	cmph_uint32 i = 0;

	while(i < keylen) {
		hash = *ptr + (hash << 6) + (hash << 16) - hash;
		++ptr, ++i;
	}
	return hash;
}

void sdbm_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buflen = sizeof(cmph_uint32);
	*buf = (char *)malloc(sizeof(cmph_uint32));
	if (!*buf)
	{
		*buflen = UINT_MAX;
		return;
	}
	memcpy(*buf, &(state->seed), sizeof(cmph_uint32));
	DEBUGP("Dumped sdbm state with seed %u\n", state->seed);
	return;
}

//hash_state_t *sdbm_state_copy(hash_state_t *src_state)
//{
//	hash_state_t *dest_state = (hash_state_t *)malloc(sizeof(hash_state_t));
//	dest_state->hashfunc = src_state->hashfunc;
//	return dest_state;
//}

hash_state_t *sdbm_state_load(const char *buf, cmph_uint32 buflen)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
        const unsigned char *p = (const unsigned char *)buf;
	(void)buflen;
        if ((long)buf % 4)
                state->seed = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
        else
                state->seed = *(cmph_uint32 *)buf;
	state->hashfunc = CMPH_HASH_SDBM;
	DEBUGP("Loaded sdbm state with seed %u\n", state->seed);
	return state;
}

void sdbm_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	hashes[0] = sdbm_hash_seed(state->seed, k, keylen);
	hashes[1] = sdbm_hash_seed(state->seed+1, k, keylen);
	hashes[2] = sdbm_hash_seed(state->seed+2, k, keylen);
}

cmph_uint32 sdbm_hash_packed(void *packed, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
        hash_state_t state = { .seed = *(cmph_uint32*)packed };
	sdbm_hash_vector(&state, k, keylen, hashes);
	return hashes[2];
}

void sdbm_hash_vector_packed(void *packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
        hash_state_t state = { .seed = *(cmph_uint32*)packed };
	sdbm_hash_vector(&state, k, keylen, hashes);
}
