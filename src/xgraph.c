#include "xgraph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

//#define DEBUG
#include "debug.h"

typedef struct
{
	cmph_uint32 v;
	cmph_uint32 root;
	cmph_uint32 from;
} wanted_t;

typedef struct 
{
	cmph_uint32 first;
	cmph_uint32 last;
	cmph_uint32 offset;
	cmph_uint32 size;
} xgraph_block_t;

struct __xgraph_t
{
	char *edges_fname;
	int edges_fd;
	char *adj_fname;
	int adj_fd;
	char *walk_fname;
	int walk_fd;

	cmph_uint32 nnodes;
	cmph_uint32 nedges;
	cmph_uint32 memory;

	cmph_uint8 *visited;
	cmph_uint8 *dead;

	cmph_uint32 **roots;
	cmph_uint32 *steps;
	cmph_uint32 nroots;

	wanted_t *wanted;
	cmph_uint32 nwanted;

	xgraph_block_t *blocks;
	cmph_uint32 nblocks;
	cmph_uint8 *visited_blocks;
};

typedef struct __xgraph_edge_t
{
	cmph_uint32 v1;
	cmph_uint32 v2;
	cmph_uint32 id;
} xgraph_edge_t;

static cmph_uint32 *write_adj(xgraph_t *g);
static void init_roots(xgraph_t *g, cmph_uint32 *list_sizes);
static void build_walk(xgraph_t *g);
static char dump_walk(xgraph_t *g);
static char append_root(xgraph_t *g, cmph_uint32 root, xgraph_adj_t list);
static char append_list(xgraph_t *g, cmph_uint32 root, cmph_uint32 from, xgraph_adj_t list, cmph_uint32 wanted_from);
static void remove_root(xgraph_t *g, cmph_uint32 root);
static cmph_uint32 next_block(xgraph_t *g);
static void print_roots(xgraph_t *g);

xgraph_t *xgraph_new(cmph_uint32 nnodes, cmph_uint32 memory)
{
	xgraph_t *g = (xgraph_t *)malloc(sizeof(xgraph_t));
	char *tmpl = NULL;
	if (!g) return g;
	tmpl = strdup("cmph_xgraph_edges.XXXXXX");
	g->edges_fd = mkstemp(tmpl);
	if (g->edges_fd == -1) return NULL;
	g->edges_fname = tmpl;
	tmpl = strdup("cmph_xgraph_adj.XXXXXX");
	g->adj_fd = mkstemp(tmpl);
	if (g->adj_fd == -1) return NULL;
	g->adj_fname = tmpl;
	g->nnodes = nnodes;
	g->nedges = 0;
	g->memory = memory;
	g->walk_fname = NULL;
	g->walk_fd = 0;
	return g;
}

void xgraph_destroy(xgraph_t *g)
{
	free(g->edges_fname);
	close(g->edges_fd);
	free(g->adj_fname);
	close(g->adj_fd);
	free(g);
}
void xgraph_add_edge(xgraph_t *g, cmph_uint32 v1, cmph_uint32 v2)
{
	int c = 0;
	xgraph_edge_t e1, e2;

	DEBUGP("Adding edge %u->%u\n", v1, v2);
	e1.v1 = v1;
	e1.v2 = v2;
	e1.id = g->nedges;
	c = write(g->edges_fd, &e1, sizeof(e1));
	assert(c >= 0);
	e2.v1 = v2;
	e2.v2 = v1;
	e2.id = g->nedges;
	c = write(g->edges_fd, &e2, sizeof(e2));
	assert(c >= 0);
	++(g->nedges);
}

static int xgraph_edge_cmp(const void *a, const void *b)
{
	xgraph_edge_t *e1 = (xgraph_edge_t *)a;
	xgraph_edge_t *e2 = (xgraph_edge_t *)b;
	return e1->v1 - e2->v1;
}
cmph_uint32 xgraph_size(xgraph_t *g)
{
	return g->nnodes;
}


void xgraph_pbfs_reset(xgraph_t *g)
{
	lseek(g->adj_fd, 0, SEEK_SET);
}

xgraph_adj_t xgraph_pbfs_next(xgraph_t *g)
{
	cmph_uint32 i = 0;
	xgraph_adj_t list;
	read(g->adj_fd, &(list.v), sizeof(cmph_uint32));
	read(g->adj_fd, &(list.degree), sizeof(cmph_uint32));
	list.adj = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*list.degree);
	list.ids = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*list.degree);
	for (i = 0; i < list.degree; ++i)
	{
		read(g->adj_fd, &(list.adj[i]), sizeof(cmph_uint32));
		read(g->adj_fd, &(list.ids[i]), sizeof(cmph_uint32));
	}
	return list;
}



