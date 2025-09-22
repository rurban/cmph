#include "jenkins_hash.h"
#include <stdlib.h>
#ifdef WIN32
#define _USE_MATH_DEFINES //For M_LOG2E
#endif
#include <math.h>
#include <limits.h>
#include <string.h>

#include "hash.h"

//#define DEBUG
#include "debug.h"

#define hashsize(n) ((cmph_uint32)1<<(n))
#define hashmask(n) (hashsize(n)-1)

//#define NM2 /* Define this if you do not want power of 2 table sizes*/

/*
   --------------------------------------------------------------------
   mix -- mix 3 32-bit values reversibly.
   For every delta with one or two bits set, and the deltas of all three
   high bits or all three low bits, whether the original value of a,b,c
   is almost all zero or is uniformly distributed,
 * If mix() is run forward or backward, at least 32 bits in a,b,c
 have at least 1/4 probability of changing.
 * If mix() is run forward, every bit of c will change between 1/3 and
 2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 mix() was built out of 36 single-cycle latency instructions in a
 structure that could supported 2x parallelism, like so:
 a -= b;
 a -= c; x = (c>>13);
 b -= c; a ^= x;
 b -= a; x = (a<<8);
 c -= a; b ^= x;
 c -= b; x = (b>>13);
 ...
 Unfortunately, superscalar Pentiums and Sparcs can't take advantage
 of that parallelism.  They've also turned some of those single-cycle
 latency instructions into multi-cycle latency instructions.  Still,
 this is the fastest good hash I could find.  There were about 2^^68
 to choose from.  I only looked at a billion or so.
 --------------------------------------------------------------------
 */
#define mix(a,b,c) \
{ \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<<8);  \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12); \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>>5);  \
	a -= b; a -= c; a ^= (c>>3);  \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}

/*
   --------------------------------------------------------------------
   hash() -- hash a variable-length key into a 32-bit value
k       : the key (the unaligned variable-length array of bytes)
len     : the length of the key, counting by bytes
initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 6*len+35 instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (cmph_uint8 **)k, do it like this:
for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
 */
hash_state_t *jenkins_state_new(cmph_uint32 size) //size of hash table
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
	if (!state) return NULL;
	if (size > 0) state->seed = ((cmph_uint32)rand() % size);
	else state->seed = 0;
	DEBUGP("Initializing jenkins hash with seed %u\n", state->seed);
	return state;
}
void jenkins_state_destroy(hash_state_t *state)
{
	free(state);
}


