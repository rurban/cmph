#include "hashtree.h"
#include "hashtree_io_adapter.h"
#include "cmph_structs.h"
#include "hashtree_structs.h"
#include "hash.h"
#include "bitbool.h"
#include "cmph_mkstemp.h"

#include <limits.h>
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
	char *fname;
	cmph_uint8 *h1_seed;
	cmph_uint8 *h2_seed;
	hashtree_t *mph;
	cmph_uint16 *leaf_size;
	cmph_uint16 max_leaf_size;
} state_t;

static cmph_uint8 gen_seeds(state_t *state);
static cmph_int64 assign_leaves(state_t *state);
static cmph_uint8 sort_keys(const cmph_config_t *config, int fd, cmph_uint64 nkeys);
static cmph_uint8 create_leaf_mph(state_t *state);

int hashtree_tmp_fd(const char *mask, char **fname)
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
	if (fname) *fname = tmpl;
	else free(tmpl);
	return fd;
}

cmph_t *hashtree_new(const cmph_config_t *config, cmph_io_adapter_t *source)
{
	cmph_uint32 i = 0;
	int c = 0;
	cmph_uint32 iterations = config->impl.hashtree.iterations;
	cmph_t *ret = (cmph_t *)malloc(sizeof(cmph_t));
	hashtree_t *mph = (hashtree_t *)malloc(sizeof(hashtree_t));
	state_t *state = (state_t *)malloc(sizeof(state_t));
	if ((!ret) || (!mph) || (!state)) return NULL;

	memset(state, 0, sizeof(state_t));
	state->config = config;
	state->source = source;
	state->mph = mph;
	state->max_leaf_size = ceil(256 / state->config->impl.hashtree.leaf_c);
	state->mph->k = ceil(state->max_leaf_size * state->config->impl.hashtree.root_c);
	state->h1_seed = malloc(sizeof(cmph_uint16) * state->mph->k);
	state->h2_seed = malloc(sizeof(cmph_uint16) * state->mph->k);
	state->mph->leaf = (cmph_t **)malloc(sizeof(cmph_t *) * state->mph->k);
	memset(state->mph->leaf, 0, sizeof(cmph_t *) * state->mph->k);
	state->fd = hashtree_tmp_fd("hashtree.XXXXXX", &(state->fname));

	while (iterations)
	{
		cmph_uint8 longbreak = 0;
		gen_seeds(state);
		if (state->fd == -1)
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
		mph->h0_seed = rand() % state->mph->k;
		//First, divide keys by leaves
		c = assign_leaves(state);
		if (c <= 0) 
		{
			--iterations;
			continue;
		}
		//Now sort keys by assigned leaf
		c = sort_keys(config, state->fd, source->nkeys);

		//Create minimal perfect hashes
		while (iterations)
		{
			c = create_leaf_mph(state);
			if (c)
			{
				longbreak = 1;
				break;
			}
			--iterations;
		}
		if (longbreak) break;
			
	}
	
	close(state->fd);
 	#ifdef WIN32
	remove(state->fname);
	#else
	unlink(state->fname);
	#endif
	free(state->h1_seed);
	free(state->h2_seed);
	mph->offset = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*mph->k);
	if (!mph->offset)
	{
		for (i = 0; i < mph->k; ++i)
		{
			cmph_destroy(mph->leaf[i]);
		}
		free(state->leaf_size);
		free(mph);
		free(ret);
		return NULL;
	}
	for (i = 1; i < mph->k; ++i)
	{
		mph->offset[i] = mph->offset[i - 1] + state->leaf_size[i - 1];
	}
	free(state->leaf_size);
	return ret;
}

