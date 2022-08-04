#include "djb2_hash.h"
#include <stdlib.h>
#include <string.h>

djb2_state_t *djb2_state_new()
{
	djb2_state_t *state = (djb2_state_t *)malloc(sizeof(djb2_state_t));
        if (!state) return NULL;
	state->hashfunc = CMPH_HASH_DJB2;
        state->seed = 5381;
	return state;
}

void djb2_state_destroy(djb2_state_t *state)
{
	free(state);
}

cmph_uint32 djb2_hash(const cmph_uint32 seed, const char *k, cmph_uint32 keylen)
{
	register cmph_uint32 hash = seed;
	const unsigned char *ptr = (unsigned char *)k;
	cmph_uint32 i = 0;
	while (i < keylen)
	{
		hash = hash*33 ^ *ptr;
		++ptr, ++i;
	}
	return hash;
}

void djb2_state_dump(djb2_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buf = NULL;
	*buflen = 0;
	return;
}

djb2_state_t *djb2_state_copy(djb2_state_t *src_state)
{
	djb2_state_t *dest_state = (djb2_state_t *)malloc(sizeof(djb2_state_t));
	dest_state->hashfunc = src_state->hashfunc;
	dest_state->seed = src_state->seed;
	return dest_state;
}

djb2_state_t *djb2_state_load(const char *buf, cmph_uint32 buflen)
{
	djb2_state_t *state = (djb2_state_t *)malloc(sizeof(djb2_state_t));
	state->hashfunc = CMPH_HASH_DJB2;
	return state;
}

void djb2_hash_vector(cmph_uint32 seed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	hashes[0] = djb2_hash(seed, k, keylen);
	hashes[1] = djb2_hash(seed+1, k, keylen);
	hashes[2] = djb2_hash(seed+2, k, keylen);
}

/** \fn void djb2_state_pack(djb2_state_t *state, void *djb2_packed);
 *  \brief Support the ability to pack a djb2 function into a preallocated contiguous memory space pointed by djb2_packed.
 *  \param state points to the djb2 function
 *  \param djb2_packed pointer to the contiguous memory area used to store the djb2 function. The size of djb2_packed must be at least djb2_state_packed_size()
 */
void djb2_state_pack(djb2_state_t *state, void *djb2_packed)
{
	if (state && djb2_packed)
	{
		memcpy(djb2_packed, &(state->seed), sizeof(cmph_uint32));
	}
}

/** \fn cmph_uint32 djb2_state_packed_size(djb2_state_t *state);
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
cmph_uint32 djb2_hash_packed(void *djb2_packed, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
	djb2_hash_vector(*((cmph_uint32 *)djb2_packed), k, keylen, hashes);
	return hashes[2];
}

/** \fn djb2_hash_vector_packed(void *djb2_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param djb2_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void djb2_hash_vector_packed(void *djb2_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	djb2_hash_vector(*((cmph_uint32 *)djb2_packed), k, keylen, hashes);
}