char xgraph_pack(xgraph_t *g)
{
	cmph_uint32 *list_sizes = NULL;
	DEBUGP("Writing adjacency lists\n");
	list_sizes = write_adj(g);
	DEBUGP("Creating roots\n");
	init_roots(g, list_sizes);
	free(list_sizes);
	DEBUGP("Building walk\n");
	build_walk(g);
	char cyclic = dump_walk(g);
	if (cyclic) return 0;
	xgraph_pbfs_reset(g);
	return 1;
}

static cmph_uint32 *write_adj(xgraph_t *g)
{
	//Sort edges list
	//Please change me for external sort
	DEBUGP("Sorting edge list\n");
	lseek(g->edges_fd, 0, SEEK_SET);
	xgraph_edge_t *edges = (xgraph_edge_t *)malloc(sizeof(xgraph_edge_t)*g->nedges*2);
	read(g->edges_fd, edges, sizeof(xgraph_edge_t)*g->nedges*2);
	qsort(edges, g->nedges*2, sizeof(xgraph_edge_t), xgraph_edge_cmp);

	DEBUGP("Writing adjacency lists\n");
	cmph_uint32 *list_sizes = NULL;
	list_sizes = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*g->nnodes);
	//Write adjancency lists
	lseek(g->adj_fd, 0, SEEK_SET);

	xgraph_adj_t clist;
	clist.v = 0;
	clist.degree = 0;
	clist.ids = NULL;
	clist.adj = NULL;
	cmph_uint32 i = 0;
	cmph_uint32 j = 0;
	cmph_uint32 edges_it = 0;
	for (i = 0; i < g->nnodes; ++i)
	{
		while (edges_it < g->nedges * 2 && edges[edges_it].v1 == clist.v)
		{
			++(clist.degree);
			clist.ids = (cmph_uint32 *)realloc(clist.ids, sizeof(cmph_uint32)*clist.degree);
			clist.adj = (cmph_uint32 *)realloc(clist.adj, sizeof(cmph_uint32)*clist.degree);
			clist.ids[clist.degree - 1] = edges[edges_it].id;
			clist.adj[clist.degree - 1] = edges[edges_it].v2;
			++edges_it;
		}
#ifdef DEBUG
		DEBUGP("Writing entry %u degree %u: ", i, clist.degree);
		for (j = 0; j < clist.degree; ++j) fprintf(stderr, "%u ", clist.adj[j]);
		fprintf(stderr, "\n");
#endif
		int c = write(g->adj_fd, &(clist.degree), sizeof(clist.degree));
		assert(c >= 0);
		for (j = 0; j < clist.degree; ++j)
		{
			c = write(g->adj_fd, &(clist.adj[j]), sizeof(clist.adj[j]));
			assert(c >= 0);
			c = write(g->adj_fd, &(clist.ids[j]), sizeof(clist.ids[j]));
			assert(c >= 0);
		}
		list_sizes[i] = sizeof(cmph_uint32) + (2 * sizeof(cmph_uint32)*clist.degree);
		++(clist.v);
		clist.degree = 0;
		free(clist.adj);
		clist.adj = NULL;
		free(clist.ids);
		clist.ids = NULL;
	}
	return list_sizes;
}
static void init_roots(xgraph_t *g, cmph_uint32 *list_sizes)
{
	g->blocks = NULL;
	g->nblocks = 0;
	cmph_uint32 size = 0;
	cmph_uint32 offset = 0;
	cmph_uint32 first = 0;
	cmph_uint32 i = 0;
	for (i = 0; i < g->nnodes; ++i)
	{
		DEBUGP("Adding list with size %u\n", list_sizes[i]);
		size += list_sizes[i];
		if (size >= g->memory)
		{
			g->blocks = (xgraph_block_t *)realloc(g->blocks, sizeof(xgraph_block_t)*(g->nblocks + 1));
			g->blocks[g->nblocks].size = size;
			g->blocks[g->nblocks].offset = offset;
			g->blocks[g->nblocks].first = first;
			g->blocks[g->nblocks].last = i + 1;
			DEBUGP("Created block sized %u\n", size);
			++(g->nblocks);

			offset += size;
			size = 0;
			first = i + 1;
		}
	}
	if (size)
	{
		g->blocks = (xgraph_block_t *)realloc(g->blocks, sizeof(xgraph_block_t)*(g->nblocks + 1));
		g->blocks[g->nblocks].size = size;
		g->blocks[g->nblocks].offset = offset;
		g->blocks[g->nblocks].first = first;
		g->blocks[g->nblocks].last = g->nnodes;
		DEBUGP("Created last block sized %u\n", size);
		++(g->nblocks);
	}
	DEBUGP("Created %u blocks\n", g->nblocks);

	g->visited = (cmph_uint8 *)malloc(sizeof(cmph_uint8) * g->nnodes);
	memset(g->visited, 0, g->nnodes);
	g->dead = (cmph_uint8 *)malloc(sizeof(cmph_uint8) * g->nnodes);
	memset(g->dead, 0, g->nnodes);
	assert(g->nblocks);
	g->visited_blocks = (cmph_uint8 *)malloc(sizeof(cmph_uint8) * g->nblocks);
	memset(g->visited_blocks, 0, g->nblocks);
	g->nroots = 0;
	g->roots = NULL;
	g->steps = NULL;
	g->wanted = NULL;
	g->nwanted = 0;
	DEBUGP("Initialized roots and visited structures\n");
}

