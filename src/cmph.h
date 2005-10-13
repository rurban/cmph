#ifndef __CMPH_H__
#define __CMPH_H__

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "cmph_types.h"
#include "cmph_io_adapter.h"

typedef struct __cmph_config_t cmph_config_t;
typedef struct __cmph_t cmph_t;

/** Minimal perfect hash algorithms available */
typedef enum { CMPH_BMZ, CMPH_BMZ8, CMPH_CHM, CMPH_BRZ, CMPH_HASHTREE, CMPH_COUNT } CMPH_ALGO; // Always append new algorithms in the end!!
extern const char *cmph_names[];

/** Hash algorithms available */
typedef enum { CMPH_HASH_DJB2, CMPH_HASH_FNV, CMPH_HASH_JENKINS, CMPH_HASH_SDBM, CMPH_HASH_COUNT } CMPH_HASH; //keep in sync with hash.c definitions
extern const char *cmph_hash_names[];

/** Hash configuration API **/
cmph_config_t *cmph_config_new(CMPH_ALGO algo);
cmph_bool cmph_config_set_hashfuncs(cmph_config_t *config, CMPH_HASH *hashfuncs);
cmph_bool cmph_config_set_verbosity(cmph_config_t *config, cmph_uint32 verbosity);
cmph_bool cmph_config_set_graphsize(cmph_config_t *config, float c);
cmph_bool cmph_config_set_tmp_dir(cmph_config_t *config, const char *tmp_dir);
cmph_bool cmph_config_set_memory_availability(cmph_config_t *config, cmph_uint32 memory_availability);
cmph_bool cmph_config_set_seed1(cmph_config_t *config, cmph_uint32 seed);
cmph_bool cmph_config_set_seed2(cmph_config_t *config, cmph_uint32 seed);
cmph_bool cmph_config_set_iterations(cmph_config_t *config, cmph_uint32 iterations);
void cmph_config_destroy(cmph_config_t *config);

/** Minimal Perfect Hash API **/
cmph_t *cmph_new(const cmph_config_t *config, cmph_io_adapter_t *key_source);
cmph_uint32 cmph_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen);
cmph_uint32 cmph_size(const cmph_t *mphf);
void cmph_destroy(cmph_t *mphf);

/** Hash serialization/deserialization */
int cmph_dump(cmph_t *mphf, FILE *f);
cmph_t *cmph_load(FILE *f);

#ifdef __cplusplus
}
#endif

#endif
