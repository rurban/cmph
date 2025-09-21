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
	cmph_uint32 hash = state->seed;
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

hash_state_t *sdbm_state_load(const char *buf)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
        const unsigned char *p = (const unsigned char *)buf;
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

void sdbm_prep_compile(bool do_vector, FILE* out) {
	fprintf(out,
"/* sdbm_hash */\n"
"static uint32_t sdbm_hash(uint32_t seed, const unsigned char *k, const uint32_t keylen) {\n"
"    uint32_t hash = seed;\n"
"    const unsigned char *ptr = k;\n"
"    uint32_t i = 0;\n"
"    while (i < keylen) {\n"
"        hash = *ptr + (hash << 6) + (hash << 16) - hash;\n"
"	 ++ptr, ++i;\n"
"    }\n"
"    return hash;\n"
"}\n"
		"\n");
	if (do_vector)
		fprintf(out,
"/* 3x 32bit hashes. */\n"
"static inline void sdbm_hash_vector(uint32_t seed, const unsigned char *key, uint32_t keylen, uint32_t *hashes)\n"
"{\n"
"    hashes[0] = sdbm_hash(seed++, key, keylen);\n"
"    hashes[1] = sdbm_hash(seed++, key, keylen);\n"
"    hashes[2] = sdbm_hash(seed, key, keylen);\n"
"}\n"
"\n");
}

void sdbm_state_compile_seed(int i, cmph_uint32 seed, bool do_vector, FILE* out) {
    if (!do_vector)
	fprintf(out, "static inline uint32_t sdbm_hash_%d(const unsigned char *key, uint32_t keylen) {\n"
	       "    return sdbm_hash(%uU, key, keylen);\n"
	       "}\n", i, seed);
}
