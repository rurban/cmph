#ifndef __XGRAPH_H__
#include <limits.h>
#include "cmph_types.h"

typedef struct __xgraph_t xgraph_t;
typedef struct __xgraph_adj__t
{
	cmph_uint32 v;
	cmph_uint32 degree;
	cmph_uint32 *adj;
	cmph_uint32 *ids;
} xgraph_adj_t;



xgraph_t *xgraph_new(cmph_uint32 nnodes, cmph_uint32 memory);
void xgraph_add_edge(xgraph_t *g, cmph_uint32 v1, cmph_uint32 v2);
/** Close the graph
 * \return whether the graph is acyclic
 */
char xgraph_pack(xgraph_t *g);
/** Restart a "paralell bfs traversal
 */
void xgraph_pbfs_reset(xgraph_t *g);
/** Get next vertex and its adjacency list in
 * "paralell bfs traversal" order.
 * This is only available to _acyclic_ graphs
 * \return the adjacency list available
 */
xgraph_adj_t xgraph_pbfs_next(xgraph_t *g);
/** Gives the number of nodes in the graph
 * \return the number of nodes in the graph
 */
cmph_uint32 xgraph_size(xgraph_t *g);
void xgraph_destroy(xgraph_t *g);

#endif// __ZGRAPH_H__
