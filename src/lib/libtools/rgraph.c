
/* 
 * Rajiv Wickremesinghe 2/2003
 */


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "rgraph.h"

void
gInit(graph_t *g) {
  TAILQ_INIT(&g->nodes);
  g->current_id = 1;
}


void
gInitFromList(graph_t *g, graph_t *src) {
  node_t *prev=NULL, *node, *np;

  gInit(g);
  GLIST(src, np) {
    node = gNewNode(g, np->label);
    if(prev) gAddEdge(g, prev, node);
    prev = node;
  }
}

void
gClear(graph_t *g) {
  node_t *np;

  GFOREACH(g, np) {
    free(np);
  }  
}

node_t *
gNewNode(graph_t *g, char *label) {
  node_t *np;
  np = (node_t *)malloc(sizeof(node_t));
  assert(np);
  np->id = g->current_id++;
  np->label = strdup(label);
  TAILQ_INIT(&np->edges);

  TAILQ_INSERT_TAIL(&g->nodes, np, link);
  return np;
}


void
gAddEdge(graph_t *g, node_t *u, node_t *v) {
  edge_t *ep;

  ep = (edge_t *)malloc(sizeof(edge_t));
  assert(ep);
  ep->node = v;

  TAILQ_INSERT_TAIL(&u->edges, ep, link);
}



void
gPrintNode(node_t *np) {
  edge_t *ep;

  printf("{<%s>, ", np->label);

  printf("E(");
  TAILQ_FOREACH(ep, &np->edges, link) {
    printf(" <%s>", ep->node->label);
  }
  printf(" )}\n");
}


/* ********************************************************************** */

#define print_list(list,link) 			\
{						\
  node_t *np;					\
  TAILQ_FOREACH(np, (list), link) {		\
    gPrintNode(np);				\
  }						\
}

void
gPrint(graph_t *g) {
  //printf("nodes:\n");
  print_list(&g->nodes, link);
}


/* ********************************************************************** */

int
dfs_visit(node_t *np, int *time) {
  int loop = 0;
  edge_t *ep;

  switch(np->color) {
  case 0:
    np->td = ++(*time);
    np->color = 1;
    TAILQ_FOREACH(ep, &np->edges, link) {
      loop += dfs_visit(ep->node, time);
    }
    np->tf = ++(*time);
    break;
  case 1:
    loop = 1;
    break;
  case 2:
    break;
  default:
    assert(0);
  }
  return loop;
}


void
gDFS(graph_t *g) {
  node_t *np;
  int time=0;
  int loop = 0;

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }
  TAILQ_FOREACH(np, &g->nodes, link) {
    loop = dfs_visit(np, &time);
  }
  if(loop) {
    fprintf(stderr, "loop detected\n");
  }
}

/* ********************************************************************** */

static int
topo_visit(node_t *np, nodelist_t *list) {
  int loop = 0;
  edge_t *ep;

  if(np->color == 1) return 1;	/* loop */

  if(np->color == 2) return 0;	/* bottom of recursion */

  np->color = 1;
  //fprintf(stderr, "visiting %s\n", np->label);
  TAILQ_FOREACH(ep, &np->edges, link) {
    loop += topo_visit(ep->node, list);
  }
  np->color = 2;
  if(loop) return loop;

  //fprintf(stderr, "added %s\n", np->label);
  TAILQ_INSERT_HEAD(list, np, olink);

  return 0;
}


int
gTopoSort(graph_t *g) {
  node_t *np;
  int loop = 0;

  TAILQ_INIT(&g->olist);

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }
  TAILQ_FOREACH(np, &g->nodes, link) {
    if(np->color == 0) loop += topo_visit(np, &g->olist);
  }

  if(loop) {
    fprintf(stderr, "loop detected while doing TopoSort\n");
    return 1;
  }

  //printf("topo list:\n");
  //print_list(&g->olist, olink);
  return 0;
}


/* ********************************************************************** */

typedef struct pq_elm_t {
  int prio;
  void *ptr;
} pq_elm_t;

typedef struct pq_t {
  int len;
  int capacity;
  pq_elm_t heap[0];		/* variable size struct */
} pq_t;


static void
heapify() {
}

