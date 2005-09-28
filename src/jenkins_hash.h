#ifndef __JEKINS_HASH_H__
#define __JEKINS_HASH_H__

#include "hash.h"

cmph_uint32 jenkins_hash(cmph_uint32 seed, const char *k, cmph_uint32 klen);

#endif