//Create seeds for leaves which mph generation failed
static cmph_uint8 gen_seeds(state_t *state)
{
	cmph_uint32 i = 0;
	for (i = 0; i < state->mph->k; ++i)
	{
		if (!(state->mph->leaf[i]))
		{
			state->h1_seed[i] = rand() % 256;
			state->h2_seed[i] = rand() % 256;
		}
	}
}
//Distribute keys in subgroups for minimal perfect hash generation. Each subgroup
//can have at most ceil(256/c) keys. The reason for this is that we can have at most
//256 vertices in the graph we will generate in the next 
//step (number of vertices = ceil(nkeys * c))
static cmph_int64 assign_leaves(state_t *state)
{
	int c = 0;
	cmph_uint64 i = 0;

	state->leaf_size = (cmph_uint16 *)realloc(state->leaf_size, sizeof(cmph_uint16) * state->mph->k);
	memset(state->leaf_size, 0, state->mph->k * sizeof(cmph_uint16));

	DEBUGP("Assigning keys to %u leaves with at most %u keys each.\n", state->mph->k, state->max_leaf_size);
	c = ftruncate64(state->fd, sizeof(leaf_key_t) * ((cmph_uint64)state->source->nkeys));
	if (c == -1) return 0;

	for (i = 0; i < state->source->nkeys; ++i)
	{
		leaf_key_t leaf_key;
		cmph_uint32 keylen;
		char *key;
		state->source->read(state->source->data, &key, &keylen);
		leaf_key.h0 = HASHKEY(state->mph->hash[0], state->mph->h0_seed, key, keylen) % state->mph->k;
		leaf_key.h1 = HASHKEY(state->mph->hash[1], state->h1_seed[leaf_key.h0], key, keylen);
		leaf_key.h2 = HASHKEY(state->mph->hash[2], state->h2_seed[leaf_key.h0], key, keylen);
		if (state->leaf_size[leaf_key.h0] == state->max_leaf_size)
		{
			if (state->config->verbosity)
			{
				fprintf(stderr, "Too many collisions at leaf %u\n", leaf_key.h0);
			}
			return -1;
		}
		++(state->leaf_size[leaf_key.h0]);
		c = write(state->fd, &leaf_key, sizeof(leaf_key));
		if (c != sizeof(leaf_key))
		{
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
	char *fname = NULL;
	int tmpfd = hashtree_tmp_fd("hashtree_sorted.XXXXXX", &fname);;
	int c = 0;
	cmph_uint32 blocksize = floor(config->impl.hashtree.memory / ((double)sizeof(leaf_key_t)));
	cmph_uint32 nblocks = ceil(nkeys / (double)blocksize);
	cmph_uint64 sorted = 0;
	leaf_key_t *block = (leaf_key_t *)malloc(sizeof(leaf_key_t) * blocksize);
	cmph_uint32 *blockfill = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * nblocks);
	leaf_key_t *blockhead = (leaf_key_t *)malloc(sizeof(leaf_key_t) * nblocks);
	cmph_uint32 current_block = 0;
	if (!block || !blockfill || !blockhead || tmpfd == -1) return 0;
	c = lseek(fd, 0, SEEK_SET);
	if (c == -1)
	{
		if (config->verbosity)
		{
			fprintf(stderr, "Failed rewinding file\n");
		}
		free(blockfill);
		free(blockhead);
		free(block);
		return 0;
	}


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
					read(tmpfd, &(blockhead[current_block]), sizeof(blockhead[current_block]));
					--blockfill[current_block];	
				}
				if (min.h0 != UINT_MAX) 
				{
					write(fd, &min, sizeof(min));
					++sorted;
				}
			}
		}
	}
 	#ifdef WIN32
	remove(fname);
	#else
	unlink(fname);
	#endif
	free(fname);
}
static cmph_uint8 create_leaf_mph(state_t *state)
{
	cmph_uint32 i = 0;
	cmph_uint64 offset = 0;
	cmph_uint8 failure = 0;
	for (i = 0; i < state->mph->k; ++i)
	{
		if (state->mph->leaf[i]) 
		{
			offset += state->leaf_size[i];
			continue;
		}
		cmph_config_t *leaf_config = cmph_config_new(state->config->impl.hashtree.leaf_algo);
		cmph_config_set_graphsize(leaf_config, state->config->impl.hashtree.leaf_c);
		cmph_config_set_seed1(leaf_config, state->h1_seed[i]);
		cmph_config_set_seed2(leaf_config, state->h2_seed[i]);
		cmph_config_set_iterations(leaf_config, 1);

		cmph_io_adapter_t *leaf_source = hashtree_io_adapter(state->fd, offset, i,state->leaf_size[i],
				cmph_hashfuncs[state->config->impl.hashtree.hash[0]], state->h1_seed[i], 
				cmph_hashfuncs[state->config->impl.hashtree.hash[1]], state->h2_seed[i]);

		cmph_t *leaf_mph = cmph_new(leaf_config, leaf_source);

		hashtree_io_adapter_destroy(leaf_source);
		cmph_config_destroy(leaf_config);
		if (!leaf_mph) failure = 1;
		state->mph->leaf[i] = leaf_mph;

		offset += state->leaf_size[i];
	}
	return 0 == failure;
}
	
cmph_uint32 hashtree_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	hashtree_t *hashtree = (hashtree_t *)mphf->impl;
	cmph_uint32 h0 = HASHKEY(hashtree->hash[0], hashtree->h0_seed, key, keylen) % hashtree->k;
	return cmph_search(hashtree->leaf[h0], key, keylen) + hashtree->offset[h0];
}
void hashtree_destroy(cmph_t *mphf)
{
	hashtree_t *mph = (hashtree_t *)mphf->impl;
	cmph_uint32 i = 0;
	for (i = 0; i < mph->k; ++i)
	{
		cmph_destroy(mph->leaf[i]);
	}
	free(mph->offset);
	free(mph);
	free(mphf);
}
