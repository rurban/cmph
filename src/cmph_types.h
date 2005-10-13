#ifndef __CMPH_TYPES_H__
#define __CMPH_TYPES_H__

typedef unsigned char cmph_uint8;
typedef unsigned short cmph_uint16;
typedef unsigned int cmph_uint32;
typedef unsigned long long int cmph_uint64;
typedef long long int cmph_int64;
typedef float cmph_float32;
typedef unsigned char cmph_bool;

/** Signature of a hash function 
 * \param seed value to be used as a disturber of the hash function
 * \param key byte buffer containing the key
 * \param keylen length of the key (bytes)
 * \return uniform distributed (hopefully) value in the range [0;UINT_MAX]
 */
typedef cmph_uint32 (*cmph_hashfunc_t)(cmph_uint32, const char *, cmph_uint32);


#endif
