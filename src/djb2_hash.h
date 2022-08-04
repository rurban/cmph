#ifndef __DJB2_HASH_H__
#define __DJB2_HASH_H__

#include "hash.h"

typedef struct __djb2_state_t
{
	CMPH_HASH hashfunc;
	cmph_uint32 seed;
} djb2_state_t;

djb2_state_t *djb2_state_new();
cmph_uint32 djb2_hash(const cmph_uint32 seed, const char *k, cmph_uint32 keylen);
void djb2_state_dump(djb2_state_t *state, char **buf, cmph_uint32 *buflen);
djb2_state_t *djb2_state_copy(djb2_state_t *src_state);
djb2_state_t *djb2_state_load(const char *buf, cmph_uint32 buflen);
void djb2_state_destroy(djb2_state_t *state);

void djb2_hash_vector(const cmph_uint32 seed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
void djb2_state_pack(djb2_state_t *state, void *djb2_packed);
cmph_uint32 djb2_state_packed_size(void);
cmph_uint32 djb2_hash_packed(void *djb2_packed, const char *k, cmph_uint32 keylen);
void djb2_hash_vector_packed(void *djb2_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
  
#endif
