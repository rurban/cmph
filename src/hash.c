#include "config.h"
#include "hash.h"
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "jenkins_hash.h"
#include "wyhash_hash.h"
#include "djb2_hash.h"
#include "fnv_hash.h"
#include "sdbm_hash.h"
#include "crc32_hash.h"

//#define DEBUG
#include "debug.h"

// the matching enum is at cmph_types.h
const char *cmph_hash_names[] = { "jenkins", "wyhash", "djb2", "fnv", "sdbm", "crc32", NULL };

hash_state_t *hash_state_new(CMPH_HASH hashfunc, cmph_uint32 hashsize)
{
	hash_state_t *state = NULL;
	switch (hashfunc)
	{
		case CMPH_HASH_JENKINS:
	  		DEBUGP("Jenkins function - %u\n", hashsize);
			state = jenkins_state_new(hashsize);
			break;
		case CMPH_HASH_WYHASH:
	  		DEBUGP("Wyhash function - %u\n", hashsize);
			state = wyhash_state_new(hashsize);
			break;
		case CMPH_HASH_DJB2:
	  		DEBUGP("DJB2 function - %u\n", hashsize);
			state = djb2_state_new(hashsize);
			break;
		case CMPH_HASH_FNV:
	  		DEBUGP("FNV function - %u\n", hashsize);
			state = fnv_state_new(hashsize);
			break;
		case CMPH_HASH_SDBM:
	  		DEBUGP("SDBM function - %u\n", hashsize);
			state = sdbm_state_new(hashsize);
			break;
		case CMPH_HASH_CRC32:
	  		DEBUGP("CRC32 function - %u\n", hashsize);
			state = crc32_state_new(hashsize);
			break;
		default:
			assert(0);
	}
	state->hashfunc = hashfunc;
	return state;
}
cmph_uint32 hash(hash_state_t *state, const char *key, cmph_uint32 keylen)
{
	switch (state->hashfunc)
	{
		case CMPH_HASH_JENKINS:
			return jenkins_hash(state, key, keylen);
		case CMPH_HASH_WYHASH:
			return wyhash_hash(state, key, keylen);
		case CMPH_HASH_DJB2:
			return djb2_hash(state, key, keylen);
		case CMPH_HASH_FNV:
			return fnv_hash(state, key, keylen);
		case CMPH_HASH_SDBM:
			return sdbm_hash(state, key, keylen);
		case CMPH_HASH_CRC32:
			return crc32_hash(state, key, keylen);
		default:
			assert(0);
	}
	assert(0);
	return 0;
}

void hash_vector(hash_state_t *state, const char *key, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	switch (state->hashfunc)
	{
		case CMPH_HASH_JENKINS:
			jenkins_hash_vector_(state, key, keylen, hashes);
			break;
		case CMPH_HASH_WYHASH:
			wyhash_hash_vector_(state, key, keylen, hashes);
			break;
		case CMPH_HASH_DJB2:
			djb2_hash_vector(state, key, keylen, hashes);
			break;
		case CMPH_HASH_FNV:
			fnv_hash_vector(state, key, keylen, hashes);
			break;
		case CMPH_HASH_SDBM:
			sdbm_hash_vector(state, key, keylen, hashes);
			break;
		case CMPH_HASH_CRC32:
			crc32_hash_vector(state, key, keylen, hashes);
			break;
		default:
			assert(0);
	}
}


