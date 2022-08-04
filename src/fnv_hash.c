#include "fnv_hash.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "debug.h"

fnv_state_t *fnv_state_new()
{
	fnv_state_t *state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
        if (!state) return NULL;
	state->hashfunc = CMPH_HASH_FNV;
        state->seed = 0;
	return state;
}

void fnv_state_destroy(fnv_state_t *state)
{
	free(state);
}

cmph_uint32 fnv_hash(const cmph_uint32 seed, const char *k, cmph_uint32 keylen)
{
	const unsigned char *bp = (const unsigned char *)k;
	const unsigned char *be = bp + keylen;
	unsigned int hval = (unsigned int)seed;

	while (bp < be)
	{

		//hval *= 0x01000193; good for non-gcc compiler
		hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24); //good for gcc

		hval ^= *bp++;
	}
	return hval;
}


void fnv_state_dump(fnv_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buflen = sizeof(cmph_uint32);
	*buf = (char *)malloc(sizeof(cmph_uint32));
	if (!*buf)
	{
		*buflen = UINT_MAX;
		return;
	}
	memcpy(*buf, &(state->seed), sizeof(cmph_uint32));
	DEBUGP("Dumped fnv state with seed %u\n", state->seed);
	return;
}

fnv_state_t * fnv_state_copy(fnv_state_t *src_state)
{
	fnv_state_t *dest_state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
        if (!dest_state) return NULL;
	dest_state->hashfunc = src_state->hashfunc;
	dest_state->seed = src_state->seed;
	return dest_state;
}

fnv_state_t *fnv_state_load(const char *buf, cmph_uint32 buflen)
{
	fnv_state_t *state = (fnv_state_t *)malloc(sizeof(fnv_state_t));
	state->hashfunc = CMPH_HASH_FNV;
	return state;
}

void fnv_hash_vector(const cmph_uint32 seed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	hashes[0] = fnv_hash(seed, k, keylen);
	hashes[1] = fnv_hash(seed+1, k, keylen);
	hashes[2] = fnv_hash(seed+2, k, keylen);
}

/** \fn void fnv_state_pack(fnv_state_t *state, void *fnv_packed);
 *  \brief Support the ability to pack a fnv function into a preallocated contiguous memory space pointed by fnv_packed.
 *  \param state points to the fnv function
 *  \param fnv_packed pointer to the contiguous memory area used to store the fnv function. The size of fnv_packed must be at least fnv_state_packed_size()
 */
void fnv_state_pack(fnv_state_t *state, void *fnv_packed)
{
	if (state && fnv_packed)
	{
		memcpy(fnv_packed, &(state->seed), sizeof(cmph_uint32));
	}
}

/** \fn cmph_uint32 fnv_state_packed_size(fnv_state_t *state);
 *  \brief Return the amount of space needed to pack a fnv function.
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 fnv_state_packed_size(void)
{
	return sizeof(cmph_uint32);
}


/** \fn cmph_uint32 fnv_hash_packed(void *fnv_packed, const char *k, cmph_uint32 keylen);
 *  \param fnv_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 fnv_hash_packed(void *fnv_packed, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
	fnv_hash_vector(*((cmph_uint32 *)fnv_packed), k, keylen, hashes);
	return hashes[2];
}

/** \fn fnv_hash_vector_packed(void *fnv_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param fnv_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void fnv_hash_vector_packed(void *fnv_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	fnv_hash_vector(*((cmph_uint32 *)fnv_packed), k, keylen, hashes);
}
