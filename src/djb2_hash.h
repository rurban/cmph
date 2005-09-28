#ifndef __DJB2_HASH_H__
#define __DJB2_HASH_H__

#include "hash.h"

cmph_uint32 djb2_hash(cmph_uint32 seed, const char *key, cmph_uint32 keylen);

#endif