void hash_state_compile(int count, hash_state_t **states, bool do_vector, FILE* out)
{
	fprintf(out, "#include <stdint.h>\n");
	// see if we need a hash_vector API or seperate hash() calls are
	// enough. a vector is only good with all 2-3 the same
	for (int i=0; i < count; i++) {
		hash_state_t *state = states[i];
		switch (state->hashfunc)
		{
		case CMPH_HASH_JENKINS:
			DEBUGP("Compile hash[%d] jenkins with seed %u\n", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_JENKINS)
				jenkins_prep_compile(do_vector, out);
			jenkins_state_compile_seed(i, state->seed, do_vector, out);
			break;
		case CMPH_HASH_WYHASH:
			DEBUGP("Compile hash[%d] wyhash with seed %u\n", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_WYHASH)
				wyhash_prep_compile(do_vector, out);
			wyhash_state_compile_seed(i, state->seed, do_vector, out);
			break;
		case CMPH_HASH_CRC32:
			DEBUGP("Compile hash[%d] crc32 with seed %u\n", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_CRC32)
				crc32_prep_compile(do_vector, out);
			crc32_state_compile_seed(i, state->seed, do_vector, out);
			break;
		case CMPH_HASH_FNV:
			DEBUGP("Compile hash[%d] fnv with seed %u\n", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_FNV)
				fnv_prep_compile(do_vector, out);
			fnv_state_compile_seed(i, state->seed, do_vector, out);
			break;
		case CMPH_HASH_DJB2:
			DEBUGP("Compile hash%s[%d] djb2 with seed %u\n", do_vector ? "_vector" : "", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_DJB2)
				djb2_prep_compile(do_vector, out);
			djb2_state_compile_seed(i, state->seed, do_vector, out);
			break;
		case CMPH_HASH_SDBM:
			DEBUGP("Compile hash[%d] sdbm with seed %u\n", i, state->seed);
			if (i == 0 || states[0]->hashfunc != CMPH_HASH_SDBM)
				sdbm_prep_compile(do_vector, out);
			sdbm_state_compile_seed(i, state->seed, do_vector, out);
			break;
		default:
			if (state->hashfunc < CMPH_HASH_COUNT)
				fprintf(stderr, "-C not yet supported with hash function %s\n",
					cmph_hash_names[state->hashfunc]);
			else
				fprintf(stderr, "Illegal hash function\n");
			exit(1);
		}
	}
	return;
}
void hash_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen)
{
	char *algobuf;
	size_t len;
	switch (state->hashfunc)
	{
		case CMPH_HASH_JENKINS:
                        DEBUGP("Dump hash jenkins\n");
			jenkins_state_dump(state, &algobuf, buflen);
			break;
		case CMPH_HASH_WYHASH:
                        DEBUGP("Dump hash wyhash\n");
			wyhash_state_dump(state, &algobuf, buflen);
			break;
		case CMPH_HASH_DJB2:
                        DEBUGP("Dump hash djb2\n");
			djb2_state_dump(state, &algobuf, buflen);
			break;
		case CMPH_HASH_FNV:
                        DEBUGP("Dump hash fnv\n");
			fnv_state_dump(state, &algobuf, buflen);
			break;
		case CMPH_HASH_SDBM:
                        DEBUGP("Dump hash sdbm\n");
			sdbm_state_dump(state, &algobuf, buflen);
			break;
		case CMPH_HASH_CRC32:
                        DEBUGP("Dump hash crc32\n");
			crc32_state_dump(state, &algobuf, buflen);
			break;
		default:
			assert(0);
	}
        if (*buflen == UINT_MAX) {
                goto cmph_cleanup;
        }
	*buf = (char *)malloc(strlen(cmph_hash_names[state->hashfunc]) + 1 + *buflen);
	memcpy(*buf, cmph_hash_names[state->hashfunc], strlen(cmph_hash_names[state->hashfunc]) + 1);
	DEBUGP("Algobuf is %u\n", *(cmph_uint32 *)algobuf);
	len = *buflen;
	memcpy(*buf + strlen(cmph_hash_names[state->hashfunc]) + 1, algobuf, len);
	*buflen  = (cmph_uint32)strlen(cmph_hash_names[state->hashfunc]) + 1 + *buflen;
cmph_cleanup:
	free(algobuf);
	return;
}

//hash_state_t * hash_state_copy(hash_state_t *src_state)
//{
//	hash_state_t *dest_state = NULL;
//	switch (src_state->hashfunc)
//	{
//		case CMPH_HASH_JENKINS:
//			dest_state = (hash_state_t *)jenkins_state_copy(&src_state->jenkins);
//			break;
//		case CMPH_HASH_WYHASH:
//			dest_state = (hash_state_t *)wyhash_state_copy(&src_state->wyhash);
//			break;
//		case CMPH_HASH_DJB2:
//			dest_state = (hash_state_t *)djb2_state_copy(&src_state->djb2);
//			break;
//		case CMPH_HASH_FNV:
//			dest_state = fnv_state_copy(&src_state->fnv);
//			break;
//		case CMPH_HASH_SDBM:
//			dest_state = sdbm_state_copy(&src_state->fnv);
//			break;
//		case CMPH_HASH_CRC32:
//			dest_state = crc32_state_copy(&src_state->fnv);
//			break;
//		default:
//			assert(0);
//	}
//	dest_state->hashfunc = src_state->hashfunc;
//	return dest_state;
//}

hash_state_t *hash_state_load(const char *buf)
{
	cmph_uint32 i;
	cmph_uint32 offset;
	CMPH_HASH hashfunc = CMPH_HASH_COUNT;
	for (i = 0; i < CMPH_HASH_COUNT; ++i)
	{
		if (strcmp(buf, cmph_hash_names[i]) == 0)
		{
			hashfunc = (CMPH_HASH)(i);
			break;
		}
	}
	if (hashfunc == CMPH_HASH_COUNT) return NULL;
	offset = (cmph_uint32)strlen(cmph_hash_names[hashfunc]) + 1;
	switch (hashfunc)
	{
		case CMPH_HASH_JENKINS:
                        DEBUGP("Hash is jenkins\n");
			return jenkins_state_load(buf + offset);
		case CMPH_HASH_WYHASH:
                        DEBUGP("Hash is wyhash\n");
			return wyhash_state_load(buf + offset);
		case CMPH_HASH_DJB2:
                        DEBUGP("Hash is djb2\n");
			return djb2_state_load(buf + offset);
		case CMPH_HASH_FNV:
                        DEBUGP("Hash is fnv\n");
			return fnv_state_load(buf + offset);
		case CMPH_HASH_SDBM:
                        DEBUGP("Hash is sdbm\n");
			return sdbm_state_load(buf + offset);
		case CMPH_HASH_CRC32:
#ifdef HAVE_CRC32_HW
                        DEBUGP("Hash is HW crc32\n");
#else
                        DEBUGP("Hash is SW crc32\n");
#endif
			return crc32_state_load(buf + offset);
		default:
                        DEBUGP("Unknown Hash\n");
			return NULL;
	}
	return NULL;
}
void hash_state_destroy(hash_state_t *state)
{
	switch (state->hashfunc)
	{
		case CMPH_HASH_JENKINS:
			jenkins_state_destroy(state);
			break;
		case CMPH_HASH_WYHASH:
			wyhash_state_destroy(state);
			break;
		case CMPH_HASH_DJB2:
			djb2_state_destroy(state);
			break;
		case CMPH_HASH_FNV:
			fnv_state_destroy(state);
			break;
		case CMPH_HASH_SDBM:
			sdbm_state_destroy(state);
			break;
		case CMPH_HASH_CRC32:
			crc32_state_destroy(state);
			break;
		default:
			assert(0);
	}
	return;
}

