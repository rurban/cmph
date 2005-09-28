#include "djb2_hash.h"
#include <stdlib.h>

cmph_uint32 djb2_hash(cmph_uint32 seed, const char *k, cmph_uint32 klen)
{
	register cmph_uint32 hash = 5381;
	const unsigned char *ptr = k;
	cmph_uint32 i = 0;
	while (i < klen) 
	{
		hash = hash*33 ^ *ptr;
		++ptr, ++i;
	}
	return hash;
}
