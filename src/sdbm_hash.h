#ifndef __SDBM_HASH_H__
#define __SDBM_HASH_H__

#include "hash.h"

cmph_uint32 sdbm_hash(cmph_uint32 seed, const char *k, cmph_uint32 klen);

#endif
