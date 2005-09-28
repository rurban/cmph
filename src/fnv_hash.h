#ifndef __FNV_HASH_H__
#define __FNV_HASH_H__

#include "hash.h"

cmph_uint32 fnv_hash(cmph_uint32 seed, const char *k, cmph_uint32 keylen);

#endif
