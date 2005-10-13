#ifndef __CMPH_HASH_H__
#define __CMPH_HASH_H__

#include "cmph_types.h"

extern cmph_hashfunc_t cmph_hashfuncs[];

#define HASHKEY(func, seed, key, keylen)\
	(*(cmph_hashfuncs[func]))(seed, key, keylen)

#endif
