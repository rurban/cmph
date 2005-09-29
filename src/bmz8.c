#include "graph.h"
#include "bmz8.h"
#include "cmph_structs.h"
#include "bmz8_structs.h"
#include "hash.h"
#include "vqueue.h"
#include "bitbool.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define DEBUG
#include "debug.h"

#define HASHKEY(func, seed, key, keylen)\
	(*(cmph_hashfuncs[func]))(seed, key, keylen)

typedef struct
{
	const cmph_config_t *config;
	bmz8_t *mph;
	cmph_io_adapter_t *key_source;
	cmph_uint8 biggest_g_value;
	cmph_uint8 biggest_edge_value;
	cmph_uint8 *used_edges;
	cmph_uint8 *visited;
	graph_t *graph;
} state_t;

static int bmz8_gen_edges(state_t *state);
static cmph_uint8 bmz8_traverse_critical_nodes(state_t *state, cmph_uint8 v);
static cmph_uint8 bmz8_traverse_critical_nodes(state_t *state, cmph_uint8 v);
static cmph_uint8 bmz8_traverse_critical_nodes_heuristic(state_t *state, cmph_uint8 v);
static void bmz8_traverse_non_critical_nodes(state_t *state);

cmph_t *bmz8_new(const cmph_config_t *config, cmph_io_adapter_t *key_source)
{
	cmph_t *ret = (cmph_t *)malloc(sizeof(cmph_t));
	bmz8_t *mph = (bmz8_t *)malloc(sizeof(bmz8_t));
	state_t *state = (state_t *)malloc(sizeof(state_t));
	cmph_uint8 i;
	cmph_uint8 iterations;
	cmph_uint8 iterations_map = 20;
	cmph_uint8 restart_mapping = 0;
	if ((!ret) || (!mph) || (!state))
	{
		free(ret);
		free(mph);
		free(state);
		return NULL;
	}
	memset(state, 0, sizeof(state_t));	
	memset(mph, 0, sizeof(bmz8_t));
	ret->impl = mph;
	ret->algo = CMPH_BMZ8;
	mph->hash[0] = config->impl.bmz8.hashfuncs[0];
	mph->hash[1] = config->impl.bmz8.hashfuncs[1];
	state->config = config;
	state->key_source = key_source;
	state->mph = mph;

	if (key_source->nkeys >= 256)
	{
		if (config->verbosity) fprintf(stderr, "The number of keys in BMZ8 must be lower than 256.\n");
		return NULL;
	}

	DEBUGP("c: %f\n", config->impl.bmz8.c);
	mph->m = key_source->nkeys;	
	mph->n = ceil(config->impl.bmz8.c * key_source->nkeys);	
	DEBUGP("m (edges): %u n (vertices): %u c: %f\n", mph->m, mph->n, config->impl.bmz8.c);
	state->graph = graph_new(mph->n, mph->m);
	DEBUGP("Created graph\n");

	do
	{
		// Mapping step
		state->biggest_g_value = 0;
		state->biggest_edge_value = 1;
		iterations = 100;
		if (config->verbosity)
		{
			fprintf(stderr, "Entering mapping step for mph creation of %u keys with graph sized %u\n", mph->m, mph->n);
		}
		while(1)
		{
			int ok;
			DEBUGP("hash function 1\n");
			mph->seed[0] = rand() % mph->n;
			mph->seed[1] = rand() % mph->n;
			DEBUGP("Generating edges\n");
			ok = bmz8_gen_edges(state);
			if (!ok)
			{
				--iterations;
				DEBUGP("%u iterations remaining\n", iterations);
				if (config->verbosity)
				{
					fprintf(stderr, "simple graph creation failure - %u iterations remaining\n", iterations);
				}
				if (iterations == 0) break;
			} 
			else break;	
		}
		if (iterations == 0)
		{
			graph_destroy(state->graph);
			return NULL;
		}

		// Ordering step
		if (config->verbosity)
		{
			fprintf(stderr, "Starting ordering step\n");
		}

		graph_obtain_critical_nodes(state->graph);

		// Searching step
		if (config->verbosity)
		{
			fprintf(stderr, "Starting Searching step.\n");
			fprintf(stderr, "\tTraversing critical vertices.\n");
		}
		DEBUGP("Searching step\n");
		state->visited = (char *)malloc(mph->n/8 + 1);
		memset(state->visited, 0, mph->n/8 + 1);
		state->used_edges = (cmph_uint8 *)malloc(mph->m/8 + 1);
		memset(state->used_edges, 0, mph->m/8 + 1);
		free(mph->g);
		mph->g = calloc(mph->n, sizeof(cmph_uint8));
		if ((!state->visited) || (!state->used_edges) || (!mph->g))
		{
			return NULL;
		}
		for (i = 0; i < mph->n; ++i) // critical nodes
		{
			if (graph_node_is_critical(state->graph, i) && (!GETBIT(state->visited,i)))
			{
				if(config->impl.bmz.c > 1.14) restart_mapping = bmz8_traverse_critical_nodes(state, i);
				else restart_mapping = bmz8_traverse_critical_nodes_heuristic(state, i);
				if(restart_mapping) break;
			}
		}
		if(!restart_mapping)
		{
			if (config->verbosity)
			{
				fprintf(stderr, "\tTraversing non critical vertices.\n");
			}
			bmz8_traverse_non_critical_nodes(state); // non_critical_nodes
		}
		else 
		{
			iterations_map--;
			if (config->verbosity) fprintf(stderr, "Restarting mapping step. %u iterations remaining.\n", iterations_map);
		}    
		free(state->used_edges);
		free(state->visited);
	} while(restart_mapping && iterations_map > 0);
	graph_destroy(state->graph);	
	state->graph = NULL;
	if (iterations_map == 0) 
	{
		return NULL;
	}
	if (config->verbosity)
	{
		fprintf(stderr, "Successfully generated minimal perfect hash function\n");
	}
	return ret;
}