pq_t *
pqNew(int n) {
  pq_t *this;

  this = (pq_t *)malloc(sizeof(pq_t) + sizeof(pq_elm_t) * n);
  assert(this);
  this->capacity = n;
  this->len = 0;
  return this;
}

void
pqDelete(pq_t *this) {
  free(this);
}


void
pqInsert(pq_t *pq, node_t *ptr, int prio) {

  assert(pq->len < pq->capacity);
  pq->heap[pq->len].prio = prio;
  pq->heap[pq->len].ptr = ptr;
  pq->len++;

  heapify();

}

void
pqReduce(pq_t *pq, void *ptr, int prio) {


  heapify();
}

void *
pqDeleteMin(pq_t *pq) {
  heapify();
}

int
pqEmpty(pq_t *pq) {
  return pq->len == 0;
}

/* ********************************************************************** */

void
gSSSP(graph_t *g, node_t *src) {
  node_t *np;
  edge_t *ep;
  pq_t *pq;

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->td = INT_MAX;		/* inf depth */
    np->color = 0;		/* not visited */
    pqInsert(pq, np, INT_MAX);
  }
  src->td = 0;
  pqReduce(pq, src, 0);
  
  while(!pqEmpty(pq)) {
    np = pqDeleteMin(pq);
    np->color = 1;
    TAILQ_FOREACH(ep, &np->edges, link) {    
      if(ep->node->color == 0) {
	if(ep->node->td > np->td + ep->node->val) {
	  ep->node->td = np->td + ep->node->val;
	  pqReduce(pq, ep->node, ep->node->td);
	  ep->node->visit = np;
	}
      }
    }
  
  }
}


/* ********************************************************************** */

static void
export_node(FILE *fp, node_t *np, int indent) {
  edge_t *ep;
  int count = 0;
  if(np->color) return;
    
  np->color = 1;

  fprintf(fp, "l(\"%d\",n(\"A\",", np->id);
  //fprintf(fp, "[a(\"OBJECT\",\"%s\")],\n", np->label);
  fprintf(fp, "[a(\"OBJECT\",\"%s\")", np->label);
  //fprintf(fp, ",\na(\"COLOR\",\"red\")");
  fprintf(fp, "],\n");

  /* export edges */
  fprintf(fp, "\t[");
  TAILQ_FOREACH(ep, &np->edges, link) {
    if(count++) fprintf(fp, ",\n\t");
    fprintf(fp, "l(\"%d-%d\",e(\"B\",[],r(\"%d\")))",
	    np->id, ep->node->id, ep->node->id);
    //export_node(fp, ep->node, indent+1);    
  }
  fprintf(fp, "\n\t]))\n");
}


/* export in daVinci format */
void
gExport(graph_t *g, char *filename) {
  node_t *np;
  int indent = 0;
  FILE *fp;
  int count = 0;

  fp = fopen(filename, "w");
  if(!fp) {
    perror(filename);
    exit(1);
  }

  TAILQ_FOREACH(np, &g->nodes, link) {
    np->color = 0;
  }
  fprintf(fp, "[\n");
  TAILQ_FOREACH(np, &g->nodes, link) {
    if(count++) fprintf(fp, ",");
    fprintf(fp, "\n");
    export_node(fp, np, indent);
  }
  fprintf(fp, "]\n");

  fclose(fp);
}

/* ********************************************************************** */

#if 0
int
main(int argc, char **argv) {
  graph_t g;
  node_t *node[10];
  int i;
  char buf[BUFSIZ];
  char *filename = "export.daVinci";

  gInit(&g);
  for(i=0; i<10; i++) {
    sprintf(buf, "node%d", i);
    node[i] = gNewNode(&g, buf);
  }
  gAddEdge(&g, node[1], node[2]);
  gAddEdge(&g, node[2], node[3]);
  gAddEdge(&g, node[3], node[4]);
  gAddEdge(&g, node[1], node[4]);
  gAddEdge(&g, node[4], node[5]);
  gAddEdge(&g, node[4], node[6]);
  gPrint(&g);

  gTopoSort(&g);

  fprintf(stderr, "export to %s\n", filename);
  gExport(&g, filename);

  return 0;
}
#endif
