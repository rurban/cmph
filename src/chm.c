#include "graph.h"
#include "chm.h"
#include "cmph_structs.h"
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

static int chm_gen_edges(cmph_config_t *mph);
static void chm_traverse(chm_config_data_t *chm, cmph_uint8 *visited, cmph_uint32 v);

chm_config_data_t *chm_config_new()
{
	chm_config_data_t *chm = NULL; 	
	chm = (chm_config_data_t *)malloc(sizeof(chm_config_data_t));
	chm->hashfuncs[0] = CMPH_HASH_JENKINS;
	chm->g = NULL;
	chm->graph = NULL;
	chm->h0 = NULL;
	chm->h1 = NULL;
	chm->h2 = NULL;
	chm->k = 1;
	assert(chm);
	return chm;
}
void chm_config_destroy(cmph_config_t *mph)
{
	chm_config_data_t *data = (chm_config_data_t *)mph->data;
	DEBUGP("Destroying algorithm dependent data\n");
	free(data);
}

void chm_config_set_ncomponents(cmph_config_t *mph, cmph_uint32 K)
{
	DEBUGP("Setting number of components to %u\n", K);
	chm_config_data_t *chm = (chm_config_data_t *)mph->data;
	chm->k = K;
}
void chm_config_set_hashfuncs(cmph_config_t *mph, CMPH_HASH *hashfuncs)
{
	chm_config_data_t *chm = (chm_config_data_t *)mph->data;
	CMPH_HASH *hashptr = hashfuncs;
	cmph_uint32 i = 0;
	while(*hashptr != CMPH_HASH_COUNT)
	{
		if (i >= 1) break; //chmk only use 1 user-defined hash function
		chm->hashfuncs[i] = *hashptr;	
		++i, ++hashptr;
	}
}

