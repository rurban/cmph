#ifndef __FNV_HASH_H__
#define __FNV_HASH_H__

#include "hash.h"

typedef struct __fnv_state_t
{
	CMPH_HASH hashfunc;
  	cmph_uint32 seed;
} fnv_state_t;

fnv_state_t *fnv_state_new();
cmph_uint32 fnv_hash(const cmph_uint32 seed, const char *k, cmph_uint32 keylen);
void fnv_state_dump(fnv_state_t *state, char **buf, cmph_uint32 *buflen);
fnv_state_t *fnv_state_copy(fnv_state_t *src_state);
fnv_state_t *fnv_state_load(const char *buf, cmph_uint32 buflen);
void fnv_state_destroy(fnv_state_t *state);

void fnv_hash_vector(const cmph_uint32 seed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
void fnv_state_pack(fnv_state_t *state, void *fnv_packed);
cmph_uint32 fnv_state_packed_size(void);
cmph_uint32 fnv_hash_packed(void *fnv_packed, const char *k, cmph_uint32 keylen);
void fnv_hash_vector_packed(void *fnv_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);


#endif
