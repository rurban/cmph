#include "xgraph.h"
#include "xchmr.h"
#include "cmph_structs.h"
#include "xchmr_structs.h"
#include "hash.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

static int xchmr_gen_edges(cmph_config_t *mph);
static void xchmr_traverse(xchmr_config_data_t *xchmr, cmph_uint8 *visited);

xchmr_config_data_t *xchmr_config_new()
{
	xchmr_config_data_t *xchmr = NULL; 	
	xchmr = (xchmr_config_data_t *)malloc(sizeof(xchmr_config_data_t));
	xchmr->hashfuncs[0] = CMPH_HASH_JENKINS;
	xchmr->hashfuncs[1] = CMPH_HASH_JENKINS;
	xchmr->g = NULL;
	xchmr->graph = NULL;
	xchmr->hashes = NULL;
	assert(xchmr);
	return xchmr;
}
void xchmr_config_destroy(cmph_config_t *mph)
{
	xchmr_config_data_t *data = (xchmr_config_data_t *)mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void xchmr_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	xchmr_config_data_t *xchmr = (xchmr_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 2) break; //xchmr only uses two hash functions
		xchmr->hashfuncs[i] = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_t *xchmr_new(cmph_config_t *mph, float c)
{
	cmph_t *mphf = NULL;
	xchmr_data_t *xchmrf = NULL;

	cmph_uint32 i;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	xchmr_config_data_t *xchmr = (xchmr_config_data_t *)mph->data;
	xchmr->m = mph->key_source->nkeys;	
	xchmr->n = ceil(c * mph->key_source->nkeys);	
	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", xchmr->m, xchmr->n, c);
	DEBUGP("Created graph\n");

	xchmr->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*3);
	for(i = 0; i < 3; ++i) xchmr->hashes[i] = NULL;
	//Mapping step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", xchmr->m, xchmr->n);
	}
	while(1)
	{
		int ok;
		cmph_uint32 memory = xchmr->n * 2 * sizeof(cmph_uint32); //use memory equal the size of the hash
		xchmr->hashes[0] = hash_state_new(xchmr->hashfuncs[0], xchmr->n);
		xchmr->hashes[1] = hash_state_new(xchmr->hashfuncs[1], xchmr->n);
		xchmr->graph = xgraph_new(xchmr->n, memory);
		ok = xchmr_gen_edges(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(xchmr->hashes[0]);
			xchmr->hashes[0] = NULL;
			hash_state_destroy(xchmr->hashes[1]);
			xchmr->hashes[1] = NULL;
			DEBUGP("%u iterations remaining\n", iterations);
			if (mph->verbosity)
			{
				fprintf(stderr, "Acyclic graph creation failure - %u iterations remaining\n", iterations);
			}
			xchmr->graph = xgraph_new(xchmr->n, memory);
			if (iterations == 0) break;
		} 
		else break;	
	}
	if (iterations == 0)
	{
		xgraph_destroy(xchmr->graph);	
		return NULL;
	}

	//Assignment step
	if (mph->verbosity)
	{
		fprintf(stderr, "Starting assignment step\n");
	}
	DEBUGP("Assignment step\n");
 	visited = (char *)malloc(xchmr->n/8 + 1);
	memset(visited, 0, xchmr->n/8 + 1);
	free(xchmr->g);
	xchmr->g = malloc(xchmr->n * sizeof(cmph_uint32));
	memset(xchmr->g, 0, xchmr->n * sizeof(cmph_uint32));
	assert(xchmr->g);
	xchmr_traverse(xchmr, visited);
	xgraph_destroy(xchmr->graph);	
	free(visited);
	xchmr->graph = NULL;

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	xchmrf = (xchmr_data_t *)malloc(sizeof(xchmr_config_data_t));
	xchmrf->g = xchmr->g;
	xchmr->g = NULL; //transfer memory ownership
	xchmrf->hashes = xchmr->hashes;
	xchmr->hashes = NULL; //transfer memory ownership
	xchmrf->n = xchmr->n;
	xchmrf->m = xchmr->m;
	mphf->data = xchmrf;
	mphf->size = xchmr->m;
	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

