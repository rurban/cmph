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
void xgraph_pack(xgraph_t *g);
void xgraph_pbfs_reset(xgraph_t *g);
xgraph_adj_t xgraph_pbfs_next(xgraph_t *g);
cmph_uint32 xgraph_size(xgraph_t *g);
void xgraph_destroy(xgraph_t *g);

#endif// __ZGRAPH_H__
