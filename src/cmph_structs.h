#ifndef __CMPH_STRUCTS_H__
#define __CMPH_STRUCTS_H__

#include "cmph_types.h"
#include "bmz_structs.h"
#include "bmz8_structs.h"
#include "chm_structs.h"
#include "brz_structs.h"
#include "hashtree_structs.h"

/** Hash generation algorithm data
  */
typedef union
{
	bmz_config_t bmz;
	bmz8_config_t bmz8;
	chm_config_t chm;
	brz_config_t brz;
	hashtree_config_t hashtree;
} impl_config_t;

struct __cmph_config_t
{
	CMPH_ALGO algo;
	int errno;
	int verbosity;
	impl_config_t impl; //algorithm dependent data
};

/** Hash querying algorithm data
 * Do not use union trick here to avoid memory
 * usage overhead.
 */
struct __cmph_t
{
	CMPH_ALGO algo;
	void *impl; //algorithm dependent data
};

void __cmph_dump(cmph_t *mphf, FILE *);
cmph_t *__cmph_load(FILE *f);

#endif
