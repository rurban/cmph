#include "hashtree.h"
#include "hashtree_io_adapter.h"
#include "graph.h"
#include "cmph_structs.h"
#include "hashtree_structs.h"
#include "hash.h"
#include "bitbool.h"
#include "cmph_mkstemp.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

typedef struct
{
	const cmph_config_t *config;
	cmph_io_adapter_t *source;
	int fd;
	uint8 *h1_seed;
	uint8 *h2_seed;
	hashtree_t *mph;
	uint8 *leaf_failed;
} state_t;

static cmph_uint8 gen_seeds(state_t *state);
static cmph_int64 assign_leaves(state_t *state);
static cmph_uint8 sort_keys(state_t *state);

int hashtree_tmp_fd(const char *mask)
{
	cmph_uint32 tmp_dir_len = strlen(cmph_get_tmp_dir());
	char *tmpl = (char *)calloc(tmp_dir_len + strlen(mask)+ 2, 1);
	strncpy(tmpl, cmph_get_tmp_dir(), tmp_dir_len);
	#ifdef WIN32
	strncat(tmpl, "\\", 1);
	#else
	strncat(tmpl, "/", 1);
	#endif
	strncat(tmpl, mask, strlen(mask));
	int fd = cmph_mkstemp(tmpl);
	free(tmpl);
	return fd;
}

cmph_t *hashtree_new(const cmph_config_t *config, cmph_io_adapter_t *source)
{
	int c = 0;
	cmph_uint32 iterations = 10;
	cmph_t *ret = (cmph_t *)malloc(sizeof(cmph_t));
	hashtree_t *mph = (hashtree_t *)malloc(sizeof(hashtree_t));
	if ((!ret) || (!mph)) return NULL;

	while (iterations)
	{
		int fd = hashtree_tmp_fd("hashtree.XXXXXX");
		if (fd == -1)
		{
			if (config->verbosity)
			{
				fprintf(stderr, "Failed opening temp file at directory %s\n", cmph_get_tmp_dir());
			}
			free(ret);
			free(mph);
			return NULL;
		}
			
		source->rewind(source->data);
		//First, divide keys by leaves
		c = assign_leaves(config, mph, source, fd);
		if (c <= 0) 
		{
			--iterations;
			continue;
		}
		//Now sort keys by assigned leaf
		c = sort_keys(config, fd, source->nkeys);
	}
	return ret;
}

//Distribute keys in subgroups for minimal perfect hash generation. Each subgroup
//can have at most ceil(256/c) keys. The reason for this is that we can have at most
//256 vertices in the graph we will generate in the next 
//step (number of vertices = ceil(nkeys * c))
static cmph_int64 assign_leaves(const cmph_config_t *config, hashtree_t *mph, cmph_io_adapter_t *source, int fd)
{
	int c = 0;
	cmph_uint64 i = 0;
	cmph_uint32 maxkeys = ceil(256 / config->impl.hashtree.leaf_c);
	mph->k = ceil(maxkeys * config->impl.hashtree.root_c);

	cmph_uint16 *size = (cmph_uint16 *)malloc(sizeof(cmph_uint16) * mph->k);
	memset(size, 0, mph->k * sizeof(cmph_uint16));

	DEBUGP("Assigning keys to %u leaves with at most %u keys each.\n", mph->k, maxkeys);
	c = ftruncate64(fd, sizeof(leaf_key_t) * ((cmph_uint64)source->nkeys));
	if (c == -1) return 0;

	for (i = 0; i < source->nkeys; ++i)
	{
		leaf_key_t leaf_key;
		cmph_uint32 keylen;
		char *key;
		source->read(source->data, &key, &keylen);
		leaf_key.h0 = HASHKEY(mph->hash[0], config->h0_seed, key, keylen) % mph->k;
		leaf_key.h1 = HASHKEY(mph->hash[1], config->h1_seed[leaf_key.h0], key, keylen);
		leaf_key.h2 = HASHKEY(mph->hash[2], config->h2_seed[leaf_key.h0], key, keylen);
		if (size[leaf_key.h0] == maxkeys)
		{
			free(size);
			if (config->verbosity)
			{
				fprintf("Too many collisions at leaf %u\n", leaf_key.h0);
			}
			return -1;
		}
		++(size[leaf_key.h0]);
		c = write(fd, &leaf_key, sizeof(leaf_key));
		if (c != sizeof(leaf_key))
		{
			free(size);
			return 0;
		}
	}
	return 1;
}