cmph_t *chm_new(cmph_config_t *mph, float c)
{
	cmph_t *mphf = NULL;
	chm_data_t *chmf = NULL;

	cmph_uint32 i;
	cmph_uint32 w = 0;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	chm_config_data_t *chm = (chm_config_data_t *)mph->data;
	chm->m = mph->key_source->nkeys;	
	chm->n = ceil(c * mph->key_source->nkeys);	

	//We need at least one edge per component
	if (chm->k > chm->n/2) 
	{
		DEBUGP("Reducing k from %u to %u\n", chm->k, chm->n/2);
		chm->k = chm->n/2;
	}

	//Find a suitable window value
	w = chm->n / chm->k + 1;
	//Adjust the size of n
	chm->n = w * chm->k;

	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", chm->m, chm->n, c);
	chm->graph = (graph_t **)malloc(sizeof(graph_t *)*chm->k);
	//The number of edges for each is not know in advance, so let
	//us be gentle and waste some edges
	for (i = 0; i < chm->k; ++i) chm->graph[i] = graph_new(w, w, 1);
	DEBUGP("Created %u graphs with %u vertices and %u edges\n", chm->k, w, w);

	chm->h0 = NULL;
	chm->h1 = (jenkins_state_t *)malloc(sizeof(jenkins_state_t)*chm->k);
	chm->h2 = (jenkins_state_t *)malloc(sizeof(jenkins_state_t)*chm->k);

	//Mapping step
	if (mph->verbosity)
	{
		fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", chm->m, chm->n);
	}
	while(1)
	{
		int ok;
		chm->h0 = hash_state_new(chm->hashfuncs[0], chm->k);
		for(i = 0; i < chm->k; ++i) 
		{
			jenkins_state_t j1, j2;
			do
			{
				j1.seed = rand() % w;
				j2.seed = rand() % w;
			} while (j1.seed == j2.seed);
			chm->h1[i] = j1;
			chm->h2[i] = j2;
			DEBUGP("Seeded h1[%u] with %u\n", i, chm->h1[i].seed);
		}


		ok = chm_gen_edges(mph);
		if (!ok)
		{
			--iterations;
			hash_state_destroy(chm->h0);
			chm->h0 = NULL;
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
		for (i = 0; i < chm->k; ++i) graph_destroy(chm->graph[i]);	
		free(chm->graph);
		chm->graph = NULL;
		free(chm->h1);
		chm->h1 = NULL;
		free(chm->h2);
		chm->h2 = NULL;
		return NULL;
	}

	//Assignment step
	if (mph->verbosity)
	{
		fprintf(stderr, "Starting assignment step\n");
	}
	DEBUGP("Assignment step\n");
 	visited = (char *)malloc(chm->n/8 + 1);
	memset(visited, 0, chm->n/8 + 1);
	free(chm->g);
	chm->g = malloc(chm->n * sizeof(cmph_uint32));
	memset(chm->g, 0, sizeof(cmph_uint32)*chm->n);
	assert(chm->g);
	for (i = 0; i < chm->n; ++i)
	{
	    if (!GETBIT(visited,i))
		{
			chm_traverse(chm, visited, i);
		}
	}
	for (i = 0; i < chm->k; ++i) graph_destroy(chm->graph[i]);	
	free(visited);
	free(chm->graph);
	chm->graph = NULL;

	mphf = (cmph_t *)malloc(sizeof(cmph_t));
	mphf->algo = mph->algo;
	chmf = (chm_data_t *)malloc(sizeof(chm_config_data_t));
	chmf->g = chm->g;
	chm->g = NULL; //transfer memory ownership
	chmf->h0 = chm->h0;
	chm->h0 = NULL;
	chmf->h1 = chm->h1;
	chm->h1 = NULL;
	chmf->h2 = chm->h2;
	chm->h2 = NULL;
	chmf->n = chm->n;
	chmf->m = chm->m;
	chmf->k = chm->k;
	mphf->data = chmf;
	mphf->size = chm->m;
	DEBUGP("Successfully generated minimal perfect hash\n");
	if (mph->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return mphf;
}

static void chm_traverse(chm_config_data_t *chm, cmph_uint8 *visited, cmph_uint32 v)
{

	cmph_uint32 neighbor = 0;
	cmph_uint32 w = chm->n / chm->k;
	cmph_uint32 c = v / w; //component of the graph
	cmph_uint32 local_v = v - c * w;
	DEBUGP("Getting neighbors for %u (local %u) at graph %u\n", v, local_v, c);
	graph_iterator_t it = graph_neighbors_it(chm->graph[c], local_v);

	SETBIT(visited, v);
	DEBUGP("Visiting vertex %u\n", v);
	while((neighbor = graph_next_neighbor(chm->graph[c], &it)) != GRAPH_NO_NEIGHBOR)
	{
		//Neighbor is already local
		cmph_uint32 global_neighbor = neighbor + c * w;
		DEBUGP("Visiting neighbor %u\n", global_neighbor);
		if(GETBIT(visited, global_neighbor)) continue;
		DEBUGP("Visiting neighbor %u\n", global_neighbor);
		DEBUGP("Visiting edge %u->%u at graph %u with id %u\n", local_v, neighbor, c, graph_edge_id(chm->graph[c], local_v, neighbor));
		chm->g[global_neighbor] = graph_edge_id(chm->graph[c], local_v, neighbor) - chm->g[v];
		DEBUGP("g[%u] is %u (%u - %u mod %u)\n", global_neighbor, chm->g[global_neighbor], graph_edge_id(chm->graph[c], local_v, neighbor), chm->g[v], chm->m);
		DEBUGP("Calling recursion for %u\n", global_neighbor);
		chm_traverse(chm, visited, global_neighbor);
	}
}
		
static int chm_gen_edges(cmph_config_t *mph)
{
	cmph_uint32 i = 0;
	cmph_uint32 e = 0;
	chm_config_data_t *chm = (chm_config_data_t *)mph->data;
	int cycles = 0;

	DEBUGP("Generating edges for %u vertices with hash function %s\n", chm->n, cmph_hash_names[chm->hashfuncs[0]]);
	for (i = 0; i < chm->k; ++i)
	{
		graph_clear_edges(chm->graph[i]);	
	}
	cmph_uint32 w = chm->n/chm->k;
	mph->key_source->rewind(mph->key_source->data);
	for (e = 0; e < mph->key_source->nkeys; ++e)
	{
		cmph_uint32 h0, h1, h2;
		cmph_uint32 keylen;
		char *key;
		mph->key_source->read(mph->key_source->data, &key, &keylen);
		h0 = hash(chm->h0, key, keylen) % chm->k;
		DEBUGP("Window is %u and offset is %u\n", w, w * h0);
		h1 = jenkins_hash(&(chm->h1[h0]), key, keylen) % w;
		h2 = jenkins_hash(&(chm->h2[h0]), key, keylen) % w;
		if (h1 == h2) if (++h2 >= w) h2 = 0;
		if (h1 == h2) 
		{
			if (mph->verbosity) fprintf(stderr, "Self loop for key %u\n", e);
			mph->key_source->dispose(mph->key_source->data, key, keylen);
			return 0;
		}
		DEBUGP("Adding edge: %u -> %u id %u for key %s at graph %u\n", h1, h2, e, key, h0);
		mph->key_source->dispose(mph->key_source->data, key, keylen);
		graph_add_edge_with_id(chm->graph[h0], h1, h2, e);
	}
	cycles = 0;
	for (i = 0; i < chm->k; ++i)
	{
		cycles = graph_is_cyclic(chm->graph[i]);
		DEBUGP("Looking for cycles at graph %u: %u\n", i, cycles);
		if (mph->verbosity && cycles) fprintf(stderr, "Cyclic graph %u generated\n", i);
		if (cycles) break;
	}
	return ! cycles;
}

int chm_dump(cmph_t *mphf, FILE *fd)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	chm_data_t *data = (chm_data_t *)mphf->data;
	__cmph_dump(mphf, fd);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->k), sizeof(cmph_uint32), 1, fd);

	hash_state_dump(data->h0, &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
	fwrite(buf, buflen, 1, fd);
	free(buf);

	for (i = 0; i < data->k; ++i)
	{
		assert(data->h1);
		jenkins_state_dump((jenkins_state_t *)&(data->h1[i]), &buf, &buflen);
		DEBUGP("Dumping jenkins state with %u bytes to disk\n", buflen);
		fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
		fwrite(buf, buflen, 1, fd);
		free(buf);
	}
	for (i = 0; i < data->k; ++i)
	{
		jenkins_state_dump((jenkins_state_t *)&(data->h2[i]), &buf, &buflen);
		DEBUGP("Dumping jenkins state with %u bytes to disk\n", buflen);
		fwrite(&buflen, sizeof(cmph_uint32), 1, fd);
		fwrite(buf, buflen, 1, fd);
		free(buf);
	}

	fwrite(data->g, sizeof(cmph_uint32)*data->n, 1, fd);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
	#endif
	return 1;
}

