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
	assert(chm);
	memset(chm, 0, sizeof(chm_config_data_t));
	chm->hashfuncs[0] = CMPH_HASH_JENKINS;
	chm->g = NULL;
	chm->graph = NULL;
	chm->hashes = NULL;
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

cmph_t *chm_new(cmph_config_t *mph, double c)
{
	cmph_t *mphf = NULL;
	chm_data_t *chmf = NULL;

	cmph_uint32 i;
	cmph_uint32 w = 0;
	cmph_uint32 iterations = 20;
	cmph_uint8 *visited = NULL;
	chm_config_data_t *chm = (chm_config_data_t *)mph->data;
	chm->m = mph->key_source->nkeys;
	if (c == 0) c = 2.09;
	chm->n = (cmph_uint32)ceil(c * mph->key_source->nkeys);	

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
 	visited = (cmph_uint8 *)malloc((size_t)(chm->n/8 + 1));
	memset(visited, 0, (size_t)(chm->n/8 + 1));
	free(chm->g);
	chm->g = (cmph_uint32 *)malloc(chm->n * sizeof(cmph_uint32));
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
	chmf = (chm_data_t *)malloc(sizeof(chm_data_t));
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
	cmph_uint32 two = 2; //number of hash functions
	cmph_uint32 i;
	chm_data_t *data = (chm_data_t *)mphf->data;
	register size_t nbytes;
	
	__cmph_dump(mphf, fd);

	nbytes = fwrite(&two, sizeof(cmph_uint32), (size_t)1, fd);
	hash_state_dump(data->hashes[0], &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
	hash_state_dump(data->h0, &buf, &buflen);
	DEBUGP("Dumping hash state with %u bytes to disk\n", buflen);
	nbytes = fwrite(&buflen, sizeof(cmph_uint32), (size_t)1, fd);
	nbytes = fwrite(buf, (size_t)buflen, (size_t)1, fd);
	free(buf);

	fwrite(&(data->n), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->m), sizeof(cmph_uint32), 1, fd);
	fwrite(&(data->k), sizeof(cmph_uint32), 1, fd);
	nbytes = fwrite(data->g, sizeof(cmph_uint32)*data->n, (size_t)1, fd);
/*	#ifdef DEBUG
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
	fprintf(stderr, "G: ");
	for (i = 0; i < data->n; ++i) fprintf(stderr, "%u ", data->g[i]);
	fprintf(stderr, "\n");
	#endif*/
	return 1;
}

void chm_load(FILE *f, cmph_t *mphf)
{
	cmph_uint32 nhashes;
	char *buf = NULL;
	cmph_uint32 buflen;
	cmph_uint32 i;
	chm_data_t *chm = (chm_data_t *)malloc(sizeof(chm_data_t));
	register size_t nbytes;
	DEBUGP("Loading chm mphf\n");
	mphf->data = chm;
	fread(&chm->n, sizeof(cmph_uint32), 1, f);
	fread(&chm->m, sizeof(cmph_uint32), 1, f);
	fread(&chm->k, sizeof(cmph_uint32), 1, f);
	nbytes = fread(&nhashes, sizeof(cmph_uint32), (size_t)1, f);
	chm->hashes = (hash_state_t **)malloc(sizeof(hash_state_t *)*(nhashes + 1));
	chm->hashes[nhashes] = NULL;
	DEBUGP("Reading %u hashes\n", nhashes);
	for (i = 0; i < nhashes; ++i)
	{
		hash_state_t *state = NULL;
		nbytes = fread(&buflen, sizeof(cmph_uint32), (size_t)1, f);
		DEBUGP("Hash state has %u bytes\n", buflen);
		buf = (char *)malloc((size_t)buflen);
		nbytes = fread(buf, (size_t)buflen, (size_t)1, f);
		state = hash_state_load(buf, buflen);
		chm->hashes[i] = state;
		free(buf);
		jenkins_state_destroy(state);
	}

	chm->g = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*chm->n);
	nbytes = fread(chm->g, chm->n*sizeof(cmph_uint32), (size_t)1, f);
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

