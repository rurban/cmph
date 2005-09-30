#include "hashtree.h"
#include "hashtree_io_adapter.h"
#include "graph.h"
#include "cmph_structs.h"
#include "hashtree_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

static cmph_int64 assign_leaves(const cmph_config_t *config, hashtree_t *mph, FILE *fd);
static cmph_uint8 sort_keys(FILE *fd, cmph_uint32 nkeys);

cmph_t *hashtree_new(cmph_config_t *mph)
{
	cmph_t *ret = (cmph_t *)malloc(sizeof(cmph_t));
	hashtree_t *mph = (hashtree_t *)malloc(hashtree_t);
	if ((!ret) || (!mph)) return NULL;

	//First, divide keys by leaves

	//Mapping step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", hashtree->m, hashtree->n);
	}
	while(1)
	{
		int ok;
		hashtree->hashes[0] = hash_state_new(hashtree->hashfuncs[0], hashtree->n);
		hashtree->hashes[1] = hash_state_new(hashtree->hashfuncs[1], hashtree->n);
		ok = hashtree_gen_edges(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(hashtree->hashes[0]);
			hashtree->hashes[0] = NULL;
			hash_state_destroy(hashtree->hashes[1]);
			hashtree->hashes[1] = NULL;
			DEBUGP("%u iterations remaining\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "Acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;	
	}
	if (iterations == 0)
	{
		graph_destroy(hashtree->graph);	
		return NULL;
	}

	//Assignment step
	if (mph->verbosity)
	{
		fprintf(stderr, "Starting assignment step\n");
	}
	DEBUGP("Assignment step\n");
 	visited = (char *)malloc(hashtree->n/8 + 1);
	memset(visited, 0, hashtree->n/8 + 1);
	free(hashtree->g);
	hashtree->g = malloc(hashtree->n * sizeof(cmph_uint32));
	assert(hashtree->g);
	for (i = 0; i < hashtree->n; ++i)
	{
	        if (!GETBIT(visited,i))
		{
			hashtree->g[i] = 0;
			hashtree_traverse(hashtree, visited, i);
		}
	}
	graph_destroy(hashtree->graph);	
	free(visited);
	hashtree->graph = NULL;

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	hashtreef = (hashtree_data_t *)malloc(sizeof(hashtree_data_t));
	hashtreef->g = hashtree->g;
	hashtree->g = NULL; //transfer memory ownership
	hashtreef->hashes = hashtree->hashes;
	hashtree->hashes = NULL; //transfer memory ownership
	hashtreef->n = hashtree->n;
	hashtreef->m = hashtree->m;
	mphf->data = hashtreef;
	mphf->size = hashtree->m;
	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

//Distribute keys in subgroups for minimal perfect hash generation. Each subgroup
//can have at most ceil(256/c) keys. The reason for this is that we can have at most
//256 vertices in the graph we will generate in the next 
//step (number of vertices = ceil(nkeys * c))
static cmph_int64 assign_leaves(const cmph_config_t *config, hashtree_t *mph, int fd)
{
	int c = 0;
	cmph_uint64 i = 0;
	cmph_uint32 maxkeys = ceil(256 / config->leaf_c);
	mph->k = ceil(maxkeys * config->root_c);

	cmph_uint16 *size = (cmph_uint16 *)malloc(sizeof(cmph_uint16) * mph->k);
	memset(size, 0, mph->k * sizeof(cmph_uint16));

	DEBUGP("Assigning keys to %u leaves with at most %u keys each.\n", mph->k, maxkeys);
	c = ftruncate64(fd, sizeof(leaf_key_t) * ((cmph_uint64_t)config->key_source->nkeys));
	if (c == -1) return 0;

	for (i = 0; i < config->key_source->nkeys; ++i)
	{
		leaf_key_t leaf_key;
		cmph_uint32 keylen;
		char *key;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		leaf_key.h0 = HASHKEY(mph->hash[0], mph->h0_seed, key, keylen) % mph->k;
		leaf_key.h1 = HASHKEY(mph->hash[1], mph->h1_seed[leaf_key.h0], key, keylen);
		leaf_key.h2 = HASHKEY(mph->hash[2], mph->h2_seed[leaf_key.h0], key, keylen);
		if (size[leaf_key.h0] == maxkeys)
		{
			free(size);
			if (config->verbosity)
			{
				fprintf("Too many collisions at leaf %u\n", leaf_key.h0);
			}
			return -;
		}
		++(size[leaf_key.h0]);
		c = fwrite(&leaf_key, 1, sizeof(leaf_key), fd);
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
static cmph_uint8 sort_keys(const cmph_config_t *config, FILE *fd, cmph_uint64 nkeys)
{
	FILE *tmpfd;
	int c = 0;
	cmph_uint32 blocksize = floor(config->memory / ((double)sizeof(leaf_key_t)));
	cmph_uint32 nblocks = ceil(nkeys / (double)blocksize);
	cmph_uint64 sorted = 0;
	leaf_key_t *block = (leaf_key_t *)malloc(sizeof(leaf_key_t) * blocksize);
	cmph_uint32 *blockfill = (cmph_uint32 *)malloc(sizeof(cmph_uint32) * nblocks);
	cmph_uint32 *blockhead = (cmph_uint32 *)malloc(sizeof(leaf_key_t) * nblocks);
	cmph_uint32 current_block = 0;
	if (!block || !blockfill || !blockhead) return 0;
	rewind(fd);

	//Sort blocks
	while (1)
	{
		blockfill[current_block] = 0;
		c = fread(block, sizeof(leaf_key_t), blocksize, fd);
		if (c != blocksize && c != nkeys - sorted;) 
		{
			free(block);
			free(blockfill);
			return 0;
		}
		blockfill[current_block] = c;
		qsort(block, blockfill[current_block], sizeof(leaf_key_t), leaf_key_compare);
		c = fwrite(block, sizeof(leaf_key_t), blockfill[current_block], tmpfd);
		free(block);
		if (c != blockfill[current_block]) 
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
	unlink(tmpfd);
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
