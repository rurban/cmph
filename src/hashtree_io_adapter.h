#include "cmph_io_adapter.h"
typedef struct
{
	cmph_uint32 h0;
	cmph_uint8 h1; //no mod applied
	cmph_uint8 h2; //no mod applied
	cmph_uint16 pad;
} leaf_key_t;

cmph_io_adapter_t *hashtree_io_adapter(FILE *fd, cmph_uint64 offset, cmph_uint32 leaf_id, cmph_uint32 leaf_size, cmph_hashfunc_t f1, cmph_uint32 s1, cmph_hashfunc_t f2, cmph_uint32 s2);
void hashtree_io_adapter_destroy(cmph_io_adapter_t *);