/** \fn void chm_pack(cmph_t *mphf, void *packed_mphf);
 *  \brief Support the ability to pack a perfect hash function into a preallocated contiguous memory space pointed by packed_mphf.
 *  \param mphf pointer to the resulting mphf
 *  \param packed_mphf pointer to the contiguous memory area used to store the resulting mphf. The size of packed_mphf must be at least cmph_packed_size() 
 */
void chm_pack(cmph_t *mphf, void *packed_mphf)
{
	chm_data_t *data = (chm_data_t *)mphf->data;
	cmph_uint8 * ptr = packed_mphf;

	// packing h1 type
	CMPH_HASH h1_type = hash_get_type(data->hashes[0]);
	*((cmph_uint32 *) ptr) = h1_type;
	ptr += sizeof(cmph_uint32);

	// packing h1
	hash_state_pack(data->hashes[0], ptr);
	ptr += hash_state_packed_size(h1_type);

	// packing h2 type
	CMPH_HASH h2_type = hash_get_type(data->hashes[1]);
	*((cmph_uint32 *) ptr) = h2_type;
	ptr += sizeof(cmph_uint32);

	// packing h2
	hash_state_pack(data->hashes[1], ptr);
	ptr += hash_state_packed_size(h2_type);

	// packing n
	*((cmph_uint32 *) ptr) = data->n;
	ptr += sizeof(data->n);

	// packing m
	*((cmph_uint32 *) ptr) = data->m;
	ptr += sizeof(data->m);

	// packing g
	memcpy(ptr, data->g, sizeof(cmph_uint32)*data->n);	
}

/** \fn cmph_uint32 chm_packed_size(cmph_t *mphf);
 *  \brief Return the amount of space needed to pack mphf.
 *  \param mphf pointer to a mphf
 *  \return the size of the packed function or zero for failures
 */ 
cmph_uint32 chm_packed_size(cmph_t *mphf)
{
	chm_data_t *data = (chm_data_t *)mphf->data;
	CMPH_HASH h1_type = hash_get_type(data->hashes[0]); 
	CMPH_HASH h2_type = hash_get_type(data->hashes[1]); 

	return (cmph_uint32)(sizeof(CMPH_ALGO) + hash_state_packed_size(h1_type) + hash_state_packed_size(h2_type) + 
			4*sizeof(cmph_uint32) + sizeof(cmph_uint32)*data->n);
}

/** cmph_uint32 chm_search(void *packed_mphf, const char *key, cmph_uint32 keylen);
 *  \brief Use the packed mphf to do a search. 
 *  \param  packed_mphf pointer to the packed mphf
 *  \param key key to be hashed
 *  \param keylen key legth in bytes
 *  \return The mphf value
 */
cmph_uint32 chm_search_packed(void *packed_mphf, const char *key, cmph_uint32 keylen)
{
	register cmph_uint8 *h1_ptr = packed_mphf;
	register CMPH_HASH h1_type  = *((cmph_uint32 *)h1_ptr);
	h1_ptr += 4;

	register cmph_uint8 *h2_ptr = h1_ptr + hash_state_packed_size(h1_type);
	register CMPH_HASH h2_type  = *((cmph_uint32 *)h2_ptr);
	h2_ptr += 4;
	
	register cmph_uint32 *g_ptr = (cmph_uint32 *)(h2_ptr + hash_state_packed_size(h2_type));
	
	register cmph_uint32 n = *g_ptr++;  
	register cmph_uint32 m = *g_ptr++;  
	
	register cmph_uint32 h1 = hash_packed(h1_ptr, h1_type, key, keylen) % n; 
	register cmph_uint32 h2 = hash_packed(h2_ptr, h2_type, key, keylen) % n; 
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 >= n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, g_ptr[h1], g_ptr[h2], m);
	return (g_ptr[h1] + g_ptr[h2]) % m;	
}