static xgraph_adj_t *xgraph_read_block(xgraph_t *g, xgraph_block_t block)
{
	cmph_uint32 nlists = block.last - block.first;
	lseek(g->adj_fd, block.offset, SEEK_SET);
	DEBUGP("Alloc() %u bytes for lists [%u;%u) at offset %u\n", block.size, block.first, block.last, block.offset);
	cmph_uint32 *buf = (cmph_uint32 *)malloc(block.size);
	int c = read(g->adj_fd, buf, block.size);
	assert(c == block.size);

	cmph_uint32 i = 0;
	cmph_uint32 j = 0;
	cmph_uint32 *ptr = buf;
	xgraph_adj_t *lists = (xgraph_adj_t *)malloc(sizeof(xgraph_adj_t)*nlists);
	assert(lists);
	for (i = 0; i < nlists; ++i)
	{
		xgraph_adj_t clist;
		clist.degree = *ptr++;
		DEBUGP("Got list %u with degree %u (size %u)\n", block.first + i, clist.degree, sizeof(cmph_uint32)*clist.degree*2 + sizeof(cmph_uint32));
		clist.ids = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*clist.degree);
		clist.adj = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*clist.degree);
		for (j = 0; j < clist.degree; ++j)
		{
			clist.adj[j] = *ptr++;
			clist.ids[j] = *ptr++;
		}
		lists[i] = clist;
	}
	free(buf);
	return lists;
}
		

static void free_adj_lists(xgraph_adj_t *lists, cmph_uint32 nlists)
{
	cmph_uint32 i = 0;
	for (i = 0; i < nlists; ++i)
	{
		free(lists[i].ids);
		free(lists[i].adj);
	}
	free(lists);
}

static int wanted_cmp(const void *a, const void *b)
{
	wanted_t *w1 = (wanted_t *)a;
	wanted_t *w2 = (wanted_t *)b;
	//Do not use - because of overflow
	if (w2->v == w1->v) return 0;
	if (w1->v > w2->v) return 1;
	return -1;
}

