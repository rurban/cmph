#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "hash.h"
#include "dbj2_hash.h"
#include "fnv_hash.h"
#include "jenkins_hash.h"
#include "sdbm_hash.h"

//#define DEBUG
#include "debug.h"

const char *cmph_hash_names[] = { "djb2", "fnv", "jenkins", "sdbm", NULL };
cmph_hashfunc_t cmph_hashfuncs[] = { djb2_hash, fnv_hash, jenkins_hash, sdbm, NULL };
