#ifndef __SDBM_HASH_H__
#define __SDBM_HASH_H__

#include "hash.h"

hash_state_t *sdbm_state_new(cmph_uint32 size);
cmph_uint32 sdbm_hash(hash_state_t *state, const char *k, const cmph_uint32 keylen);
void sdbm_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen);
//hash_state_t *sdbm_state_copy(hash_state_t *src_state);
hash_state_t *sdbm_state_load(const char *buf);
void sdbm_state_destroy(hash_state_t *state);
void sdbm_prep_compile(void);
void sdbm_state_compile_seed(int i, cmph_uint32 seed);

void sdbm_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
cmph_uint32 sdbm_hash_packed(void *packed, const char *k, cmph_uint32 keylen);
void sdbm_hash_vector_packed(void *packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
#endif