static void build_walk(xgraph_t *g)
{
	while(1)
	{
		cmph_uint32 block_index = next_block(g);
		if (block_index == UINT_MAX) break;
		//Update roots
		cmph_uint32 first = g->blocks[block_index].first;
		cmph_uint32 last = g->blocks[block_index].last;
		cmph_uint32 nlists = last - first;
		DEBUGP("Reading block %u with range %u:%u\n", block_index, first, last);
		xgraph_adj_t *lists = xgraph_read_block(g, g->blocks[block_index]);

		char growing = 1;
		cmph_uint32 i = 0;
			
		while (growing)
		{
			growing = 0;
			cmph_uint32 wi = 0; 

			DEBUGP("Inspecting available lists for range %u:%u\n", first, last);

			DEBUGP("Removing conflicting trees\n");
			//Remove trees that want the same adjacency list
			//A bit complicated to favor bigger trees
			cmph_uint32 ptree = 0;
			for (wi = 1; wi < g->nwanted; ++wi)
			{
				if (g->wanted[ptree].v == g->wanted[wi].v && g->wanted[wi].v != UINT_MAX)
				{
					DEBUGP("Both trees %u and %u want vertex %u\n", g->wanted[ptree].root, g->wanted[wi].root, g->wanted[wi].v);
					if (g->steps[g->wanted[ptree].root] > g->steps[g->wanted[wi].root])
					{
						remove_root(g, g->wanted[wi].root);
						g->wanted[wi].v = UINT_MAX;
					}
					else
					{
						remove_root(g, g->wanted[ptree].root);
						g->wanted[ptree].v = UINT_MAX;
						ptree = wi;
					}
				} else ++ptree;
			}

			//Add adjacency lists
			DEBUGP("Appending adjacency lists\n");
			wi = 0;
			for (i = 0; i < nlists; ++i)
			{
				//Walk wanted list until a possible candidate
				while (wi < g->nwanted && (g->wanted[wi].v < first + i || g->wanted[wi].v == UINT_MAX)) ++wi;
				//Check if someone is looking for this list
				while (wi < g->nwanted && first + i == g->wanted[wi].v)
				{
					DEBUGP("Appending wanted list %u to root %u\n", g->wanted[wi].v, g->wanted[wi].root);
					char appended = append_list(g, g->wanted[wi].root, first + i, lists[i], g->wanted[wi].from);
					if (!appended)
					{
						//Tree touched some already visited edge.
						//Remove it (there might be a better policty)
						remove_root(g, g->wanted[wi].root);
					}
					else 
					{
						DEBUGP("Grow through list append\n");
						growing = 1;
					}
					//Mark entry for removal
					g->wanted[wi].v = UINT_MAX;
					++wi;
				}
			}
			//add new trees
			DEBUGP("Adding new trees\n");
			for (i = 0; i < nlists; ++i)
			{
				if((!g->visited[first + i]) && (!g->dead[first + i]))
				{
					char appended = append_root(g, first + i, lists[i]);
					if (appended) 
					{
						DEBUGP("Grow through new root\n");
						growing = 1;
					}
				}
			}
			DEBUGP("Pruning wanted\n");
			//Sort wanted
			qsort(g->wanted, g->nwanted, sizeof(wanted_t), wanted_cmp);
			//Realloc it
			wi = 0;
			for (wi = 0; wi < g->nwanted; ++wi)
			{
				if (g->wanted[wi].v == UINT_MAX) break;
			}
			DEBUGP("Prune point is %u\n", wi);
			if (wi < g->nwanted) 
			{	
				g->wanted = (wanted_t *)realloc(g->wanted, wi*sizeof(wanted_t));
				g->nwanted = wi;
			}
			print_roots(g);
		}
		free_adj_lists(lists, nlists);

		//Check if there are still empty roots
		if (g->nroots == 0)
		{
			//if (g->visited_blocks == g->nblocks)
			break;
		}
	}
	free(g->wanted);
	g->wanted = NULL;
	g->nwanted = 0;
	free(g->visited);
	g->visited = NULL;
	free(g->dead);
	g->dead = NULL;
	free(g->visited_blocks);
	g->visited_blocks = NULL;
	free(g->blocks);
	g->blocks = NULL;
	g->nblocks = 0;
}

static cmph_uint32 *__walk = NULL; //Use ugly global for quicksort
static int adj_walk_cmp(const void *a, const void *b)
{
	xgraph_adj_t *l1 = (xgraph_adj_t *)a;
	xgraph_adj_t *l2 = (xgraph_adj_t *)b;
	return __walk[l1->v] - __walk[l2->v];
}

static char dump_walk(xgraph_t *g)
{
	cmph_uint32 walksteps = 0;
	g->walk_fname = strdup("cmph_xgraph_walk.XXXXXX");
	g->walk_fd = mkstemp(g->walk_fname);
	cmph_uint32 i = 0, j = 0;
	for (i = 0; i < g->nroots; ++i)
	{
		if (g->roots[i][0] == UINT_MAX) continue;
		write(g->walk_fd, g->roots[i], sizeof(cmph_uint32)*g->steps[i]);
		walksteps += g->steps[i];
		free(g->roots[i]);
	}
	free(g->roots);
	g->roots = NULL;
	g->nroots = 0;
	free(g->steps);
	g->steps = NULL;

	if (walksteps != g->nnodes) 
	{
		DEBUGP("CYCLIC GRAPH: walk steps: %u nodes: %u\n", walksteps, g->nnodes);
		return 0;
	}

	lseek(g->walk_fd, 0, SEEK_SET);
	cmph_uint32 *walk = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*walksteps);
	for (i = 0; i < walksteps; ++i)
	{
		cmph_uint32 step = 0;
		read(g->walk_fd, &step, sizeof(cmph_uint32));
		walk[step] = i;
	}

