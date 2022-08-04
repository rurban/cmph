#ifndef __HASH_STATE_H__
#define __HASH_STATE_H__

#include "cmph_types.h"

typedef struct __hash_state_t
{
	CMPH_HASH hashfunc;
	cmph_uint32 seed;
} hash_state_t;

#endif
