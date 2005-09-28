#ifndef __CMPH_STRUCTS_H__
#define __CMPH_STRUCTS_H__

#include "cmph.h"

/** Hash generation algorithm data
  */
struct __cmph_config_t
{
	CMPH_ALGO algo;
	void *data; //algorithm dependent data
};

/** Hash querying algorithm data
  */
struct __cmph_t
{
	CMPH_ALGO algo;
	void *data; //algorithm dependent data
};

void __cmph_dump(cmph_t *mphf, FILE *);
cmph_t *__cmph_load(FILE *f);

#endif
