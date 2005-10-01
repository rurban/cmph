#ifndef __CMPH_BMZ_H__
#define __CMPH_BMZ_H__

#include "cmph.h"
#include "bmz_structs.h"

cmph_config_t *bmz_config_new();
cmph_uint8 bmz_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs);
cmph_uint8 bmz_config_destroy(cmph_config_t *config);
cmph_t *bmz_new(cmph_config_t *config, float c);

void bmz_load(FILE *f, cmph_t *mphf);
int bmz_dump(cmph_t *mphf, FILE *f);
void bmz_destroy(cmph_t *mphf);
cmph_uint32 bmz_search(cmph_t *mphf, const char *key, cmph_uint32 keylen);
#endif
