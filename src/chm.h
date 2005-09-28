#ifndef __CMPH_CHM_H__
#define __CMPH_CHM_H__

#include "cmph.h"

typedef struct __chm_t chm_t;
typedef struct __chm_config_t chm_config_t;

/** Configuration API */
chm_config_t *chm_config_new();
void chm_config_set_hashfuncs(chm_config_t *config, CMPH_HASH *hashfuncs);
void chm_config_set_graphsize(chm_config_t *config, float c);
void chm_config_destroy(chm_config_t *config);

/** Minimal Perfect Hash API */
cmph_t *chm_new(const chm_config_t *config, cmph_io_adapter_t *key_source);
cmph_uint32 chm_search(chm_t *mphf, const char *key, cmph_uint32 keylen);
cmph_uint32 chm_size(chm_t *mphf);
void chm_destroy(chm_t *mphf);

/** Serialization API */
int chm_dump(chm_t *mphf, FILE *f);
chm_t *chm_load(FILE *f);

#endif
