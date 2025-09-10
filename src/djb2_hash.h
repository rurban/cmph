#ifndef __DJB2_HASH_H__
#define __DJB2_HASH_H__

#include "hash.h"

hash_state_t *djb2_state_new(cmph_uint32 size);
cmph_uint32 djb2_hash(hash_state_t *state, const char *k, cmph_uint32 keylen);
void djb2_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen);
hash_state_t *djb2_state_copy(hash_state_t *src_state);
hash_state_t *djb2_state_load(const char *buf);
void djb2_state_destroy(hash_state_t *state);
void djb2_prep_compile(bool do_vector);
void djb2_state_compile_seed(int i, cmph_uint32 seed, bool do_vector);

void djb2_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
void djb2_state_pack(hash_state_t *state, void *djb2_packed);
cmph_uint32 djb2_state_packed_size(void);
cmph_uint32 djb2_hash_packed(void *djb2_packed, const char *k, cmph_uint32 keylen);
void djb2_hash_vector_packed(void *djb2_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
  
#endif