static int leaf_key_compare(const void *a, const void *b) 
{
	leaf_key_t *la = (leaf_key_t *)a;
	leaf_key_t *lb = (leaf_key_t *)b;
	return la->h0 - lb->h0;
}
//Implements straight external mergesort using leaf id as key
static cmph_uint8 sort_keys(const cmph_config_t *config, int fd, cmph_uint64 nkeys)
{
	int tmpfd = hashtree_tmp_fd("hashtree_sorted.XXXXXX");;
	int c = 0;
	cmph_uint32 blocksize = floor(config->memory / ((double)sizeof(leaf_key_t)));
	cmph_uint32 nblocks = ceil(nkeys / (double)blocksize);
	cmph_uint64 sorted = 0;
	leaf_key_t *block = (leaf_key_t *)malloc(sizeof(leaf_key_t) * blocksize);
	cmph_uint32 *blockfill = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * nblocks);
	cmph_uint32 *blockhead = (cmph_uint32 *)malloc(sizeof(leaf_key_t) * nblocks);
	cmph_uint32 current_block = 0;
	if (!block || !blockfill || !blockhead || tmp_fd == -1) return 0;
	rewind(fd);

	//Sort blocks
	while (1)
	{
		cmph_uint32 nkeys_read = blocksize > nkeys - sorted ? nkeys - sorted : blocksize;
		blockfill[current_block] = 0;
		c = read(fd, block, sizeof(leaf_key_t) * nkeys_read);
		if (c != nkeys_read * sizeof(leaf_key_t)) 
		{
			free(block);
			free(blockfill);
			return 0;
		}
		blockfill[current_block] = nkeys_read;
		qsort(block, blockfill[current_block], sizeof(leaf_key_t), leaf_key_compare);
		c = write(tmpfd, block, sizeof(leaf_key_t) * blockfill[current_block]);
		free(block);
		if (c != blockfill[current_block] * sizeof(leaf_key_t)) 
		{
			free(blockfill);
			return 0;
		}
		sorted -= blockfill[current_block];
		++current_block;
	}
	assert(sorted == nkeys);
	sorted = 0;
	//Merge Blocks
	for (current_block = 0; current_block < nblocks; ++current_block)
	{
		blockhead[current_block].h0 = UINT_MAX;
	}
	while (sorted != nkeys)
	{
		leaf_key_t min;
		min.h0 = UINT_MAX;
		for (current_block = 0; current_block < nblocks; ++current_block)
		{
			if (blockhead[current_block].h0 < min.h0)
			{
				min = blockhead[current_block];
				if (blockfill[current_block])
				{
					cmph_uint64 pos = current_block * blocksize;
					pos += blockfill[current_block] - 1;
					pos *= sizeof(leaf_key_t);
					c = fseek64(tmpfd, pos, SEEK_SET);		
					fread(&(blockhead[current_block]), 1, sizeof(blockhead[current_block]), tmpfd);
					--blockfill[current_block];	
				}
				if (min.h0 != UINT_MAX) 
				{
					fwrite(&min, 1, sizeof(min), fd);
					++sorted;
				}
			}
		}
	}
 	#ifdef WIN32
	remove(lockname);
	#else
	unlink(lockname);
	#endif
}
	
cmph_uint32 hashtree_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	hashtree_data_t *hashtree = mphf->data;
	cmph_uint32 h1 = hash(hashtree->hashes[0], key, keylen) % hashtree->n;
	cmph_uint32 h2 = hash(hashtree->hashes[1], key, keylen) % hashtree->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= hashtree->n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, hashtree->g[h1], hashtree->g[h2], hashtree->m);
	return (hashtree->g[h1] + hashtree->g[h2]) % hashtree->m;
}
void hashtree_destroy(cmph_t *mphf)
{
	hashtree_data_t *data = (hashtree_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hashes[0]);
	hash_state_destroy(data->hashes[1]);
	free(data->hashes);
	free(data);
	free(mphf);
}
