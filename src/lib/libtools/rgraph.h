
/* 
 * Rajiv Wickremesinghe 2/2003
 */

#include "queue.h"

#ifndef _RGRAPH_H_
#define _RGRAPH_H_

struct node_t;
typedef struct edge_t {
  struct node_t *node;
  TAILQ_ENTRY(edge_t) link;

  /* user's data */
  int val;
} edge_t;


struct node_t;
typedef struct node_t {
  int id;			/* unique id */
  char *label;
  TAILQ_ENTRY(node_t) link;	/* link for node list */
  TAILQ_HEAD(edges, edge_t) edges; /* list of edges */

  /* temp data used/output by algorithms */
  int color;
  int td;			/* discovery time / depth etc. */
  int tf;			/* finish time */
  TAILQ_ENTRY(node_t) olink;	/* ordered link */
  struct node_t *visit;		/*  */

  /* user's data */
  int val;
  void *data;
} node_t;


typedef TAILQ_HEAD(nodes, node_t) nodelist_t;

typedef struct graph_t {
  nodelist_t nodes;
  nodelist_t olist;		/* alternate list (output of some funcs) see GLIST */
  int current_id;
} graph_t;


/* initialize structs */
void gInit(graph_t *g);
void gInitFromList(graph_t *g, graph_t *src);

/* deallocate (does not free g) */
void gClear(graph_t *g);

/* allocate and add a new unconnected to graph */
node_t *gNewNode(graph_t *g, char *label);

/* add a directed edge to the graph */
void gAddEdge(graph_t *g, node_t *u, node_t *v);

/* print to stdout */
void gPrint(graph_t *g);
void gPrintNode(node_t *np);

/* export in daVinci format to filename */
void gExport(graph_t *g, char *filename);

/* a macro to visit all nodes.
 * eg: GFOREACH(graph, nodeptr) { gPrintNode(nodeptr); }
 */
#define GFOREACH(g,np)  TAILQ_FOREACH((np), &(g)->nodes, link)

/* a macro to use the list output */
#define GLIST(g,np)  TAILQ_FOREACH((np), &(g)->olist, olink)


/* 
 * algorithms
 */

/* do topological sort. output will be in olist */
int gTopoSort(graph_t *g);

/* run Dijkstra. node weights read from node->val.
 * output (depth) will be in node->td, SP can be found via node->visit
 */
void gSSSP(graph_t *g, node_t *src);

#endif /* _RGRAPH_H_ */