#ifdef DEBUG
	DEBUGP("Dumping walk:\n");
	for (i = 0; i < walksteps; ++i) fprintf(stderr, "%u ", walk[i]);
	fprintf(stderr, "\n");
#endif

	xgraph_adj_t *lists = (xgraph_adj_t *)malloc(sizeof(xgraph_adj_t)*walksteps);
	//Use external sort
	lseek(g->adj_fd, 0, SEEK_SET);
	for (i = 0; i < walksteps; ++i)
	{
		xgraph_adj_t clist;
		clist.v = i;
		read(g->adj_fd, &(clist.degree), sizeof(cmph_uint32)); 
		clist.adj = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*clist.degree);
		clist.ids = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*clist.degree);
		for (j = 0; j < clist.degree; ++j)
		{
			read(g->adj_fd, &(clist.adj[j]), sizeof(cmph_uint32));
			read(g->adj_fd, &(clist.ids[j]), sizeof(cmph_uint32));
		}
		lists[i] = clist;
	}
	__walk = walk;
	qsort(lists, walksteps, sizeof(xgraph_adj_t), adj_walk_cmp);
	__walk = NULL;
	lseek(g->adj_fd, 0, SEEK_SET);
	for (i = 0; i < walksteps; ++i)
	{
		xgraph_adj_t clist = lists[i];
		DEBUGP("Writing adjacency list for vertex %u\n", clist.v);
		write(g->adj_fd, &(clist.v), sizeof(cmph_uint32)); 
		write(g->adj_fd, &(clist.degree), sizeof(cmph_uint32)); 
		for (j = 0; j < clist.degree; ++j)
		{
			write(g->adj_fd, &(clist.adj[j]), sizeof(cmph_uint32));
			write(g->adj_fd, &(clist.ids[j]), sizeof(cmph_uint32));
		}
		free(clist.ids);
		free(clist.adj);
	}
	free(lists);
	lists = NULL;
	return 1;
}

static void print_roots(xgraph_t *g)
{
#ifdef DEBUG
	cmph_uint32 i = 0, j = 0;
	DEBUGP("Printing %u roots\n", g->nroots);
	for (i = 0; i < g->nroots; ++i)
	{
		assert(g->roots[i]);
		if (g->roots[i][0] == UINT_MAX) fprintf(stderr, "*\n");
		else
		{
			for (j = 0; j < g->steps[i]; ++j)
			{
				fprintf(stderr, "%u ", g->roots[i][j]);
			}
			fprintf(stderr, "\n");
		}
	}
	fprintf(stderr, "Wanted vector:\n");
	for (i = 0; i < g->nwanted; ++i) fprintf(stderr, "adj %u root %u\n", g->wanted[i].v, g->wanted[i].root);
#endif
}

static cmph_uint32 next_block(xgraph_t *g)
{
	cmph_uint32 maxscore = 0;
	cmph_uint32 best_block = 0;
	cmph_uint32 current_score = 0;
	cmph_uint32 current_block = 0;
	cmph_uint32 wi = 0;


	DEBUGP("Choosing next block\n");
	//There is no wanted vertex, but we need to check
	//unseen blocks for isolated components
	if (g->nwanted == 0)
	{
		cmph_uint32 i = 0;
		for(i = 0; i < g->nblocks; ++i)
		{
			if (!g->visited_blocks[i]) 
			{
				g->visited_blocks[i] = 1;
				return i;
			}
		}
		return UINT_MAX;
	}

	//wanted vector is sorted
	for (wi = 0; wi < g->nwanted; ++wi)
	{
		while (g->wanted[wi].v >= g->blocks[current_block].last)
		{
			if (maxscore < current_score)
			{
				best_block = current_block;
				maxscore = current_score;
			}
			++current_block;
			current_score = 0;
		}
		++current_score;
	}
	if (maxscore < current_score)
	{
		maxscore = current_score;
		best_block = current_block;
	}
	DEBUGP("Best block is %u with score %u\n", best_block, maxscore);
	g->visited_blocks[best_block] = 1;
	return best_block;
}

