#include "djb2_hash.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "debug.h"

hash_state_t *djb2_state_new(cmph_uint32 size)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
        if (!state) return NULL;
	state->hashfunc = CMPH_HASH_DJB2;
	if (size > 0) state->seed = ((cmph_uint32)rand() % size);
        else state->seed = 5381;
	DEBUGP("Initializing djb2 hash with seed %u\n", state->seed);
	return state;
}

void djb2_state_destroy(hash_state_t *state)
{
	free(state);
}

cmph_uint32 djb2_hash(hash_state_t *state, const char *k, cmph_uint32 keylen)
{
	register cmph_uint32 hash = state->seed;
	const unsigned char *ptr = (unsigned char *)k;
	cmph_uint32 i = 0;
	while (i < keylen)
	{
		hash = hash*33 ^ *ptr;
		++ptr, ++i;
	}
	return hash;
}

void djb2_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buflen = sizeof(cmph_uint32);
	*buf = (char *)malloc(sizeof(cmph_uint32));
	if (!*buf)
	{
		*buflen = UINT_MAX;
		return;
	}
	memcpy(*buf, &(state->seed), sizeof(cmph_uint32));
	DEBUGP("Dumped djb2 state with seed %u\n", state->seed);
	return;
}

//hash_state_t *djb2_state_copy(hash_state_t *src_state)
//{
//	hash_state_t *dest_state = (hash_state_t *)malloc(sizeof(hash_state_t));
//	dest_state->hashfunc = src_state->hashfunc;
//	dest_state->seed = src_state->seed;
//	return dest_state;
//}

hash_state_t *djb2_state_load(const char *buf, cmph_uint32 buflen)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
	state->hashfunc = CMPH_HASH_DJB2;
        if ((long)buf % 4)
                state->seed = (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
        else
                state->seed = *(cmph_uint32 *)buf;
	DEBUGP("Loaded djb2 state with seed %u\n", state->seed);
	return state;
}

void djb2_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	hashes[0] = djb2_hash(state, k, keylen);
        state->seed++;
	hashes[1] = djb2_hash(state, k, keylen);
        state->seed++;
	hashes[2] = djb2_hash(state, k, keylen);
        state->seed -= 2;
}

/** \fn void djb2_state_pack(hash_state_t *state, void *djb2_packed);
 *  \brief Support the ability to pack a djb2 function into a preallocated contiguous memory space pointed by djb2_packed.
 *  \param state points to the djb2 function
 *  \param djb2_packed pointer to the contiguous memory area used to store the djb2 function. The size of djb2_packed must be at least djb2_state_packed_size()
 */
void djb2_state_pack(hash_state_t *state, void *djb2_packed)
{
	if (state && djb2_packed)
	{
		memcpy(djb2_packed, &(state->seed), sizeof(cmph_uint32));
	}
}

/** \fn cmph_uint32 djb2_state_packed_size(hash_state_t *state);
 *  \brief Return the amount of space needed to pack a djb2 function.
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 djb2_state_packed_size(void)
{
	return sizeof(cmph_uint32);
}


/** \fn cmph_uint32 djb2_hash_packed(void *djb2_packed, const char *k, cmph_uint32 keylen);
 *  \param djb2_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 djb2_hash_packed(void *packed, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
        hash_state_t state = { .seed = *(cmph_uint32*)packed };
	djb2_hash_vector(&state, k, keylen, hashes);
	return hashes[2];
}

/** \fn djb2_hash_vector_packed(void *djb2_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param djb2_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void djb2_hash_vector_packed(void *packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
        hash_state_t state = { .seed = *(cmph_uint32*)packed };
	djb2_hash_vector(&state, k, keylen, hashes);
}