static inline void __jenkins_hash_vector(cmph_uint32 seed, const unsigned char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	register cmph_uint32 len, length;

	/* Set up the internal state */
	length = keylen;
	len = length;
	hashes[0] = hashes[1] = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	hashes[2] = seed;   /* the previous hash value - seed in our case */

	/*---------------------------------------- handle most of the key */
	while (len >= 12)
	{
		hashes[0] += ((cmph_uint32)k[0] +((cmph_uint32)k[1]<<8) +((cmph_uint32)k[2]<<16) +((cmph_uint32)k[3]<<24));
		hashes[1] += ((cmph_uint32)k[4] +((cmph_uint32)k[5]<<8) +((cmph_uint32)k[6]<<16) +((cmph_uint32)k[7]<<24));
		hashes[2] += ((cmph_uint32)k[8] +((cmph_uint32)k[9]<<8) +((cmph_uint32)k[10]<<16)+((cmph_uint32)k[11]<<24));
		mix(hashes[0],hashes[1],hashes[2]);
		k += 12; len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	hashes[2]  += length;
	switch(len)              /* all the case statements fall through */
	{
	        case 11:
			hashes[2] +=((cmph_uint32)k[10]<<24);
			// fallthrough
		case 10:
			hashes[2] +=((cmph_uint32)k[9]<<16);
			// fallthrough
		case 9 :
			hashes[2] +=((cmph_uint32)k[8]<<8);
			/* the first byte of hashes[2] is reserved for the length */
			// fallthrough
		case 8 :
			hashes[1] +=((cmph_uint32)k[7]<<24);
			// fallthrough
		case 7 :
			hashes[1] +=((cmph_uint32)k[6]<<16);
			// fallthrough
		case 6 :
			hashes[1] +=((cmph_uint32)k[5]<<8);
			// fallthrough
		case 5 :
			hashes[1] +=(cmph_uint8) k[4];
			// fallthrough
		case 4 :
			hashes[0] +=((cmph_uint32)k[3]<<24);
			// fallthrough
		case 3 :
			hashes[0] +=((cmph_uint32)k[2]<<16);
			// fallthrough
		case 2 :
			hashes[0] +=((cmph_uint32)k[1]<<8);
			// fallthrough
		case 1 :
			hashes[0] +=(cmph_uint8)k[0];
			// fallthrough
		/* case 0: nothing left to add */
	}

	mix(hashes[0],hashes[1],hashes[2]);
}

cmph_uint32 jenkins_hash(hash_state_t *state, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
	__jenkins_hash_vector(state->seed, (const unsigned char*)k, keylen, hashes);
	return hashes[2];
/*	cmph_uint32 a, b, c;
	cmph_uint32 len, length;

	// Set up the internal state
	length = keylen;
	len = length;
	a = b = 0x9e3779b9;  // the golden ratio; an arbitrary value
	c = state->seed;   // the previous hash value - seed in our case

	// handle most of the key
	while (len >= 12)
	{
		a += (k[0] +((cmph_uint32)k[1]<<8) +((cmph_uint32)k[2]<<16) +((cmph_uint32)k[3]<<24));
		b += (k[4] +((cmph_uint32)k[5]<<8) +((cmph_uint32)k[6]<<16) +((cmph_uint32)k[7]<<24));
		c += (k[8] +((cmph_uint32)k[9]<<8) +((cmph_uint32)k[10]<<16)+((cmph_uint32)k[11]<<24));
		mix(a,b,c);
		k += 12; len -= 12;
	}

	// handle the last 11 bytes
	c  += length;
	switch(len)              /// all the case statements fall through
	{
		case 11:
			c +=((cmph_uint32)k[10]<<24);
		case 10:
			c +=((cmph_uint32)k[9]<<16);
		case 9 :
			c +=((cmph_uint32)k[8]<<8);
			// the first byte of c is reserved for the length
		case 8 :
			b +=((cmph_uint32)k[7]<<24);
		case 7 :
			b +=((cmph_uint32)k[6]<<16);
		case 6 :
			b +=((cmph_uint32)k[5]<<8);
		case 5 :
			b +=k[4];
		case 4 :
			a +=((cmph_uint32)k[3]<<24);
		case 3 :
			a +=((cmph_uint32)k[2]<<16);
		case 2 :
			a +=((cmph_uint32)k[1]<<8);
		case 1 :
			a +=k[0];
		// case 0: nothing left to add
	}

	mix(a,b,c);

	/// report the result

	return c;
	*/
}

void jenkins_hash_vector_(hash_state_t *state, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	__jenkins_hash_vector(state->seed, (const unsigned char*)k, keylen, hashes);
}

void jenkins_state_dump(hash_state_t *state, char **buf, cmph_uint32 *buflen)
{
	*buflen = sizeof(cmph_uint32);
	*buf = (char *)malloc(sizeof(cmph_uint32));
	if (!*buf)
	{
		*buflen = UINT_MAX;
		return;
	}
	memcpy(*buf, &(state->seed), sizeof(cmph_uint32));
	DEBUGP("Dumped jenkins state with seed %u\n", state->seed);
	return;
}

//hash_state_t *jenkins_state_copy(hash_state_t *src_state)
//{
//	hash_state_t *dest_state = (hash_state_t *)malloc(sizeof(hash_state_t));
//	dest_state->hashfunc = src_state->hashfunc;
//	dest_state->seed = src_state->seed;
//	return dest_state;
//}

hash_state_t *jenkins_state_load(const char *buf)
{
	hash_state_t *state = (hash_state_t *)malloc(sizeof(hash_state_t));
        const unsigned char *p = (const unsigned char *)buf;
        if ((long)buf % 4)
                state->seed = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
        else
                state->seed = *(cmph_uint32 *)buf;
	state->hashfunc = CMPH_HASH_JENKINS;
	DEBUGP("Loaded jenkins state with seed %u\n", state->seed);
	return state;
}


/** \fn void jenkins_state_pack(hash_state_t *state, void *jenkins_packed);
 *  \brief Support the ability to pack a jenkins function into a preallocated contiguous memory space pointed by jenkins_packed.
 *  \param state points to the jenkins function
 *  \param jenkins_packed pointer to the contiguous memory area used to store the jenkins function. The size of jenkins_packed must be at least jenkins_state_packed_size()
 */
void jenkins_state_pack(hash_state_t *state, void *jenkins_packed)
{
	if (state && jenkins_packed)
	{
		memcpy(jenkins_packed, &(state->seed), sizeof(cmph_uint32));
	}
}

/** \fn cmph_uint32 jenkins_state_packed_size(hash_state_t *state);
 *  \brief Return the amount of space needed to pack a jenkins function.
 *  \return the size of the packed function or zero for failures
 */
cmph_uint32 jenkins_state_packed_size(void)
{
	return sizeof(cmph_uint32);
}


/** \fn cmph_uint32 jenkins_hash_packed(void *jenkins_packed, const char *k, cmph_uint32 keylen);
 *  \param jenkins_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \return an integer that represents a hash value of 32 bits.
 */
cmph_uint32 jenkins_hash_packed(void *jenkins_packed, const char *k, cmph_uint32 keylen)
{
	cmph_uint32 hashes[3];
	__jenkins_hash_vector(*((cmph_uint32 *)jenkins_packed), (const unsigned char*)k, keylen, hashes);
	return hashes[2];
}

/** \fn jenkins_hash_vector_packed(void *jenkins_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes);
 *  \param jenkins_packed is a pointer to a contiguous memory area
 *  \param key is a pointer to a key
 *  \param keylen is the key length
 *  \param hashes is a pointer to a memory large enough to fit three 32-bit integers.
 */
void jenkins_hash_vector_packed(void *jenkins_packed, const char *k, cmph_uint32 keylen, cmph_uint32 * hashes)
{
	__jenkins_hash_vector(*((cmph_uint32 *)jenkins_packed), (const unsigned char*)k, keylen, hashes);
}

/* jenkins is always vectored
   TODO optimize to skip the mix-step if not vectored
 */
void jenkins_prep_compile(bool do_vector, FILE* out) {
	(void)do_vector;
	fputs(
"/* jenkins_hash */\n"
"#define mix(a,b,c) \\\n"
"{\\\n"
"	a -= b; a -= c; a ^= (c>>13); \\\n"
"	b -= c; b -= a; b ^= (a<<8);  \\\n"
"	c -= a; c -= b; c ^= (b>>13); \\\n"
"	a -= b; a -= c; a ^= (c>>12); \\\n"
"	b -= c; b -= a; b ^= (a<<16); \\\n"
"	c -= a; c -= b; c ^= (b>>5);  \\\n"
"	a -= b; a -= c; a ^= (c>>3);  \\\n"
"	b -= c; b -= a; b ^= (a<<10); \\\n"
"	c -= a; c -= b; c ^= (b>>15); \\\n"
"}\n"
"static inline void jenkins_hash_vector(const uint32_t seed, const unsigned char *k, uint32_t keylen, uint32_t *hashes)\n"
"{\n"
"	uint32_t len, length;\n"
"\n"
"	/* Set up the internal state */\n"
"	length = keylen;\n"
"	len = length;\n"
"	hashes[0] = hashes[1] = 0x9e3779b9;  /* the golden ratio; an arbitrary value */\n"
"	hashes[2] = seed;   /* the previous hash value - seed in our case */\n"
"\n"
"	/*---------------------------------------- handle most of the key */\n"
"	while (len >= 12)\n"
"	{\n"
"		hashes[0] += ((uint32_t)k[0] +((uint32_t)k[1]<<8) +((uint32_t)k[2]<<16) +((uint32_t)k[3]<<24));\n"
"		hashes[1] += ((uint32_t)k[4] +((uint32_t)k[5]<<8) +((uint32_t)k[6]<<16) +((uint32_t)k[7]<<24));\n"
"		hashes[2] += ((uint32_t)k[8] +((uint32_t)k[9]<<8) +((uint32_t)k[10]<<16)+((uint32_t)k[11]<<24));\n"
"		mix(hashes[0], hashes[1], hashes[2]);\n"
"		k += 12; len -= 12;\n"
"	}\n"
"	/*------------------------------------- handle the last 11 bytes */\n"
"	hashes[2] += length;\n"
"	switch(len)              /* all the case statements fall through */\n"
"	{\n"
"	        case 11:\n"
"			hashes[2] +=((uint32_t)k[10]<<24);\n"
"			// fallthrough\n"
"		case 10:\n"
"			hashes[2] +=((uint32_t)k[9]<<16);\n"
"			// fallthrough\n"
"		case 9 :\n"
"			hashes[2] +=((uint32_t)k[8]<<8);\n"
"			/* the first byte of hashes[2] is reserved for the length */\n"
"			// fallthrough\n"
"		case 8 :\n"
"			hashes[1] +=((uint32_t)k[7]<<24);\n"
"			// fallthrough\n"
"		case 7 :\n"
"			hashes[1] +=((uint32_t)k[6]<<16);\n"
"			// fallthrough\n"
"		case 6 :\n"
"			hashes[1] +=((uint32_t)k[5]<<8);\n"
"			// fallthrough\n"
"		case 5 :\n"
"			hashes[1] +=(uint8_t) k[4];\n"
"			// fallthrough\n"
"		case 4 :\n"
"			hashes[0] +=((uint32_t)k[3]<<24);\n"
"			// fallthrough\n"
"		case 3 :\n"
"			hashes[0] +=((uint32_t)k[2]<<16);\n"
"			// fallthrough\n"
"		case 2 :\n"
"			hashes[0] +=((uint32_t)k[1]<<8);\n"
"			// fallthrough\n"
"		case 1 :\n"
"			hashes[0] +=(uint8_t)k[0];\n"
"			// fallthrough\n"
"		/* case 0: nothing left to add */\n"
"	}\n"
"	mix(hashes[0], hashes[1], hashes[2]);\n"
"}\n"
"\n", out);
}
// if not vectored, compile extra call
void jenkins_state_compile_seed(int i, cmph_uint32 seed, bool do_vector, FILE* out) {
    if (!do_vector) {
	fprintf(out,
	       "static uint32_t jenkins_hash_%d(const unsigned char *key, uint32_t keylen) {\n"
	       "    uint32_t hashes[3];\n"
	       "    jenkins_hash_vector(%uU, key, keylen, hashes);\n"
	       "    return hashes[2];\n"
	       "}\n", i, seed);
    }
    return;
}
