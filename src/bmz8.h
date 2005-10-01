#ifndef __CMPH_BMZ8_H__
#define __CMPH_BMZ8_H__

#include "cmph.h"

/** Configuration API */
cmph_config_t *bmz8_config_new();
void cm_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs);
void bmz8_config_set_graphsize(cmph_config_t *config, float c);
cmph_bool bmz8_set_seed1(cmph_config_t *config, cmph_uint32 s1);
cmph_bool bmz8_set_seed2(cmph_config_t *config, cmph_uint32 s2);
cmph_bool bmz8_set_iterations(cmph_config_t *config, cmph_uint32 iterations);
void bmz8_config_destroy(cmph_config_t *config);

/** Minimal Perfect Hash API */
cmph_t *bmz8_new(const cmph_config_t *config, cmph_io_adapter_t *key_source);
cmph_uint32 bmz8_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen);
cmph_uint32 bmz8_size(const cmph_t *mphf);
void bmz8_destroy(cmph_t *mphf);

/** Serialization API */
int bmz8_dump(cmph_t *mphf, FILE *f);
cmph_t *bmz8_load(FILE *f);

#endif
