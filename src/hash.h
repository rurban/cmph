#ifndef __CMPH_HASH_H__
#define __CMPH_HASH_H__

#include "cmph_types.h"

/** Signature of a hash function 
 * \param seed value to be used as a disturber of the hash function
 * \param key byte buffer containing the key
 * \param keylen length of the key (bytes)
 * \return uniform distributed (hopefully) value in the range [0;UINT_MAX]
 */
typedef cmph_uint32 (*cmph_hashfunc_t)(cmph_uint32, const char *, cmph_uint32);
extern cmph_hashfunc_t cmph_hashfuncs[];
#endif
