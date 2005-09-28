#include "fnv_hash.h"
#include <stdlib.h>

cmph_uint32 fnv_hash(cmph_uint32 seed, const char *k, cmph_uint32 klen)
{
	const unsigned char *bp = (const unsigned char *)k;	
	const unsigned char *be = bp + klen;	
	static unsigned int hval = 0;	

	while (bp < be) 
	{
		
		//hval *= 0x01000193; good for non-gcc compiler
		hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24); //good for gcc

		hval ^= *bp++;
	}
	return hval;
}
