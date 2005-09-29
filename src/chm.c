#include "graph.h"
#include "chm.h"
#include "chm_structs.h"
#include "cmph_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

static int chm_gen_edges(const cmph_config_t *config, cmph_io_adapter_t *key_source, chm_t *mph, graph_t *graph);
static void chm_traverse(cmph_uint32 *g, graph_t *graph, cmph_uint8 *visited, cmph_uint32 v);

#define HASHKEY(func, seed, key, keylen)\
	(*(cmph_hashfuncs[func]))(seed, key, keylen)

cmph_t *chm_new(const cmph_config_t *config, cmph_io_adapter_t *key_source)
{
	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	graph_t *graph = NULL;
	chm_t *mph = (chm_t *)malloc(sizeof(chm_t));
	cmph_t *ret = (cmph_t *)malloc(sizeof(cmph_t));
	if ((!mph) || (!ret)) return NULL;
	ret->algo = CMPH_CHM;
	ret->impl = mph;

	mph->m = key_source->nkeys;	
	mph->n = ceil(config->impl.chm.c * key_source->nkeys);	
	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", mph->m, mph->n, config->chm.c);
	graph = graph_new(mph->n, mph->m);
	DEBUGP("Created graph\n");

	//Mapping step
	if (config->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", mph->m, mph->n);
	}
	while(1)
	{
		int ok;
		mph->h1_seed = rand() % mph->n;
		mph->h2_seed = rand() % mph->m;
		ok = chm_gen_edges(config, key_source, mph, graph);
		if (!ok)
		{
			--iterations;
			DEBUGP("%u iterations remaining\n", iterations);
			if (config->verbosity)
			{
				fprintf(stderr, "Acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			if (iterations == 0) break;
		} 
		else break;	
	}
	if (iterations == 0)
	{
		graph_destroy(graph);	
		return NULL;
	}

	//Assignment step
	if (config->verbosity)
	{
		fprintf(stderr, "Starting assignment step\n");
	}
	DEBUGP("Assignment step\n");
 	visited = (char *)malloc(mph->n/8 + 1);
	memset(visited, 0, mph->n/8 + 1);
	free(mph->g);
	mph->g = malloc(mph->n * sizeof(cmph_uint32));
	assert(mph->g);
	for (i = 0; i < mph->n; ++i)
	{
	        if (!GETBIT(visited,i))
		{
			mph->g[i] = 0;
			chm_traverse(mph->g, graph, visited, i);
		}
	}
	graph_destroy(graph);	
	free(visited);

	DEBUGP("Successfully generated minimal perfect hash\n");
	if (config->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return ret;
}

static void chm_traverse(cmph_uint32 *g, graph_t *graph, cmph_uint8 *visited, cmph_uint32 v)
{

	graph_iterator_t it = graph_neighbors_it(graph, v);
	cmph_uint32 neighbor = 0;
	SETBIT(visited, v);
	
	DEBUGP("Visiting vertex %u\n", v);
	while((neighbor = graph_next_neighbor(graph, &it)) != GRAPH_NO_NEIGHBOR)
	{
		DEBUGP("Visiting neighbor %u\n", neighbor);
		if(GETBIT(visited,neighbor)) continue;
		DEBUGP("Visiting neighbor %u\n", neighbor);
		DEBUGP("Visiting edge %u->%u with id %u\n", v, neighbor, graph_edge_id(graph, v, neighbor));
		g[neighbor] = graph_edge_id(graph, v, neighbor) - g[v];
		chm_traverse(g, graph, visited, neighbor);
	}
}
		
static int chm_gen_edges(const cmph_config_t *config, cmph_io_adapter_t *key_source, chm_t *mph, graph_t *graph)
{
	cmph_uint32 e;
	int cycles = 0;

	graph_clear_edges(graph);	
	key_source->rewind(key_source->data);
	for (e = 0; e < key_source->nkeys; ++e)
	{
		cmph_uint32 h1, h2;
		cmph_uint32 keylen;
		char *key;
		key_source->read(key_source->data, &key, &keylen);
		h1 = HASHKEY(mph->hashfuncs[0], mph->h1_seed, key, keylen) % mph->n;
		h2 = HASHKEY(mph->hashfuncs[1], mph->h2_seed, key, keylen) % mph->n;
		if (h1 == h2) if (++h2 >= mph->n) h2 = 0;
		if (h1 == h2) 
		{
			if (config->verbosity) fprintf(stderr, "Self loop for key %u\n", e);
			key_source->dispose(key_source->data, key, keylen);
			return 0;
		}
		DEBUGP("Adding edge: %u -> %u for key %s\n", h1, h2, key);
		key_source->dispose(key_source->data, key, keylen);
		graph_add_edge(graph, h1, h2);
	}
	cycles = graph_is_cyclic(graph);
	if (config->verbosity && cycles) fprintf(stderr, "Cyclic graph generated\n");
	DEBUGP("Looking for cycles: %u\n", cycles);

	return ! cycles;
}

cmph_uint32 chm_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	chm_t *mph = (chm_t *)mphf->impl;
	cmph_uint32 h1, h2;
	h1 = HASHKEY(mph->hashfuncs[0], mph->h1_seed, key, keylen) % mph->n;
	h2 = HASHKEY(mph->hashfuncs[1], mph->h2_seed, key, keylen) % mph->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= mph->n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, mph->g[h1], mph->g[h2], mph->m);
	return (mph->g[h1] + mph->g[h2]) % mph->m;
}
void chm_destroy(cmph_t *mphf)
{
	chm_t *mph = (chm_t *)mphf->impl;
	free(mph->g);	
	free(mph);
	free(mphf);
}