static cmph_uint8 bmz8_traverse_critical_nodes(state_t *state, cmph_uint8 v)
{
	cmph_uint8 next_g;
	cmph_uint32 u;   /* Auxiliary vertex */
	cmph_uint32 lav; /* lookahead vertex */
	cmph_uint8 collision;
	vqueue_t * q = vqueue_new((cmph_uint32)(graph_ncritical_nodes(state->graph)));
	graph_iterator_t it, it1;

	DEBUGP("Labelling critical vertices\n");
	state->mph->g[v] = (cmph_uint8)ceil ((double)(state->biggest_edge_value)/2) - 1;
	SETBIT(state->visited, v);
	next_g    = (cmph_uint8)floor((double)(state->biggest_edge_value/2)); /* next_g is incremented in the do..while statement*/
	vqueue_insert(q, v);
	while(!vqueue_is_empty(q))
	{
		v = vqueue_remove(q);
		it = graph_neighbors_it(state->graph, v);		
		while ((u = graph_next_neighbor(state->graph, &it)) != GRAPH_NO_NEIGHBOR)
		{		        
			if (graph_node_is_critical(state->graph, u) && (!GETBIT(state->visited,u)))
			{
				collision = 1;
				while(collision) // lookahead to resolve collisions
				{
					next_g = state->biggest_g_value + 1; 
					it1 = graph_neighbors_it(state->graph, u);
					collision = 0;
					while((lav = graph_next_neighbor(state->graph, &it1)) != GRAPH_NO_NEIGHBOR)
					{
						if (graph_node_is_critical(state->graph, lav) && GETBIT(state->visited, lav))
						{
							if(next_g + state->mph->g[lav] >= state->mph->m)
							{
								vqueue_destroy(q);
								return 1; // restart mapping step.
							}
							if (GETBIT(state->used_edges, next_g + state->mph->g[lav])) 
							{
								collision = 1;
								break;
							}
						}
					}
					if (next_g > state->biggest_g_value) state->biggest_g_value = next_g;
				}		
				// Marking used edges...
				it1 = graph_neighbors_it(state->graph, u);
				while((lav = graph_next_neighbor(state->graph, &it1)) != GRAPH_NO_NEIGHBOR)
				{
					if (graph_node_is_critical(state->graph, lav) && GETBIT(state->visited, lav))
					{
						SETBIT(state->used_edges, next_g + state->mph->g[lav]);
						if(next_g + state->mph->g[lav] > state->biggest_edge_value) state->biggest_edge_value = next_g + state->mph->g[lav];
					}
				}
				state->mph->g[u] = next_g; // Labelling vertex u.
				SETBIT(state->visited, u);
				vqueue_insert(q, u);
			}			
		}

	}
	vqueue_destroy(q);
	return 0;
}

