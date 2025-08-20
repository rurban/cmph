#ifndef __FNV_HASH_H__
#define __FNV_HASH_H__

#include "hash.h"

void fnv_state_init(hash_state_t *state, cmph_uint32 size);
cmph_uint32 fnv_hash(hash_state_t *state, const char *k, cmph_uint32 keylen);
void fnv_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen);
//hash_state_t *fnv_state_copy(hash_state_t *src_state);
hash_state_t *fnv_state_load(const char *buf);
void fnv_state_destroy(hash_state_t *state);

void fnv_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
void fnv_state_pack(hash_state_t *state, void *fnv_packed);
cmph_uint32 fnv_state_packed_size(void);
cmph_uint32 fnv_hash_packed(void *fnv_packed, const char *k, cmph_uint32 keylen);
void fnv_hash_vector_packed(void *fnv_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);


#endif
