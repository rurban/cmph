#include "sdbm_hash.h"
#include <stdlib.h>

cmph_uint32 sdbm_hash(cmph_uint32 seed, const char *k, cmph_uint32 klen)
{
	register cmph_uint32 hash = 0;
	const unsigned char *ptr = k;
	cmph_uint32 i = 0;

	while(i < klen) {
		hash = *ptr + (hash << 6) + (hash << 16) - hash;
		++ptr, ++i;
	}
	return hash;
}