static void xchmr_traverse(xchmr_config_data_t *xchmr, cmph_uint8 *visited)
{

	assert(xchmr->graph);
	xgraph_pbfs_reset(xchmr->graph);
	cmph_uint32 i = 0, j = 0;
	for (i = 0; i < xchmr->n; ++i)
	{
		xgraph_adj_t list = xgraph_pbfs_next(xchmr->graph);
		SETBIT(visited, list.v);
		DEBUGP("Walking adjacency list for vertex %u\n", list.v);
		for (j = 0; j < list.degree; ++j)
		{
			if (GETBIT(visited, list.adj[j])) continue;
			xchmr->g[list.adj[j]] = list.ids[j] - xchmr->g[list.v];
			SETBIT(visited, list.adj[j]);
		}
	}
}
		
static int xchmr_gen_edges(cmph_config_t *mph)
{
	cmph_uint32 e;
	xchmr_config_data_t *xchmr = (xchmr_config_data_t *)mph->data;
	int cycles = 0;

	DEBUGP("Generating edges for %u vertices with hash functions %s and %s\n", xchmr->n, cmph_hash_names[xchmr->hashfuncs[0]], cmph_hash_names[xchmr->hashfuncs[1]]);
	mph->key_source->rewind(mph->key_source->data);
	for (e = 0; e < mph->key_source->nkeys; ++e)
	{
		cmph_uint32 h1, h2;
		cmph_uint32 keylen;
		char *key;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h1 = hash(xchmr->hashes[0], key, keylen) % xchmr->n;
		h2 = hash(xchmr->hashes[1], key, keylen) % xchmr->n;
		if (h1 == h2) if (++h2 >= xchmr->n) h2 = 0;
		if (h1 == h2) 
		{
			if (mph->verbosity) fprintf(stderr, "Self loop for key %e\n", e);
			mph->key_source->dispose(mph->key_source->data, key, keylen);
			return 0;
		}
		DEBUGP("Adding edge: %u -> %u for key %s\n", h1, h2, key);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		xgraph_add_edge(xchmr->graph, h1, h2);
	}
	cycles = xgraph_pack(xchmr->graph);
	if (mph->verbosity && cycles) fprintf(stderr, "Cyclic graph generated\n");
	DEBUGP("Looking for cycles: %u\n", cycles);

	return ! cycles;
}

int xchmr_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	cmph_uint32 two = 2; //number of hash functions
	xchmr_data_t *data = (xchmr_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	fwrite(&two, sizeof(cmph_uint32), 1, fd);
	hash_state_dump(data->hashes[0], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	hash_state_dump(data->hashes[1], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	
	fwrite(data->g, sizeof(cmph_uint32)*data->n, 1, fd);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void xchmr_load(FILE *f, cmph_t *mphf)
{
	cmph_uint32 nhashes;
	char fbuf[BUFSIZ];
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	hash_state_t *state;
	xchmr_data_t *xchmr = (xchmr_data_t *)malloc(sizeof(xchmr_data_t));

	DEBUGP("Loading xchmr mphf\n");
	mphf->data = xchmr;
	fread(&nhashes, sizeof(cmph_uint32), 1, f);
	xchmr->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*(nhashes + 1));
	xchmr->hashes[nhashes] = NULL;
	DEBUGP("Reading %u hashes\n", nhashes);
	for (i = 0; i < nhashes; ++i)
	{
		hash_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = hash_state_load(buf, buflen);
		xchmr->hashes[i] = state;
		free(buf);
	}

	DEBUGP("Reading m and n\n");
	fread(&(xchmr->n), sizeof(cmph_uint32), 1, f);	
	fread(&(xchmr->m), sizeof(cmph_uint32), 1, f);	

	xchmr->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*xchmr->n);
	fread(xchmr->g, xchmr->n*sizeof(cmph_uint32), 1, f);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < xchmr->n; ++i) fprintf(stderr, "%u ", xchmr->g[i]);
	fprintf(stderr, "\n");
	#endif
	return;
}
		

cmph_uint32 xchmr_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	xchmr_data_t *xchmr = mphf->data;
	cmph_uint32 h1 = hash(xchmr->hashes[0], key, keylen) % xchmr->n;
	cmph_uint32 h2 = hash(xchmr->hashes[1], key, keylen) % xchmr->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= xchmr->n) h2 = 0;
	cmph_uint32 id = (xchmr->g[h1] + xchmr->g[h2]) % xchmr->m;
	DEBUGP("id: %u g[h1]: %u g[h2]: %u edges: %u\n", id, xchmr->g[h1], xchmr->g[h2], xchmr->m);
	return id;
}
void xchmr_destroy(cmph_t *mphf)
{
	xchmr_data_t *data = (xchmr_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->hashes[0]);
	hash_state_destroy(data->hashes[1]);
	free(data->hashes);
	free(data);
	free(mphf);
}
