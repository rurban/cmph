#ifndef __CMPH_CHM_H__
#define __CMPH_CHM_H__

#include "cmph.h"

/** Configuration API */
cmph_config_t *chm_config_new();
cmph_bool chm_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs);
cmph_bool chm_config_set_graphsize(cmph_config_t *config, float c);
void chm_config_destroy(cmph_config_t *config);

/** Minimal Perfect Hash API */
cmph_t *chm_new(const cmph_config_t *config, cmph_io_adapter_t *key_source);
cmph_uint32 chm_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen);
cmph_uint32 chm_size(const cmph_t *mphf);
void chm_destroy(cmph_t *mphf);

/** Serialization API */
int chm_dump(cmph_t *mphf, FILE *f);
cmph_t *chm_load(FILE *f);

#endif
