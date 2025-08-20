#ifndef __CRC32_HASH_H__
#define __CRC32_HASH_H__

#include "hash.h"

void crc32_state_init(hash_state_t *state, cmph_uint32 size);
cmph_uint32 crc32_hash(hash_state_t *state, const char *k, const cmph_uint32 keylen);
void crc32_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen);
//hash_state_t *crc32_state_copy(hash_state_t *src_state);
hash_state_t *crc32_state_load(const char *buf);
void crc32_state_destroy(hash_state_t *state);

void crc32_hash_vector(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
cmph_uint32 crc32_hash_packed(void *packed, const char *k, cmph_uint32 keylen);
void crc32_hash_vector_packed(void *packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);

#endif