static char append_root(xgraph_t *g, cmph_uint32 root, xgraph_adj_t list)
{
	DEBUGP("Appending new root %u\n", root);
	cmph_uint32 i = 0;
	//Check if this conflict with another tree
	for (i = 0; i < list.degree; ++i)
	{
		//Tree is violating another one.
		//Refuse to continue and let tree be removed later
		if (g->visited[list.adj[i]] || g->dead[list.adj[i]]) return 0;
	}
	g->roots = (cmph_uint32 **)realloc(g->roots, sizeof(cmph_uint32 *)*(g->nroots + 1));
	g->steps = (cmph_uint32 *)realloc(g->steps, sizeof(cmph_uint32)*(g->nroots + 1));
	g->roots[g->nroots] = (cmph_uint32 *)malloc(sizeof(cmph_uint32)*(1 + list.degree));
	g->roots[g->nroots][0] = root;
	g->steps[g->nroots] = 1 + list.degree;
	for (i = 0; i < list.degree; ++i)
	{
		g->visited[list.adj[i]] = 1;
		g->roots[g->nroots][i + 1] = list.adj[i];
	}
	g->visited[root] = 1;
	++g->nroots;

	//Schedule the need of new adjacency lists
	DEBUGP("Reallocing wanted with %u bytes\n", (g->nwanted + list.degree) * sizeof(wanted_t));
	g->wanted = (wanted_t *)realloc(g->wanted, (g->nwanted + list.degree)*sizeof(wanted_t));
	for (i = 0; i < list.degree; ++i)
	{
		g->wanted[(g->nwanted + i)].v = list.adj[i];
		g->wanted[(g->nwanted + i)].root = root;
		g->wanted[(g->nwanted + i)].from = root;
	}
	g->nwanted += list.degree;
	return 1;
}
static char append_list(xgraph_t *g, cmph_uint32 root, cmph_uint32 from, xgraph_adj_t list, cmph_uint32 wanted_from)
{
	DEBUGP("Appending list from %u sized %u to root %u asked by %u\n", from, list.degree, root, wanted_from);
	//Find root. Please change to binary search later.
	cmph_uint32 root_ptr = UINT_MAX;
	cmph_uint32 i = 0;
	for (i = 0; i < g->nroots; ++i)
	{
		if (g->roots[i][0] == root)
		{
			root_ptr = i;
			break;
		}
	}
	if (root_ptr == UINT_MAX)
	{
		DEBUGP("Tree removed. Ignoring.\n");
		return 1;
	}
	
	//Concat adjacency list with the current walk
	g->roots[root_ptr] = (cmph_uint32 *)realloc(g->roots[root_ptr], (g->steps[root_ptr] + list.degree - 1)*sizeof(cmph_uint32));
	for (i = 0; i < list.degree; ++i)
	{
		if (list.adj[i] == wanted_from) continue;
		//Tree is violating another one.
		//Refuse to continue and let tree be removed later
		if (g->visited[list.adj[i]]) 
		{
			DEBUGP("Tree wants to touch visited vertex %u\n", list.adj[i]);
			return 0;
		}

		g->roots[root_ptr][g->steps[root_ptr]] = list.adj[i];
		++(g->steps[root_ptr]);
		g->visited[list.adj[i]] = 1;
	}

	//Schedule the need of new adjacency lists
	if (list.degree) g->wanted = (wanted_t *)realloc(g->wanted, (g->nwanted + list.degree - 1)*sizeof(wanted_t));
	for (i = 0; i < list.degree; ++i)
	{
		if (list.adj[i] == wanted_from) continue;
		g->wanted[g->nwanted].v = list.adj[i];
		g->wanted[g->nwanted].root = root;
		g->wanted[g->nwanted].from = from;
		++g->nwanted;
	}
	return 1;
}

static void remove_root(xgraph_t *g, cmph_uint32 root)
{
	DEBUGP("Removing tree rooted at %u\n", root);
	//Find root. Please change to binary search later.
	cmph_uint32 root_ptr = UINT_MAX;
	cmph_uint32 i = 0;
	for (i = 0; i < g->nroots; ++i)
	{
		if (g->roots[i][0] == root)
		{
			root_ptr = i;
			break;
		}
	}
	if (root_ptr == UINT_MAX) return;
	for (i = 0; i < g->steps[root_ptr]; ++i)
	{
		g->dead[g->roots[root_ptr][i]] = 1;
		g->visited[g->roots[root_ptr][i]] = 0;
	}
	g->roots[root_ptr][0] = UINT_MAX; //no realloc by now
									  //just mark it
}