/** \fn void hash_state_pack(hash_state_t *state, void *hash_packed)
 *  \brief Support the ability to pack a hash function into a preallocated contiguous memory space pointed by hash_packed.
 *  \param state points to the hash function
 *  \param hash_packed pointer to the contiguous memory area used to store the hash function. The size of hash_packed must be at least hash_state_packed_size()
 *
 * Support the ability to pack a hash function into a preallocated contiguous memory space pointed by hash_packed.
 * However, the hash function type must be packed outside.
 */
void hash_state_pack(hash_state_t *state, void *hash_packed)
{
	switch (state->hashfunc)
	{
		case CMPH_HASH_JENKINS:
			jenkins_state_pack(state, hash_packed);
			break;
		case CMPH_HASH_WYHASH:
			wyhash_state_pack(state, hash_packed);
			break;
		case CMPH_HASH_DJB2:
			djb2_state_pack(state, hash_packed);
			break;
		case CMPH_HASH_FNV:
                case CMPH_HASH_SDBM:
                case CMPH_HASH_CRC32:
			fnv_state_pack(state, hash_packed);
			break;
		default:
			assert(0);
	}
	return;
}

/** \fn cmph_uint32 hash_state_packed_size(CMPH_HASH hashfunc)
 *  \brief Return the amount of space needed to pack a hash function.
 *  \param hashfunc function type
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 hash_state_packed_size(CMPH_HASH hashfunc)
{
	switch (hashfunc)
	{
		case CMPH_HASH_JENKINS:
			return jenkins_state_packed_size();
		case CMPH_HASH_WYHASH:
			return wyhash_state_packed_size();
		case CMPH_HASH_DJB2:
			return djb2_state_packed_size();
		case CMPH_HASH_FNV:
		case CMPH_HASH_SDBM:
		case CMPH_HASH_CRC32:
			return fnv_state_packed_size();
		default:
			assert(0);
	}
	return 0;
}

/** \fn cmph_uint32 hash_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen)
 *  \param hash_packed is a pointer to a contiguous memory area
 *  \param hashfunc is the type of the hash function packed in hash_packed
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 hash_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen)
{
	switch (hashfunc)
	{
		case CMPH_HASH_JENKINS:
			return jenkins_hash_packed(hash_packed, k, keylen);
		case CMPH_HASH_WYHASH:
			return wyhash_hash_packed(hash_packed, k, keylen);
		case CMPH_HASH_DJB2:
			return djb2_hash_packed(hash_packed, k, keylen);
		case CMPH_HASH_FNV:
			return fnv_hash_packed(hash_packed, k, keylen);
                case CMPH_HASH_SDBM:
			return sdbm_hash_packed(hash_packed, k, keylen);
                case CMPH_HASH_CRC32:
			return crc32_hash_packed(hash_packed, k, keylen);
		default:
			assert(0);
	}
	assert(0);
	return 0;
}

/** \fn hash_vector_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
 *  \param hash_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void hash_vector_packed(void *hash_packed, CMPH_HASH hashfunc, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	switch (hashfunc)
	{
		case CMPH_HASH_JENKINS:
			jenkins_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
		case CMPH_HASH_WYHASH:
			wyhash_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
		case CMPH_HASH_DJB2:
			djb2_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
		case CMPH_HASH_FNV:
			fnv_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
                case CMPH_HASH_SDBM:
			sdbm_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
                case CMPH_HASH_CRC32:
			crc32_hash_vector_packed(hash_packed, k, keylen, hashes);
			break;
		default:
			assert(0);
	}
}


/** \fn CMPH_HASH hash_get_type(hash_state_t *state);
 *  \param state is a pointer to a hash_state_t structure
 *  \return the hash function type pointed by state
 */
CMPH_HASH hash_get_type(hash_state_t *state)
{
	return state->hashfunc;
}