static cmph_uint8 bmz8_traverse_critical_nodes_heuristic(state_t *state, cmph_uint8 v)
{
	cmph_uint8 next_g;
	cmph_uint32 u;   /* Auxiliary vertex */
	cmph_uint32 lav; /* lookahead vertex */
	cmph_uint8 collision;
	cmph_uint8 * unused_g_values = NULL;
	cmph_uint8 unused_g_values_capacity = 0;
	cmph_uint8 nunused_g_values = 0;
	vqueue_t * q = vqueue_new((cmph_uint32)(graph_ncritical_nodes(state->graph)));
	graph_iterator_t it, it1;

	DEBUGP("Labelling critical vertices\n");
	state->mph->g[v] = (cmph_uint8)ceil ((double)(state->biggest_edge_value)/2) - 1;
	SETBIT(state->visited, v);
	next_g    = (cmph_uint8)floor((double)(state->biggest_edge_value/2)); /* next_g is incremented in the do..while statement*/
	vqueue_insert(q, v);
	while(!vqueue_is_empty(q))
	{
		v = vqueue_remove(q);
		it = graph_neighbors_it(state->graph, v);		
		while ((u = graph_next_neighbor(state->graph, &it)) != GRAPH_NO_NEIGHBOR)
		{		        
			if (graph_node_is_critical(state->graph, u) && (!GETBIT(state->visited, u)))
			{
				cmph_uint8 next_g_index = 0;
				collision = 1;
				while(collision) // lookahead to resolve collisions
				{
					if (next_g_index < nunused_g_values) 
					{
						next_g = unused_g_values[next_g_index++];
					}
					else    
					{
						next_g = state->biggest_g_value + 1; 
						next_g_index = 255;//UINT_MAX;
					}
					it1 = graph_neighbors_it(state->graph, u);
					collision = 0;
					while((lav = graph_next_neighbor(state->graph, &it1)) != GRAPH_NO_NEIGHBOR)
					{
						if (graph_node_is_critical(state->graph, lav) && GETBIT(state->visited, lav))
						{
							if(next_g + state->mph->g[lav] >= state->mph->m)
							{
								vqueue_destroy(q);
								free(unused_g_values);
								return 1; // restart mapping step.
							}
							if (GETBIT(state->used_edges, next_g + state->mph->g[lav])) 
							{
								collision = 1;
								break;
							}
						}
					}
					if(collision && (next_g > state->biggest_g_value)) // saving the current g value stored in next_g.
					{
						if(nunused_g_values == unused_g_values_capacity)
						{
							unused_g_values = realloc(unused_g_values, (unused_g_values_capacity + BUFSIZ)*sizeof(cmph_uint8));
							unused_g_values_capacity += BUFSIZ;  							
						} 
						unused_g_values[nunused_g_values++] = next_g;							

					}
					if (next_g > state->biggest_g_value) state->biggest_g_value = next_g;
				}	
				next_g_index--;
				if (next_g_index < nunused_g_values) unused_g_values[next_g_index] = unused_g_values[--nunused_g_values];

				// Marking used edges...
				it1 = graph_neighbors_it(state->graph, u);
				while((lav = graph_next_neighbor(state->graph, &it1)) != GRAPH_NO_NEIGHBOR)
				{
					if (graph_node_is_critical(state->graph, lav) && GETBIT(state->visited, lav))
					{
						SETBIT(state->used_edges,next_g + state->mph->g[lav]);
						if(next_g + state->mph->g[lav] > state->biggest_edge_value) state->biggest_edge_value = next_g + state->mph->g[lav];
					}
				}
				state->mph->g[u] = next_g; // Labelling vertex u.
				SETBIT(state->visited, u);
				vqueue_insert(q, u);
			}			
		}

	}
	vqueue_destroy(q);
	free(unused_g_values);
	return 0;  
}

static cmph_uint8 next_unused_edge(state_t *state, cmph_uint8 unused_edge_index)
{
       while(1)
       {
		assert(unused_edge_index < state->mph->m);
		if(GETBIT(state->used_edges, unused_edge_index)) unused_edge_index ++;
		else break;
       }
       return unused_edge_index;
}

