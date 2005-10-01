#ifndef __CMPH_HASHTREE_H__
#define __CMPH_HASHTREE_H__

#include "cmph.h"

/** Configuration API */
cmph_config_t *hashtree_config_new();
cmph_bool hashtree_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs);
cmph_bool hashtree_config_set_root_c(cmph_config_t *config, float c);
cmph_bool hashtree_config_set_leaf_c(cmph_config_t *config, float c);
cmph_bool hashtree_config_set_leaf_algo(cmph_config_t *config, CMPH_ALGO algo);
cmph_bool hashtree_config_destroy(cmph_config_t *config);

/** Minimal Perfect Hash API */
cmph_t *hashtree_new(const cmph_config_t *config, cmph_io_adapter_t *key_source);
cmph_uint32 hashtree_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen);
cmph_uint32 hashtree_size(const cmph_t *mphf);
void hashtree_destroy(cmph_t *mphf);

/** Serialization API */
int hashtree_dump(cmph_t *mphf, FILE *f);
cmph_t *hashtree_load(FILE *f);

#endif
