#ifndef __CMPH_XCHMR_H__
#define __CMPH_XCHMR_H__

#include "cmph.h"

typedef struct __xchmr_data_t xchmr_data_t;
typedef struct __xchmr_config_data_t xchmr_config_data_t;

xchmr_config_data_t *xchmr_config_new();
void xchmr_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs);
void xchmr_config_destroy(cmph_config_t *mph);
cmph_t *xchmr_new(cmph_config_t *mph, float c);

void xchmr_load(FILE *f, cmph_t *mphf);
int xchmr_dump(cmph_t *mphf, FILE *f);
void xchmr_destroy(cmph_t *mphf);
cmph_uint32 xchmr_search(cmph_t *mphf, const char *key, cmph_uint32 keylen);
#endif