static void bmz8_traverse(state_t *state, cmph_uint8 v, cmph_uint8 * unused_edge_index)
{
	graph_iterator_t it = graph_neighbors_it(state->graph, v);
	cmph_uint32 neighbor = 0;
	while((neighbor = graph_next_neighbor(state->graph, &it)) != GRAPH_NO_NEIGHBOR)
	{
		if(GETBIT(state->visited, neighbor)) continue;
		//DEBUGP("Visiting neighbor %u\n", neighbor);
		*unused_edge_index = next_unused_edge(state, *unused_edge_index);
		state->mph->g[neighbor] = *unused_edge_index - state->mph->g[v];
		//if (bmz8->g[neighbor] >= bmz8->m) bmz8->g[neighbor] += bmz8->m;
		SETBIT(state->visited, neighbor);
		(*unused_edge_index)++;
		bmz8_traverse(state, neighbor, unused_edge_index);

	}  
}

static void bmz8_traverse_non_critical_nodes(state_t *state)
{

	cmph_uint8 i, v1, v2, unused_edge_index = 0;
	DEBUGP("Labelling non critical vertices\n");
	for(i = 0; i < state->mph->m; i++)
	{
		v1 = graph_vertex_id(state->graph, i, 0);
		v2 = graph_vertex_id(state->graph, i, 1);
		if((GETBIT(state->visited, v1) && GETBIT(state->visited, v2)) || (!GETBIT(state->visited, v1) && !GETBIT(state->visited, v2))) continue;				  	
		if(GETBIT(state->visited, v1)) bmz8_traverse(state, v1, &unused_edge_index);
		else bmz8_traverse(state, v2, &unused_edge_index);

	}

	for(i = 0; i < state->mph->n; i++)
	{
		if(!GETBIT(state->visited, i))
		{ 	                        
			state->mph->g[i] = 0;
			SETBIT(state->visited, i);
			bmz8_traverse(state, i, &unused_edge_index);
		}
	}

}
		
static int bmz8_gen_edges(state_t *state)
{
	cmph_uint8 e;
	cmph_uint8 multiple_edges = 0;
	graph_clear_edges(state->graph);	
	state->key_source->rewind(state->key_source->data);
	for (e = 0; e < state->key_source->nkeys; ++e)
	{
		cmph_uint8 h1, h2;
		cmph_uint32 keylen;
		char *key = NULL;
		state->key_source->read(state->key_source->data, &key, &keylen);
		
//		if (key == NULL)fprintf(stderr, "key = %s -- read BMZ\n", key);
		h1 = HASHKEY(state->mph->hash[0], state->mph->seed[0], key, keylen) % state->mph->n;
		h2 = HASHKEY(state->mph->hash[1], state->mph->seed[1], key, keylen) % state->mph->n;
		if (h1 == h2) if (++h2 >= state->mph->n) h2 = 0;
		if (h1 == h2) 
		{
			if (state->config->verbosity) fprintf(stderr, "Self loop for key %u\n", e);
			state->key_source->dispose(state->key_source->data, key, keylen);
			return 0;
		}
		//DEBUGP("Adding edge: %u -> %u for key %s\n", h1, h2, key);
		state->key_source->dispose(state->key_source->data, key, keylen);
//		fprintf(stderr, "key = %s -- dispose BMZ\n", key);
		multiple_edges = graph_contains_edge(state->graph, h1, h2);
		if (state->config->verbosity && multiple_edges) 
		{
			fprintf(stderr, "A non simple graph was generated\n");
		}
		if (multiple_edges) return 0; // checking multiple edge restriction.
		graph_add_edge(state->graph, h1, h2);
	}
	return !multiple_edges;
}

cmph_uint32 bmz8_search(const cmph_t *mphf, const char *key, cmph_uint32 keylen)
{
	const bmz8_t *mph = (bmz8_t *)mphf->impl;
	cmph_uint32 h1, h2;
	h1 = HASHKEY(mph->hash[0], mph->seed[0], key, keylen) % mph->n;
	h2 = HASHKEY(mph->hash[1], mph->seed[1], key, keylen) % mph->n;
	DEBUGP("key: %s h1: %u h2: %u\n", key, h1, h2);
	if (h1 == h2 && ++h2 > mph->n) h2 = 0;
	DEBUGP("key: %s g[h1]: %u g[h2]: %u edges: %u\n", key, mph->g[h1], mph->g[h2], mph->m);
	return (mph->g[h1] + mph->g[h2]);
}
cmph_uint32 bmz8_size(const cmph_t *mphf)
{
	const bmz8_t *mph = (bmz8_t *)mphf->impl;
	return mph->m;
}
void bmz8_destroy(cmph_t *mphf)
{
	bmz8_t *mph = (bmz8_t *)mphf->impl;
	free(mph->g);	
	free(mph);
	free(mphf);
}
