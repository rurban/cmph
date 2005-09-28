#include "graph.h"
#include "chm.h"
#include "chm_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

static int chm_gen_edges(const chm_config_t *config, cmph_io_adapter_t *key_source, chm_t *mph, graph_t *graph);
static void chm_traverse(uint32 *g, graph_t *graph, cmph_uint8 *visited, cmph_uint32 v);

cmph_t *chm_new(const chm_config_t *config, cmph_io_adapter_t *key_source)
{
	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	graph_t *graph = NULL;
	chm_t *mph = (chm_t *)malloc(sizeof(chm_t));
	if (!mph) return NULL;

	mph->m = key_source->nkeys;	
	mph->n = ceil(config->c * key_source->nkeys);	
	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", mph->m, mph->n, config->c);
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
	memset(visited, 0, chm->n/8 + 1);
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
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mph;
}

static void chm_traverse(uint32 *g, graph_t *graph, cmph_uint8 *visited, cmph_uint32 v)
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
		
static int chm_gen_edges(const chm_config_t *config, cmph_io_adapter_t *key_source, chm_t *mph, graph_t *graph)
{
	cmph_uint32 e;
	int cycles = 0;

	DEBUGP("Generating edges for %u vertices with hash functions %s and %s\n", mph->n, cmph_hash_names[config->hashfuncs[0]], cmph_hash_names[config->hashfuncs[1]]);
	graph_clear_edges(graph);	
	key_source->rewind(key_source->data);
	for (e = 0; e < key_source->nkeys; ++e)
	{
		cmph_uint32 h1, h2;
		cmph_uint32 keylen;
		char *key;
		key_source->read(key_source->data, &key, &keylen);
		h1 = hash(chm->hashes[0], key, keylen) % chm->n;
		h2 = hash(chm->hashes[1], key, keylen) % chm->n;
		if (h1 == h2) if (++h2 >= chm->n) h2 = 0;
		if (h1 == h2) 
		{
			if (mph->verbosity) fprintf(stderr, "Self loop for key %u\n", e);
			mph->key_source->dispose(mph->key_source->data, key, keylen);
			return 0;
		}
		DEBUGP("Adding edge: %u -> %u for key %s\n", h1, h2, key);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		graph_add_edge(chm->graph, h1, h2);
	}
	cycles = graph_is_cyclic(chm->graph);
	if (mph->verbosity && cycles) fprintf(stderr, "Cyclic graph generated\n");
	DEBUGP("Looking for cycles: %u\n", cycles);

	return ! cycles;
}

cmph_uint32 chm_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	chm_data_t *chm = mphf->data;
	cmph_uint32 h1 = hash(chm->hashes[0], key, keylen) % chm->n;
	cmph_uint32 h2 = hash(chm->hashes[1], key, keylen) % chm->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= chm->n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, chm->g[h1], chm->g[h2], chm->m);
	return (chm->g[h1] + chm->g[h2]) % chm->m;
}
void chm_destroy(cmph_t *mphf)
{
	chm_data_t *data = (chm_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hashes[0]);
	hash_state_destroy(data->hashes[1]);
	free(data->hashes);
	free(data);
	free(mphf);
}