void chm_load(FILE *f, cmph_t *mphf)
{
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	chm_data_t *chm = (chm_data_t *)malloc(sizeof(chm_data_t));

	DEBUGP("Loading chm mphf\n");
	mphf->data = chm;
	fread(&chm->n, sizeof(cmph_uint32), 1, f);
	fread(&chm->m, sizeof(cmph_uint32), 1, f);
	fread(&chm->k, sizeof(cmph_uint32), 1, f);

	DEBUGP("Reading hash h0\n");
	fread(&buflen, sizeof(cmph_uint32), 1, f);
	buf = (char *)malloc(buflen);
	fread(buf, buflen, 1, f);
	chm->h0 = hash_state_load(buf, buflen);
	free(buf);

	DEBUGP("Reading %u h1 hashes\n", chm->k);
	chm->h1 = (jenkins_state_t *)malloc(sizeof(jenkins_state_t)*chm->k);
	for (i = 0; i < chm->k; ++i)
	{
		jenkins_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = jenkins_state_load(buf, buflen);
		chm->h1[i] = *state;
		free(buf);
		jenkins_state_destroy(state);
	}

	DEBUGP("Reading %u h2 hashes\n", chm->k);
	chm->h2 = (jenkins_state_t *)malloc(sizeof(jenkins_state_t)*chm->k);
	for (i = 0; i < chm->k; ++i)
	{
		jenkins_state_t *state = NULL;
		fread(&buflen, sizeof(cmph_uint32), 1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc(buflen);
		fread(buf, buflen, 1, f);
		state = jenkins_state_load(buf, buflen);
		chm->h2[i] = *state;
		free(buf);
		jenkins_state_destroy(state);
	}

	chm->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*chm->n);
	fread(chm->g, chm->n*sizeof(cmph_uint32), 1, f);
	#ifdef DEBUG
	fprintf(stderr, "G: ");
	for (i = 0; i < chm->n; ++i) fprintf(stderr, "%u ", chm->g[i]);
	fprintf(stderr, "\n");
	#endif
	return;
}
		

cmph_uint32 chm_search(cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	chm_data_t *chm = mphf->data;
	cmph_uint32 w = chm->n / chm->k;
	cmph_uint32 h0 = hash(chm->h0, key, keylen) % chm->k;
	cmph_uint32 h1 = (jenkins_hash(&(chm->h1[h0]), key, keylen) % w) + w * h0;
	cmph_uint32 h2 = (jenkins_hash(&(chm->h2[h0]), key, keylen) % w) + w * h0;
	DEBUGP("key: %s h1: %u h2: %u at graph %u\n", key, h1, h2, h0);
	if (h1 == h2 && ++h2 >= w * (h0 + 1)) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, chm->g[h1], chm->g[h2], chm->m);
	return (chm->g[h1] + chm->g[h2]) % chm->m;
}
void chm_destroy(cmph_t *mphf)
{
	chm_data_t *data = (chm_data_t *)mphf->data;
	free(data->g);	
	hash_state_destroy(data->h0);
	free(data->h1);
	free(data->h2);
	free(data);
	free(mphf);
}
