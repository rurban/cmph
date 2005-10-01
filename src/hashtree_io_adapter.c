#include "hashtree_io_adapter.h"
#include <stdio.h>

typedef struct
{
	int fd;
	cmph_hashfunc_t f1;
	cmph_uint32 s1;
	cmph_hashfunc_t f2;
	cmph_uint32 s2;
	cmph_uint64 offset;
	cmph_uint32 pos;
	cmph_uint32 leaf_id;
	cmph_uint32 leaf_size;
	leaf_key_t current_key;
} leaf_source_t;
	
void hashtree_rewind(void *data)
{
	leaf_source_t *leaf_source = (leaf_source_t *)data;
	lseek64(leaf_source->fd, sizeof(leaf_key_t) * leaf_source->offset, SEEK_SET); 
}

int hashtree_hash(void *data, cmph_hashfunc_t f, cmph_uint32 seed, cmph_uint32 *hval)
{
	leaf_source_t *leaf_source = (leaf_source_t *)data;
	if (leaf_source->f1 == f && leaf_source->s1 == seed) *hval = leaf_source->current_key.h1;
	else if (leaf_source->f2 == f && leaf_source->s2 == seed) *hval = leaf_source->current_key.h2;
	else return 0;
	return 1;
}

int hashtree_next(void *data)
{
	leaf_source_t *leaf_source = (leaf_source_t *)data;
	int c = read(leaf_source->fd, &(leaf_source->current_key), sizeof(leaf_source->current_key));
	return c == sizeof(leaf_source->current_key);
}
cmph_io_adapter_t *hashtree_io_adapter(int fd, cmph_uint64 offset, cmph_uint32 leaf_id, cmph_uint32 leaf_size, cmph_hashfunc_t f1, cmph_uint32 s1, cmph_hashfunc_t f2, cmph_uint32 s2)
{
	leaf_source_t *leaf_source = (leaf_source_t *)malloc(sizeof(leaf_source_t));
	cmph_io_adapter_t *source = (cmph_io_adapter_t *)malloc(sizeof(cmph_io_adapter_t));
	if (!source || !leaf_source) return NULL;
	leaf_source->leaf_size = leaf_size;
	leaf_source->leaf_id = leaf_id;
	leaf_source->pos = 0;
	leaf_source->offset = offset;
	leaf_source->f1 = f1;
	leaf_source->s1 = s1;
	leaf_source->f2 = f2;
	leaf_source->s2 = s2;
	leaf_source->fd = fd;
	source->data = leaf_source;
	source->nkeys = leaf_size;
	source->read = NULL;
	source->dispose = NULL;
	source->rewind = hashtree_rewind;
	source->hash = hashtree_hash;
	source->next = hashtree_next;
	hashtree_rewind(source->data);
	hashtree_next(source->data);
	return source;
}

void hashtree_io_adapter_destroy(cmph_io_adapter_t *source)
{
	leaf_source_t *leaf_source = (leaf_source_t *)(source->data);
	free(leaf_source);
	free(source);
}

